// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***********************************************************************************

 Multi Game Hunter floppy board

 The copier's floppy drive hangs off a daughterboard with its own P87C51, an
 MCS3201 FDC and 32KB of SRAM. Both BIOSes (Mega Drive: Z80 $8004/$8005,
 SNES: 65816 $03:5804/$5805) talk to it through two ports that work like a
 765's main status and data registers, with a cut-down command set:
   status  r: bit 7 = ready for a byte, bit 6 = direction (1 = board to BIOS)
   data    rw
   commands:
     $00 d        disk type: 0 = 720K, 1 = 800K, 2 = 1.44M, 3 = 1.6M
     $02          format the disk as the type set, then write an empty FAT12
                  file system on it ($FF per cylinder as it goes, then $00)
     $04          drive status: ST3-like byte (bit 6 = write protect), result
     $05 t h s    write sector: 512 bytes in, then result
     $06 t h s    read sector: 512 bytes out, then result
     $0F t        seek, result
   the SNES BIOS also sends:
     $01          three bytes, result
     $0B          sync: reply ends $55 $AA (sent until it does)
     $0D t        seek, result
     $0E          result, after writes
     $15/$16      as $05/$06
   result 0 = OK.

 The 87C51 is read protected (it reads back as $FF with the lock bits set),
 so its firmware is not dumped. This device replaces it with a high-level
 sequencer: each command, worked out from what the BIOSes send and expect,
 becomes a list of operations on the MCS3201. Reads go through a cache of
 one side of a track, as the BIOS asks for one sector at a time. The host
 ports are the boundary a real 87C51 would sit behind.

 TODO:
 - dump the 87C51 and emulate it
 - what command $01's three bytes mean
 - whether the firmware caches tracks, and its real timing

***********************************************************************************/

#include "emu.h"
#include "mghfdd.h"

#define VERBOSE 0
#include "logmacro.h"


namespace {

// disk types, as the BIOS's BPB templates ($3468): format geometry and the
// BPB from byte 13 of the boot sector (sectors per cluster to heads)
struct disk_type
{
	uint8_t cylinders, sectors, gap, ccr;
	uint8_t bpb[15];
};

constexpr disk_type DISK_TYPES[4] =
{
	{ 80,  9, 0x50, 0x02, { 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xa0, 0x05, 0xf9, 0x03, 0x00, 0x09, 0x00, 0x02, 0x00 } },
	{ 80, 10, 0x2a, 0x02, { 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0x40, 0x06, 0xf9, 0x03, 0x00, 0x0a, 0x00, 0x02, 0x00 } },
	{ 80, 18, 0x54, 0x00, { 0x01, 0x01, 0x00, 0x02, 0xe0, 0x00, 0x40, 0x0b, 0xf0, 0x09, 0x00, 0x12, 0x00, 0x02, 0x00 } },
	{ 82, 20, 0x0c, 0x00, { 0x01, 0x01, 0x00, 0x02, 0x40, 0x00, 0xd0, 0x0c, 0xf0, 0x0a, 0x00, 0x14, 0x00, 0x02, 0x00 } },
};

// command lengths, including the command byte
constexpr uint8_t CMD_LENGTHS[16] = { 2, 1, 1, 1, 1, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 2 };

void mgh_floppies(device_slot_interface &device)
{
	device.option_add("35hd", FLOPPY_35_HD);
	device.option_add("35dd", FLOPPY_35_DD);
}

ROM_START( mgh_fdd )
	ROM_REGION( 0x1000, "mcu", 0 )
	ROM_LOAD( "p87c51.u1", 0x0000, 0x1000, NO_DUMP )
ROM_END

} // anonymous namespace


DEFINE_DEVICE_TYPE(MGH_FDD, mgh_fdd_device, "mgh_fdd", "Multi Game Hunter floppy board")

mgh_fdd_device::mgh_fdd_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, MGH_FDD, tag, owner, clock)
	, m_fdc(*this, "fdc")
	, m_floppy(*this, "fdc:0")
	, m_step_timer(nullptr)
	, m_motor_timer(nullptr)
	, m_cmd_len(0)
	, m_reply_len(0)
	, m_reply_pos(0)
	, m_wcount(0)
	, m_prog_cmd_len(0)
	, m_pc(0)
	, m_count(0)
	, m_wait(0)
	, m_busy(false)
	, m_out_pos(0)
	, m_cache_valid(false)
	, m_cache_track(0)
	, m_cache_head(0)
	, m_fdc_irq(false)
	, m_density(0)
{
}

const tiny_rom_entry *mgh_fdd_device::device_rom_region() const
{
	return ROM_NAME( mgh_fdd );
}

void mgh_fdd_device::device_add_mconfig(machine_config &config)
{
	MCS3201(config, m_fdc, 24_MHz_XTAL);
	m_fdc->intrq_wr_callback().set(FUNC(mgh_fdd_device::fdc_irq_w));

	FLOPPY_CONNECTOR(config, m_floppy, mgh_floppies, "35hd", floppy_image_device::default_pc_floppy_formats).enable_sound(true);
}

void mgh_fdd_device::device_start()
{
	m_step_timer = timer_alloc(FUNC(mgh_fdd_device::step), this);
	m_motor_timer = timer_alloc(FUNC(mgh_fdd_device::motor_off), this);

	if (floppy_image_device *const floppy = m_floppy->get_device())
	{
		floppy->setup_load_cb(floppy_image_device::load_cb(&mgh_fdd_device::disk_changed, this));
		floppy->setup_unload_cb(floppy_image_device::unload_cb(&mgh_fdd_device::disk_changed, this));
	}

	std::fill(std::begin(m_cmd), std::end(m_cmd), 0);
	std::fill(std::begin(m_prog_cmd), std::end(m_prog_cmd), 0);
	std::fill(std::begin(m_res), std::end(m_res), 0);

	save_item(NAME(m_cmd));
	save_item(NAME(m_cmd_len));
	save_item(NAME(m_reply));
	save_item(NAME(m_reply_len));
	save_item(NAME(m_reply_pos));
	save_item(NAME(m_wbuf));
	save_item(NAME(m_wcount));
	save_item(NAME(m_prog_cmd));
	save_item(NAME(m_prog_cmd_len));
	save_item(NAME(m_pc));
	save_item(NAME(m_count));
	save_item(NAME(m_wait));
	save_item(NAME(m_busy));
	save_item(NAME(m_out_pos));
	save_item(NAME(m_res));
	save_item(NAME(m_cache));
	save_item(NAME(m_cache_valid));
	save_item(NAME(m_cache_track));
	save_item(NAME(m_cache_head));
	save_item(NAME(m_fdc_irq));
	save_item(NAME(m_density));
}

void mgh_fdd_device::device_reset()
{
	m_cmd_len = 0;
	m_reply_len = 0;
	m_reply_pos = 0;
	m_wcount = 0;
	m_busy = false;
	m_density = 0;
	m_cache_valid = false;
	m_prog_op.clear();
	m_prog_arg.clear();
	m_out.clear();
	m_err.clear();
	m_step_timer->adjust(attotime::never);
	m_motor_timer->adjust(attotime::never);
}

// the FDC resets after its parent: motor off, out of reset
void mgh_fdd_device::device_reset_after_children()
{
	m_fdc->dor_w(0x0c);
}

// The operation list is not saved: rebuild it from the command
void mgh_fdd_device::device_post_load()
{
	if (m_busy)
		build_program();
}

void mgh_fdd_device::disk_changed(floppy_image_device *floppy)
{
	m_cache_valid = false;
}

uint8_t mgh_fdd_device::status_r()
{
	if (m_reply_pos < m_reply_len)
		return 0xc0;
	return m_busy ? 0x00 : 0x80;
}

uint8_t mgh_fdd_device::data_r()
{
	if (m_reply_pos >= m_reply_len)
		return 0xff;
	uint8_t const data = m_reply[m_reply_pos];
	if (!machine().side_effects_disabled() && ++m_reply_pos == m_reply_len)
		m_reply_len = m_reply_pos = 0;
	return data;
}

void mgh_fdd_device::data_w(uint8_t data)
{
	if (m_busy)
	{
		LOG("%s: busy, %02x ignored\n", machine().describe_context(), data);
		return;
	}
	if (m_wcount)
	{
		// sector data after a write command
		m_wbuf[SECTOR - m_wcount] = data;
		if (!--m_wcount)
		{
			build_program();
			run();
		}
		return;
	}
	m_cmd[m_cmd_len++] = data;
	if (m_cmd_len >= CMD_LENGTHS[m_cmd[0] & 0x0f])
	{
		command();
		m_cmd_len = 0;
	}
}

void mgh_fdd_device::reply(uint8_t data)
{
	if (m_reply_len < REPLY_MAX)
		m_reply[m_reply_len++] = data;
}

void mgh_fdd_device::reply_error()
{
	for (uint8_t b : m_err)
		reply(b);
}

void mgh_fdd_device::op(uint8_t code, uint16_t arg)
{
	m_prog_op.push_back(code);
	m_prog_arg.push_back(arg);
}

void mgh_fdd_device::op_out(std::initializer_list<uint8_t> bytes)
{
	for (uint8_t b : bytes)
		op(OP_OUT, b);
}

void mgh_fdd_device::op_seek(uint8_t track)
{
	if (track)
		op_out({ 0x0f, 0x00, track });
	else
	{
		// a recalibrate gives up after 77 steps, short of the far
		// cylinders, so do it twice (as PC BIOSes do)
		op_out({ 0x07, 0x00 });
		op(OP_WAIT_IRQ);
		op_out({ 0x08 });
		op(OP_RESULT, 2);
		op_out({ 0x07, 0x00 });
	}
	op(OP_WAIT_IRQ);
	op_out({ 0x08 });
	op(OP_RESULT, 2);
	op(OP_CHECK_SEEK);
}

// A command as it arrives: state changes and immediate replies, then the
// operation list for anything that needs the FDC
void mgh_fdd_device::command()
{
	if (VERBOSE)
	{
		std::string args;
		for (unsigned i = 0; i < m_cmd_len; i++)
			args += util::string_format(" %02x", m_cmd[i]);
		LOG("%s: command%s\n", machine().describe_context(), args);
	}

	std::copy_n(m_cmd, m_cmd_len, m_prog_cmd);
	m_prog_cmd_len = m_cmd_len;

	uint8_t const t = m_cmd_len > 1 ? m_cmd[1] : 0;
	uint8_t const h = m_cmd_len > 2 ? m_cmd[2] & 1 : 0;
	uint8_t const sec = m_cmd_len > 3 ? m_cmd[3] : 1;
	switch (m_cmd[0])
	{
	case 0x00: // disk type
		m_density = t & 3;
		m_cache_valid = false;
		break;

	case 0x02: // format
	case 0x04: // drive status: the start of a disk operation, the disk may have been changed
	case 0x0d: // seek
	case 0x0f:
		m_cache_valid = false;
		break;

	case 0x05: // write sector: build and run once the 512 bytes are in
	case 0x15:
		m_cache_valid = false;
		m_wcount = SECTOR;
		return;

	case 0x06: // read sector
	case 0x16:
		if (!sec || sec > DISK_TYPES[m_density].sectors)
		{
			for (unsigned i = 0; i < SECTOR; i++)
				reply(0x00);
			reply(0x01);
			return;
		}
		if (m_cache_valid && m_cache_track == t && m_cache_head == h)
		{
			for (unsigned i = 0; i < SECTOR; i++)
				reply(m_cache[(sec - 1) * SECTOR + i]);
			reply(0x00);
			return;
		}
		m_cache_valid = false;
		m_cache_track = t;
		m_cache_head = h;
		break;

	case 0x01:
	case 0x0b:
	case 0x0e:
		break;

	default:
		LOG("unknown command %02x\n", m_cmd[0]);
		reply(0x01);
		return;
	}
	build_program();
	run();
}

// The operation list for m_prog_cmd. It depends only on the command, the
// disk type and the write data, so it can be rebuilt after a state load.
void mgh_fdd_device::build_program()
{
	m_prog_op.clear();
	m_prog_arg.clear();
	m_out.clear();
	m_err.clear();

	uint8_t const cmd = m_prog_cmd[0];
	uint8_t const t = m_prog_cmd_len > 1 ? m_prog_cmd[1] : 0;
	uint8_t const h = m_prog_cmd_len > 2 ? m_prog_cmd[2] & 1 : 0;
	uint8_t const sec = m_prog_cmd_len > 3 ? m_prog_cmd[3] : 1;
	disk_type const &type = DISK_TYPES[m_density];

	// motor on, drive 0; non-DMA
	op(OP_DOR, 0x1c);
	op_out({ 0x03, 0xdf, 0x03 });

	switch (cmd)
	{
	case 0x00: // disk type
		op(OP_CCR, type.ccr);
		op(OP_REPLY, 0x00);
		break;

	case 0x01: // three bytes, then the result
		op(OP_REPLY, 0x01);
		op(OP_REPLY, 0x00);
		op(OP_REPLY, 0x00);
		op(OP_REPLY, 0x00);
		break;

	case 0x02: // format, then an empty file system
	{
		uint8_t const sc = type.sectors;
		for (int c = 0; c < type.cylinders; c++)
		{
			op_seek(c);
			for (int hd = 0; hd < 2; hd++)
			{
				op_out({ 0x4d, uint8_t(hd << 2), 0x02, sc, type.gap, 0xf6 });
				for (int r = 1; r <= sc; r++)
					m_out.insert(m_out.end(), { uint8_t(c), uint8_t(hd), uint8_t(r), 0x02 });
				op(OP_WRITE, sc * 4);
				op(OP_RESULT, 7);
				op(OP_CHECK_RW);
			}
			if (c < type.cylinders - 1)
				op(OP_REPLY, 0xff);
		}

		// boot sector, both FATs, root directory
		uint8_t const *const bpb = type.bpb;
		unsigned const fat_sectors = bpb[9] | bpb[10] << 8;
		unsigned const root_sectors = (bpb[4] | bpb[5] << 8) / 16;
		unsigned const system_sectors = 1 + bpb[3] * fat_sectors + root_sectors;
		for (unsigned lba = 0; lba < system_sectors; lba++)
		{
			uint8_t sector[SECTOR] = { };
			if (!lba)
			{
				static constexpr uint8_t head[13] = { 0xeb, 0x3c, 0x90, 'M', 'G', 'H', ' ', ' ', ' ', ' ', ' ', 0x00, 0x02 };
				std::copy(std::begin(head), std::end(head), sector);
				std::copy_n(bpb, 15, sector + 13);
				sector[510] = 0x55;
				sector[511] = 0xaa;
			}
			else if (lba < 1 + bpb[3] * fat_sectors && !((lba - 1) % fat_sectors))
			{
				sector[0] = bpb[8];
				sector[1] = 0xff;
				sector[2] = 0xff;
			}
			uint8_t const c = lba / (sc * 2), hd = (lba / sc) & 1, r = lba % sc + 1;
			op_seek(c);
			op_out({ 0x45, uint8_t(hd << 2), c, hd, r, 0x02, r, 0x1b, 0xff });
			m_out.insert(m_out.end(), std::begin(sector), std::end(sector));
			op(OP_WRITE_TC, SECTOR);
			op(OP_RESULT, 7);
			op(OP_CHECK_RW);
		}
		op(OP_REPLY, 0x00);
		m_err.push_back(0x01);
		break;
	}

	case 0x04: // drive status
		op_out({ 0x04, 0x00 });
		op(OP_RESULT, 1);
		op(OP_REPLY_ST3);
		op(OP_REPLY_DISK);
		m_err = { 0x00, 0x01 };
		break;

	case 0x05: // write sector
	case 0x15:
		op_seek(t);
		op_out({ 0x45, uint8_t(h << 2), t, h, sec, 0x02, sec, 0x1b, 0xff });
		m_out.insert(m_out.end(), std::begin(m_wbuf), std::end(m_wbuf));
		op(OP_WRITE_TC, SECTOR);
		op(OP_RESULT, 7);
		op(OP_CHECK_RW);
		op(OP_REPLY, 0x00);
		m_err.push_back(0x01);
		break;

	case 0x06: // read sector: the whole side of the track, into the cache
	case 0x16:
		m_err.insert(m_err.end(), SECTOR, 0x00);
		m_err.push_back(0x01);
		op_seek(t);
		op_out({ 0x46, uint8_t(h << 2), t, h, 1, 0x02, type.sectors, 0x1b, 0xff });
		op(OP_READ, type.sectors * SECTOR);
		op(OP_RESULT, 7);
		op(OP_CHECK_RW);
		op(OP_CACHE_VALID);
		op(OP_REPLY_SECTOR, sec);
		op(OP_REPLY, 0x00);
		break;

	case 0x0b: // sync: the SNES BIOS sends it until the reply ends $55 $AA
		op(OP_REPLY, 0x00);
		op(OP_REPLY, 0x55);
		op(OP_REPLY, 0xaa);
		break;

	case 0x0e: // after writes: sectors are written straight away here
		op(OP_REPLY, 0x00);
		break;

	case 0x0d: // seek
	case 0x0f:
		op_seek(t);
		op(OP_REPLY, 0x00);
		m_err.push_back(0x01);
		break;
	}
}

void mgh_fdd_device::run()
{
	m_pc = 0;
	m_count = 0;
	m_out_pos = 0;
	m_wait = 0;
	m_busy = true;
	m_motor_timer->adjust(attotime::never);
	m_step_timer->adjust(attotime::from_usec(4), 0, attotime::from_usec(4));
}

void mgh_fdd_device::done()
{
	m_busy = false;
	m_step_timer->adjust(attotime::never);
	m_motor_timer->adjust(attotime::from_seconds(2));
}

void mgh_fdd_device::fail()
{
	LOG("command %02x failed at step %d (op %d), ST0 %02x ST1 %02x ST2 %02x\n",
			m_prog_cmd[0], m_pc, m_prog_op[m_pc], m_res[0], m_res[1], m_res[2]);
	// reset the FDC, as a timed-out command may still be running
	m_fdc->dor_w(0x18);
	m_fdc->dor_w(0x1c);
	reply_error();
	done();
}

// TODO: run from the FDC's interrupt and data request rather than a timer
TIMER_CALLBACK_MEMBER(mgh_fdd_device::step)
{
	for (int i = 0; i < 16 && m_busy; i++)
	{
		if (m_pc >= m_prog_op.size())
		{
			done();
			return;
		}
		int const r = step_op();
		if (r < 0)
		{
			fail();
			return;
		}
		if (!r)
		{
			// a second and a half without progress: no disk, or a dead drive
			if (++m_wait > 375000)
				fail();
			return;
		}
		m_wait = 0;
	}
}

// runs one step of the current operation: 1 = progress, 0 = waiting, -1 = failed
int mgh_fdd_device::step_op()
{
	uint16_t const arg = m_prog_arg[m_pc];
	uint8_t const msr = m_fdc->msr_r();
	switch (m_prog_op[m_pc])
	{
	case OP_DOR:
		m_fdc->dor_w(arg);
		break;

	case OP_CCR:
		m_fdc->ccr_w(arg);
		break;

	case OP_OUT:
		if ((msr & 0xc0) != 0x80)
			return 0;
		m_fdc->fifo_w(arg);
		break;

	case OP_WAIT_IRQ:
		if (!m_fdc_irq)
			return 0;
		break;

	case OP_RESULT:
		if ((msr & 0xc0) != 0xc0)
			return 0;
		m_res[m_count] = m_fdc->fifo_r();
		if (++m_count < arg)
			return 1;
		m_count = 0;
		break;

	case OP_READ:
		if ((msr & 0xe0) == 0xe0)
		{
			m_cache[m_count] = m_fdc->fifo_r();
			if (++m_count < arg)
				return 1;
			m_fdc->tc_w(true);
			m_fdc->tc_w(false);
		}
		else if ((msr & 0xe0) != 0xc0)
			return 0;
		// (result phase already: the read stopped early, the result says why)
		m_count = 0;
		break;

	case OP_WRITE:
	case OP_WRITE_TC:
		if ((msr & 0xe0) == 0xa0)
		{
			m_fdc->fifo_w(m_out_pos < m_out.size() ? m_out[m_out_pos] : 0);
			m_out_pos++;
			if (++m_count < arg)
				return 1;
			if (m_prog_op[m_pc] == OP_WRITE_TC)
			{
				m_fdc->tc_w(true);
				m_fdc->tc_w(false);
			}
		}
		else if ((msr & 0xe0) != 0xc0)
			return 0;
		m_count = 0;
		break;

	case OP_CHECK_SEEK:
		if ((m_res[0] & 0xe0) != 0x20)
			return -1;
		break;

	case OP_CHECK_RW:
		if (m_res[0] & 0xc0)
			return -1;
		break;

	case OP_REPLY:
		reply(arg);
		break;

	case OP_REPLY_SECTOR:
		for (unsigned i = 0; i < SECTOR; i++)
			reply(m_cache[(arg - 1) * SECTOR + i]);
		break;

	case OP_CACHE_VALID:
		m_cache_valid = true;
		break;

	case OP_REPLY_ST3:
		reply(m_res[0]);
		break;

	case OP_REPLY_DISK:
		reply(m_floppy->get_device() && m_floppy->get_device()->exists() ? 0x00 : 0x01);
		break;
	}
	m_pc++;
	return 1;
}

TIMER_CALLBACK_MEMBER(mgh_fdd_device::motor_off)
{
	m_fdc->dor_w(0x0c);
}

void mgh_fdd_device::fdc_irq_w(int state)
{
	m_fdc_irq = state;
}

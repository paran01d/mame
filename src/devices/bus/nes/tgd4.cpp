// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***************************************************************************

  Venus Turbo Game Doctor 4+

  A Famicom floppy copier in a cartridge case: it plugs into the Famicom,
  and the FDS RAM adapter plugs into it. It has an 8KB BIOS, 512KB of PRG
  DRAM and 32KB of CHR RAM, and loads Game Doctor (and Magic Card) disks
  through the FDS.

  It passes everything through to the RAM adapter until the RAM check
  button is held at reset, or the FDS BIOS writes the first byte of a
  doctor header to $43FF or $4FFF. Then it maps in its BIOS at $E000 (and
  BIOS $0800-$0FFF at $4800) and a 32KB DRAM window at $6000-$DFFF.

  The BIOS loads the game, writes the mode byte to $42FC-$42FF and jumps
  through the game's reset vector. From then on the copier plays the game:
  DRAM through the latch modes below at $8000-$FFFF, adapter RAM at $6000,
  CHR RAM through the latch or 1KB banks.

  Registers ($4100-$5FFF):
    $42FC-$42FF  mode: bits 7-5 latch mode, bit 4 write protect; the
                 address's low bits select the mirroring
    $43FE        2M mode: bit 5 A17, bits 1-0 CHR bank
    $43FF        load window (bits 7-4) and CHR bank
    $4400-$4407  1KB CHR banks (with $4411 bit 6)
    $440C/$440D  IRQ counter: counts up on M2 while negative, IRQ at $FFFF
    $4410-$4415  configuration ($4411 bit 7: 4M mode, bit 6: 1KB CHR,
                 bits 3-2 and 1-0: adapter RAM at $6000 in load mode)
    $4420        read: a guess, see read_l()

  TODO:
  - what actually starts play mode: here it is the reset vector fetch
    after a mode write, which is what the BIOS does to start a game
  - what $4420 really returns (the scanline here is a guess)
  - how the load window decodes CHR for Magic Card files
  - the Game Saver connector

***************************************************************************/

#include "emu.h"
#include "tgd4.h"

#include "disksys.h"
#include "nes_carts.h"

#define VERBOSE 0
#include "logmacro.h"


namespace {

ROM_START( tgd4 )
	ROM_REGION( 0x2000, "bios", 0 )
	ROM_LOAD( "tgd4.bin", 0x0000, 0x2000, CRC(e76b974a) SHA1(136216daccd8945d79b9742393596734a695b5e5) )
ROM_END

INPUT_PORTS_START( tgd4 )
	PORT_START("CFG")
	PORT_DIPNAME(0x01, 0x00, "RAM Check at Reset")
	PORT_DIPSETTING(0x00, DEF_STR(Off))
	PORT_DIPSETTING(0x01, DEF_STR(On))
	// not part of the copier: turns to the next side of the disk image
	// when the BIOS waits for one
	PORT_CONFNAME(0x02, 0x02, "Automatic Disk Swap")
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x02, DEF_STR(On))
INPUT_PORTS_END


class nes_tgd4_device : public device_t, public device_nes_cart_interface
{
public:
	nes_tgd4_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	virtual bool has_internal_bios() const override { return true; }

	virtual u8 read_ex(offs_t offset) override;
	virtual u8 read_l(offs_t offset) override;
	virtual u8 read_m(offs_t offset) override;
	virtual u8 read_h(offs_t offset) override;
	virtual void write_ex(offs_t offset, u8 data) override;
	virtual void write_l(offs_t offset, u8 data) override;
	virtual void write_m(offs_t offset, u8 data) override;
	virtual void write_h(offs_t offset, u8 data) override;

	virtual u8 chr_r(offs_t offset) override;
	virtual void chr_w(offs_t offset, u8 data) override;
	virtual u8 nt_r(offs_t offset) override;
	virtual void nt_w(offs_t offset, u8 data) override;

	virtual void hblank_irq(int scanline, bool vblank, bool blanked) override;
	virtual void scanline_irq(int scanline, bool vblank, bool blanked) override;
	virtual void ppu_latch(offs_t offset) override;

	virtual void pcb_start(running_machine &machine, u8 *ciram_ptr, bool cart_mounted) override;
	virtual void pcb_reset() override;
	virtual void disk_flip_side() override;

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

private:
	device_nes_cart_interface *fds() const { return m_fdsslot->m_cart; }
	void map_in();
	void enter_play();
	u32 dram_offset(u16 cpu_addr) const;
	u32 load_chr_bank() const;
	u32 play_prg_offset(u16 cpu_addr) const;
	u32 play_chr_offset(offs_t offset) const;
	u32 dram8(u32 bank8) const;
	void apply_latch(u8 data);
	int nt_page(offs_t offset) const;
	void auto_swap();

	// IRQ: the copier's counter and the RAM adapter share the CPU's line
	void fds_irq_w(int state);
	void update_irq();
	u16 irq_count() const;
	void irq_sync();
	void irq_schedule();
	TIMER_CALLBACK_MEMBER(irq_expired);

	required_device<nes_cart_slot_device> m_fdsslot;
	required_region_ptr<u8> m_bios;
	required_ioport m_cfg;

	std::unique_ptr<u8[]> m_dram; // 512KB PRG DRAM
	std::unique_ptr<u8[]> m_aram; // 32KB adapter RAM (4 x 8KB at $6000)
	std::unique_ptr<u8[]> m_cram; // 32KB CHR RAM
	emu_timer *m_irq_timer;

	bool m_mapped;      // BIOS and DRAM in CPU space (load or play)
	bool m_play;        // playing: BIOS unmapped from $E000, latch banking
	bool m_play_pending; // a mode write: play starts at the reset vector fetch
	u32 m_slot128;      // 128KB PRG slot taken at play entry
	u8 m_prg_window;    // last $43FF used for a DRAM PRG write in load mode
	u8 m_latch;
	u8 m_chr8;          // last 8KB CHR bank from the latch
	u8 m_latch_mode;    // $42FF bits 7-5
	u8 m_mirror;        // $42FC-$42FF address bits 1-0
	bool m_prg_protect;
	u8 m_42ff;
	u8 m_43ff;
	u8 m_4410;
	u8 m_4411;
	u8 m_4412;
	u8 m_4413;
	u8 m_4414;
	u8 m_4415;
	u8 m_prg8[4];       // 2M/4M 8KB PRG banks (PPPPPPCC)
	u8 m_chr1k[8];      // $4400-$4407
	bool m_2m;
	u8 m_2m_a17;
	u16 m_irq_latch;
	u16 m_irq_count;    // the counter at m_irq_base
	attotime m_irq_base;
	bool m_irq_on;
	bool m_irq_asserted;
	bool m_fds_irq;
	int m_scanline;     // the PPU's current scanline, for $4420

	// automatic disk swap
	u16 m_status_reads; // $4032 reads this frame
	u16 m_data_reads;   // $4031 reads this frame
	u8 m_idle_frames;   // frames of status polling with no data read
};


nes_tgd4_device::nes_tgd4_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, NES_TGD4, tag, owner, clock)
	, device_nes_cart_interface(mconfig, *this)
	, m_fdsslot(*this, "fds")
	, m_bios(*this, "bios")
	, m_cfg(*this, "CFG")
	, m_irq_timer(nullptr)
	, m_mapped(false)
	, m_play(false)
	, m_play_pending(false)
	, m_slot128(0)
	, m_prg_window(0)
	, m_latch(0)
	, m_chr8(0)
	, m_latch_mode(4)
	, m_mirror(3)
	, m_prg_protect(false)
	, m_42ff(0)
	, m_43ff(0)
	, m_4410(0)
	, m_4411(0)
	, m_4412(0)
	, m_4413(0)
	, m_4414(0)
	, m_4415(0)
	, m_2m(false)
	, m_2m_a17(0)
	, m_irq_latch(0)
	, m_irq_count(0)
	, m_irq_on(false)
	, m_irq_asserted(false)
	, m_fds_irq(false)
	, m_scanline(0)
	, m_status_reads(0)
	, m_data_reads(0)
	, m_idle_frames(0)
{
}

void nes_tgd4_device::device_add_mconfig(machine_config &config)
{
	NES_CART_SLOT(config, m_fdsslot, DERIVED_CLOCK(1, 1), disksys_only, "disksys").set_fixed(true);
	m_fdsslot->irq_callback().set(FUNC(nes_tgd4_device::fds_irq_w));
}

ioport_constructor nes_tgd4_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(tgd4);
}

const tiny_rom_entry *nes_tgd4_device::device_rom_region() const
{
	return ROM_NAME(tgd4);
}

void nes_tgd4_device::device_start()
{
	m_dram = make_unique_clear<u8[]>(0x80000);
	m_aram = make_unique_clear<u8[]>(0x8000);
	m_cram = make_unique_clear<u8[]>(0x8000);
	m_irq_timer = timer_alloc(FUNC(nes_tgd4_device::irq_expired), this);

	save_pointer(NAME(m_dram), 0x80000);
	save_pointer(NAME(m_aram), 0x8000);
	save_pointer(NAME(m_cram), 0x8000);
	save_item(NAME(m_mapped));
	save_item(NAME(m_play));
	save_item(NAME(m_play_pending));
	save_item(NAME(m_slot128));
	save_item(NAME(m_prg_window));
	save_item(NAME(m_latch));
	save_item(NAME(m_chr8));
	save_item(NAME(m_latch_mode));
	save_item(NAME(m_mirror));
	save_item(NAME(m_prg_protect));
	save_item(NAME(m_42ff));
	save_item(NAME(m_43ff));
	save_item(NAME(m_4410));
	save_item(NAME(m_4411));
	save_item(NAME(m_4412));
	save_item(NAME(m_4413));
	save_item(NAME(m_4414));
	save_item(NAME(m_4415));
	save_item(NAME(m_prg8));
	save_item(NAME(m_chr1k));
	save_item(NAME(m_2m));
	save_item(NAME(m_2m_a17));
	save_item(NAME(m_irq_latch));
	save_item(NAME(m_irq_count));
	save_item(NAME(m_irq_base));
	save_item(NAME(m_irq_on));
	save_item(NAME(m_irq_asserted));
	save_item(NAME(m_fds_irq));
	save_item(NAME(m_scanline));
	save_item(NAME(m_status_reads));
	save_item(NAME(m_data_reads));
	save_item(NAME(m_idle_frames));
}

void nes_tgd4_device::pcb_start(running_machine &machine, u8 *ciram_ptr, bool cart_mounted)
{
	device_nes_cart_interface::pcb_start(machine, ciram_ptr, cart_mounted);
	fds()->vram_alloc(0x2000);
	fds()->prgram_alloc(0x8000);
	m_fdsslot->pcb_start(ciram_ptr);
}

void nes_tgd4_device::pcb_reset()
{
	m_fdsslot->pcb_reset();

	m_play = false;
	m_play_pending = false;
	m_slot128 = 0;
	m_prg_window = 0;
	m_latch = 0;
	m_chr8 = 0;
	m_latch_mode = 4;
	m_mirror = 3;
	m_prg_protect = false;
	m_42ff = 0x80;
	m_43ff = 0x00;
	m_4411 = 0x03;
	m_4412 = 0x00;
	m_4413 = 0x00;
	m_4414 = 0x00;
	m_4415 = 0x80;
	for (int i = 0; i < 4; i++)
		m_prg8[i] = u8(i << 2); // 8KB banks 0-3 (the first 32KB)
	for (int i = 0; i < 8; i++)
		m_chr1k[i] = u8(i);
	m_2m = false;
	m_2m_a17 = 0;
	m_irq_latch = 0;
	m_irq_count = 0;
	m_irq_on = false;
	m_irq_asserted = false;
	m_irq_timer->adjust(attotime::never);
	update_irq();
	m_status_reads = m_data_reads = 0;
	m_idle_frames = 0;
	m_scanline = 0;

	// the RAM check button forces $4410 bits 7 and 6
	m_mapped = false;
	m_4410 = 0x02;
	if (m_cfg->read() & 0x01)
	{
		map_in();
		m_4410 = 0xc2;
	}
}

void nes_tgd4_device::map_in()
{
	if (m_mapped)
		return;
	// A trainer is an FDS file at $7000, loaded before the doctor header
	// write that maps the copier in: copy adapter $6000-$7FFF into bank 0
	for (int i = 0; i < 0x2000; i++)
		m_aram[i] = fds()->read_m(i);
	m_mapped = true;
	m_play = false;
	m_4410 = 0x02;  // load mode
}

void nes_tgd4_device::enter_play()
{
	m_play_pending = false;
	if (m_play)
		return;
	m_play = true;
	m_latch = 0;
	m_chr8 = 0;
	// the 1M latch modes see 128KB: the slot the BIOS wrote the PRG to
	m_slot128 = u32(m_prg_window >> 6) << 17;
	m_4410 = (m_4410 & 0xfc) | 0x03;
	update_irq();
	LOG("play: mode %u protect %d mirror %u slot %05X\n", m_latch_mode, m_prg_protect, m_mirror, m_slot128);
}

void nes_tgd4_device::disk_flip_side()
{
	fds()->disk_flip_side();
}


//-------------------------------------------------
//  IRQ
//-------------------------------------------------

void nes_tgd4_device::fds_irq_w(int state)
{
	m_fds_irq = state == ASSERT_LINE;
	update_irq();
}

void nes_tgd4_device::update_irq()
{
	// in play mode the RAM adapter's IRQ (its timer and disk transfer) does
	// not reach the console
	set_irq_line((m_irq_asserted || (m_fds_irq && !m_play)) ? ASSERT_LINE : CLEAR_LINE);
}

// The counter counts up on M2 while it is negative, and stops at $FFFF
u16 nes_tgd4_device::irq_count() const
{
	if (!m_irq_on || !BIT(m_irq_count, 15) || m_irq_count == 0xffff)
		return m_irq_count;
	u64 const elapsed = attotime_to_clocks(machine().time() - m_irq_base);
	return u16(std::min<u64>(0xffff, m_irq_count + elapsed));
}

void nes_tgd4_device::irq_sync()
{
	m_irq_count = irq_count();
	m_irq_base = machine().time();
}

void nes_tgd4_device::irq_schedule()
{
	if (m_irq_on && BIT(m_irq_count, 15) && m_irq_count != 0xffff)
		m_irq_timer->adjust(clocks_to_attotime(0xffff - m_irq_count));
	else
		m_irq_timer->adjust(attotime::never);
}

TIMER_CALLBACK_MEMBER(nes_tgd4_device::irq_expired)
{
	m_irq_count = 0xffff;
	m_irq_base = machine().time();
	m_irq_asserted = true;
	update_irq();
}


//-------------------------------------------------
//  mapping
//-------------------------------------------------

u32 nes_tgd4_device::dram_offset(u16 cpu_addr) const
{
	// $43FF bits 7-4 select a 32KB window at $6000-$DFFF
	return ((u32(m_43ff >> 4) << 15) + (cpu_addr - 0x6000)) & 0x7ffff;
}

u32 nes_tgd4_device::load_chr_bank() const
{
	// Magic Card files set the window too ($F4 and so on): CHR bank in bits
	// 1-0. Game Doctor files write the file type, 8KB banks 0-3 as 0/2/4/6.
	if (m_43ff & 0xf0)
		return m_43ff & 3;
	return (m_43ff >> 1) & 3;
}

u32 nes_tgd4_device::dram8(u32 bank8) const
{
	// The BIOS loads $8000-$DFFF of each 32KB window and wraps to $6000, so
	// 8KB banks 4W+0..+2 are at +$2000/+$4000/+$6000 and 4W+3 at +0
	u32 const win = (bank8 / 4) * 0x8000;
	static constexpr u32 off[4] = { 0x2000, 0x4000, 0x6000, 0x0000 };
	return (win + off[bank8 & 3]) & 0x7ffff;
}

u32 nes_tgd4_device::play_prg_offset(u16 cpu_addr) const
{
	u16 const addr = 0x8000 | cpu_addr;
	u32 const slot = m_slot128;
	u8 const l = m_latch;
	u32 const page = (addr >> 13) & 3;

	if (m_4411 & 0x80) // 4M: 6-bit 8KB banks, 512KB
		return dram8((m_prg8[page] >> 2) & 0x3f) + (addr & 0x1fff);
	if (m_2m) // 2M: 4-bit 8KB banks plus A17 from $43FE
	{
		u32 b = (m_prg8[page] >> 2) & 0x0f;
		if (m_2m_a17)
			b |= 0x10;
		return dram8(b) + (addr & 0x1fff);
	}

	auto bank8 = [&] (u32 b) { return (slot + (b & 15) * 0x2000 + (addr & 0x1fff)) & 0x7ffff; };
	auto bank16 = [&] (u32 b)
	{
		u32 const win = slot + ((b >> 1) & 7) * 0x8000;
		u16 const a = addr & 0x3fff;
		if (!(b & 1))
			return (win + 0x2000 + a) & 0x7ffff;
		if (a < 0x2000)
			return (win + 0x6000 + a) & 0x7ffff;
		return (win + (a - 0x2000)) & 0x7ffff;
	};
	auto bank32_win = [&] (u32 win)
	{
		u16 const a = addr & 0x7fff;
		if (a < 0x6000)
			return (win + 0x2000 + a) & 0x7ffff;
		return (win + (a - 0x6000)) & 0x7ffff;
	};

	switch (m_latch_mode)
	{
	case 0: // UNROM: 16KB at $8000, bank 7 fixed at $C000
		return addr >= 0xc000 ? bank16(7) : bank16(l & 7);
	case 1: // UN1ROM + CHR switch
		return addr >= 0xc000 ? bank16(7) : bank16((l >> 2) & 15);
	case 2: // UOROM
		return addr >= 0xc000 ? bank16(15) : bank16(l & 15);
	case 3: // reverse UOROM
		return addr < 0xc000 ? bank16(15) : bank16(l & 15);
	case 4: // GNROM: 32KB
		return bank32_win(slot + ((l >> 4) & 3) * 0x8000);
	case 5: // CNROM / NROM: 32KB at the window the BIOS filled
		return bank32_win(u32(m_prg_window >> 4) << 15);
	case 6: // 8KB + 8KB, bank 7 fixed at $C000
		if (addr < 0xa000) return bank8(l & 15);
		if (addr < 0xc000) return bank8(l >> 4);
		return bank16(7);
	default: // 7
		if (addr < 0xa000) return bank8(l & 0x0e);
		if (addr < 0xc000) return bank8((l >> 4) | 1);
		return bank16(7);
	}
}

u32 nes_tgd4_device::play_chr_offset(offs_t offset) const
{
	if (m_4411 & 0x40) // 1KB CHR: 32 x 1KB in the 32KB CHR RAM
		return u32(m_chr1k[(offset >> 10) & 7] & 31) * 0x400 + (offset & 0x3ff);
	u8 bank = m_chr8;
	switch (m_latch_mode)
	{
	case 1: bank = m_latch & 3; break;
	case 3: bank = (m_latch >> 4) & 3; break;
	case 4:
	case 5: bank = m_latch & 3; break;
	default: break;
	}
	return (bank & 3) * 0x2000 + (offset & 0x1fff);
}

void nes_tgd4_device::apply_latch(u8 data)
{
	m_latch = data;
	switch (m_latch_mode)
	{
	case 1: m_chr8 = data & 3; break;
	case 3: m_chr8 = (data >> 4) & 3; break;
	case 4:
	case 5: m_chr8 = data & 3; break;
	default: break;
	}
}

int nes_tgd4_device::nt_page(offs_t offset) const
{
	switch (m_mirror & 3)
	{
	case 0: return 0;                 // one screen, page 0
	case 1: return 1;                 // one screen, page 1
	case 2: return BIT(offset, 10);   // vertical
	default: return BIT(offset, 11);  // horizontal
	}
}


//-------------------------------------------------
//  CPU side
//-------------------------------------------------

u8 nes_tgd4_device::read_ex(offs_t offset)
{
	if (!machine().side_effects_disabled())
	{
		if (offset == 0x12)
			m_status_reads++;   // $4032 drive status
		else if (offset == 0x11)
			m_data_reads++;     // $4031 disk data
	}
	return fds()->read_ex(offset);
}

void nes_tgd4_device::write_ex(offs_t offset, u8 data)
{
	fds()->write_ex(offset, data);
}

u8 nes_tgd4_device::read_l(offs_t offset)
{
	u16 const addr = 0x4100 + offset;

	switch (addr)
	{
	case 0x42ff: return m_42ff;
	case 0x43ff: return m_43ff;
	case 0x4400: case 0x4401: case 0x4402: case 0x4403:
	case 0x4404: case 0x4405: case 0x4406: case 0x4407:
		return m_chr1k[addr & 7];
	case 0x440c: return u8(irq_count() >> 8);
	case 0x440d: return u8(irq_count());
	case 0x4410: return m_4410;
	case 0x4411: return m_4411;
	case 0x4412: return m_4412;
	case 0x4413: return m_4413;
	case 0x4414: return m_4414;
	case 0x4415: return m_4415;
	case 0x4420:
		// A GUESS, not from hardware. Venus conversions read it:
		// - Quarth: LDA $4420 / AND #$E0 / BEQ ok / JMP $0000 (needs bits
		//   7-5 clear, so it is not open bus)
		// - Paperboy: waits until it reads $50 or more
		// The PPU scanline while drawing, 0 otherwise, satisfies both.
		// To settle it on a real 4+: from a test ROM, start a loop at NMI
		// that reads $4420 into RAM a fixed number of CPU cycles apart for
		// a whole frame, then print the table. Run it with rendering on and
		// off ($2001), and with 1KB CHR mode ($4411 bit 6) on, since the
		// value might follow PPU address lines or the CHR bank instead.
		// Holding a sprite 0 hit at a known line gives a cross-check.
		return (m_scanline >= 0 && m_scanline < 240) ? u8(m_scanline) : 0x00;
	default: break;
	}

	if (m_mapped && addr >= 0x4800 && addr <= 0x4fff)
		return m_bios[0x0800 | (addr & 0x07ff)];

	if (!m_mapped)
		return fds()->read_l(offset);

	return 0xff;
}

void nes_tgd4_device::write_l(offs_t offset, u8 data)
{
	u16 const addr = 0x4100 + offset;

	if (addr == 0x43ff || addr == 0x4fff)
		map_in();

	switch (addr)
	{
	case 0x42fc: case 0x42fd: case 0x42fe: case 0x42ff:
		m_42ff = data;
		m_latch_mode = (data >> 5) & 7;
		m_prg_protect = BIT(data, 4);
		m_mirror = addr & 3;
		if (m_mapped && !m_play)
			m_play_pending = true;
		return;
	case 0x43fe:
		m_2m = true;
		m_2m_a17 = BIT(data, 5);
		m_chr8 = data & 3;
		m_43ff = data;
		m_4414 = data;
		return;
	case 0x43ff:
		m_2m = false;
		m_43ff = data;
		m_4414 = data;
		return;
	case 0x4400: case 0x4401: case 0x4402: case 0x4403:
	case 0x4404: case 0x4405: case 0x4406: case 0x4407:
		m_chr1k[addr & 7] = data;
		return;
	case 0x440c:
		irq_sync();
		m_irq_latch = (m_irq_latch & 0x00ff) | (u16(data) << 8);
		m_irq_asserted = false;
		if (!BIT(data, 7))
		{
			m_irq_count = 0;
			m_irq_on = false;
		}
		else
			m_irq_on = true;
		irq_schedule();
		update_irq();
		return;
	case 0x440d:
		m_irq_latch = (m_irq_latch & 0xff00) | data;
		m_irq_count = m_irq_latch;
		m_irq_base = machine().time();
		m_irq_asserted = false;
		m_irq_on = BIT(m_irq_count, 15);
		irq_schedule();
		update_irq();
		return;
	case 0x4410: m_4410 = data; return;
	case 0x4411: m_4411 = data; return;
	case 0x4412: m_4412 = data; return;
	case 0x4413: m_4413 = data; return;
	case 0x4414: m_4414 = data; return;
	case 0x4415: m_4415 = data; return;
	default: break;
	}

	if (!m_mapped)
		fds()->write_l(offset, data);
}

u8 nes_tgd4_device::read_m(offs_t offset)
{
	if (!m_mapped)
		return fds()->read_m(offset);
	if (m_play)
		return m_aram[offset & 0x1fff];
	if (m_4411 & 0x04)
		return m_aram[((m_4411 & 0x03) << 13) | (offset & 0x1fff)];
	return m_dram[dram_offset(0x6000 + offset)];
}

void nes_tgd4_device::write_m(offs_t offset, u8 data)
{
	if (!m_mapped)
		fds()->write_m(offset, data);
	else if (m_play)
		m_aram[offset & 0x1fff] = data;
	else if (m_4411 & 0x04)
		m_aram[((m_4411 & 0x03) << 13) | (offset & 0x1fff)] = data;
	else
	{
		m_dram[dram_offset(0x6000 + offset)] = data;
		m_prg_window = m_43ff;
	}
}

u8 nes_tgd4_device::read_h(offs_t offset)
{
	if (!m_mapped)
		return fds()->read_h(offset);

	// the BIOS starts the game with a mode write and JMP ($FFFC)
	if (m_play_pending && offset == 0x7ffc && !machine().side_effects_disabled())
		enter_play();

	u16 const addr = 0x8000 + offset;
	if (m_play)
		return m_dram[play_prg_offset(addr)];
	if (addr >= 0xe000)
		return m_bios[addr & 0x1fff];
	return m_dram[dram_offset(addr)];
}

void nes_tgd4_device::write_h(offs_t offset, u8 data)
{
	if (!m_mapped)
	{
		fds()->write_h(offset, data);
		return;
	}

	if (m_play)
	{
		m_prg8[(offset >> 13) & 3] = data;
		// In 2M and 4M mode the bank registers are PPPPPPCC: CC is the CHR
		// bank. The latch modes switch CHR in apply_latch(), if at all.
		if ((m_4411 & 0x80) || m_2m)
			m_chr8 = data & 3;
		// No bus conflicts. In the GNROM and CNROM modes (4-5) $8000 is
		// always the latch; the UNROM modes write the DRAM when it is not
		// write protected.
		bool const bank_mode = (m_4411 & 0x80) || m_2m;
		if (!m_prg_protect && m_latch_mode < 4 && !bank_mode)
			m_dram[play_prg_offset(0x8000 + offset)] = data;
		apply_latch(data);
		return;
	}

	u16 const addr = 0x8000 + offset;
	if (addr < 0xe000)
	{
		m_dram[dram_offset(addr)] = data;
		m_prg_window = m_43ff;
	}
}


//-------------------------------------------------
//  PPU side
//-------------------------------------------------

u8 nes_tgd4_device::chr_r(offs_t offset)
{
	if (!m_mapped)
		return fds()->chr_r(offset);
	if (m_play)
		return m_cram[play_chr_offset(offset)];
	return m_cram[load_chr_bank() * 0x2000 + (offset & 0x1fff)];
}

void nes_tgd4_device::chr_w(offs_t offset, u8 data)
{
	if (!m_mapped)
		fds()->chr_w(offset, data);
	else if (m_play)
	{
		// CHR is write protected in the CNROM and GNROM modes
		if (m_latch_mode < 4)
			m_cram[play_chr_offset(offset)] = data;
	}
	else
		m_cram[load_chr_bank() * 0x2000 + (offset & 0x1fff)] = data;
}

u8 nes_tgd4_device::nt_r(offs_t offset)
{
	if (m_play)
		return m_ciram[nt_page(offset) * 0x400 + (offset & 0x3ff)];
	return fds()->nt_r(offset);
}

void nes_tgd4_device::nt_w(offs_t offset, u8 data)
{
	if (m_play)
		m_ciram[nt_page(offset) * 0x400 + (offset & 0x3ff)] = data;
	else
		fds()->nt_w(offset, data);
}

void nes_tgd4_device::hblank_irq(int scanline, bool vblank, bool blanked)
{
	m_scanline = scanline;
	if (scanline == 0)
		auto_swap();
	if (!m_play)
		fds()->hblank_irq(scanline, vblank, blanked);
}

void nes_tgd4_device::scanline_irq(int scanline, bool vblank, bool blanked)
{
	if (!m_play)
		fds()->scanline_irq(scanline, vblank, blanked);
}

void nes_tgd4_device::ppu_latch(offs_t offset)
{
	fds()->ppu_latch(offset);
}

// Once a frame: if the BIOS has been polling the drive status for a second
// without reading the disk, it wants the next side, so put it in.
void nes_tgd4_device::auto_swap()
{
	auto *const disk = dynamic_cast<nes_disksys_device *>(fds());
	bool const waiting = m_mapped && !m_play && m_status_reads && !m_data_reads;
	m_status_reads = m_data_reads = 0;

	if (!waiting || !(m_cfg->read() & 0x02) || !disk || !disk->current_side() || disk->current_side() >= disk->sides())
	{
		m_idle_frames = 0;
		return;
	}
	if (++m_idle_frames >= 60)
	{
		m_idle_frames = 0;
		LOG("automatic disk swap\n");
		disk->disk_flip_side();
	}
}

} // anonymous namespace


DEFINE_DEVICE_TYPE_PRIVATE(NES_TGD4, device_nes_cart_interface, nes_tgd4_device, "nes_tgd4", "Venus Turbo Game Doctor 4+")

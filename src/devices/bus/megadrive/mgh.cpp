// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***********************************************************************************

 Multi Game Hunter (Mega Drive side)

 SNES + Mega Drive floppy copier. The 27C256 holds the Mega
 Drive BIOS (Z80, $0000-$3FFF) and the SNES BIOS (65816, $4000-$7FFF).
 As on the Double Pro Fighter the Mega Drive BIOS is a Master System program.

 The floppy drive hangs off a daughterboard with its own controller
 (machine/mghfdd.cpp), at $8004 (status) and $8005 (data).

 Z80 map in BIOS mode:
   $0000-$3FFF  EPROM
   $4000-$5FFF  EPROM (icons, font) while register 0 = $80 (boot), else the
                paged memory (file loads go here)
   $6000-$7FFF  8KB window, register 3: $00 the IC card (register 2 bit 0
                picks the MD or SFC slot), $04 the paged memory, $05 the
                buffer SRAM (FAT, directory and name files)
   $8004/$8005  floppy MCU
   $FFx0-$FFxF  16 write-only registers (A0-A3 decoded), seen through the
                console RAM's top mirror

 Paged memory: page = $FFF7 bit 0 << 8 | $FFF6 rotated left once (the BIOS
 writes page bit 0 to $FFF6 bit 7). The DRAM is two 1MB byte lanes, pages
 $000-$07F the low (odd) bytes of the game and $100-$17F the high (even)
 bytes, which is the Game Doctor file layout: the first half of a file holds
 the odd bytes, the second half the even ones. Pages with bit 7 set
 ($180 + n): the 32KB game SRAM.

 The IC card is paged the same way, its pages $180 + n being its save RAM
 (odd bytes from $200001). The BIOS sizes ROM and save RAM by where reads
 repeat; a card's save RAM is repeated at its size here, as its own address
 decoding does on hardware, or the BIOS reports none.

 Starting a game: the BIOS copies a short routine to console RAM ($CE00),
 sets the registers and then writes $A0 to register 3 in a loop until the
 copier resets the console. Register 2 says what to start:
   $01  the DRAM, as a Mega Drive game (Disk > Run, IC card Play on mgh);
        the game SRAM answers on the odd bytes from $200001
   $02  the IC card itself (IC card Play on MD)
 The 68000 then reads word n as {high lane[n], low lane[n]}.

 The run mode is a latch on the copier: it survives the console reset that
 starts the game, and only power brings the BIOS back.

 TODO:
 - registers 0, 1, 8, 9, E, F
 - the SFC IC card slot (not reachable from a Mega Drive here)
 - save RAM of cards over 16Mbit, which those cards only switch in through
   $A130F1

***********************************************************************************/

#include "emu.h"
#include "mgh.h"

#include "md_carts.h"

#include "machine/mghfdd.h"

#include "softlist_dev.h"

#define VERBOSE 0
#include "logmacro.h"


namespace {

constexpr uint32_t LANE_SIZE = 0x100000;
constexpr uint32_t SRAM_SIZE = 0x8000;

ROM_START( mghmd )
	ROM_REGION( 0x8000, "bios", 0 )
	ROM_LOAD( "mgh_bios.bin", 0x0000, 0x8000, CRC(fad86fb9) SHA1(3ef3af3249daf272765c1eeb69528bf144df1273) )
ROM_END


class md_multi_game_hunter_device : public device_t, public device_md_cart_interface, public device_nvram_interface
{
public:
	md_multi_game_hunter_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// Mega Drive mode (68000)
	virtual uint16_t read(offs_t offset) override;
	virtual void write(offs_t offset, uint16_t data, uint16_t mem_mask = ~0) override;
	virtual uint16_t read_a13(offs_t offset) override;
	virtual void write_a13(offs_t offset, uint16_t data) override;

	// Mark III mode (BIOS, Z80)
	virtual bool m3_r() override { return m_mode == MODE_BIOS; }
	virtual bool has_internal_bios() const override { return true; }
	virtual uint8_t read_sms(offs_t offset) override;
	virtual void write_sms(offs_t offset, uint8_t data) override;

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

	// the 32KB game SRAM is battery backed
	virtual void nvram_default() override;
	virtual bool nvram_read(util::read_stream &file) override;
	virtual bool nvram_write(util::write_stream &file) override;

private:
	enum : uint8_t { MODE_BIOS, MODE_DRAM, MODE_CART };

	uint8_t *window_ptr(offs_t offset);
	uint32_t dram_index(int lane, unsigned page, offs_t offset) const;
	uint8_t iccard_byte(offs_t offset);
	uint8_t iccard_sram(uint16_t page, offs_t offset, uint8_t data, bool write);

	required_region_ptr<uint8_t> m_bios;
	required_device<mgh_fdd_device> m_fdd;
	required_device<md_cart_slot_device> m_iccard;

	std::unique_ptr<uint8_t[]> m_dram;
	std::unique_ptr<uint8_t[]> m_sram;
	std::unique_ptr<uint8_t[]> m_buffer;
	uint8_t m_regs[16];
	uint8_t m_mode;
};


md_multi_game_hunter_device::md_multi_game_hunter_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, MD_MULTI_GAME_HUNTER, tag, owner, clock)
	, device_md_cart_interface(mconfig, *this)
	, device_nvram_interface(mconfig, *this)
	, m_bios(*this, "bios")
	, m_fdd(*this, "fdd")
	, m_iccard(*this, "iccard")
	, m_mode(MODE_BIOS)
{
}

void md_multi_game_hunter_device::device_add_mconfig(machine_config &config)
{
	MGH_FDD(config, m_fdd);
	SOFTWARE_LIST(config, "flop_list").set_original("megadriv_mgh_flop");

	MD_CART_SLOT(config, m_iccard, md_cart_nocopier, nullptr);
}

const tiny_rom_entry *md_multi_game_hunter_device::device_rom_region() const
{
	return ROM_NAME( mghmd );
}

void md_multi_game_hunter_device::device_start()
{
	m_dram = std::make_unique<uint8_t[]>(LANE_SIZE * 2);
	m_sram = std::make_unique<uint8_t[]>(SRAM_SIZE);
	m_buffer = std::make_unique<uint8_t[]>(SRAM_SIZE);

	// DRAM powers up holding garbage, which the BIOS's size probe relies on
	// (it compares checksums of pages to find where the address wraps, and
	// takes all $FF as no memory); so does the SRAM until it is used
	uint32_t seed = 0x2545f491;
	for (uint32_t i = 0; i < LANE_SIZE * 2; i++)
	{
		seed = seed * 1103515245 + 12345;
		m_dram[i] = seed >> 24;
	}
	std::fill_n(m_buffer.get(), SRAM_SIZE, 0x00);
	std::fill(std::begin(m_regs), std::end(m_regs), 0);

	save_pointer(NAME(m_dram), LANE_SIZE * 2);
	save_pointer(NAME(m_sram), SRAM_SIZE);
	save_pointer(NAME(m_buffer), SRAM_SIZE);
	save_item(NAME(m_regs));
	save_item(NAME(m_mode));
}

void md_multi_game_hunter_device::nvram_default()
{
	// never used battery RAM holds garbage, which the BIOS's size probe needs
	// (all $FF reads as no memory)
	uint32_t seed = 0x1b873593;
	for (uint32_t i = 0; i < SRAM_SIZE; i++)
	{
		seed = seed * 1103515245 + 12345;
		m_sram[i] = seed >> 24;
	}
}

bool md_multi_game_hunter_device::nvram_read(util::read_stream &file)
{
	auto const [err, actual] = util::read(file, m_sram.get(), SRAM_SIZE);
	return !err && (actual == SRAM_SIZE);
}

bool md_multi_game_hunter_device::nvram_write(util::write_stream &file)
{
	auto const [err, actual] = util::write(file, m_sram.get(), SRAM_SIZE);
	return !err;
}

uint8_t *md_multi_game_hunter_device::window_ptr(offs_t offset)
{
	uint8_t const r = m_regs[6];
	uint16_t const page = (m_regs[7] & 1) << 8 | uint8_t(r << 1 | r >> 7);
	if (offset >= 0x6000)
	{
		switch (m_regs[3])
		{
		case 0x04:
			break;
		case 0x05:
			return &m_buffer[(page & 3) << 13 | (offset & 0x1fff)];
		default:
			// $00: the IC card, read through iccard_byte()
			return nullptr;
		}
	}
	else if (m_regs[0] == 0x80)
		return &m_bios[offset];
	offset &= 0x1fff;
	if (BIT(page, 7))
		return &m_sram[(page & 3) << 13 | offset];
	return &m_dram[dram_index(BIT(page, 8), page & 0x7f, offset)];
}

// DRAM byte for lane / page / offset
uint32_t md_multi_game_hunter_device::dram_index(int lane, unsigned page, offs_t offset) const
{
	return lane * LANE_SIZE + (page << 13 | (offset & 0x1fff));
}

// IC card through the $6000 window (register 3 = $00), paged as the DRAM:
// the page's lane bit picks the high or low byte. Register 2 bit 0 picks
// the slot: 0 the Mega Drive one (emulated), 1 the Super Famicom one.
uint8_t md_multi_game_hunter_device::iccard_byte(offs_t offset)
{
	if (BIT(m_regs[2], 0) || !m_iccard->exists())
		return 0xff;
	uint8_t const r = m_regs[6];
	uint16_t const page = (m_regs[7] & 1) << 8 | uint8_t(r << 1 | r >> 7);
	if (BIT(page, 7))
		return iccard_sram(page, offset, 0, false);
	uint16_t const word = m_iccard->read((page & 0xff) << 13 | (offset & 0x1fff));
	return BIT(page, 8) ? word >> 8 : word & 0xff;
}

// The card's save RAM, through the BIOS's SRAM pages ($180 + n): the odd
// bytes from $200001, repeated at its size as a real card decodes it (the
// BIOS sizes it by where it repeats: 8KB, 32KB, otherwise "Nil")
uint8_t md_multi_game_hunter_device::iccard_sram(uint16_t page, offs_t offset, uint8_t data, bool write)
{
	device_md_cart_interface *const cart = m_iccard->m_cart;
	uint32_t const size = cart ? cart->get_nvram_size() / 2 : 0;
	if (!size)
		return 0xff;
	offs_t const addr = 0x100000 + (((page & 0x3f) << 13 | (offset & 0x1fff)) % size);
	if (write)
	{
		m_iccard->write(addr, data, 0x00ff);
		return 0;
	}
	return m_iccard->read(addr) & 0xff;
}

uint16_t md_multi_game_hunter_device::read(offs_t offset)
{
	switch (m_mode)
	{
	case MODE_DRAM:
		// the game SRAM on the odd bytes from $200001 (games here are 16Mbit
		// at most, so it never covers ROM)
		if (offset >= 0x100000 && offset < 0x180000)
			return 0xff00 | m_sram[offset & (SRAM_SIZE - 1)];
		offset &= LANE_SIZE - 1;
		return m_dram[dram_index(1, offset >> 13, offset)] << 8 | m_dram[dram_index(0, offset >> 13, offset)];
	case MODE_CART:
		return m_iccard->read(offset);
	default:
		return 0xffff;
	}
}

void md_multi_game_hunter_device::write(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (m_mode == MODE_CART)
		m_iccard->write(offset, data, mem_mask);
	else if (m_mode == MODE_DRAM && offset >= 0x100000 && offset < 0x180000 && ACCESSING_BITS_0_7)
		m_sram[offset & (SRAM_SIZE - 1)] = data & 0xff;
}

uint16_t md_multi_game_hunter_device::read_a13(offs_t offset)
{
	return m_mode == MODE_CART ? m_iccard->read_a13(offset) : 0xffff;
}

void md_multi_game_hunter_device::write_a13(offs_t offset, uint16_t data)
{
	if (m_mode == MODE_CART)
		m_iccard->write_a13(offset, data);
}

uint8_t md_multi_game_hunter_device::read_sms(offs_t offset)
{
	if (offset < 0x4000)
		return m_bios[offset];
	if (offset >= 0x6000 && offset < 0x8000 && m_regs[3] == 0x00)
		return iccard_byte(offset);
	if (offset < 0x8000)
	{
		if (uint8_t *const p = window_ptr(offset))
			return *p;
	}
	if (offset == 0x8004)
		return m_fdd->status_r();
	if (offset == 0x8005)
		return m_fdd->data_r();
	if (!machine().side_effects_disabled())
		LOG("%s: read %04x\n", machine().describe_context(), offset);
	return 0xff;
}

void md_multi_game_hunter_device::write_sms(offs_t offset, uint8_t data)
{
	if (offset >= 0x6000 && offset < 0x8000 && m_regs[3] == 0x00)
	{
		// the IC card: only its save RAM can be written
		uint8_t const r = m_regs[6];
		uint16_t const page = (m_regs[7] & 1) << 8 | uint8_t(r << 1 | r >> 7);
		if (BIT(page, 7) && !BIT(m_regs[2], 0) && m_iccard->exists())
			iccard_sram(page, offset, data, true);
		return;
	}
	if (offset >= 0x4000 && offset < 0x8000)
	{
		if (offset >= 0x6000 || m_regs[0] != 0x80)
		{
			if (uint8_t *const p = window_ptr(offset))
			{
				*p = data;
				return;
			}
		}
	}
	if (offset == 0x8005)
		m_fdd->data_w(data);
	else if (offset >= 0xff00)
	{
		if ((offset & 0x0f) != 3 || data != 0xa0 || m_regs[3] != 0xa0)
			LOG("%s: reg %x = %02x\n", machine().describe_context(), offset & 0x0f, data);
		m_regs[offset & 0x0f] = data;
		if ((offset & 0x0f) == 3 && data == 0xa0 && m_mode == MODE_BIOS)
		{
			// start a game: the console is reset with /M3 released
			// TODO: reset the console through the cartridge slot (VRES on
			// the newer bus/megadrive/cart) rather than the whole machine
			if (m_regs[2] == 0x01)
			{
				LOG("starting DRAM game\n");
				m_mode = MODE_DRAM;
				machine().schedule_soft_reset();
			}
			else if (m_regs[2] == 0x02)
			{
				LOG("starting IC card\n");
				m_mode = MODE_CART;
				machine().schedule_soft_reset();
			}
			else
				LOG("start with register 2 = %02x not known\n", m_regs[2]);
		}
	}
	else if (offset < 0xc000 || offset >= 0xf000)
		LOG("%s: write %04x = %02x\n", machine().describe_context(), offset, data);
}

} // anonymous namespace


DEFINE_DEVICE_TYPE_PRIVATE(MD_MULTI_GAME_HUNTER, device_md_cart_interface, md_multi_game_hunter_device, "mghmd", "Multi Game Hunter (Mega Drive)")

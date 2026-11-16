// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***********************************************************************************

 Multi Game Hunter (SNES side)

 SNES + Mega Drive floppy copier. The 27C256 holds the Mega Drive BIOS
 (Z80, $0000-$3FFF) and the SNES BIOS (65816, $4000-$7FFF); the SNES sees
 the whole EPROM at $8000-$FFFF.

 65816 map in BIOS mode:
   $00:8000-$FFFF    EPROM
   $01:8000-$BFFF    the paged memory (DRAM, game SRAM), twice; with
                     register 3 = $00 the IC card at $A000
   $6000-$7FFF       8KB of the buffer SRAM (register 3 = $04): the BIOS's
                     directory cache and settings
   $03:5800-$5807    registers; $5804/$5805 the floppy board
                     (machine/mghfdd.cpp)

 Paged memory: page = register 7 bit 0 << 8 | register 6 rotated left once.
 Bit 8 picks the DRAM lane (two 1MB byte lanes, as seen from the Mega
 Drive), bit 7 the 32KB game SRAM. The BIOS numbers a file's pages so that
 it lands in the DRAM in order (page n: lane n.7, page n & $7F).

 Starting a game: the BIOS copies a short routine to $0400, sets registers
 2, 3 and 1 and then writes 0 to register 0, which switches the copier to
 the game at once, and jumps through the game's reset vector:
   register 2 $01, register 3 $A4  the DRAM (Run Disk File, Run IC Card on
                                   DRAM)
   register 2 $03, register 3 $A0  the IC card (Run IC Card)
 The DRAM game is mapped for both LoROM and Game Doctor HiROM files: banks
 of 32KB at $8000 from the start of the DRAM, the low halves of banks
 $40-$7D from its middle (a Game Doctor HiROM file holds the high halves of
 the banks first). The game SRAM is at $70-$7D:0000 and $20-$3F:6000.

 The game mode is a latch: a console reset keeps the game running.

 TODO:
 - registers 0 and 1 beyond the start sequence
 - how the copier reads the low halves of a HiROM IC card (the BIOS copies
   card pages $00-$FF to the DRAM with nothing to say the card is HiROM)
 - whether a console reset returns to the BIOS on hardware

***********************************************************************************/

#include "emu.h"
#include "mgh.h"

#include "snes_carts.h"

#include "machine/mghfdd.h"

#include "softlist_dev.h"

#define VERBOSE 0
#include "logmacro.h"


namespace {

constexpr uint32_t DRAM_SIZE = 0x200000;
constexpr uint32_t SRAM_SIZE = 0x8000;

ROM_START( mghsnes )
	ROM_REGION( 0x8000, "bios", 0 )
	ROM_LOAD( "mgh_bios.bin", 0x0000, 0x8000, CRC(fad86fb9) SHA1(3ef3af3249daf272765c1eeb69528bf144df1273) )
ROM_END


class sns_multi_game_hunter_device : public device_t, public device_sns_cart_interface, public device_nvram_interface
{
public:
	sns_multi_game_hunter_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual bool has_internal_bios() const override { return true; }
	virtual void install_io(address_space &space) override ATTR_COLD;
	virtual uint8_t read_l(offs_t offset) override;
	virtual uint8_t read_h(offs_t offset) override;
	virtual void write_l(offs_t offset, uint8_t data) override;
	virtual uint8_t chip_read(offs_t offset) override;
	virtual void chip_write(offs_t offset, uint8_t data) override;

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

	uint8_t regs_r(offs_t offset);
	void regs_w(offs_t offset, uint8_t data);
	uint8_t *page_ptr(offs_t offset);
	uint8_t *window_ptr(offs_t offset);
	bool iccard_hirom() const;
	uint8_t iccard_page_r(offs_t offset);
	void iccard_page_w(offs_t offset, uint8_t data);
	uint8_t iccard_r(offs_t offset);
	void iccard_w(offs_t offset, uint8_t data);
	uint8_t game_r(offs_t offset);
	void game_w(offs_t offset, uint8_t data);

	required_region_ptr<uint8_t> m_bios;
	required_device<mgh_fdd_device> m_fdd;
	required_device<sns_cart_slot_device> m_iccard;

	std::unique_ptr<uint8_t[]> m_dram;
	std::unique_ptr<uint8_t[]> m_sram;
	std::unique_ptr<uint8_t[]> m_buffer;
	uint8_t m_regs[8];
	uint8_t m_mode;
};


sns_multi_game_hunter_device::sns_multi_game_hunter_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SNS_MULTI_GAME_HUNTER, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
	, device_nvram_interface(mconfig, *this)
	, m_bios(*this, "bios")
	, m_fdd(*this, "fdd")
	, m_iccard(*this, "iccard")
	, m_mode(MODE_BIOS)
{
}

void sns_multi_game_hunter_device::device_add_mconfig(machine_config &config)
{
	MGH_FDD(config, m_fdd);
	SOFTWARE_LIST(config, "flop_list").set_original("snes_mgh_flop");

	SNS_CART_SLOT(config, m_iccard, snes_cart_nocopier, nullptr);
}

const tiny_rom_entry *sns_multi_game_hunter_device::device_rom_region() const
{
	return ROM_NAME( mghsnes );
}

void sns_multi_game_hunter_device::device_start()
{
	m_dram = std::make_unique<uint8_t[]>(DRAM_SIZE);
	m_sram = std::make_unique<uint8_t[]>(SRAM_SIZE);
	m_buffer = std::make_unique<uint8_t[]>(SRAM_SIZE);

	// DRAM powers up holding garbage
	uint32_t seed = 0x2545f491;
	for (uint32_t i = 0; i < DRAM_SIZE; i++)
	{
		seed = seed * 1103515245 + 12345;
		m_dram[i] = seed >> 24;
	}
	std::fill_n(m_buffer.get(), SRAM_SIZE, 0x00);
	std::fill(std::begin(m_regs), std::end(m_regs), 0);

	save_pointer(NAME(m_dram), DRAM_SIZE);
	save_pointer(NAME(m_sram), SRAM_SIZE);
	save_pointer(NAME(m_buffer), SRAM_SIZE);
	save_item(NAME(m_regs));
	save_item(NAME(m_mode));
}

void sns_multi_game_hunter_device::nvram_default()
{
	// never used battery RAM holds garbage
	uint32_t seed = 0x1b873593;
	for (uint32_t i = 0; i < SRAM_SIZE; i++)
	{
		seed = seed * 1103515245 + 12345;
		m_sram[i] = seed >> 24;
	}
}

bool sns_multi_game_hunter_device::nvram_read(util::read_stream &file)
{
	auto const [err, actual] = util::read(file, m_sram.get(), SRAM_SIZE);
	return !err && (actual == SRAM_SIZE);
}

bool sns_multi_game_hunter_device::nvram_write(util::write_stream &file)
{
	auto const [err, actual] = util::write(file, m_sram.get(), SRAM_SIZE);
	return !err;
}

void sns_multi_game_hunter_device::install_io(address_space &space)
{
	space.install_read_handler(0x035800, 0x035807, 0, 0x800000, 0, read8sm_delegate(*this, FUNC(sns_multi_game_hunter_device::regs_r)));
	space.install_write_handler(0x035800, 0x035807, 0, 0x800000, 0, write8sm_delegate(*this, FUNC(sns_multi_game_hunter_device::regs_w)));
}

uint8_t sns_multi_game_hunter_device::regs_r(offs_t offset)
{
	switch (offset)
	{
	case 4:
		return m_fdd->status_r();
	case 5:
		return m_fdd->data_r();
	default:
		if (!machine().side_effects_disabled())
			LOG("%s: read reg %d\n", machine().describe_context(), offset);
		return 0xff;
	}
}

void sns_multi_game_hunter_device::regs_w(offs_t offset, uint8_t data)
{
	if (offset == 5)
	{
		m_fdd->data_w(data);
		return;
	}
	LOG("%s: reg %d = %02x\n", machine().describe_context(), offset, data);
	m_regs[offset] = data;

	// starting a game: register 0 goes from $C0 to $00 with register 1 at
	// $A0, and the BIOS jumps through the game's reset vector at once
	if (offset == 0 && data == 0x00 && m_regs[1] == 0xa0 && m_mode == MODE_BIOS)
	{
		if (m_regs[2] == 0x01)
		{
			LOG("starting DRAM game\n");
			m_mode = MODE_DRAM;
		}
		else if (m_regs[2] == 0x03 && m_iccard->exists())
		{
			LOG("starting IC card\n");
			m_mode = MODE_CART;
		}
		else
			LOG("start with register 2 = %02x not known\n", m_regs[2]);
	}
}

// $01:8000-$9FFF and $A000-$BFFF: the paged memory; with register 3 = $00
// the second window shows the IC card instead (the BIOS copies a card 512
// bytes at a time from $A000 through WRAM to $8000). Page = register 7 bit 0 << 8 |
// register 6 rotated left once; bit 8 picks the DRAM lane (1MB each), bit 7
// the 32KB game SRAM. The BIOS numbers game pages so that a file lands in
// the DRAM in order (page n: lane n.7, page n & $7F)
uint8_t *sns_multi_game_hunter_device::page_ptr(offs_t offset)
{
	uint8_t const r = m_regs[6];
	uint16_t const page = (m_regs[7] & 1) << 8 | uint8_t(r << 1 | r >> 7);
	offset &= 0x1fff;
	if (BIT(page, 7))
		return &m_sram[(page & 3) << 13 | offset];
	return &m_dram[BIT(page, 8) << 20 | (page & 0x7f) << 13 | offset];
}

// $6000-$7FFF: 8KB of the buffer SRAM, the BIOS's directory cache and
// settings, whatever page is selected (register 3 = $04)
uint8_t *sns_multi_game_hunter_device::window_ptr(offs_t offset)
{
	if (!BIT(m_regs[3], 2))
		return nullptr;
	return &m_buffer[offset & 0x1fff];
}

bool sns_multi_game_hunter_device::iccard_hirom() const
{
	switch (m_iccard->get_type())
	{
	case SNES_MODE21:
	case SNES_DSP_MODE21:
	case SNES_SRTC:
	case SNES_SPC7110:
	case SNES_SPC7110_RTC:
	case SNES_BSXHI:
		return true;
	default:
		return false;
	}
}

// The IC card through the $A000 window (register 3 = $00), with the
// same page numbers as the DRAM: pages $000-$0FF are the card read as LoROM
// (32KB banks at $8000, which for a HiROM card are the high halves of its
// banks), $100-$17F the low halves of HiROM banks, and pages $1xx with bit
// 7 set its save RAM. The BIOS sizes ROM and save RAM by where reads repeat.
uint8_t sns_multi_game_hunter_device::iccard_page_r(offs_t offset)
{
	if (!m_iccard->exists())
		return 0xff;
	uint8_t const r = m_regs[6];
	uint16_t const page = (m_regs[7] & 1) << 8 | uint8_t(r << 1 | r >> 7);
	offset &= 0x1fff;
	if (!BIT(page, 8))
		return m_iccard->read_l((page >> 2) << 16 | 0x8000 | (page & 3) << 13 | offset);
	if (!BIT(page, 7))
		return m_iccard->read_l((0x40 | (page & 0x7f) >> 2) << 16 | (page & 3) << 13 | offset);
	uint32_t const size = m_iccard->m_cart ? m_iccard->m_cart->get_nvram_size() : 0;
	return size ? m_iccard->read_ram(((page & 0x3f) << 13 | offset) % size) : 0xff;
}

void sns_multi_game_hunter_device::iccard_page_w(offs_t offset, uint8_t data)
{
	uint8_t const r = m_regs[6];
	uint16_t const page = (m_regs[7] & 1) << 8 | uint8_t(r << 1 | r >> 7);
	uint32_t const size = (m_iccard->exists() && m_iccard->m_cart) ? m_iccard->m_cart->get_nvram_size() : 0;
	if ((page & 0x180) == 0x180 && size)
		m_iccard->write_ram(((page & 0x3f) << 13 | (offset & 0x1fff)) % size, data);
}

// Run IC Card: the card on the bus as if it were plugged into the console
uint8_t sns_multi_game_hunter_device::iccard_r(offs_t offset)
{
	uint8_t const bank = (offset >> 16) & 0x7f;
	uint16_t const addr = offset & 0xffff;
	uint32_t const size = m_iccard->m_cart ? m_iccard->m_cart->get_nvram_size() : 0;
	if (iccard_hirom())
	{
		if (addr >= 0x6000 && addr < 0x8000 && bank >= 0x20 && bank < 0x40)
			return size ? m_iccard->read_ram(((bank << 13 | (addr - 0x6000)) & 0x7fff) % size) : 0xff;
	}
	else if (addr < 0x8000 && bank >= 0x70)
		return size ? m_iccard->read_ram(((bank - 0x70) << 15 | addr) % size) : 0xff;
	if (addr < 0x8000 && bank < 0x40)
		return 0xff;
	return BIT(offset, 23) ? m_iccard->read_h(offset) : m_iccard->read_l(offset);
}

void sns_multi_game_hunter_device::iccard_w(offs_t offset, uint8_t data)
{
	uint8_t const bank = (offset >> 16) & 0x7f;
	uint16_t const addr = offset & 0xffff;
	uint32_t const size = m_iccard->m_cart ? m_iccard->m_cart->get_nvram_size() : 0;
	if (!size)
		return;
	if (iccard_hirom())
	{
		if (addr >= 0x6000 && addr < 0x8000 && bank >= 0x20 && bank < 0x40)
			m_iccard->write_ram(((bank << 13 | (addr - 0x6000)) & 0x7fff) % size, data);
	}
	else if (addr < 0x8000 && bank >= 0x70)
		m_iccard->write_ram(((bank - 0x70) << 15 | addr) % size, data);
}

// the game: LoROM banks of 32KB from the start of the DRAM; banks $40-$7D
// below $8000 from the middle (the second half of a Game Doctor HiROM file
// holds the low halves of its banks); the game SRAM at $70-$7D:0000 (LoROM)
// and $20-$3F:6000 (HiROM)
uint8_t sns_multi_game_hunter_device::game_r(offs_t offset)
{
	uint8_t const bank = (offset >> 16) & 0x7f;
	uint16_t const addr = offset & 0xffff;
	if (addr >= 0x8000)
		return m_dram[(bank << 15 | (addr & 0x7fff)) & (DRAM_SIZE - 1)];
	if (bank >= 0x70)
		return m_sram[addr & (SRAM_SIZE - 1)];
	if (bank >= 0x40)
		return m_dram[(DRAM_SIZE / 2 + ((bank & 0x3f) << 15 | addr)) & (DRAM_SIZE - 1)];
	if (bank >= 0x20 && addr >= 0x6000)
		return m_sram[((bank & 0x03) << 13 | (addr & 0x1fff)) & (SRAM_SIZE - 1)];
	return 0xff;
}

void sns_multi_game_hunter_device::game_w(offs_t offset, uint8_t data)
{
	uint8_t const bank = (offset >> 16) & 0x7f;
	uint16_t const addr = offset & 0xffff;
	if (addr < 0x8000 && bank >= 0x70)
		m_sram[addr & (SRAM_SIZE - 1)] = data;
	else if (addr >= 0x6000 && addr < 0x8000 && bank >= 0x20 && bank < 0x40)
		m_sram[((bank & 0x03) << 13 | (addr & 0x1fff)) & (SRAM_SIZE - 1)] = data;
}

uint8_t sns_multi_game_hunter_device::read_l(offs_t offset)
{
	return read_h(offset);
}

uint8_t sns_multi_game_hunter_device::read_h(offs_t offset)
{
	if (m_mode == MODE_DRAM)
		return game_r(offset);
	if (m_mode == MODE_CART)
		return iccard_r(offset);
	uint8_t const bank = (offset >> 16) & 0x7f;
	uint16_t const addr = offset & 0xffff;
	if (bank == 0 && addr >= 0x8000)
		return m_bios[addr & 0x7fff];
	if (bank == 1 && addr >= 0x8000 && addr < 0xc000)
		return (addr < 0xa000 || BIT(m_regs[3], 2)) ? *page_ptr(addr) : iccard_page_r(addr);
	if (!machine().side_effects_disabled())
		LOG("%s: read %06x\n", machine().describe_context(), offset);
	return 0xff;
}

void sns_multi_game_hunter_device::write_l(offs_t offset, uint8_t data)
{
	if (m_mode == MODE_DRAM)
		game_w(offset, data);
	else if (m_mode == MODE_CART)
		iccard_w(offset, data);
	else
		LOG("%s: write %06x = %02x\n", machine().describe_context(), offset, data);
}

uint8_t sns_multi_game_hunter_device::chip_read(offs_t offset)
{
	if (m_mode == MODE_DRAM)
		return offset >= 0x100 ? game_r(offset) : 0xff;
	if (m_mode == MODE_CART)
		return offset >= 0x100 ? iccard_r(offset) : 0xff;
	uint16_t const addr = offset & 0xffff;
	if (addr >= 0x6000 && addr < 0x8000)
	{
		if (uint8_t *const p = window_ptr(addr))
			return *p;
	}
	if (!machine().side_effects_disabled() && offset >= 0x100)
		LOG("%s: chip read %06x\n", machine().describe_context(), offset);
	return 0xff;
}

void sns_multi_game_hunter_device::chip_write(offs_t offset, uint8_t data)
{
	if (m_mode != MODE_BIOS)
	{
		if (offset >= 0x100)
		{
			if (m_mode == MODE_DRAM)
				game_w(offset, data);
			else
				iccard_w(offset, data);
		}
		return;
	}
	uint16_t const addr = offset & 0xffff;
	uint8_t const bank = (offset >> 16) & 0x7f;
	if (bank == 1 && addr >= 0x8000 && addr < 0xc000)
	{
		if (addr < 0xa000 || BIT(m_regs[3], 2))
			*page_ptr(addr) = data;
		else
			iccard_page_w(addr, data);
		return;
	}
	if (addr >= 0x6000 && addr < 0x8000)
	{
		if (uint8_t *const p = window_ptr(addr))
		{
			*p = data;
			return;
		}
	}
	LOG("%s: chip write %06x = %02x\n", machine().describe_context(), offset, data);
}

} // anonymous namespace


DEFINE_DEVICE_TYPE_PRIVATE(SNS_MULTI_GAME_HUNTER, device_sns_cart_interface, sns_multi_game_hunter_device, "mghsnes", "Multi Game Hunter (SNES)")

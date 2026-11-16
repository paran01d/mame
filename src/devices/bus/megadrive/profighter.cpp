// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***********************************************************************************

 Double Pro. Fighter (Mega Drive side)

 The Mega Drive half of the SNES + Mega Drive Double Pro Fighter floppy copier
 (the SNES half is bus/snes/profighter.cpp). Both BIOSes share one 27512; the
 Mega Drive one is the 16KB at $0000, repeated at $4000.

 The BIOS is a Master System program. The copier holds /M3 (pin B30) low, so
 the console starts in Mark III mode and the Z80 runs the menu, drives the
 82077 and fills the DRAM. To start a game the BIOS sets a run mode and the
 copier releases /M3 and resets the console, which then boots the DRAM (or the
 cartridge in the pass-through slot) as a Mega Drive.

 Z80 map in BIOS mode:
   $0000-$1FFF  EPROM $0000-$1FFF
   $2000-$200F  mode registers (address decoded, as the SNES side's $E000-$E00F)
   $2800-$28FF  write: page select, the page is the low address byte
   $3000-$30FF  82077 and PC link (& $0F):
                  0 r: bit 7 = FDC INT
                  1 r: bit 7 = no disk (the drive's pin 34)
                  2 w: DOR ($2D = drive 1 motor on, $04 = motor off)
                  3 r: bit 7 = PC link busy (a byte waiting, or a nibble
                       not yet taken)
                  4 rw: FIFO
                  5 r: main status
                  7 w: CCR
                  8 r: PC link byte   w: nibble to the PC (bits 0-3)
                  9 w: PC link acknowledge (toggles the handshake)
   $4000-$7FFF  window, chosen by the last write to $200C or $200D:
                  $200C  IC card: one 16KB block of the pass-through cartridge,
                         $4000-$5FFF its odd bytes and $6000-$7FFF its even ones
                  $200D  copier SRAM at $4000-$5FFF (8KB pages), EPROM
                         $2000-$3FFF at $6000-$7FFF (font, code called at $7000)
   $8000-$BFFF  16KB DRAM page (up to 256 pages = 32Mbit)
   $C000-$DFFF  console RAM

 A 16KB page holds one Super Magic Drive block, odd (low) bytes of 8K words
 first and then the even (high) bytes, the order SMD files store them in. The
 DRAM is loaded straight from the file and the IC card is read out the same
 way, so neither the load nor SAVE IC CARD has to reorder anything.

 The SRAM answers only for pages below $80: the boot code tests for RAM at
 page $80 with SRAM selected and, if it finds some, reports a 16M card.

 Mode registers, from the BIOS's RUN FILE / RUN IC CARD code:
   $2002  SRAM on (header byte 1 bit 7): 32KB on the odd bytes at $200001,
          switched by $A130F1 for games over 16Mbit
   $2006  run the IC card
   $2007  run the DRAM
   $2009  keep /M3 low (a Master System game): the Z80 then runs the DRAM,
          loaded as a plain ROM image, through a Sega mapper ($FFFD-$FFFF)
          that mirrors at the loaded size, as the game's own cartridge would
   $200C/$200D select the window (above)
   TODO: $2000, $2004, $2008 and $2801 are written but not known.

 PC link (BIOS $1D48, checked whenever the menu reads the joypad): the Front
 Far East protocol of the Super Magic Drive (machine/ffelink.cpp), with Z80
 addresses:
   0  write memory          1  read memory
   2  write VRAM            3  read VRAM
   4  jump                  5  select page (address low byte, a $28xx write)
   6  address 0: run the game whose SMD header is at $DC00, as RUN FILE does
 As on the Super Magic Drive, the card size in Mbit is kept at $DFF1. The run
 code also needs the number of pages loaded at $DEAA, which RUN FILE keeps.
 uCON64 drives it with --xfigmd and --xfigmds.

***********************************************************************************/

#include "emu.h"
#include "profighter.h"

#include "md_carts.h"
#include "md_slot.h"

#include "imagedev/floppy.h"
#include "machine/ffelink.h"
#include "machine/upd765.h"

#include "softlist_dev.h"

#define VERBOSE 0
#include "logmacro.h"


namespace {

constexpr uint32_t DRAM_SIZE = 0x400000;
constexpr uint32_t SRAM_SIZE = 0x8000;

void dpf_floppies(device_slot_interface &device)
{
	device.option_add("35hd", FLOPPY_35_HD);
	device.option_add("35dd", FLOPPY_35_DD);
}

ROM_START( dblprofightermd )
	ROM_REGION( 0x10000, "bios", 0 )
	ROM_LOAD( "dpf_bios.bin", 0x00000, 0x10000, CRC(bae38801) SHA1(d9677fe8df2f33871409ce933df0492bfc67971e) )
ROM_END


class md_double_pro_fighter_device : public device_t, public device_md_cart_interface, public device_nvram_interface
{
public:
	md_double_pro_fighter_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// Mega Drive mode (68000)
	virtual uint16_t read(offs_t offset) override;
	virtual void write(offs_t offset, uint16_t data, uint16_t mem_mask = ~0) override;
	virtual uint16_t read_a13(offs_t offset) override;
	virtual void write_a13(offs_t offset, uint16_t data) override;

	// Mark III mode (BIOS, Z80)
	virtual bool m3_r() override { return m_mode == MODE_BIOS || m_keep_m3; }
	virtual bool has_internal_bios() const override { return true; }
	virtual uint8_t read_sms(offs_t offset) override;
	virtual void write_sms(offs_t offset, uint8_t data) override;

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

	// the 32KB SRAM is battery backed
	virtual void nvram_default() override;
	virtual bool nvram_read(util::read_stream &file) override;
	virtual bool nvram_write(util::write_stream &file) override;

private:
	enum : uint8_t { MODE_BIOS, MODE_CART, MODE_DRAM };

	uint8_t reg_r(offs_t offset);
	void reg_w(offs_t offset, uint8_t data);
	uint8_t iccard_byte(offs_t addr);
	void fdc_irq_w(int state);
	bool sram_visible(offs_t offset) const;
	bool dram_addr(offs_t &addr) const;
	uint8_t dram_r(offs_t addr) const;
	void dram_w(offs_t addr, uint8_t data);
	uint8_t fdc_drive_select(uint8_t data);

	required_region_ptr<uint8_t> m_bios;
	required_device<n82077aa_device> m_fdc;
	required_device<floppy_connector> m_floppy;
	required_device<md_cart_slot_device> m_iccard;
	required_device<ffe_link_device> m_link;

	std::unique_ptr<uint8_t[]> m_dram;
	std::unique_ptr<uint8_t[]> m_sram;
	uint8_t m_mode;
	bool m_keep_m3;
	bool m_sram_on;
	bool m_sram_enable;
	bool m_sram_wp;
	uint8_t m_page;
	bool m_card_window;
	uint8_t m_sms_bank[3];
	uint8_t m_loaded_mask;
	bool m_fdc_irq;
	uint8_t m_dram_banks;
	uint8_t m_fdc_cmd;
	uint8_t m_fdc_cmd_pos;
};


md_double_pro_fighter_device::md_double_pro_fighter_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, MD_DOUBLE_PRO_FIGHTER, tag, owner, clock)
	, device_md_cart_interface(mconfig, *this)
	, device_nvram_interface(mconfig, *this)
	, m_bios(*this, "bios")
	, m_fdc(*this, "fdc")
	, m_floppy(*this, "fdc:1")
	, m_iccard(*this, "iccard")
	, m_link(*this, "link")
	, m_mode(MODE_BIOS)
	, m_keep_m3(false)
	, m_sram_on(false)
	, m_sram_enable(false)
	, m_sram_wp(false)
	, m_page(0)
	, m_card_window(false)
	, m_loaded_mask(0)
	, m_fdc_irq(false)
	, m_dram_banks(4)
	, m_fdc_cmd(0)
	, m_fdc_cmd_pos(0)
{
}

const tiny_rom_entry *md_double_pro_fighter_device::device_rom_region() const
{
	return ROM_NAME( dblprofightermd );
}

void md_double_pro_fighter_device::device_add_mconfig(machine_config &config)
{
	N82077AA(config, m_fdc, 24_MHz_XTAL);
	m_fdc->set_ready_polled(false);
	m_fdc->intrq_wr_callback().set(FUNC(md_double_pro_fighter_device::fdc_irq_w));

	FLOPPY_CONNECTOR(config, m_floppy, dpf_floppies, "35hd", floppy_image_device::default_pc_floppy_formats).enable_sound(true);
	SOFTWARE_LIST(config, "flop_list").set_original("megadriv_dpf_flop");

	MD_CART_SLOT(config, m_iccard, md_cart_nocopier, nullptr);

	FFE_LINK(config, m_link);
}

void md_double_pro_fighter_device::device_start()
{
	m_dram = std::make_unique<uint8_t[]>(DRAM_SIZE);
	m_sram = std::make_unique<uint8_t[]>(SRAM_SIZE);
	std::fill_n(m_dram.get(), DRAM_SIZE, 0xff);

	// The run mode is a latch on the copier: it survives the reset that
	// starts the game. Only power (a hard reset) brings the BIOS back.
	m_mode = MODE_BIOS;
	m_keep_m3 = false;
	m_sram_on = false;
	m_loaded_mask = 0;

	save_pointer(NAME(m_dram), DRAM_SIZE);
	save_pointer(NAME(m_sram), SRAM_SIZE);
	save_item(NAME(m_mode));
	save_item(NAME(m_keep_m3));
	save_item(NAME(m_sram_on));
	save_item(NAME(m_sram_enable));
	save_item(NAME(m_sram_wp));
	save_item(NAME(m_page));
	save_item(NAME(m_card_window));
	save_item(NAME(m_sms_bank));
	save_item(NAME(m_loaded_mask));
	save_item(NAME(m_fdc_irq));
	save_item(NAME(m_fdc_cmd));
	save_item(NAME(m_fdc_cmd_pos));
}

void md_double_pro_fighter_device::nvram_default()
{
	std::fill_n(m_sram.get(), SRAM_SIZE, 0xff);
}

bool md_double_pro_fighter_device::nvram_read(util::read_stream &file)
{
	auto const [err, actual] = util::read(file, m_sram.get(), SRAM_SIZE);
	return !err && (actual == SRAM_SIZE);
}

bool md_double_pro_fighter_device::nvram_write(util::write_stream &file)
{
	auto const [err, actual] = util::write(file, m_sram.get(), SRAM_SIZE);
	return !err;
}

static INPUT_PORTS_START( dblprofightermd )
	PORT_START("DRAM_SIZE")
	PORT_CONFNAME(0x03, 0x03, "DRAM Size")
	PORT_CONFSETTING(0x00, "8 Mbit (1MB)")
	PORT_CONFSETTING(0x01, "16 Mbit (2MB)")
	PORT_CONFSETTING(0x02, "24 Mbit (3MB)")
	PORT_CONFSETTING(0x03, "32 Mbit (4MB)")
INPUT_PORTS_END

ioport_constructor md_double_pro_fighter_device::device_input_ports() const
{
	return INPUT_PORTS_NAME( dblprofightermd );
}

// DRAM byte address -> what a board of the configured size holds
bool md_double_pro_fighter_device::dram_addr(offs_t &addr) const
{
	switch (m_dram_banks)
	{
	case 1: addr &= 0x0fffff; break;
	case 2: addr &= 0x1fffff; break;
	case 3: if (addr >= 0x300000) return false; break;
	default: break;
	}
	return true;
}

uint8_t md_double_pro_fighter_device::dram_r(offs_t addr) const
{
	if (!dram_addr(addr))
		return 0xff;
	return m_dram[addr];
}

void md_double_pro_fighter_device::dram_w(offs_t addr, uint8_t data)
{
	if (dram_addr(addr))
		m_dram[addr] = data;
}

void md_double_pro_fighter_device::device_reset()
{
	m_dram_banks = (ioport("DRAM_SIZE")->read() & 3) + 1;
	m_page = 0;
	m_card_window = false;
	m_sram_enable = false;
	m_sram_wp = false;
	m_sms_bank[0] = 0;
	m_sms_bank[1] = 1;
	m_sms_bank[2] = 2;
}

void md_double_pro_fighter_device::fdc_irq_w(int state)
{
	m_fdc_irq = state;
}

// The 82077's drive select outputs follow the DOR, not the drive number in
// a command, and the BIOS relies on it: it recalibrates "drive 0" ($07 $00)
// while the DOR selects drive 1, where the only drive is.
// TODO: machine/upd765.cpp takes the drive from the command for the 82077
// too. Until it follows the DOR, put the DOR's selection into the drive byte
// of every command that has one.
uint8_t md_double_pro_fighter_device::fdc_drive_select(uint8_t data)
{
	uint8_t const msr = m_fdc->msr_r();
	if (!(msr & 0x10))          // not busy: this byte is an opcode
	{
		m_fdc_cmd = data & 0x1f;
		m_fdc_cmd_pos = 0;
		return data;
	}
	if (msr & 0x20)             // execution phase: data
		return data;

	if (++m_fdc_cmd_pos == 1)
	{
		switch (m_fdc_cmd)
		{
		case 0x02: case 0x04: case 0x05: case 0x06: case 0x07: case 0x09: case 0x0a:
		case 0x0c: case 0x0d: case 0x0f: case 0x11: case 0x16: case 0x19: case 0x1d:
			return (data & ~3) | (m_fdc->dor_r() & 3);
		default:
			break;
		}
	}
	return data;
}


//-------------------------------------------------
//  Mark III mode (BIOS)
//-------------------------------------------------

// byte addr of the IC card, 68000 byte order (even = high byte)
uint8_t md_double_pro_fighter_device::iccard_byte(offs_t addr)
{
	uint16_t const word = m_iccard->read(addr >> 1);
	return BIT(addr, 0) ? (word & 0xff) : (word >> 8);
}

uint8_t md_double_pro_fighter_device::read_sms(offs_t offset)
{
	if (m_mode == MODE_DRAM) // a Master System game ($2009): DRAM behind a Sega mapper
	{
		if (offset < 0x400)
			return dram_r(offset);
		if (offset < 0xc000)
			// games write bank numbers with spare high bits set (Alex Kidd
			// selects bank $82) and rely on the cartridge ignoring them
			return dram_r((offs_t(m_sms_bank[offset >> 14] & m_loaded_mask) << 14) | (offset & 0x3fff));
		return 0xff;
	}
	if (m_mode == MODE_CART)
		return m_iccard->read_sms(offset);

	switch (offset >> 13)
	{
	case 0: // $0000-$1FFF
		return m_bios[offset];

	case 1: // $2000-$3FFF
		return reg_r(offset);

	case 2: // $4000-$5FFF
		if (m_card_window)
			return iccard_byte((((offs_t(m_page) << 13) | (offset & 0x1fff)) << 1) | 1);
		return BIT(m_page, 7) ? 0xff : m_sram[((m_page & 3) << 13) | (offset & 0x1fff)];

	case 3: // $6000-$7FFF
		if (m_card_window)
			return iccard_byte(((offs_t(m_page) << 13) | (offset & 0x1fff)) << 1);
		return m_bios[0x2000 | (offset & 0x1fff)];

	case 4: // $8000-$BFFF: DRAM
	case 5:
		return dram_r((offs_t(m_page) << 14) | (offset & 0x3fff));

	default:
		return 0xff;
	}
}

void md_double_pro_fighter_device::write_sms(offs_t offset, uint8_t data)
{
	if (m_mode == MODE_DRAM)
	{
		if (offset >= 0xfffd)
			m_sms_bank[offset - 0xfffd] = data;
		return;
	}
	if (m_mode == MODE_CART)
	{
		m_iccard->write_sms(offset, data);
		return;
	}

	switch (offset >> 13)
	{
	case 1:
		reg_w(offset, data);
		break;

	case 2:
		if (!m_card_window && !BIT(m_page, 7))
			m_sram[((m_page & 3) << 13) | (offset & 0x1fff)] = data;
		break;

	case 4:
	case 5:
		dram_w((offs_t(m_page) << 14) | (offset & 0x3fff), data);
		// Track the size loaded (as a page mask) for the Master System
		// mapper and the save RAM: loads start at page 0, which the
		// boot-time DRAM size probe never writes.
		if (m_page == 0)
			m_loaded_mask = 0;
		while (m_page > m_loaded_mask)
			m_loaded_mask = (m_loaded_mask << 1) | 1;
		break;

	default:
		break;
	}
}

uint8_t md_double_pro_fighter_device::reg_r(offs_t offset)
{
	if ((offset & 0x1f00) == 0x1000) // $3000-$30FF
	{
		switch (offset & 0x0f)
		{
		case 0: return m_fdc_irq ? 0x80 : 0x00;
		case 1: // drive pin 34 as "disk present": the BIOS's timeout here reports NO DISK
		{
			floppy_image_device *const fl = m_floppy->get_device();
			return (fl && fl->exists()) ? 0x00 : 0x80;
		}
		case 3: return m_link->busy_r() ? 0x80 : 0x00;
		case 4: return m_fdc->fifo_r();
		case 5: return m_fdc->msr_r();
		case 8: return m_link->data_r();
		default: break;
		}
	}

	if (!machine().side_effects_disabled())
		LOG("%s: unmapped register read $%04X\n", machine().describe_context(), 0x2000 | offset);
	return 0xff;
}

void md_double_pro_fighter_device::reg_w(offs_t offset, uint8_t data)
{
	switch (offset & 0x1f00)
	{
	case 0x0000: // $2000-$20FF: mode registers
	{
		uint8_t const reg = offset & 0x0f;
		LOG("%s: mode $%04X = %02X\n", machine().describe_context(), 0x2000 | offset, data);
		switch (reg)
		{
		case 0x02:
			m_sram_on = true;
			break;
		case 0x09:
			m_keep_m3 = true;
			break;
		case 0x0c:
		case 0x0d:
			m_card_window = (reg == 0x0c);
			break;
		case 0x06:
		case 0x07:
			// The copier resets the console into the game. The mode latch
			// survives that reset; only power brings the BIOS back.
			// TODO: reset the console through the cartridge slot (VRES on
			// the newer bus/megadrive/cart) rather than the whole machine.
			m_mode = (reg == 0x06) ? MODE_CART : MODE_DRAM;
			LOG("starting %s%s\n", (reg == 0x06) ? "IC card" : "DRAM", m_keep_m3 ? " (Mark III)" : "");
			machine().schedule_soft_reset();
			break;
		default:
			break;
		}
		break;
	}

	case 0x0800: // $2800-$28FF: page select
		m_page = offset & 0xff;
		break;

	case 0x1000: // $3000-$30FF
		switch (offset & 0x0f)
		{
		case 2: m_fdc->dor_w(data); break;
		case 4: m_fdc->fifo_w(fdc_drive_select(data)); break;
		case 7: m_fdc->ccr_w(data); break;
		case 8: m_link->nibble_w(data); break;
		case 9: m_link->ack_w(); break;
		default:
			LOG("%s: register $%04X = %02X\n", machine().describe_context(), 0x2000 | offset, data);
			break;
		}
		break;

	default:
		LOG("%s: register $%04X = %02X\n", machine().describe_context(), 0x2000 | offset, data);
		break;
	}
}


//-------------------------------------------------
//  Mega Drive mode
//-------------------------------------------------

uint16_t md_double_pro_fighter_device::read(offs_t offset)
{
	switch (m_mode)
	{
	case MODE_CART:
		return m_iccard->read(offset);

	case MODE_DRAM:
		if (sram_visible(offset))
			return 0xff00 | m_sram[offset & (SRAM_SIZE - 1)];
		if (offset < DRAM_SIZE / 2)
		{
			offs_t const block = offs_t(offset >> 13) << 14;
			offs_t const i = offset & 0x1fff;
			return (dram_r(block | 0x2000 | i) << 8) | dram_r(block | i);
		}
		return 0xffff;

	default:
		return 0xffff;
	}
}

void md_double_pro_fighter_device::write(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	switch (m_mode)
	{
	case MODE_CART:
		m_iccard->write(offset, data, mem_mask);
		break;

	case MODE_DRAM:
		if (sram_visible(offset) && ACCESSING_BITS_0_7 && !m_sram_wp)
			m_sram[offset & (SRAM_SIZE - 1)] = data & 0xff;
		break;

	default:
		break;
	}
}

uint16_t md_double_pro_fighter_device::read_a13(offs_t offset)
{
	return (m_mode == MODE_CART) ? m_iccard->read_a13(offset) : 0xffff;
}

void md_double_pro_fighter_device::write_a13(offs_t offset, uint16_t data)
{
	if (m_mode == MODE_CART)
		m_iccard->write_a13(offset, data);
	else if (m_mode == MODE_DRAM && offset == 0xf0 / 2) // $A130F1
	{
		m_sram_enable = BIT(data, 0);
		m_sram_wp = BIT(data, 1);
	}
}

// The save RAM sits on the odd bytes at $200001. A game of 16Mbit or less
// never reads ROM there, so the SRAM is always on; a bigger one (Phantasy
// Star IV) has ROM there too and switches the SRAM in with $A130F1.
bool md_double_pro_fighter_device::sram_visible(offs_t offset) const
{
	if (!m_sram_on || (offset & 0xf00000 / 2) != 0x200000 / 2)
		return false;
	return m_sram_enable || m_loaded_mask < 0x80;
}

} // anonymous namespace


DEFINE_DEVICE_TYPE_PRIVATE(MD_DOUBLE_PRO_FIGHTER, device_md_cart_interface, md_double_pro_fighter_device, "dblprofightermd", "Double Pro. Fighter (Mega Drive)")

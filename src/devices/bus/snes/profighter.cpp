// license:BSD-3-Clause
// copyright-holders:

/*
Super Pro Fighter Q / QA / QB / X SNES copier devices

These devices sit between the SNES and a game cartridge, providing:
- Floppy disk interface (FDC) for loading/saving game images
- DRAM for storing game images loaded from disk
- IC card slot for inserting game cartridges
- Cart passthrough for direct play or cart-to-disk copying

Memory map (QB, System Mode 0 / BIOS mode):
  $8000-$9FFF: DRAM page window (8KB, page selected by $E000-$E003)
  $A000-$BFFF: Cart/SRAM page window (when enabled via $7000)
  $C000-$DFFF: Reserved / I/O
  $E000-$E00F: Hardware registers (PAL-decoded)
  $E000-$FFFF: Firmware ROM (bank 0)

Hardware registers ($E000-$E00F):
  $E000-$E003 W: DRAM/cart page select (8KB page number)
  $E005 W:       System mode 1 — cart passthrough latch (survives reset)
  $E006 W:       System mode 2 — DRAM cartridge emulation (via $E00D)
  $E00D W:       DRAM mode latch (bit 0, survives reset)

Page register + bank addressing:
  DRAM write window ($8000-$9FFF):
    offset = bank * 32KB + page * 8KB + (addr - $8000)
  Cart read window ($A000-$BFFF):
    offset = bank * 32KB + (page & 3) * 8KB + (addr - $A000)
*/

#include "emu.h"
#include "profighter.h"

#include "bus/snes/snes_carts.h"
#include "imagedev/floppy.h"
#include "machine/upd765.h"


namespace {

void profght_floppies(device_slot_interface &device)
{
	device.option_add("35hd", FLOPPY_35_HD);
	device.option_add("35dd", FLOPPY_35_DD);
}

} // anonymous namespace


//===========================================================================
//  Pro Fighter Q (variant 1)
//===========================================================================

DEFINE_DEVICE_TYPE(SNS_PRO_FIGHTER_Q, sns_pro_fighter_q_device, "profighterq", "Super Pro. Fighter Q (variant 1)")

sns_pro_fighter_q_device::sns_pro_fighter_q_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SNS_PRO_FIGHTER_Q, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
{
}

void sns_pro_fighter_q_device::device_start()
{
}

void sns_pro_fighter_q_device::device_reset()
{
}

ROM_START( profghtq )
	ROM_REGION( 0x4000, "program", 0 )
	ROM_LOAD( "u7", 0x0000, 0x4000, CRC(55e3ea49) SHA1(e98f03a97d3bfbba29afc1ffe9bdbf7a79bf3a6e) )
ROM_END

void sns_pro_fighter_q_device::device_add_mconfig(machine_config &config)
{
}

const tiny_rom_entry *sns_pro_fighter_q_device::device_rom_region() const
{
	return ROM_NAME( profghtq );
}


//===========================================================================
//  Pro Fighter QA (variant 2)
//===========================================================================

DEFINE_DEVICE_TYPE(SNS_PRO_FIGHTER_QA, sns_pro_fighter_qa_device, "profighterqa", "Super Pro. Fighter Q (variant 2)")

sns_pro_fighter_qa_device::sns_pro_fighter_qa_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SNS_PRO_FIGHTER_QA, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
{
}

void sns_pro_fighter_qa_device::device_start()
{
}

void sns_pro_fighter_qa_device::device_reset()
{
}

ROM_START( profghtqa )
	ROM_REGION( 0x8000, "program", 0 )
	ROM_LOAD( "u7", 0x0000, 0x8000, CRC(6cf83a73) SHA1(bf2e28b5eb1717e9987ef49e647cf3dbd4f7a3ea) )
ROM_END

void sns_pro_fighter_qa_device::device_add_mconfig(machine_config &config)
{
}

const tiny_rom_entry *sns_pro_fighter_qa_device::device_rom_region() const
{
	return ROM_NAME( profghtqa );
}


//===========================================================================
//  Pro Fighter QB (variant 3)
//===========================================================================

DEFINE_DEVICE_TYPE(SNS_PRO_FIGHTER_QB, sns_pro_fighter_qb_device, "profighterqb", "Super Pro. Fighter Q (variant 3)")

sns_pro_fighter_qb_device::sns_pro_fighter_qb_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SNS_PRO_FIGHTER_QB, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
	, m_program_rom(*this, "program")
	, m_fdc(*this, "fdc")
	, m_iccard(*this, "cartslot")
	, m_dram_mode(false)
{
}

ROM_START( profghtqb )
	ROM_REGION( 0x4000, "program", 0 )
	ROM_LOAD( "u7", 0x0000, 0x4000, CRC(4f27e1fe) SHA1(02ccb359c39d50c7e22deeb8f5e3cbef47029e02) )

	ROM_REGION( 0xe00, "pals", ROMREGION_ERASE00 )
	ROM_LOAD( "peel18cv8p.u2",  0x000, 0x155, CRC(e40bc5c0) SHA1(207437a41ae04bf4d5093775a6f2e602d0840d12) )
	ROM_LOAD( "peel18cv8p.u4",  0x200, 0x155, CRC(bd152ad4) SHA1(bc79994d63efa7ef6e3cea4bd6f7940b68c40176) )
	ROM_LOAD( "peel18cv8p.u5",  0x400, 0x155, CRC(b03c21a0) SHA1(dde7644cca7b383ab19e3598f30250a6695816fd) )
	ROM_LOAD( "gal16v8b.u9",    0x600, 0x117, CRC(c9d37428) SHA1(9c06739d34a25095c0b0420910b82252ab0d05f8) )
	ROM_LOAD( "peel18cv8p.u10", 0x800, 0x155, CRC(e0a3a486) SHA1(20162b8c174e73fb7d57d9a221eef87fb88005a5) )
	ROM_LOAD( "gal16v8b.u11",   0xa00, 0x117, CRC(0b8a2c8f) SHA1(0cc9c9ed671f85a3ad8622b65f94b546bb13e6b6) )
	ROM_LOAD( "gal16v8b.u12",   0xc00, 0x117, CRC(a1d23a1a) SHA1(d83dc1b50771f5d8b68293c85b0b94e257521bd3) )
ROM_END


void sns_pro_fighter_qb_device::device_start()
{
	// nvram layout:
	//   [0x000000-0x007FFF] 32KB  D43256AGU-15LL static RAM (work RAM)
	//   [0x008000]          1B    control register (DRAM mode switch)
	//   [0x008010-0x008011] 2B    FDC status/data stubs
	//   [0x010000-0x40FFFF] 4MB   DRAM (4x GM71C4400BJ70 + 4x KM44C1000BLJ-6)
	nvram_alloc(0x410000);
	m_dram_mode = false;
	m_cart_passthrough = false;
	m_cart_enable = false;
	m_dma_addr = 0;
	m_dram_page = 0;
	m_dram_write_log = 0;
	save_item(NAME(m_dram_mode));
	save_item(NAME(m_cart_passthrough));
	save_item(NAME(m_cart_enable));
	save_item(NAME(m_dram_page));
}

void sns_pro_fighter_qb_device::device_reset()
{
	// m_dram_mode and m_cart_passthrough are intentionally NOT reset here.
	// These are hardware latches that persist across SNES soft reset.
	m_dma_addr = 0;
	m_dram_page = 0;
	m_cart_enable = false;
}

const tiny_rom_entry *sns_pro_fighter_qb_device::device_rom_region() const
{
	return ROM_NAME( profghtqb );
}

// nvram offsets for I/O registers (see device_start for layout)
static constexpr uint32_t QB_CTRL_REG   = 0x008000;
static constexpr uint32_t QB_FDC_BASE   = 0x008010;
static constexpr uint32_t QB_DRAM_START = 0x010000;  // first byte of DRAM in nvram

uint8_t sns_pro_fighter_qb_device::read_h(offs_t offset)
{
	// Cart passthrough mode ($E005 latch): IC card cartridge provides ROM directly
	if (m_cart_passthrough && m_iccard->exists())
	{
		const uint8_t bank = (offset >> 16) & 0x3f;
		const uint32_t cart_offset = (uint32_t(bank) * 0x8000) + ((offset & 0xffff) - 0x8000);
		if (m_iccard->m_cart->get_rom_size() > 0)
			return m_iccard->m_cart->get_rom_base()[cart_offset % m_iccard->m_cart->get_rom_size()];
		return 0xff;
	}

	// DRAM mode ($E00D latch): game runs from DRAM
	if (m_dram_mode)
	{
		if (m_nvram.empty()) return 0xff;
		const uint8_t bank = (offset >> 16) & 0x3f;
		const uint32_t dram_addr = QB_DRAM_START + (uint32_t(bank) * 0x8000) + ((offset & 0xffff) - 0x8000);
		return (dram_addr < m_nvram.size()) ? m_nvram[dram_addr] : 0xff;
	}

	// Boot mode: $A000-$BFFF is cart page window when cart is enabled
	{
		const uint16_t addr = offset & 0xffff;
		const uint8_t bk = (offset >> 16) & 0x3f;
		if (addr >= 0xa000 && addr < 0xc000 && m_cart_enable && m_iccard->exists())
		{
			const uint32_t cart_offset = (uint32_t(bk) * 0x8000)
				+ (uint32_t(m_dram_page & 0x03) * 0x2000)
				+ (addr - 0xa000);
			if (cart_offset < m_iccard->m_cart->get_rom_size())
				return m_iccard->m_cart->get_rom_base()[cart_offset];
			return 0xff;
		}
	}

	// Boot mode default: firmware ROM at $C000-$FFFF
	return m_program_rom->base()[offset & (m_program_rom->bytes() - 1) & 0x3fff];
}

uint8_t sns_pro_fighter_qb_device::read_l(offs_t offset)
{
	const uint16_t addr = offset & 0xffff;
	const uint8_t  bank = (offset >> 16) & 0xff;

	// Cart passthrough mode: IC card provides all ROM reads
	if (m_cart_passthrough && m_iccard->exists())
		return read_h(offset);

	// $8000-$BFFF region
	if (addr < 0xc000)
	{
		// $A000-$BFFF: cart page window when enabled
		if (addr >= 0xa000 && m_cart_enable && m_iccard->exists())
		{
			const uint32_t cart_offset = (uint32_t(bank) * 0x8000)
				+ (uint32_t(m_dram_page & 0x03) * 0x2000)
				+ (addr - 0xa000);
			if (cart_offset < m_iccard->m_cart->get_rom_size())
				return m_iccard->m_cart->get_rom_base()[cart_offset];
			return 0xff;
		}

		// $8000-$9FFF: DRAM
		if (m_nvram.empty()) return 0xff;
		const uint32_t dram_addr = QB_DRAM_START + (uint32_t(bank) * 0x8000) + (addr - 0x8000);
		if (dram_addr < m_nvram.size()) return m_nvram[dram_addr];
		return 0xff;
	}

	// Banks $40-$6F: $C000-$FFFF is also DRAM (no firmware ROM here)
	if (bank >= 0x40 && bank < 0x70)
	{
		if (m_nvram.empty()) return 0xff;
		const uint32_t dram_addr = QB_DRAM_START + (uint32_t(bank) * 0x8000) + (addr - 0x8000);
		if (dram_addr < m_nvram.size()) return m_nvram[dram_addr];
		return 0xff;
	}

	// Banks $00-$3F, addr $C000-$FFFF: firmware ROM in boot mode, DRAM in game mode
	return read_h(offset);
}

uint8_t sns_pro_fighter_qb_device::read_ram(offs_t offset)
{
	if (offset == QB_FDC_BASE)
		return m_fdc->msr_r();
	if (offset == QB_FDC_BASE + 1)
		return m_fdc->fifo_r();

	if (!m_nvram.empty() && offset < m_nvram.size())
		return m_nvram[offset];
	return 0xff;
}

void sns_pro_fighter_qb_device::write_ram(offs_t offset, uint8_t data)
{
	if (offset == QB_CTRL_REG)
	{
		m_dram_mode = BIT(data, 0);
		return;
	}
	if (offset == QB_FDC_BASE)
	{
		m_fdc->dsr_w(data);
		return;
	}
	if (offset == QB_FDC_BASE + 1)
	{
		m_fdc->fifo_w(data);
		return;
	}

	if (!m_nvram.empty() && offset < m_nvram.size())
		m_nvram[offset] = data;
}

uint8_t sns_pro_fighter_qb_device::chip_read(offs_t offset)
{
	const uint16_t addr = offset & 0xffff;

	// FDC registers at $2800-$28FF (offset is relative: 0x00-0xFF)
	if (addr <= 0xff)
	{
		switch (addr & 7)
		{
			case 0: return m_fdc->sra_r();
			case 1: return m_fdc->srb_r();
			case 2: return m_fdc->dor_r();
			case 3: return m_fdc->tdr_r();
			case 4: return m_fdc->msr_r();
			case 5: return m_fdc->fifo_r();
			case 7: return m_fdc->dir_r();
			default: return 0xff;
		}
	}

	// $6000-$7FFF: IC card slot (open bus when no cart inserted)
	return 0xff;
}

void sns_pro_fighter_qb_device::chip_write(offs_t offset, uint8_t data)
{
	const uint16_t addr = offset & 0xffff;
	const uint8_t bank = (offset >> 16) & 0xff;

	// FDC registers at $2800-$28FF (offset is relative: 0x00-0xFF)
	if (addr <= 0xff)
	{
		switch (addr & 7)
		{
			case 2: m_fdc->dor_w(data); return;
			case 3: m_fdc->tdr_w(data); return;
			case 4: m_fdc->dsr_w(data); return;
			case 5: m_fdc->fifo_w(data); return;
			case 7: m_fdc->ccr_w(data); return;
		}
		return;
	}

	// $6000-$7FFF: IC card slot / QB control area
	if (addr >= 0x6000 && addr < 0x8000)
	{
		if (addr == 0x7000)
			m_cart_enable = (data == 0x55);
		return;
	}

	// $A000-$BFFF: cart SRAM passthrough when cart is enabled
	if (addr >= 0xa000 && addr < 0xc000 && m_cart_enable && m_iccard->exists())
	{
		if (m_iccard->m_cart->get_nvram_size() > 0)
		{
			const uint32_t sram_offset = (uint32_t(bank) * 0x8000)
				+ (uint32_t(m_dram_page & 0x03) * 0x2000)
				+ (addr - 0xa000);
			m_iccard->write_ram(sram_offset, data);
		}
		return;
	}

	// $8000-$FFFF: hardware registers and DRAM
	if (addr >= 0x8000)
	{
		// $E000-$E00F: PAL-decoded hardware registers
		if (addr >= 0xe000 && addr <= 0xe00f)
		{
			if (addr >= 0xe000 && addr <= 0xe003)
			{
				// Page register: selects 8KB page for DRAM/cart windows
				m_dram_page = data;
			}
			else if (addr == 0xe005 && !m_cart_passthrough)
			{
				// Cart passthrough latch (survives reset)
				m_cart_passthrough = BIT(data, 7);
				if (m_cart_passthrough)
					machine().schedule_soft_reset();
			}
			else if (addr == 0xe00d && !m_dram_mode)
			{
				// DRAM mode latch (survives reset)
				m_dram_mode = BIT(data, 0);
				if (m_dram_mode)
					machine().schedule_soft_reset();
			}
			return;
		}

		// $8000-$9FFF: DRAM page window write
		if (!m_nvram.empty())
		{
			uint32_t dram_addr;
			if (addr < 0xa000)
			{
				// Page window: bank * 32KB + page * 8KB + offset
				dram_addr = QB_DRAM_START + (uint32_t(bank) * 0x8000) + (uint32_t(m_dram_page) * 0x2000) + (addr - 0x8000);
			}
			else
			{
				// Outside page window: bank-based linear mapping
				dram_addr = QB_DRAM_START + (uint32_t(bank) * 0x8000) + (addr - 0x8000);
			}
			if (dram_addr < m_nvram.size())
				m_nvram[dram_addr] = data;
		}
		return;
	}
}

void sns_pro_fighter_qb_device::write_l(offs_t offset, uint8_t data)
{
	const uint16_t addr = offset & 0xffff;
	const uint8_t bank = (offset >> 16) & 0xff;

	// Banks $40-$6F, addresses $8000-$FFFF: DRAM
	if (addr >= 0x8000 && bank >= 0x40 && bank < 0x70)
	{
		if (m_nvram.empty()) return;
		const uint32_t dram_addr = QB_DRAM_START + (uint32_t(bank) * 0x8000) + (addr - 0x8000);
		if (dram_addr < m_nvram.size())
			m_nvram[dram_addr] = data;
	}
}

void sns_pro_fighter_qb_device::fdc_drq_w(int state)
{
	if (!state || m_nvram.empty())
		return;
	const uint8_t data = m_fdc->dma_r();
	const uint32_t nvram_addr = QB_DRAM_START + m_dma_addr;
	if (nvram_addr < m_nvram.size())
		m_nvram[nvram_addr] = data;
	m_dma_addr = (m_dma_addr + 1) & 0x3fffff;
}

void sns_pro_fighter_qb_device::device_add_mconfig(machine_config &config)
{
	auto &fdc = N82077AA(config, "fdc", 24_MHz_XTAL);
	fdc.set_ready_polled(false);

	FLOPPY_CONNECTOR(config, "fdc:1", profght_floppies, "35hd", floppy_image_device::default_pc_floppy_formats).enable_sound(true);

	SNS_CART_SLOT(config, "cartslot", 0, snes_cart, nullptr);
}


/*
Pro Fighter X (MB03D) copier
*/

DEFINE_DEVICE_TYPE(SNS_PRO_FIGHTER_X, sns_pro_fighter_x_device, "profighterx", "Pro Fighter X (MB03D)")

sns_pro_fighter_x_device::sns_pro_fighter_x_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SNS_PRO_FIGHTER_X, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
{
}

void sns_pro_fighter_x_device::device_start()
{
}

void sns_pro_fighter_x_device::device_reset()
{
}

ROM_START( profghtx )
	ROM_REGION( 0x10000, "program", 0 )
	ROM_LOAD( "u7", 0x0000, 0x10000, CRC(3e351e28) SHA1(a0e5af71c5fb2b4f4e5e0e3954f110d23c511299) )
ROM_END

void sns_pro_fighter_x_device::device_add_mconfig(machine_config &config)
{
}

const tiny_rom_entry *sns_pro_fighter_x_device::device_rom_region() const
{
	return ROM_NAME( profghtx );
}

// license:BSD-3-Clause
// copyright-holders:Ivan Vangelista, Mike Bissett

/***********************************************************************************************************

 Super Pro Fighter Q (3 variants)
 Super Pro Fighter X (MB03D)
 Double Pro Fighter (SNES side)

 TODO: Q, QA and X are stubs. QB and the Double Pro Fighter are emulated.

 Memory map (QB, Mode 0 / BIOS mode):
   $8000-$9FFF: DRAM page window (8KB, page selected by $E000-$E003)
   $A000-$BFFF: IC card page window ($E00C) or copier SRAM ($E00D)
   $C000-$FFFF: firmware ROM (16KB)
   $E000-$E0FF: registers (write only, mirrored every 16 bytes)
   $2800-$2807: floppy disk controller
   $70-$7D:0000-$7FFF: 32KB SRAM

 Hardware registers ($E000-$E00F):
   $E000-$E003 W: DRAM/cart page select (8KB page number)
   $E005 W:       System mode 1 — cart passthrough latch (survives reset)
   $E006/7 W:     System mode 2/3 — DRAM cartridge emulation
   $E00C W:       Select IC card at $A000-$BFFF
   $E00D W:       Select copier SRAM at $A000-$BFFF

 Page register + bank addressing:
   DRAM write window ($8000-$9FFF):
     offset = bank * 32KB + page * 8KB + (addr - $8000)
   Cart read window ($A000-$BFFF):
     offset = bank * 32KB + (page & 3) * 8KB + (addr - $A000)

 ***********************************************************************************************************/


#include "emu.h"
#include "profighter.h"

#include "snes_carts.h"

#include "imagedev/floppy.h"
#include "machine/ffelink.h"
#include "machine/upd765.h"

#include "softlist_dev.h"

#define VERBOSE 0
#include "logmacro.h"


DEFINE_DEVICE_TYPE(SNS_PRO_FIGHTER_Q, sns_pro_fighter_q_device, "profighterq", "Super Pro. Fighter Q (variant 1)");
DEFINE_DEVICE_TYPE(SNS_PRO_FIGHTER_QA, sns_pro_fighter_qa_device, "profighterqa", "Super Pro. Fighter Q (variant 2)");
DEFINE_DEVICE_TYPE(SNS_PRO_FIGHTER_QB, sns_pro_fighter_qb_device, "profighterqb", "Super Pro. Fighter Q (variant 3)");
DEFINE_DEVICE_TYPE(SNS_PRO_FIGHTER_X, sns_pro_fighter_x_device, "profighterx", "Pro Fighter X (MB03D)");
DEFINE_DEVICE_TYPE(SNS_DOUBLE_PRO_FIGHTER, sns_double_pro_fighter_device, "dblprofighter", "Double Pro. Fighter (SNES)");


static void profght_floppies(device_slot_interface &device)
{
	device.option_add("35dd", FLOPPY_35_DD);
}

static void profghtqb_floppies(device_slot_interface &device)
{
	device.option_add("35hd", FLOPPY_35_HD);
	device.option_add("35dd", FLOPPY_35_DD);
}


/*
Super Pro. Fighter Q copier (variant 1)

Main components:

MCCS3201FN floppy disk controller
24 MHz XTAL (for the FDC)
ST10198P CIC replacement
HM6264LP-15 standard SRAM
MCM60L256AF10 standard SRAM
*/

sns_pro_fighter_q_device::sns_pro_fighter_q_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SNS_PRO_FIGHTER_Q, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
{
}

ROM_START( profghtq )
	ROM_REGION( 0x4000, "program", 0 )
	ROM_LOAD( "u9", 0x0000, 0x4000, CRC(4f27e1fe) SHA1(ee11ac36b5e51476ab92c66135a2353e038c014b) )

	ROM_REGION( 0x1200, "plds", ROMREGION_ERASE00 )
	ROM_LOAD( "palce16v8h.u1",  0x0000, 0x0117, CRC(3191765b) SHA1(3042c29cad1c6eebfa756514939da973b5856bd2) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "palce20v8h.u5",  0x0200, 0x0157, CRC(5fa3c6bf) SHA1(edb825a15256a3a0c2fefdb3d8bf2482fd630744) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "palce16v8h.u6",  0x0400, 0x0117, CRC(f9a94549) SHA1(48eab926a13d4015e91833ea4b2e3da8a461392e) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "palce20v8h.u7",  0x0600, 0x0157, CRC(b321697d) SHA1(902459c06e1e35b3ad052f9439fd54add6ad2e5b) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "palce16v8h.u12", 0x0800, 0x0117, CRC(40f86bce) SHA1(97edea87fc3bcfa296202fc106b1838ceffb1730) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "palce16v8h.u13", 0x0a00, 0x0117, CRC(691d8a86) SHA1(f61a93cd661035440461076ee42ec7bee98ce8ff) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "ami18cv8b.u14",  0x0c00, 0x0155, CRC(472405e4) SHA1(615196f1ab9819b2e22f5f48d5edd35d4db70098) ) // this is actually a recreation targeted for a PEEL18CV8 device
	ROM_LOAD( "palce16v8h.u15", 0x0e00, 0x0117, CRC(0b8a2c8f) SHA1(0cc9c9ed671f85a3ad8622b65f94b546bb13e6b6) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "palce16v8h.u16", 0x1000, 0x0117, CRC(70346ef5) SHA1(d9c0a7a2c466fa1fcc1680ee5dc3777714c05fe5) ) // this is actually a recreation targeted for a GAL16V8 device
ROM_END


void sns_pro_fighter_q_device::device_start()
{
}

void sns_pro_fighter_q_device::device_reset()
{

}

const tiny_rom_entry *sns_pro_fighter_q_device::device_rom_region() const
{
	return ROM_NAME( profghtq );
}

void sns_pro_fighter_q_device::device_add_mconfig(machine_config &config)
{
	N82077AA(config, "fdc", 24_MHz_XTAL); // actually MCCS3201FN, divider?
	FLOPPY_CONNECTOR(config, "fdc:0", profght_floppies, "35dd", floppy_image_device::default_pc_floppy_formats).enable_sound(true);

	SNS_CART_SLOT(config, "cartslot", snes_cart, nullptr);
}


/*
Super Pro. Fighter Q copier (variant 2)

Main components:

GM82C765B floppy disk controller
16 MHz XTAL (for the FDC)
74LS216 CIC replacement (not to be confused with the TTL)
MK4864N-120 standard SRAM
M5M5256FP CMOS Static RAM
6 x HY524800J-80 Fast Page DRAM (on a sub board)
*/

sns_pro_fighter_qa_device::sns_pro_fighter_qa_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SNS_PRO_FIGHTER_QA, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
{
}

ROM_START( profghtqa )
	ROM_REGION( 0x8000, "program", 0 )
	ROM_LOAD( "u10", 0x0000, 0x8000, CRC(56132c4e) SHA1(6649b1f13a96d144907ebab65ce00ac168b0dbe5) )

	ROM_REGION( 0x1400, "plds", ROMREGION_ERASE00 )
	ROM_LOAD( "ami18cv8b.u2",   0x0000, 0x0155, CRC(4ee0c889) SHA1(a5d060c8892dd24cbc1c14bc937ad5c301831dea) ) // this is actually a recreation targeted for a PEEL18CV8 device
	ROM_LOAD( "palce16v8h.u3",  0x0200, 0x0117, CRC(17e897b9) SHA1(e4e9806187ffb51d4363ef9fd34d5b4a3e62e1d3) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "palce20v8h.u4",  0x0400, 0x0157, CRC(5fa3c6bf) SHA1(edb825a15256a3a0c2fefdb3d8bf2482fd630744) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "palce20v8h.u8",  0x0600, 0x0157, CRC(a9f5c9ed) SHA1(395f523b77d9d9ed4f20c124153697c7b301cfcc) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "palce16v8h.u13", 0x0800, 0x0117, CRC(c77839b0) SHA1(6370c3862caee28dd9cc543c42c1ee9e87bdd64d) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "palce16v8h.u14", 0x0a00, 0x0117, CRC(12c79d25) SHA1(9796270dd2897803686638bbd87aca1364e41933) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "palce16v8h.u15", 0x0c00, 0x0117, CRC(0b8a2c8f) SHA1(0cc9c9ed671f85a3ad8622b65f94b546bb13e6b6) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "ami18cv8b.u16",  0x0e00, 0x0155, CRC(481a6974) SHA1(aff914837a7d8ef804b0ac4049d6f23ab23a1cda) ) // this is actually a recreation targeted for a PEEL18CV8 device
	ROM_LOAD( "palce16v8h.u17", 0x1000, 0x0117, CRC(70346ef5) SHA1(d9c0a7a2c466fa1fcc1680ee5dc3777714c05fe5) ) // this is actually a recreation targeted for a GAL16V8 device
	ROM_LOAD( "palce16v8h.u18", 0x1200, 0x0117, CRC(a3bbb6b1) SHA1(bdf291d2e3601d7b48c348b1d99b8872b1b4fedd) ) // this is actually a recreation targeted for a GAL16V8 device
ROM_END


void sns_pro_fighter_qa_device::device_start()
{
}

void sns_pro_fighter_qa_device::device_reset()
{

}

const tiny_rom_entry *sns_pro_fighter_qa_device::device_rom_region() const
{
	return ROM_NAME( profghtqa );
}

void sns_pro_fighter_qa_device::device_add_mconfig(machine_config &config)
{
	WD37C65B(config, "fdc", 16_MHz_XTAL); // actually GM82C765B
	FLOPPY_CONNECTOR(config, "fdc:0", profght_floppies, "35dd", floppy_image_device::default_pc_floppy_formats).enable_sound(true);

	SNS_CART_SLOT(config, "cartslot", snes_cart, nullptr);
}


/*
Super Pro. Fighter Q copier (variant 3)

Main components:

ACCMicro 3201 floppy disk controller
24 MHz XTAL (for the FDC)
unmarked CIC replacement
D43256AGU-15LL CMOS Static RAM
4 x GM71C4400BJ70 + 4 x KM44C1000BLJ-6 Fast Page DRAM (on a sub board)
*/

sns_pro_fighter_qb_device::sns_pro_fighter_qb_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: sns_pro_fighter_qb_device(mconfig, SNS_PRO_FIGHTER_QB, tag, owner, clock)
{
}

sns_pro_fighter_qb_device::sns_pro_fighter_qb_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
	, m_program_rom(*this, "program")
	, m_fdc(*this, "fdc")
	, m_dram_mode(false)
	, m_fdc_irq(false)
	, m_iccard(*this, "cartslot")
	, m_dram_size_port(*this, "DRAM_SIZE")
	, m_dram_size(DRAM_MAX)
	, m_cart_passthrough(false)
	, m_cart_enable(false)
	, m_hirom(false)
	, m_dram_page(0)
{
}

// The DRAM sub board holds eight 1M x 4 chips, two per megabyte (D0-D3 and
// D4-D7).
static INPUT_PORTS_START(profighter_qb)
	PORT_START("DRAM_SIZE")
	PORT_CONFNAME(0x03, 0x03, "DRAM Size")
	PORT_CONFSETTING(0x00, "8 Mbit")
	PORT_CONFSETTING(0x01, "16 Mbit")
	PORT_CONFSETTING(0x02, "24 Mbit")
	PORT_CONFSETTING(0x03, "32 Mbit")
INPUT_PORTS_END

ioport_constructor sns_pro_fighter_qb_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(profighter_qb);
}

ROM_START( profghtqb ) // same program as profghtrq but different plds
	ROM_REGION( 0x4000, "program", 0 )
	ROM_LOAD( "u7", 0x0000, 0x4000, CRC(4f27e1fe) SHA1(ee11ac36b5e51476ab92c66135a2353e038c014b) )

	ROM_REGION( 0xe00, "plds", ROMREGION_ERASE00 ) // probably all recreations, but chip types match
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
	m_dram = std::make_unique<uint8_t[]>(DRAM_MAX);
	m_sram = std::make_unique<uint8_t[]>(SRAM_SIZE);

	save_pointer(NAME(m_dram), DRAM_MAX);
	save_pointer(NAME(m_sram), SRAM_SIZE);
	save_item(NAME(m_dram_mode));
	save_item(NAME(m_cart_passthrough));
	save_item(NAME(m_cart_enable));
	save_item(NAME(m_hirom));
	save_item(NAME(m_dram_page));
	save_item(NAME(m_fdc_irq));
}

void sns_pro_fighter_qb_device::device_reset()
{
	// The mode latches (m_dram_mode, m_cart_passthrough) survive a console
	// reset: the BIOS sets Mode 1 and then resets the console into the game.
	m_dram_page = 0;
	m_cart_enable = false;

	m_dram_size = ((m_dram_size_port->read() & 0x03) + 1) * 0x100000;
	LOG("DRAM %u Mbit\n", m_dram_size >> 17);
}

const tiny_rom_entry *sns_pro_fighter_qb_device::device_rom_region() const
{
	return ROM_NAME( profghtqb );
}

// DRAM above the installed size reads $FF
uint8_t sns_pro_fighter_qb_device::dram_r(uint32_t offset) const
{
	if (offset >= m_dram_size)
		return 0xff;

	return m_dram[offset];
}

void sns_pro_fighter_qb_device::dram_w(uint32_t offset, uint8_t data)
{
	if (offset < m_dram_size)
		m_dram[offset] = data;
}

// Mode 2 (DRAM as the cartridge ROM): LoROM is $8000-$FFFF of banks $00-$7D,
// HiROM is the whole of banks $40-$7D and $C0-$FF. Reads and writes decode
// the same way.
bool sns_pro_fighter_qb_device::dram_mode_offset(offs_t offset, uint32_t &dram) const
{
	const uint16_t addr = offset & 0xffff;
	const uint8_t bk = (offset >> 16) & 0x7f;

	if (m_hirom)
	{
		dram = uint32_t(bk & 0x3f) * 0x10000 + addr;
		return true;
	}
	if (addr >= 0x8000)
	{
		dram = uint32_t(bk) * 0x8000 + (addr - 0x8000);
		return true;
	}
	return false;
}

// The IC card window drives the card with a normal SNES address (page bits
// on A13-A14), and the card decodes it itself, so LoROM and HiROM carts both
// read correctly. For a HiROM cart, LoROM-mode reads at $80:A000 page 3 show
// ROM $E000-$FFFF, where its header lives.
uint8_t sns_pro_fighter_qb_device::iccard_read(uint8_t bank, uint16_t addr)
{
	const offs_t a = (offs_t(bank & 0x7f) << 16) | (addr & 0x8000) | (offs_t(m_dram_page & 0x03) << 13) | (addr & 0x1fff);
	return m_iccard->read_h(a);
}

uint8_t sns_pro_fighter_qb_device::firmware_r(uint16_t addr) const
{
	return m_program_rom->base()[addr & (m_program_rom->bytes() - 1) & 0x3fff];
}

uint8_t sns_pro_fighter_qb_device::read_h(offs_t offset)
{
	const uint16_t addr = offset & 0xffff;
	const uint8_t bk = (offset >> 16) & 0x7f;

	// Mode 1: the IC card is the cartridge
	if (m_cart_passthrough && m_iccard->exists())
		return m_iccard->read_h(offset & 0x7fffff);

	uint32_t dram;
	if (m_dram_mode && dram_mode_offset(offset, dram))
		return dram_r(dram);

	// HiROM, Mode 0: banks $C0-$FF have the DRAM window at $0000/$8000 and
	// the IC card window at $2000/$A000
	if (m_hirom && !m_dram_mode && bk >= 0x40)
	{
		const uint16_t w = addr & 0x7fff;
		if (w < 0x2000)
			return dram_r(uint32_t(bk & 0x3f) * 0x10000 + (addr & 0x8000) + uint32_t(m_dram_page & 0x03) * 0x2000 + (addr & 0x1fff));
		if (w < 0x4000 && m_cart_enable && m_iccard->exists())
			return iccard_read(bk, addr);
		if (addr < 0x8000)
			return 0xff;
	}

	// $0000-$7FFF: the IC card window at $2000-$3FFF of banks $40+ (the BIOS
	// reads $40:2000-3FFF to tell HiROM cards from LoROM ones), SRAM in
	// banks $70+, otherwise nothing
	if (addr < 0x8000)
	{
		if (!m_dram_mode && bk >= 0x40 && (addr & 0x6000) == 0x2000 && m_cart_enable && m_iccard->exists())
			return iccard_read(bk, addr);
		if (bk >= 0x70)
			return m_sram[addr & (SRAM_SIZE - 1)];
		return 0xff;
	}

	// Mode 0: DRAM page window at $8000-$9FFF (all 128 banks, for 4MB)
	if (addr < 0xa000)
		return dram_r(uint32_t(bk) * 0x8000 + uint32_t(m_dram_page) * 0x2000 + (addr - 0x8000));

	// Mode 0: IC card window at $A000-$BFFF
	if (addr < 0xc000 && m_cart_enable && m_iccard->exists())
		return iccard_read(bk, addr);

	return firmware_r(addr);
}

uint8_t sns_pro_fighter_qb_device::read_l(offs_t offset)
{
	const uint16_t addr = offset & 0xffff;
	const uint8_t bank = (offset >> 16) & 0xff;

	// Mode 1, Mode 2, HiROM and $0000-$7FFF map the same as the hi banks
	if ((m_cart_passthrough && m_iccard->exists()) || m_dram_mode || m_hirom || addr < 0x8000)
		return read_h(offset);

	if (addr < 0xc000)
	{
		// $A000-$BFFF: IC card window
		if (addr >= 0xa000 && m_cart_enable && m_iccard->exists())
			return iccard_read(bank, addr);

		// $8000-$BFFF: DRAM page window
		return dram_r(uint32_t(bank) * 0x8000 + uint32_t(m_dram_page) * 0x2000 + (addr - 0x8000));
	}

	// $C000-$FFFF: DRAM in banks $40-$6F, firmware below
	if (bank >= 0x40)
		return dram_r(uint32_t(bank) * 0x8000 + (addr - 0x8000));

	return read_h(offset);
}

void sns_pro_fighter_qb_device::fdc_irq_w(int state)
{
	m_fdc_irq = state;
}

uint8_t sns_pro_fighter_qb_device::read_ram(offs_t offset)
{
	return m_sram[offset & (SRAM_SIZE - 1)];
}

void sns_pro_fighter_qb_device::write_ram(offs_t offset, uint8_t data)
{
	m_sram[offset & (SRAM_SIZE - 1)] = data;
}

// $2800-$28FF (relative offset): the floppy disk controller, mirrored
uint8_t sns_pro_fighter_qb_device::chip_read(offs_t offset)
{
	const uint16_t addr = offset & 0xffff;

	if (addr <= 0xff)
	{
		switch (addr & 7)
		{
		case 0: return m_fdc_irq ? 0x80 : 0x00; // the FDC's input port (see device_add_mconfig)
		case 4: return m_fdc->msr_r();
		case 5: return m_fdc->fifo_r();
		case 7: return m_fdc->dir_r();
		default: return 0xff;
		}
	}

	return 0xff;
}

void sns_pro_fighter_qb_device::chip_write(offs_t offset, uint8_t data)
{
	const uint16_t addr = offset & 0xffff;
	const uint8_t bank = (offset >> 16) & 0xff;

	if (addr <= 0xff)
	{
		switch (addr & 7)
		{
		case 2: m_fdc->dor_w(data); break;
		case 5: m_fdc->fifo_w(data); break;
		case 7: m_fdc->ccr_w(data); break;
		}
		return;
	}

	if (addr < 0x8000)
		return;

	// $E000-$E0FF: registers. The address selects the register and the data
	// is ignored, except for the page register. They mirror every 16 bytes:
	// the BIOS writes $E04A/$E04B for FIG headers with byte 5 bit 7 set.
	if ((addr & 0xff00) == 0xe000)
	{
		switch (addr & 0x000f)
		{
		case 0x0: case 0x1: case 0x2: case 0x3:
			m_dram_page = data;
			break;
		case 0x4:   // Mode 0: the BIOS
			m_dram_mode = false;
			m_cart_passthrough = false;
			break;
		case 0x5:   // Mode 1: the IC card, after a console reset
			if (!m_cart_passthrough && BIT(data, 7))
			{
				m_cart_passthrough = true;
				machine().schedule_soft_reset();
			}
			break;
		case 0x6: case 0x7:     // Mode 2: DRAM. The BIOS jumps through ($FFFC) next.
			m_dram_mode = true;
			break;
		case 0xc:   // $A000-$BFFF shows the IC card
			m_cart_enable = true;
			break;
		case 0xd:   // $A000-$BFFF shows the copier SRAM
			m_cart_enable = false;
			break;
		case 0xe:   // LoROM
			m_hirom = false;
			break;
		case 0xf:   // HiROM
			m_hirom = true;
			break;
		}
		return;
	}

	uint32_t dram;
	if (m_dram_mode)
	{
		if (dram_mode_offset(offset, dram))
			dram_w(dram, data);
		return;
	}

	// Mode 0: $A000-$BFFF writes the IC card's SRAM while the card is shown
	if (addr >= 0xa000 && addr < 0xc000 && m_cart_enable && m_iccard->exists())
	{
		if (m_iccard->m_cart && m_iccard->m_cart->get_nvram_size() > 0)
			m_iccard->write_ram(uint32_t(bank) * 0x8000 + uint32_t(m_dram_page & 0x03) * 0x2000 + (addr - 0xa000), data);
		return;
	}

	// Mode 0: page window at $8000-$9FFF, linear above
	if (addr < 0xa000)
		dram_w(uint32_t(bank) * 0x8000 + uint32_t(m_dram_page) * 0x2000 + (addr - 0x8000), data);
	else
		dram_w(uint32_t(bank) * 0x8000 + (addr - 0x8000), data);
}

void sns_pro_fighter_qb_device::write_l(offs_t offset, uint8_t data)
{
	const uint16_t addr = offset & 0xffff;
	const uint8_t bank = (offset >> 16) & 0xff;

	uint32_t dram;
	if (m_dram_mode && dram_mode_offset(offset, dram))
	{
		dram_w(dram, data);
		return;
	}

	// HiROM, Mode 0: DRAM page window at $0000/$8000 of banks $C0-$FF
	if (m_hirom && bank >= 0x40)
	{
		if ((addr & 0x7fff) < 0x2000)
			dram_w(uint32_t(bank & 0x3f) * 0x10000 + (addr & 0x8000) + uint32_t(m_dram_page & 0x03) * 0x2000 + (addr & 0x1fff), data);
		return;
	}

	// $70-$7D:0000-$7FFF: SRAM
	if (addr < 0x8000)
	{
		if (bank >= 0x70)
			m_sram[addr & (SRAM_SIZE - 1)] = data;
		return;
	}

	// Mode 0, banks $40-$7D: page window at $8000-$9FFF, linear above
	if (bank >= 0x40)
	{
		if (addr < 0xa000)
			dram_w(uint32_t(bank) * 0x8000 + uint32_t(m_dram_page) * 0x2000 + (addr - 0x8000), data);
		else
			dram_w(uint32_t(bank) * 0x8000 + (addr - 0x8000), data);
	}
}

void sns_pro_fighter_qb_device::device_add_mconfig(machine_config &config)
{
	// The BIOS uses only the 3201's registers: $2800 (the input port), DOR,
	// MSR, FIFO and CCR. On the input port, bit 7 is the FDC's own
	// interrupt, which the BIOS polls after a command; bits 3-0 and 5 are the
	// PC link's data and strobe, with $2808-$2810 the rest of the link.
	// Nothing is connected to the link, so it stays idle.
	// TODO: the PC link
	MCS3201(config, m_fdc, 24_MHz_XTAL);
	m_fdc->intrq_wr_callback().set(FUNC(sns_pro_fighter_qb_device::fdc_irq_w));

	// the BIOS selects drive 1
	FLOPPY_CONNECTOR(config, "fdc:1", profghtqb_floppies, "35hd", floppy_image_device::default_pc_floppy_formats).enable_sound(true);
	SOFTWARE_LIST(config, "flop_list").set_original("snes_profighter_flop");

	SNS_CART_SLOT(config, m_iccard, snes_cart, nullptr);
}


/*
Double Pro Fighter (SNES side)

Older SNES + Mega Drive version of the Pro Fighter Q, with up to 32 Mbit of
DRAM. The SNES side has the same $E000-$E00F mode, page and window decode as
the QB (the QB diagnostic ROM runs on it), and its BIOS keeps the 8KB page
counter in 16 bits, so 32 Mbit loads, saves and cart copies work.

One 27512 holds both BIOSes:
  $0000-$3FFF  Mega Drive BIOS (bus/megadrive/profighter.cpp), repeated at $4000
  $8000-$BFFF  SNES BIOS low half, at $4400-$5FFF of banks $00-$3F in Mode 0
               (bank bit 0 selects $8000-$9FFF or $A000-$BFFF)
  $C000-$FFFF  SNES BIOS high half, at $C000-$FFFF in Mode 0

$2800-$28FF registers (mirrored every 16 bytes):
  $2800 R: bit 7 = FDC INT
  $2801 R: bit 7 = disk changed (DIR)
  $2802 W: FDC DOR
  $2803 R: bit 7 = PC link busy (machine/ffelink.cpp)
  $2804 RW: FDC data              $2805 R: FDC main status
  $2807 W: FDC CCR
  $2808 R: PC link byte           W: nibble to the PC
  $2809 W: PC link acknowledge

The BIOS moves sector data by polling the FDC (SPECIFY ND=1) into WRAM, not
by DMA.

TODO: $2800 writes ($80/$00 at game start), $E008-$E00B and $E800-$E8FF
(written at game start) are not known.
*/

sns_double_pro_fighter_device::sns_double_pro_fighter_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: sns_pro_fighter_qb_device(mconfig, SNS_DOUBLE_PRO_FIGHTER, tag, owner, clock)
	, m_link(*this, "link")
{
}

ROM_START( dblprofighter )
	ROM_REGION( 0x10000, "program", 0 )
	ROM_LOAD( "dpf_bios.bin", 0x0000, 0x10000, CRC(bae38801) SHA1(d9677fe8df2f33871409ce933df0492bfc67971e) )
ROM_END

const tiny_rom_entry *sns_double_pro_fighter_device::device_rom_region() const
{
	return ROM_NAME( dblprofighter );
}

void sns_double_pro_fighter_device::device_start()
{
	sns_pro_fighter_qb_device::device_start();
}

void sns_double_pro_fighter_device::device_add_mconfig(machine_config &config)
{
	sns_pro_fighter_qb_device::device_add_mconfig(config);

	// TODO: the controller's part number; the BIOS uses only registers the
	// QB's 3201 has too
	N82077AA(config.replace(), m_fdc, 24_MHz_XTAL);
	m_fdc->set_ready_polled(false);
	m_fdc->intrq_wr_callback().set([this] (int state) { fdc_irq_w(state); });
	FLOPPY_CONNECTOR(config, "fdc:1", profghtqb_floppies, "35hd", floppy_image_device::default_pc_floppy_formats).enable_sound(true);

	FFE_LINK(config, m_link);
}

// $4400-$5FFF of banks $00-$3F (and $80-$BF): 8KB of the BIOS low half in
// Mode 0, chosen by bank bit 0 (code at $00:5000, font at $01:5000)
template <unsigned Half>
uint8_t sns_double_pro_fighter_device::bios_low_r(offs_t offset)
{
	if (m_dram_mode)
		return read_open_bus();
	return m_program_rom->base()[0x8000 | (Half << 13) | ((0x4400 + offset) & 0x1fff)];
}

void sns_double_pro_fighter_device::install_io(address_space &space)
{
	space.install_read_handler(0x004400, 0x005fff, 0, 0xbe0000, 0, emu::rw_delegate(*this, FUNC(sns_double_pro_fighter_device::bios_low_r<0>)));
	space.install_read_handler(0x014400, 0x015fff, 0, 0xbe0000, 0, emu::rw_delegate(*this, FUNC(sns_double_pro_fighter_device::bios_low_r<1>)));
}

uint8_t sns_double_pro_fighter_device::firmware_r(uint16_t addr) const
{
	return m_program_rom->base()[0xc000 | (addr & 0x3fff)];
}

uint8_t sns_double_pro_fighter_device::chip_read(offs_t offset)
{
	const uint16_t addr = offset & 0xffff;

	if (addr <= 0xff)
	{
		switch (addr & 0x0f)
		{
		case 0: return m_fdc_irq ? 0x80 : 0x00;
		case 1: return m_fdc->dir_r() & 0x80;
		case 3: return m_link->busy_r() ? 0x80 : 0x00;
		case 4: return m_fdc->fifo_r();
		case 5: return m_fdc->msr_r();
		case 8: return m_link->data_r();
		default: return 0xff;
		}
	}

	return 0xff;
}

void sns_double_pro_fighter_device::chip_write(offs_t offset, uint8_t data)
{
	const uint16_t addr = offset & 0xffff;

	if (addr <= 0xff)
	{
		switch (addr & 0x0f)
		{
		case 0: LOG("$2800 = %02X\n", data); break;
		case 2: m_fdc->dor_w(data); break;
		case 4: m_fdc->fifo_w(data); break;
		case 7: m_fdc->ccr_w(data); break;
		case 8: m_link->nibble_w(data); break;
		case 9: m_link->ack_w(); break;
		default: break;
		}
		return;
	}

	if (addr < 0x8000)
		return;

	if ((addr & 0xff00) == 0xe800)
	{
		LOG("$%04X = %02X\n", addr, data);
		return;
	}

	sns_pro_fighter_qb_device::chip_write(offset, data);
}


/*
Pro Fighter X (MB03D) copier

Main components:

GM82C765B floppy disk controller
16 MHz XTAL (for the FDC)
96813 CIC replacement on one PCB, 265111 CIC replacement on another
DDP1 custom (GRAPHIC DDP1-1 on one PCB, GINGER DDP1 9407 on another)
MK4864N-120 standard SRAM, 8k x 8
MS62256L-10PC CMOS Static RAM on one PCB; CXK58257M-10LL Static RAM on another
2 x D421700G5-80L-7JD Fast Page DRAM (on a sub board)
*/

sns_pro_fighter_x_device::sns_pro_fighter_x_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SNS_PRO_FIGHTER_X, tag, owner, clock)
	, device_sns_cart_interface(mconfig, *this)
{
}

ROM_START( profghtx )
	ROM_REGION( 0x10000, "program", 0 )
	ROM_LOAD( "pfx ver1.20.u3", 0x00000, 0x10000, CRC(04635d41) SHA1(e2765907d03f62329db224c6570d65aea5df5154) )

	ROM_REGION( 0x1200, "plds", ROMREGION_ERASE00 )
	ROM_LOAD( "palce20v8h.u7",  0x0000, 0x0157, CRC(2a46e9b7) SHA1(3c2aca7dfa943e00b4227194ce90dfa89290d1a6) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "palce20v8h.u10", 0x0200, 0x0157, CRC(738765b4) SHA1(1357ca21112b9d6da735a1070deeaa8097cf2a6e) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "peel18cv8b.u12", 0x0400, 0x0155, CRC(e1ee3a45) SHA1(983918d504c16b20ae185679ed489b167c993609) ) // this is actually a recreation
	ROM_LOAD( "palce16v8h.u13", 0x0600, 0x0117, CRC(611c95a9) SHA1(b138041dd99af0bed37f178d2733764309279ee6) ) // probably actual dump
	ROM_LOAD( "palce16v8h.u14", 0x0800, 0x0117, CRC(82d9c905) SHA1(70c16fb86a16319246eae02d9e5439bc65453174) ) // probably actual dump
	ROM_LOAD( "palce20v8h.u16", 0x0a00, 0x0157, CRC(3593d7e8) SHA1(47094c6469fed5f10bea9f0f993b0257fc39e12f) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "palce20v8h.u17", 0x0c00, 0x0157, CRC(f58539ae) SHA1(c5105f0dd9c7d0f966b84444c17fe798cfcdcdc1) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "palce20v8h.u18", 0x0e00, 0x0157, CRC(97d8652f) SHA1(6bff3088fed8253cfd2b57a101e91e0a0e66bcff) ) // this is actually a recreation targeted for a GAL20V8 device
	ROM_LOAD( "palce20v8h.u19", 0x1000, 0x0157, CRC(2e42ed8f) SHA1(51f461756668a51e4f9b800168412dab26cd5ca9) ) // this is actually a recreation targeted for a GAL20V8 device
ROM_END


void sns_pro_fighter_x_device::device_start()
{
}

void sns_pro_fighter_x_device::device_reset()
{

}

const tiny_rom_entry *sns_pro_fighter_x_device::device_rom_region() const
{
	return ROM_NAME( profghtx );
}

void sns_pro_fighter_x_device::device_add_mconfig(machine_config &config)
{
	WD37C65B(config, "fdc", 16_MHz_XTAL); // actually GM82C765B
	FLOPPY_CONNECTOR(config, "fdc:0", profght_floppies, "35dd", floppy_image_device::default_pc_floppy_formats).enable_sound(true);

	SNS_CART_SLOT(config, "cartslot", snes_cart, nullptr);
}

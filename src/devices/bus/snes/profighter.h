// license:BSD-3-Clause
// copyright-holders:Ivan Vangelista, Mike Bissett

#ifndef MAME_BUS_SNES_PROFIGHTER_H
#define MAME_BUS_SNES_PROFIGHTER_H

#pragma once

#include "snes_slot.h"

class upd765_family_device;

class sns_pro_fighter_q_device : public device_t,
						public device_sns_cart_interface
{
public:
	sns_pro_fighter_q_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
};

class sns_pro_fighter_qa_device : public device_t,
						public device_sns_cart_interface
{
public:
	sns_pro_fighter_qa_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
};

class sns_pro_fighter_qb_device : public device_t,
						public device_sns_cart_interface
{
public:
	sns_pro_fighter_qb_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual bool has_internal_bios() const override { return true; }
	virtual uint8_t read_l(offs_t offset) override;
	virtual uint8_t read_h(offs_t offset) override;
	virtual uint8_t read_ram(offs_t offset) override;
	virtual void write_ram(offs_t offset, uint8_t data) override;
	virtual uint8_t chip_read(offs_t offset) override;
	virtual void chip_write(offs_t offset, uint8_t data) override;
	virtual void write_l(offs_t offset, uint8_t data) override;

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

private:
	static constexpr uint32_t DRAM_MAX = 0x400000;    // 32 Mbit
	static constexpr uint32_t SRAM_SIZE = 0x8000;

	uint8_t firmware_r(uint16_t addr) const;
	uint8_t dram_r(uint32_t offset) const;
	void dram_w(uint32_t offset, uint8_t data);
	bool dram_mode_offset(offs_t offset, uint32_t &dram) const;
	uint8_t iccard_read(uint8_t bank, uint16_t addr);
	void fdc_irq_w(int state);

	required_memory_region m_program_rom;
	required_device<upd765_family_device> m_fdc;
	required_device<sns_cart_slot_device> m_iccard;
	required_ioport m_dram_size_port;

	std::unique_ptr<uint8_t[]> m_dram;
	std::unique_ptr<uint8_t[]> m_sram;
	uint32_t m_dram_size;       // installed DRAM (configuration)

	bool m_dram_mode;           // Mode 2: DRAM is the cartridge ROM ($E006/$E007)
	bool m_cart_passthrough;    // Mode 1: the IC card is the cartridge ($E005)
	bool m_cart_enable;         // $A000-$BFFF shows the IC card ($E00C) or the SRAM ($E00D)
	bool m_hirom;               // HiROM ($E00F) or LoROM ($E00E) mapping
	uint8_t m_dram_page;        // 8KB page for the windows ($E000-$E003)
	bool m_fdc_irq;             // the FDC's interrupt, on $2800 bit 7
};

class sns_pro_fighter_x_device : public device_t,
						public device_sns_cart_interface
{
public:
	sns_pro_fighter_x_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
};


DECLARE_DEVICE_TYPE(SNS_PRO_FIGHTER_Q, sns_pro_fighter_q_device);
DECLARE_DEVICE_TYPE(SNS_PRO_FIGHTER_QA, sns_pro_fighter_qa_device);
DECLARE_DEVICE_TYPE(SNS_PRO_FIGHTER_QB, sns_pro_fighter_qb_device);
DECLARE_DEVICE_TYPE(SNS_PRO_FIGHTER_X, sns_pro_fighter_x_device);

#endif // MAME_BUS_SNES_PROFIGHTER_H

// license:BSD-3-Clause
// copyright-holders:

#ifndef MAME_BUS_SNES_PROFIGHTER_H
#define MAME_BUS_SNES_PROFIGHTER_H

#pragma once

#include "snes_slot.h"

class n82077aa_device;

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

private:
	required_memory_region m_program_rom;
	required_device<n82077aa_device> m_fdc;
	required_device<sns_cart_slot_device> m_iccard;

	void fdc_drq_w(int state);

	bool     m_dram_mode;      // false=program ROM active, true=DRAM active as cartridge ROM
	bool     m_cart_passthrough; // true = IC card cart visible as ROM (set via $E005, survives reset)
	bool     m_cart_enable;    // true = $A000-$BFFF reads from IC card cart, false = reads DRAM
	uint32_t m_dma_addr;       // current DMA write offset into DRAM (auto-incremented by DRQ handler)
	uint16_t m_dram_page;      // DRAM page register (set via $E000-$E003), selects 8KB page in $8000-$9FFF window
	bool     m_read_h_logged;  // debug: log first read_h after each reset
	int      m_dram_write_log; // debug: count chip_write DRAM stores for first-write logging
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

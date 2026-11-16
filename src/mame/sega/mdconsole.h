// license:BSD-3-Clause
// copyright-holders:David Haywood
#ifndef MAME_SEGA_MDCONSOLE_H
#define MAME_SEGA_MDCONSOLE_H

#pragma once

#include "megadriv.h"

#include "mega32x.h"
#include "megacd.h"

#include "bus/generic/slot.h"
#include "bus/megadrive/md_slot.h"
#include "bus/megadrive/md_carts.h"
#include "bus/sms_ctrl/smsctrl.h"


class md_cons_state : public md_base_state
{
public:
	md_cons_state(const machine_config &mconfig, device_type type, const char *tag) :
		md_base_state(mconfig, type, tag),
		m_ctrl_ports(*this, { "ctrl1", "ctrl2", "exp" }),
		m_32x(*this,"sega32x"),
		m_segacd(*this,"segacd"),
		m_cart(*this, "mdslot"),
		m_tmss(*this, "tmss"),
		m_z80_view(*this, "z80_view"),
		m_z80_io_view(*this, "z80_io_view")
	{ }

	void init_genesis();
	void init_md_eur();
	void init_md_jpn();
	void init_genesis_tmss();

	void md_32x(machine_config &config);
	void genesis_32x(machine_config &config);
	void mdj_32x(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

	void screen_vblank_console(int state);

	DECLARE_DEVICE_IMAGE_LOAD_MEMBER( _32x_cart );

	void install_cartslot();
	void install_tmss();

	void md_ctrl_ports(machine_config &config);
	void md_exp_port(machine_config &config);
	void md_mark3(machine_config &config, bool pal);

	optional_device_array<sms_control_port_device, 3> m_ctrl_ports;
	optional_device<sega_32x_device> m_32x;
	optional_device<sega_segacd_device> m_segacd;
	optional_device<md_cart_slot_device> m_cart;
	optional_region_ptr<uint16_t> m_tmss;

private:
	// Mark III (Master System) mode, selected by the cartridge's /M3 line
	void md_cons_z80_map(address_map &map) ATTR_COLD;
	void md_cons_z80_io_map(address_map &map) ATTR_COLD;
	void set_mark3_mode(bool on);
	void mark3_configure_screen();
	uint32_t screen_update_console(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	uint8_t mark3_ram_r(offs_t offset);
	void mark3_ram_w(offs_t offset, uint8_t data);
	void mark3_io_control_w(uint8_t data);
	uint8_t mark3_port_dc_r();
	uint8_t mark3_port_dd_r();
	void mark3_n_int_w(int state);

	memory_view m_z80_view;
	memory_view m_z80_io_view;
	bool m_mark3_capable = false;
	bool m_mark3_pal = false;
	bool m_mark3 = false;
	int m_md_screen_width = 0;
	int m_md_screen_height = 0;
	rectangle m_md_visarea;
	attotime m_md_frame_period;

	void _32x_scanline_callback(int x, uint32_t priority, uint32_t &lineptr);
	void _32x_interrupt_callback(int scanline, int irq6);
	void _32x_scanline_helper_callback(int scanline);

	uint16_t tmss_r(offs_t offset);
	void tmss_swap_w(uint16_t data);
};


class md_cons_slot_state : public md_cons_state
{
public:
	md_cons_slot_state(const machine_config &mconfig, device_type type, const char *tag) :
		md_cons_state(mconfig, type, tag)
	{ }

	void ms_megadriv(machine_config &config);
	void ms_megadrivj(machine_config &config);
	void ms_megadpal(machine_config &config);
	void ms_megadriv2(machine_config &config);
	void ms_nomad(machine_config &config);
	void ms_megajet(machine_config &config);

	void genesis_tmss(machine_config &config);


protected:
	virtual void machine_start() override ATTR_COLD;
};


class md_cons_cd_state : public md_cons_state
{
public:
	md_cons_cd_state(const machine_config &mconfig, device_type type, const char *tag) :
		md_cons_state(mconfig, type, tag)
	{ }

	void genesis_scd(machine_config &config);
	void genesis2_scd(machine_config &config);
	void md_scd(machine_config &config);
	void md2_scd(machine_config &config);
	void mdj_scd(machine_config &config);

	void mdj_32x_scd(machine_config &config);
	void md2j_scd(machine_config &config);
	void genesis_32x_scd(machine_config &config);
	void md_32x_scd(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;
};

#endif // MAME_SEGA_MDCONSOLE_H

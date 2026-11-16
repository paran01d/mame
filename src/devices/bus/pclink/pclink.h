// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***************************************************************************

    PC parallel port link

    The connector on a peripheral that a PC drives through its parallel
    port with a straight-through cable, as the Front Far East style links
    on floppy copiers are used. The PC end is the card.

    The PC drives the eight data lines and /STROBE, /AUTOFD, /INIT and
    /SELECTIN. The peripheral drives the five status lines: /ACK, BUSY,
    PE, SELECT and /ERROR. Levels are as on the cable.

***************************************************************************/

#ifndef MAME_BUS_PCLINK_PCLINK_H
#define MAME_BUS_PCLINK_PCLINK_H

#pragma once


class device_pc_link_interface;

class pc_link_port_device : public device_t, public device_single_card_slot_interface<device_pc_link_interface>
{
public:
	template <typename T>
	pc_link_port_device(machine_config const &mconfig, char const *tag, device_t *owner, T &&opts, char const *dflt)
		: pc_link_port_device(mconfig, tag, owner, 0)
	{
		option_reset();
		opts(*this);
		set_default_option(dflt);
		set_fixed(false);
	}
	pc_link_port_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock = 0);

	// lines from the PC, as seen by the peripheral
	auto strobe_handler() { return m_strobe_handler.bind(); }
	uint8_t data_r() const { return m_data; }
	int strobe_r() const { return m_strobe; }
	uint8_t control_r() const { return m_control; }

	// lines to the PC
	void ack_w(int state);
	void busy_w(int state);
	void perror_w(int state);
	void select_w(int state);
	void fault_w(int state);

	// called by the card (the PC end)
	void data_w(uint8_t data) { m_data = data; }
	void strobe_w(int state);
	void control_w(uint8_t data) { m_control = data; }

protected:
	virtual void device_config_complete() override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;

private:
	devcb_write_line m_strobe_handler;
	device_pc_link_interface *m_card;

	uint8_t m_data;
	uint8_t m_control;      // /AUTOFD, /INIT, /SELECTIN as PC control register bits 1-3
	int m_strobe;           // 1 = /STROBE asserted (low on the cable)
};


class device_pc_link_interface : public device_interface
{
public:
	virtual ~device_pc_link_interface();

	// status lines from the peripheral
	virtual void input_ack(int state) { }
	virtual void input_busy(int state) { }
	virtual void input_perror(int state) { }
	virtual void input_select(int state) { }
	virtual void input_fault(int state) { }

protected:
	device_pc_link_interface(machine_config const &mconfig, device_t &device);

	pc_link_port_device *port() const { return m_port; }

private:
	friend class pc_link_port_device;
	pc_link_port_device *m_port;
};


void pc_link_devices(device_slot_interface &device);

DECLARE_DEVICE_TYPE(PC_LINK_PORT, pc_link_port_device)

#endif // MAME_BUS_PCLINK_PCLINK_H

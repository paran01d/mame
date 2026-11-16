// license:BSD-3-Clause
// copyright-holders:Mike Bissett
#ifndef MAME_MACHINE_FFELINK_H
#define MAME_MACHINE_FFELINK_H

#pragma once

#include "bus/pclink/pclink.h"


class ffe_link_device : public device_t
{
public:
	ffe_link_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock = 0);

	int busy_r() const { return m_strobe ^ m_ack; }     // 1 = a byte is waiting, or the PC has not taken a nibble
	uint8_t data_r() const { return m_port->data_r(); }
	void nibble_w(uint8_t data);
	void ack_w();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

private:
	void strobe_w(int state);
	void update_busy();

	required_device<pc_link_port_device> m_port;

	uint8_t m_strobe;
	uint8_t m_ack;
};

DECLARE_DEVICE_TYPE(FFE_LINK, ffe_link_device)

#endif // MAME_MACHINE_FFELINK_H

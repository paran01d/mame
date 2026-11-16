// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***************************************************************************

    PC parallel port link

***************************************************************************/

#include "emu.h"
#include "pclink.h"

#include "remote.h"


DEFINE_DEVICE_TYPE(PC_LINK_PORT, pc_link_port_device, "pc_link_port", "PC parallel port link")

pc_link_port_device::pc_link_port_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, PC_LINK_PORT, tag, owner, clock)
	, device_single_card_slot_interface<device_pc_link_interface>(mconfig, *this)
	, m_strobe_handler(*this)
	, m_card(nullptr)
	, m_data(0)
	, m_control(0)
	, m_strobe(0)
{
}

void pc_link_port_device::device_config_complete()
{
	m_card = get_card_device();
	if (m_card)
		m_card->m_port = this;
}

void pc_link_port_device::device_start()
{
	save_item(NAME(m_data));
	save_item(NAME(m_control));
	save_item(NAME(m_strobe));
}

void pc_link_port_device::strobe_w(int state)
{
	if (m_strobe != state)
	{
		m_strobe = state;
		m_strobe_handler(state);
	}
}

void pc_link_port_device::ack_w(int state)    { if (m_card) m_card->input_ack(state); }
void pc_link_port_device::busy_w(int state)   { if (m_card) m_card->input_busy(state); }
void pc_link_port_device::perror_w(int state) { if (m_card) m_card->input_perror(state); }
void pc_link_port_device::select_w(int state) { if (m_card) m_card->input_select(state); }
void pc_link_port_device::fault_w(int state)  { if (m_card) m_card->input_fault(state); }


device_pc_link_interface::device_pc_link_interface(machine_config const &mconfig, device_t &device)
	: device_interface(device, "pclink")
	, m_port(nullptr)
{
}

device_pc_link_interface::~device_pc_link_interface()
{
}


void pc_link_devices(device_slot_interface &device)
{
	device.option_add("remote", PC_LINK_REMOTE);
}

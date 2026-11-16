// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***************************************************************************

    Front Far East style PC link (copier side)

    The link the Super Wild Card, Super Magic Drive and Pro Fighter copiers
    use to talk to a PC parallel port. A PC byte arrives on the eight data
    lines; a copier byte goes back as two nibbles on the status lines, low
    nibble first. The handshake is one line, BUSY, which is the PC's
    /STROBE XORed with a flip-flop the copier toggles:

      PC sends:  the PC sets the data lines and toggles /STROBE (BUSY = 1);
                 the copier reads the byte and toggles its flip-flop
                 (BUSY = 0).
      PC reads:  the copier puts a nibble on the status lines and toggles
                 its flip-flop (BUSY = 1); the PC reads it and toggles
                 /STROBE (BUSY = 0).

    The copier sees BUSY as bit 7 of a status register and toggles the
    flip-flop by writing an acknowledge register. The nibble goes out on
    /ERROR, SELECT, PE and /ACK (PC status bits 3-6).

***************************************************************************/

#include "emu.h"
#include "ffelink.h"


DEFINE_DEVICE_TYPE(FFE_LINK, ffe_link_device, "ffe_link", "FFE PC link")

ffe_link_device::ffe_link_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, FFE_LINK, tag, owner, clock)
	, m_port(*this, "pc")
	, m_strobe(0)
	, m_ack(0)
{
}

void ffe_link_device::device_add_mconfig(machine_config &config)
{
	PC_LINK_PORT(config, m_port, pc_link_devices, "remote");
	m_port->strobe_handler().set(FUNC(ffe_link_device::strobe_w));
}

void ffe_link_device::device_start()
{
	save_item(NAME(m_strobe));
	save_item(NAME(m_ack));
}

void ffe_link_device::device_reset()
{
	nibble_w(0);
	update_busy();
}

void ffe_link_device::strobe_w(int state)
{
	m_strobe = state ? 1 : 0;
	update_busy();
}

void ffe_link_device::nibble_w(uint8_t data)
{
	m_port->fault_w(BIT(data, 0));
	m_port->select_w(BIT(data, 1));
	m_port->perror_w(BIT(data, 2));
	m_port->ack_w(BIT(data, 3));
}

void ffe_link_device::ack_w()
{
	m_ack ^= 1;
	update_busy();
}

void ffe_link_device::update_busy()
{
	m_port->busy_w(busy_r());
}

// license:BSD-3-Clause
// copyright-holders:Mike Bissett
/***************************************************************************

    Remote PC

    The PC end of the link is a program outside MAME that drives a parallel
    port's registers, over a bitbanger image (for example
    -bitb socket.127.0.0.1:2345). Each access is a frame of two bytes:

      $00 d    write the data register
      $02 c    write the control register (bit 0 = /STROBE asserted)
      $80 x    read the data register     -> one byte back
      $81 x    read the status register   -> one byte back
      $82 x    read the control register  -> one byte back

    The registers are those of a PC parallel port: status bit 3 is /ERROR,
    bit 4 SELECT, bit 5 PE, bit 6 /ACK and bit 7 BUSY inverted.

***************************************************************************/

#include "emu.h"
#include "pclink.h"
#include "remote.h"

#include "imagedev/bitbngr.h"


namespace {

class pc_link_remote_device : public device_t, public device_pc_link_interface
{
public:
	pc_link_remote_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock)
		: device_t(mconfig, PC_LINK_REMOTE, tag, owner, clock)
		, device_pc_link_interface(mconfig, *this)
		, m_stream(*this, "stream")
		, m_poll_timer(nullptr)
		, m_status(0x00)
		, m_data(0x00)
		, m_control(0x04)
		, m_frame_len(0)
	{
	}

	virtual void input_ack(int state) override    { m_status = (m_status & ~0x40) | (state ? 0x40 : 0); }
	virtual void input_busy(int state) override   { m_status = (m_status & ~0x80) | (state ? 0 : 0x80); }
	virtual void input_perror(int state) override { m_status = (m_status & ~0x20) | (state ? 0x20 : 0); }
	virtual void input_select(int state) override { m_status = (m_status & ~0x10) | (state ? 0x10 : 0); }
	virtual void input_fault(int state) override  { m_status = (m_status & ~0x08) | (state ? 0x08 : 0); }

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD
	{
		BITBANGER(config, m_stream, 0);
	}

	virtual void device_start() override ATTR_COLD
	{
		m_poll_timer = timer_alloc(FUNC(pc_link_remote_device::poll), this);

		save_item(NAME(m_status));
		save_item(NAME(m_data));
		save_item(NAME(m_control));
		save_item(NAME(m_frame));
		save_item(NAME(m_frame_len));
		save_item(NAME(m_last_traffic));
	}

	virtual void device_reset() override ATTR_COLD
	{
		port()->data_w(m_data);
		port()->control_w(m_control);
		port()->strobe_w(BIT(m_control, 0));
		m_last_traffic = attotime::zero;
		m_poll_timer->adjust(attotime::zero);
	}

private:
	TIMER_CALLBACK_MEMBER(poll);

	required_device<bitbanger_device> m_stream;
	emu_timer *m_poll_timer;

	uint8_t m_status;
	uint8_t m_data;
	uint8_t m_control;
	uint8_t m_frame[2];
	uint8_t m_frame_len;
	attotime m_last_traffic;
};

// Every register read is a round trip to the remote program, so poll as
// fast as a real port responds while it is talking, back off when it goes
// quiet, and only check now and then while nothing is mounted.
TIMER_CALLBACK_MEMBER(pc_link_remote_device::poll)
{
	if (!m_stream->exists())
	{
		m_frame_len = 0;
		m_poll_timer->adjust(attotime::from_msec(20));
		return;
	}

	// Writes need no answer, so several frames can arrive at once; a read is
	// always the last, as the remote end waits for its answer.
	uint32_t count;
	while ((count = m_stream->input(&m_frame[m_frame_len], 2 - m_frame_len)) != 0)
	{
		m_last_traffic = machine().time();
		m_frame_len += count;
		if (m_frame_len < 2)
			continue;

		m_frame_len = 0;
		switch (m_frame[0])
		{
		case 0x00:
			m_data = m_frame[1];
			port()->data_w(m_data);
			break;
		case 0x02:
			m_control = m_frame[1];
			port()->control_w(m_control);
			port()->strobe_w(BIT(m_control, 0));
			break;
		case 0x80: m_stream->output(m_data); break;
		case 0x81: m_stream->output(m_status); break;
		case 0x82: m_stream->output(m_control); break;
		default: logerror("unknown frame %02X %02X\n", m_frame[0], m_frame[1]); break;
		}
	}

	bool const active = (machine().time() - m_last_traffic) < attotime::from_msec(50);
	m_poll_timer->adjust(active ? attotime::from_usec(1) : attotime::from_usec(500));
}

} // anonymous namespace


DEFINE_DEVICE_TYPE_PRIVATE(PC_LINK_REMOTE, device_pc_link_interface, pc_link_remote_device, "pc_link_remote", "Remote PC (parallel port registers over a bitbanger)")

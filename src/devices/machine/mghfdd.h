// license:BSD-3-Clause
// copyright-holders:Mike Bissett
#ifndef MAME_MACHINE_MGHFDD_H
#define MAME_MACHINE_MGHFDD_H

#pragma once

#include "imagedev/floppy.h"
#include "machine/upd765.h"

#include <initializer_list>
#include <vector>


class mgh_fdd_device : public device_t
{
public:
	mgh_fdd_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	static constexpr feature_type imperfect_features() { return feature::DISK; }

	// the two host ports: status (bit 7 ready, bit 6 direction) and data
	uint8_t status_r();
	uint8_t data_r();
	void data_w(uint8_t data);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_reset_after_children() override ATTR_COLD;
	virtual void device_post_load() override;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

private:
	static constexpr unsigned SECTOR = 512;
	static constexpr unsigned MAX_SECTORS = 20;     // per side of a track (1.6M)
	static constexpr unsigned REPLY_MAX = 1024;

	// sequencer operations
	enum : uint8_t
	{
		OP_DOR, OP_CCR, OP_OUT, OP_WAIT_IRQ, OP_RESULT, OP_READ, OP_WRITE, OP_WRITE_TC,
		OP_CHECK_SEEK, OP_CHECK_RW, OP_REPLY, OP_REPLY_SECTOR, OP_REPLY_ST3, OP_REPLY_DISK,
		OP_CACHE_VALID
	};

	void command();
	void build_program();
	void op(uint8_t code, uint16_t arg = 0);
	void op_out(std::initializer_list<uint8_t> bytes);
	void op_seek(uint8_t track);
	void reply(uint8_t data);
	void reply_error();
	void run();
	void done();
	void fail();
	int step_op();
	TIMER_CALLBACK_MEMBER(step);
	TIMER_CALLBACK_MEMBER(motor_off);
	void fdc_irq_w(int state);
	void disk_changed(floppy_image_device *floppy);

	required_device<mcs3201_device> m_fdc;
	required_device<floppy_connector> m_floppy;
	emu_timer *m_step_timer;
	emu_timer *m_motor_timer;

	// host side
	uint8_t m_cmd[4];               // command being received
	uint8_t m_cmd_len;
	uint8_t m_reply[REPLY_MAX];
	uint16_t m_reply_len;
	uint16_t m_reply_pos;
	uint8_t m_wbuf[SECTOR];         // sector data for a write
	uint16_t m_wcount;              // write data still to come

	// sequencer: the program is rebuilt from m_prog_cmd, so only the
	// position in it is saved
	uint8_t m_prog_cmd[4];
	uint8_t m_prog_cmd_len;
	std::vector<uint8_t> m_prog_op;
	std::vector<uint16_t> m_prog_arg;
	std::vector<uint8_t> m_out;     // bytes the program writes to the disk
	std::vector<uint8_t> m_err;     // reply if the program fails
	uint32_t m_pc;
	uint16_t m_count;
	uint32_t m_wait;
	bool m_busy;
	uint32_t m_out_pos;
	uint8_t m_res[7];

	uint8_t m_cache[MAX_SECTORS * SECTOR];  // one side of a track
	bool m_cache_valid;
	uint8_t m_cache_track;
	uint8_t m_cache_head;
	bool m_fdc_irq;
	uint8_t m_density;
};

DECLARE_DEVICE_TYPE(MGH_FDD, mgh_fdd_device)

#endif // MAME_MACHINE_MGHFDD_H

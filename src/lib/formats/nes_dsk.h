// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic
/*********************************************************************

    formats/nes_dsk.h

    NES disk images

*********************************************************************/
#ifndef MAME_FORMATS_NES_DSK_H
#define MAME_FORMATS_NES_DSK_H

#pragma once

#include "flopimg_legacy.h"

LEGACY_FLOPPY_OPTIONS_EXTERN(nes_only);

// A Game Doctor side file: a 3-byte header, then the side's blocks with
// dummy CRCs and no gaps (so not a multiple of 65500 bytes). Several sides
// can follow each other in one file. Needs the first 18 bytes.
bool nes_dsk_is_gamedoctor(const uint8_t *header, uint64_t size);

#endif // MAME_FORMATS_NES_DSK_H

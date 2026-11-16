// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic
/*********************************************************************

    formats/nes_dsk.c

    NES disk images

*********************************************************************/

#include "formats/nes_dsk.h"
#include "formats/basicdsk.h"

#include <cstring>



/*****************************************************************************
 NES floppy core functions
*****************************************************************************/


bool nes_dsk_is_gamedoctor(const uint8_t *header, uint64_t size)
{
	return (size >= 3 + 56) && (size % 65500) && (header[3] == 0x01) && !memcmp(&header[4], "*NINTENDO-HVC*", 14);
}

static FLOPPY_IDENTIFY( nes_dsk_identify )
{
	uint64_t size;
	uint8_t header[18] = { };

	*vote = 0;

	/* first check the size of the image */
	size = floppy_image_size(floppy);

	/* get the first bytes */
	floppy_image_read(floppy, &header, 0, (size < sizeof(header)) ? size : sizeof(header));

	if ((size == 65516) || (size == 131016) || (size == 262016))
	{
		/* the image has an header, hence check the first sector for the magic string */
		if (!memcmp(header, "FDS", 3))
			*vote = 100;
	}

	if ((size == 65500) || (size == 131000) || (size == 262000))
	{
		/* the image has no header, hence let's trust the extension and load the file */
		*vote = 100;
	}

	if (nes_dsk_is_gamedoctor(header, size))
		*vote = 100;

	return FLOPPY_ERROR_SUCCESS;
}


static FLOPPY_CONSTRUCT( nes_dsk_construct )
{
	return FLOPPY_ERROR_SUCCESS;
}


/*****************************************************************************
 NES floppy options
*****************************************************************************/

LEGACY_FLOPPY_OPTIONS_START( nes_only )
	LEGACY_FLOPPY_OPTION(
		fds_dsk,
		"fds,a,b,c,d",
		"NES floppy disk image",
		nes_dsk_identify,
		nes_dsk_construct,
		nullptr,
		nullptr
	)
LEGACY_FLOPPY_OPTIONS_END0

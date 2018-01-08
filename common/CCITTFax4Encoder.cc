/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * CCITTFax4Encoder.cc
 * Based on code from libtiff/tif_fax3.c, which is:
 *   Copyright (c) 1990-1997 Sam Leffler
 *   Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *   Permission to use, copy, modify, distribute, and sell this software and
 *   its documentation for any purpose is hereby granted without fee, provided
 *   that (i) the above copyright notices and this permission notice appear in
 *   all copies of the software and related documentation, and (ii) the names of
 *   Sam Leffler and Silicon Graphics may not be used in any advertising or
 *   publicity relating to the software without the specific, prior written
 *   permission of Sam Leffler and Silicon Graphics.
 *
 * Modifications are:
 *   Copyright (C) 2016-2018 Sandro Mani <manisandro@gmail.com>
 *
 * gImageReader is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gImageReader is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CCITTFax4Encoder.hh"
#include <cassert>
#include <vector>

struct CCITTFax4Encoder::TableEntry {
	unsigned short length;  /* bit length of g3 code */
	unsigned short code;    /* g3 code */
	short runlen;           /* run length in bits */
};

#define EOL	0x001	/* EOL code value - 0000 0000 0000 1 */

// Status values returned instead of a run length
#define G3CODE_EOL     -1 /* NB: ACT_EOL - ACT_WRUNT */
#define G3CODE_INVALID -2 /* NB: ACT_INVALID - ACT_WRUNT */
#define G3CODE_EOF     -3 /* end of input data */
#define G3CODE_INCOMP  -4 /* incomplete run code */

const CCITTFax4Encoder::TableEntry CCITTFax4Encoder::whiteCodes[] = {
	{ 8, 0x35, 0 },	    /* 0011 0101 */
	{ 6, 0x7, 1 },      /* 0001 11 */
	{ 4, 0x7, 2 },      /* 0111 */
	{ 4, 0x8, 3 },      /* 1000 */
	{ 4, 0xB, 4 },      /* 1011 */
	{ 4, 0xC, 5 },      /* 1100 */
	{ 4, 0xE, 6 },      /* 1110 */
	{ 4, 0xF, 7 },      /* 1111 */
	{ 5, 0x13, 8 },     /* 1001 1 */
	{ 5, 0x14, 9 },     /* 1010 0 */
	{ 5, 0x7, 10 },     /* 0011 1 */
	{ 5, 0x8, 11 },     /* 0100 0 */
	{ 6, 0x8, 12 },     /* 0010 00 */
	{ 6, 0x3, 13 },     /* 0000 11 */
	{ 6, 0x34, 14 },    /* 1101 00 */
	{ 6, 0x35, 15 },    /* 1101 01 */
	{ 6, 0x2A, 16 },    /* 1010 10 */
	{ 6, 0x2B, 17 },    /* 1010 11 */
	{ 7, 0x27, 18 },    /* 0100 111 */
	{ 7, 0xC, 19 },	    /* 0001 100 */
	{ 7, 0x8, 20 },	    /* 0001 000 */
	{ 7, 0x17, 21 },    /* 0010 111 */
	{ 7, 0x3, 22 },     /* 0000 011 */
	{ 7, 0x4, 23 },     /* 0000 100 */
	{ 7, 0x28, 24 },    /* 0101 000 */
	{ 7, 0x2B, 25 },    /* 0101 011 */
	{ 7, 0x13, 26 },    /* 0010 011 */
	{ 7, 0x24, 27 },    /* 0100 100 */
	{ 7, 0x18, 28 },    /* 0011 000 */
	{ 8, 0x2, 29 },     /* 0000 0010 */
	{ 8, 0x3, 30 },     /* 0000 0011 */
	{ 8, 0x1A, 31 },    /* 0001 1010 */
	{ 8, 0x1B, 32 },    /* 0001 1011 */
	{ 8, 0x12, 33 },    /* 0001 0010 */
	{ 8, 0x13, 34 },    /* 0001 0011 */
	{ 8, 0x14, 35 },    /* 0001 0100 */
	{ 8, 0x15, 36 },    /* 0001 0101 */
	{ 8, 0x16, 37 },    /* 0001 0110 */
	{ 8, 0x17, 38 },    /* 0001 0111 */
	{ 8, 0x28, 39 },    /* 0010 1000 */
	{ 8, 0x29, 40 },    /* 0010 1001 */
	{ 8, 0x2A, 41 },    /* 0010 1010 */
	{ 8, 0x2B, 42 },    /* 0010 1011 */
	{ 8, 0x2C, 43 },    /* 0010 1100 */
	{ 8, 0x2D, 44 },    /* 0010 1101 */
	{ 8, 0x4, 45 },	    /* 0000 0100 */
	{ 8, 0x5, 46 },	    /* 0000 0101 */
	{ 8, 0xA, 47 },	    /* 0000 1010 */
	{ 8, 0xB, 48 },	    /* 0000 1011 */
	{ 8, 0x52, 49 },    /* 0101 0010 */
	{ 8, 0x53, 50 },    /* 0101 0011 */
	{ 8, 0x54, 51 },    /* 0101 0100 */
	{ 8, 0x55, 52 },    /* 0101 0101 */
	{ 8, 0x24, 53 },    /* 0010 0100 */
	{ 8, 0x25, 54 },    /* 0010 0101 */
	{ 8, 0x58, 55 },    /* 0101 1000 */
	{ 8, 0x59, 56 },    /* 0101 1001 */
	{ 8, 0x5A, 57 },    /* 0101 1010 */
	{ 8, 0x5B, 58 },    /* 0101 1011 */
	{ 8, 0x4A, 59 },    /* 0100 1010 */
	{ 8, 0x4B, 60 },    /* 0100 1011 */
	{ 8, 0x32, 61 },    /* 0011 0010 */
	{ 8, 0x33, 62 },    /* 0011 0011 */
	{ 8, 0x34, 63 },    /* 0011 0100 */
	{ 5, 0x1B, 64 },    /* 1101 1 */
	{ 5, 0x12, 128 },   /* 1001 0 */
	{ 6, 0x17, 192 },   /* 0101 11 */
	{ 7, 0x37, 256 },   /* 0110 111 */
	{ 8, 0x36, 320 },   /* 0011 0110 */
	{ 8, 0x37, 384 },   /* 0011 0111 */
	{ 8, 0x64, 448 },   /* 0110 0100 */
	{ 8, 0x65, 512 },   /* 0110 0101 */
	{ 8, 0x68, 576 },   /* 0110 1000 */
	{ 8, 0x67, 640 },   /* 0110 0111 */
	{ 9, 0xCC, 704 },   /* 0110 0110 0 */
	{ 9, 0xCD, 768 },   /* 0110 0110 1 */
	{ 9, 0xD2, 832 },   /* 0110 1001 0 */
	{ 9, 0xD3, 896 },   /* 0110 1001 1 */
	{ 9, 0xD4, 960 },   /* 0110 1010 0 */
	{ 9, 0xD5, 1024 },  /* 0110 1010 1 */
	{ 9, 0xD6, 1088 },  /* 0110 1011 0 */
	{ 9, 0xD7, 1152 },  /* 0110 1011 1 */
	{ 9, 0xD8, 1216 },  /* 0110 1100 0 */
	{ 9, 0xD9, 1280 },  /* 0110 1100 1 */
	{ 9, 0xDA, 1344 },  /* 0110 1101 0 */
	{ 9, 0xDB, 1408 },  /* 0110 1101 1 */
	{ 9, 0x98, 1472 },  /* 0100 1100 0 */
	{ 9, 0x99, 1536 },  /* 0100 1100 1 */
	{ 9, 0x9A, 1600 },  /* 0100 1101 0 */
	{ 6, 0x18, 1664 },  /* 0110 00 */
	{ 9, 0x9B, 1728 },  /* 0100 1101 1 */
	{ 11, 0x8, 1792 },  /* 0000 0001 000 */
	{ 11, 0xC, 1856 },  /* 0000 0001 100 */
	{ 11, 0xD, 1920 },  /* 0000 0001 101 */
	{ 12, 0x12, 1984 }, /* 0000 0001 0010 */
	{ 12, 0x13, 2048 }, /* 0000 0001 0011 */
	{ 12, 0x14, 2112 }, /* 0000 0001 0100 */
	{ 12, 0x15, 2176 }, /* 0000 0001 0101 */
	{ 12, 0x16, 2240 }, /* 0000 0001 0110 */
	{ 12, 0x17, 2304 }, /* 0000 0001 0111 */
	{ 12, 0x1C, 2368 }, /* 0000 0001 1100 */
	{ 12, 0x1D, 2432 }, /* 0000 0001 1101 */
	{ 12, 0x1E, 2496 }, /* 0000 0001 1110 */
	{ 12, 0x1F, 2560 }, /* 0000 0001 1111 */
	{ 12, 0x1, G3CODE_EOL },     /* 0000 0000 0001 */
	{ 9, 0x1, G3CODE_INVALID },  /* 0000 0000 1 */
	{ 10, 0x1, G3CODE_INVALID }, /* 0000 0000 01 */
	{ 11, 0x1, G3CODE_INVALID }, /* 0000 0000 001 */
	{ 12, 0x0, G3CODE_INVALID }, /* 0000 0000 0000 */
};

const CCITTFax4Encoder::TableEntry CCITTFax4Encoder::blackCodes[] = {
	{ 10, 0x37, 0 },    /* 0000 1101 11 */
	{ 3, 0x2, 1 },      /* 010 */
	{ 2, 0x3, 2 },      /* 11 */
	{ 2, 0x2, 3 },      /* 10 */
	{ 3, 0x3, 4 },      /* 011 */
	{ 4, 0x3, 5 },      /* 0011 */
	{ 4, 0x2, 6 },      /* 0010 */
	{ 5, 0x3, 7 },      /* 0001 1 */
	{ 6, 0x5, 8 },      /* 0001 01 */
	{ 6, 0x4, 9 },      /* 0001 00 */
	{ 7, 0x4, 10 },     /* 0000 100 */
	{ 7, 0x5, 11 },     /* 0000 101 */
	{ 7, 0x7, 12 },     /* 0000 111 */
	{ 8, 0x4, 13 },     /* 0000 0100 */
	{ 8, 0x7, 14 },     /* 0000 0111 */
	{ 9, 0x18, 15 },    /* 0000 1100 0 */
	{ 10, 0x17, 16 },   /* 0000 0101 11 */
	{ 10, 0x18, 17 },   /* 0000 0110 00 */
	{ 10, 0x8, 18 },    /* 0000 0010 00 */
	{ 11, 0x67, 19 },   /* 0000 1100 111 */
	{ 11, 0x68, 20 },   /* 0000 1101 000 */
	{ 11, 0x6C, 21 },   /* 0000 1101 100 */
	{ 11, 0x37, 22 },   /* 0000 0110 111 */
	{ 11, 0x28, 23 },   /* 0000 0101 000 */
	{ 11, 0x17, 24 },   /* 0000 0010 111 */
	{ 11, 0x18, 25 },   /* 0000 0011 000 */
	{ 12, 0xCA, 26 },   /* 0000 1100 1010 */
	{ 12, 0xCB, 27 },   /* 0000 1100 1011 */
	{ 12, 0xCC, 28 },   /* 0000 1100 1100 */
	{ 12, 0xCD, 29 },   /* 0000 1100 1101 */
	{ 12, 0x68, 30 },   /* 0000 0110 1000 */
	{ 12, 0x69, 31 },   /* 0000 0110 1001 */
	{ 12, 0x6A, 32 },   /* 0000 0110 1010 */
	{ 12, 0x6B, 33 },   /* 0000 0110 1011 */
	{ 12, 0xD2, 34 },   /* 0000 1101 0010 */
	{ 12, 0xD3, 35 },   /* 0000 1101 0011 */
	{ 12, 0xD4, 36 },   /* 0000 1101 0100 */
	{ 12, 0xD5, 37 },   /* 0000 1101 0101 */
	{ 12, 0xD6, 38 },   /* 0000 1101 0110 */
	{ 12, 0xD7, 39 },   /* 0000 1101 0111 */
	{ 12, 0x6C, 40 },   /* 0000 0110 1100 */
	{ 12, 0x6D, 41 },   /* 0000 0110 1101 */
	{ 12, 0xDA, 42 },   /* 0000 1101 1010 */
	{ 12, 0xDB, 43 },   /* 0000 1101 1011 */
	{ 12, 0x54, 44 },   /* 0000 0101 0100 */
	{ 12, 0x55, 45 },   /* 0000 0101 0101 */
	{ 12, 0x56, 46 },   /* 0000 0101 0110 */
	{ 12, 0x57, 47 },   /* 0000 0101 0111 */
	{ 12, 0x64, 48 },   /* 0000 0110 0100 */
	{ 12, 0x65, 49 },   /* 0000 0110 0101 */
	{ 12, 0x52, 50 },   /* 0000 0101 0010 */
	{ 12, 0x53, 51 },   /* 0000 0101 0011 */
	{ 12, 0x24, 52 },   /* 0000 0010 0100 */
	{ 12, 0x37, 53 },   /* 0000 0011 0111 */
	{ 12, 0x38, 54 },   /* 0000 0011 1000 */
	{ 12, 0x27, 55 },   /* 0000 0010 0111 */
	{ 12, 0x28, 56 },   /* 0000 0010 1000 */
	{ 12, 0x58, 57 },   /* 0000 0101 1000 */
	{ 12, 0x59, 58 },   /* 0000 0101 1001 */
	{ 12, 0x2B, 59 },   /* 0000 0010 1011 */
	{ 12, 0x2C, 60 },   /* 0000 0010 1100 */
	{ 12, 0x5A, 61 },   /* 0000 0101 1010 */
	{ 12, 0x66, 62 },   /* 0000 0110 0110 */
	{ 12, 0x67, 63 },   /* 0000 0110 0111 */
	{ 10, 0xF, 64 },    /* 0000 0011 11 */
	{ 12, 0xC8, 128 },  /* 0000 1100 1000 */
	{ 12, 0xC9, 192 },  /* 0000 1100 1001 */
	{ 12, 0x5B, 256 },  /* 0000 0101 1011 */
	{ 12, 0x33, 320 },	/* 0000 0011 0011 */
	{ 12, 0x34, 384 },  /* 0000 0011 0100 */
	{ 12, 0x35, 448 },  /* 0000 0011 0101 */
	{ 13, 0x6C, 512 },  /* 0000 0011 0110 0 */
	{ 13, 0x6D, 576 },  /* 0000 0011 0110 1 */
	{ 13, 0x4A, 640 },  /* 0000 0010 0101 0 */
	{ 13, 0x4B, 704 },  /* 0000 0010 0101 1 */
	{ 13, 0x4C, 768 },  /* 0000 0010 0110 0 */
	{ 13, 0x4D, 832 },  /* 0000 0010 0110 1 */
	{ 13, 0x72, 896 },  /* 0000 0011 1001 0 */
	{ 13, 0x73, 960 },  /* 0000 0011 1001 1 */
	{ 13, 0x74, 1024 }, /* 0000 0011 1010 0 */
	{ 13, 0x75, 1088 }, /* 0000 0011 1010 1 */
	{ 13, 0x76, 1152 }, /* 0000 0011 1011 0 */
	{ 13, 0x77, 1216 }, /* 0000 0011 1011 1 */
	{ 13, 0x52, 1280 }, /* 0000 0010 1001 0 */
	{ 13, 0x53, 1344 }, /* 0000 0010 1001 1 */
	{ 13, 0x54, 1408 }, /* 0000 0010 1010 0 */
	{ 13, 0x55, 1472 }, /* 0000 0010 1010 1 */
	{ 13, 0x5A, 1536 }, /* 0000 0010 1101 0 */
	{ 13, 0x5B, 1600 }, /* 0000 0010 1101 1 */
	{ 13, 0x64, 1664 }, /* 0000 0011 0010 0 */
	{ 13, 0x65, 1728 }, /* 0000 0011 0010 1 */
	{ 11, 0x8, 1792 },  /* 0000 0001 000 */
	{ 11, 0xC, 1856 },  /* 0000 0001 100 */
	{ 11, 0xD, 1920 },  /* 0000 0001 101 */
	{ 12, 0x12, 1984 }, /* 0000 0001 0010 */
	{ 12, 0x13, 2048 }, /* 0000 0001 0011 */
	{ 12, 0x14, 2112 }, /* 0000 0001 0100 */
	{ 12, 0x15, 2176 }, /* 0000 0001 0101 */
	{ 12, 0x16, 2240 }, /* 0000 0001 0110 */
	{ 12, 0x17, 2304 }, /* 0000 0001 0111 */
	{ 12, 0x1C, 2368 }, /* 0000 0001 1100 */
	{ 12, 0x1D, 2432 }, /* 0000 0001 1101 */
	{ 12, 0x1E, 2496 }, /* 0000 0001 1110 */
	{ 12, 0x1F, 2560 }, /* 0000 0001 1111 */
	{ 12, 0x1, G3CODE_EOL },     /* 0000 0000 0001 */
	{ 9, 0x1, G3CODE_INVALID },  /* 0000 0000 1 */
	{ 10, 0x1, G3CODE_INVALID }, /* 0000 0000 01 */
	{ 11, 0x1, G3CODE_INVALID }, /* 0000 0000 001 */
	{ 12, 0x0, G3CODE_INVALID }, /* 0000 0000 0000 */
};

const CCITTFax4Encoder::TableEntry CCITTFax4Encoder::horizcode = { 3, 0x1, 0 }; /* 001 */
const CCITTFax4Encoder::TableEntry CCITTFax4Encoder::passcode = { 4, 0x1, 0 };  /* 0001 */
const CCITTFax4Encoder::TableEntry CCITTFax4Encoder::vcodes[7] = {
	{ 7, 0x03, 0 }, /* 0000 011 */
	{ 6, 0x03, 0 }, /* 0000 11 */
	{ 3, 0x03, 0 }, /* 011 */
	{ 1, 0x1, 0 },  /* 1 */
	{ 3, 0x2, 0 },  /* 010 */
	{ 6, 0x02, 0 }, /* 0000 10 */
	{ 7, 0x02, 0 }  /* 0000 010 */
};

struct CCITTFax4Encoder::EncoderState {
	std::vector<uint8_t> buf;
	uint8_t data = 0;
	uint8_t bit = 8;
};

CCITTFax4Encoder::CCITTFax4Encoder() : m_state(new EncoderState) {}

CCITTFax4Encoder::~CCITTFax4Encoder() {
	delete m_state;
}

uint8_t* CCITTFax4Encoder::encode(const uint8_t *buffer, uint32_t width, uint32_t height, uint32_t rowbytes, uint32_t &outputSize) {
	m_state->buf.resize(0);
	m_state->data = 0;
	m_state->bit = 8;

	uint8_t whiteline[rowbytes];
	std::fill_n(whiteline, rowbytes, 0);

	const uint8_t* refline = whiteline;
	for(uint32_t y = 0; y < height; ++y) {
		encode2DRow(buffer, refline, width);
		refline = buffer;
		buffer += rowbytes;
	}
	putbits(EOL, 12);
	putbits(EOL, 12);
	if(m_state->bit != 8) {
		flushbits();
	}

	outputSize = m_state->buf.size();
	return m_state->buf.data();
}

void CCITTFax4Encoder::putspan(int32_t span, const TableEntry* tab) {
	while (span >= 2624) {
		const TableEntry& te = tab[63 + (2560>>6)];
		putbits(te.code, te.length);
		span -= te.runlen;
	}
	if (span >= 64) {
		const TableEntry& te = tab[63 + (span>>6)];
		assert(te.runlen == 64*(span>>6));
		putbits(te.code, te.length);
		span -= te.runlen;
	}
	const TableEntry& te = tab[span];
	putbits(te.code, te.length);
}

void CCITTFax4Encoder::putbits(uint16_t bits, uint16_t length) {
	static const int msbmask[9] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };

	while (length > m_state->bit) {
		m_state->data |= bits >> (length - m_state->bit);
		length -= m_state->bit;
		flushbits();
	}
	assert(length < 9);
	m_state->data |= (bits & msbmask[length]) << (m_state->bit - length);
	m_state->bit -= length;
	if (m_state->bit == 0) {
		flushbits();
	}
}

void CCITTFax4Encoder::flushbits() {
	m_state->buf.push_back(m_state->data);
	m_state->data = 0;
	m_state->bit = 8;
}

static inline uint8_t pixel(const uint8_t* line, uint32_t i) {
	return !(((line[i>>3]) >> (7 - (i&7))) & 1);
}

static inline uint32_t findpixel(const uint8_t* line, uint32_t start, uint32_t length, uint8_t color) {
	for(uint32_t pos = start; pos < length; ++pos) {
		if(pixel(line, pos) == color) {
			return pos;
		}
	}
	return length;
}

void CCITTFax4Encoder::encode2DRow(const uint8_t* codeline, const uint8_t* refline, uint32_t linebits) {
	uint32_t a0 = 0;
	uint32_t a1 = findpixel(codeline, 0, linebits, 1);
	uint32_t b1 = findpixel(refline, 0, linebits, 1);
	uint32_t b2 = findpixel(refline, b1 + 1, linebits, !pixel(refline, b1));

	while(true) {
		if (b2 < a1) {
			/* pass mode */
			putbits(passcode.code, passcode.length);
			a0 = b2;
		} else {
			int32_t d = b1 - a1;
			if (-3 <= d && d <= 3) {
				/* vertical mode */
				putbits(vcodes[d+3].code, vcodes[d+3].length);
				a0 = a1;
			} else {
				/* horizontal mode */
				uint32_t a2 = findpixel(codeline, a1 + 1, linebits, !pixel(codeline, a1));
				putbits(horizcode.code, horizcode.length);
				if (a0+a1 == 0 || pixel(codeline, a0) == 0) {
					putspan(a1-a0, whiteCodes);
					putspan(a2-a1, blackCodes);
				} else {
					putspan(a1-a0, blackCodes);
					putspan(a2-a1, whiteCodes);
				}
				a0 = a2;
			}
		}
		if (a0 >= linebits)
			break;
		// Next changing pixel on codeline right of a0
		a1 = findpixel(codeline, a0 + 1, linebits, !pixel(codeline, a0));
		// Next changing pixel on refline right of a0 and of opposite color than a0
		b1 = findpixel(refline, a0 + 1, linebits, !pixel(refline, a0));
		if(pixel(refline, b1) == pixel(codeline, a0)) {
			b1 = findpixel(refline, b1 + 1, linebits, !pixel(codeline, a0));
		}
		// Next changing pixel on refline right of b1
		b2 = findpixel(refline, b1 + 1, linebits, !pixel(refline, b1));
	}
}

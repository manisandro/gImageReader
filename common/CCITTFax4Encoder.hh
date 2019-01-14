/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * CCITTFax4Encoder.hh
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
 *   Copyright (C) 2016-2019 Sandro Mani <manisandro@gmail.com>
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

#ifndef FAX4ENCODER_HH
#define FAX4ENCODER_HH

#include <cstdint>

class CCITTFax4Encoder {
public:
	CCITTFax4Encoder();
	~CCITTFax4Encoder();
	uint8_t* encode(const uint8_t* buffer, uint32_t width, uint32_t height, uint32_t rowbytes, uint32_t& outputSize);

private:
	struct EncoderState;
	struct TableEntry;
	EncoderState* m_state;

	void encode2DRow(const uint8_t* codeline, const uint8_t* refline, uint32_t linebits);
	inline void putspan(int32_t span, const TableEntry* tab);
	inline void putbits(uint16_t bits, uint16_t length);
	inline void flushbits();

	static const TableEntry whiteCodes[];
	static const TableEntry blackCodes[];
	static const TableEntry horizcode;
	static const TableEntry passcode;
	static const TableEntry vcodes[];
};

#endif // CCITT_HH

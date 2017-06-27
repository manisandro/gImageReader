/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.hh
 * Copyright (C) 2017 Alexander Zaitsev <zamazan4ik@tut.by>
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

#ifndef PRLIB_COMPATIBILITY_HPP
#define PRLIB_COMPATIBILITY_HPP

#include <cctype>

typedef int LONG;
typedef unsigned char BYTE;
typedef unsigned int DWORD;
typedef unsigned short WORD;

struct BITMAPFILEHEADER
{
    ushort bfType;
    uint bfSize;
    uint bfReserved;
    uint bfOffBits;
};

struct BITMAPINFOHEADER
{
    uint biSize;
    int biWidth;
    int biHeight;
    short biPlanes;
    short biBitCount;
    uint biCompression;
    uint biSizeImage;
    int biXPelsPerMeter;
    int biYPelsPerMeter;
    uint biClrUsed;
    uint biClrImportant;
};

struct RGBQUAD
{
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
};

struct BITMAPINFO
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[256];
};

#endif //PRLIB_COMPATIBILITY_HPP

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Geometry.hh
 * Copyright (C) 2016-2020 Sandro Mani <manisandro@gmail.com>
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

#ifndef IMAGE_HH
#define IMAGE_HH

#include <cairomm/cairomm.h>

class Image {
public:
	enum Format {Format_RGB24 = 0, Format_Gray8 = 1, Format_Mono = 2} format; // This order needs to be the same as combo:pdfoptions.imageformat
	enum ConversionFlags { AutoColor = 0, ThresholdDithering = 1, DiffuseDithering = 2} conversionFlags;
	int width;
	int height;
	int sampleSize;
	int bytesPerLine;
	unsigned char* data = nullptr;

	Image(Cairo::RefPtr<Cairo::ImageSurface> src, Format targetFormat, ConversionFlags flags);
	~Image() {
		delete[] data;
	}
	void writeJpeg(int quality, uint8_t*& buf, unsigned long& bufLen);

	static Cairo::RefPtr<Cairo::ImageSurface> simulateFormat(Cairo::RefPtr<Cairo::ImageSurface> src, Format format, ConversionFlags flags);
	static Cairo::RefPtr<Cairo::ImageSurface> scale(Cairo::RefPtr<Cairo::ImageSurface> src, double scaleFactor);
};



#endif // IMAGEUTILS_HH

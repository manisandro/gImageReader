/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Geometry.hh
 * Copyright (C) 2016 Sandro Mani <manisandro@gmail.com>
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

#include "Image.hh"
#include <cstring>
#include <jpeglib.h>

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define PIXEL_A(x) x[3]
# define PIXEL_R(x) x[2]
# define PIXEL_G(x) x[1]
# define PIXEL_B(x) x[0]
#else
# define PIXEL_A(x) x[3]
# define PIXEL_R(x) x[2]
# define PIXEL_G(x) x[1]
# define PIXEL_B(x) x[0]
#endif

Image::Image(Cairo::RefPtr<Cairo::ImageSurface> src, Format targetFormat)
{
	width = src->get_width();
	height = src->get_height();
	format = targetFormat;
	int stride = src->get_stride();

	if(format == Format_RGB24) {
		sampleSize = 8;
		bytesPerLine = 3 * width;
		data = new uint8_t[width * height * 3];
		#pragma omp parallel for schedule(static)
		for(int y = 0; y < height; ++y) {
			for(int x = 0; x < width; ++x) {
				uint8_t* srcpx = &src->get_data()[y * stride + 4 * x];
				uint8_t* dstpx = &data[y * bytesPerLine + 3 * x];
				dstpx[0] = PIXEL_R(srcpx);
				dstpx[1] = PIXEL_G(srcpx);
				dstpx[2] = PIXEL_B(srcpx);
			}
		}
	} else if(format == Format_Gray8) {
		sampleSize = 8;
		bytesPerLine = width;
		data = new uint8_t[width * height];
		#pragma omp parallel for schedule(static)
		for(int y = 0; y < height; ++y) {
			for(int x = 0; x < width; ++x) {
				uint8_t* srcpx = &src->get_data()[y * stride + 4 * x];
				data[y * bytesPerLine + x] = 0.21 * PIXEL_R(srcpx) + 0.72 * PIXEL_G(srcpx) + 0.07 * PIXEL_B(srcpx);
			}
		}
	} else if(format == Format_Mono) {
		// TODO: Dithering
		sampleSize = 1;
		bytesPerLine = width / 8 + (width % 8 != 0);
		data = new uint8_t[height * bytesPerLine];
		std::memset(data, 0, height * bytesPerLine);
		#pragma omp parallel for schedule(static)
		for(int y = 0; y < height; ++y) {
			for(int x = 0; x < width; ++x) {
				uint8_t* srcpx = &src->get_data()[y * stride + 4 * x];
				uint8_t gray = 0.21 * PIXEL_R(srcpx) + 0.72 * PIXEL_G(srcpx) + 0.07 * PIXEL_B(srcpx);
				if(gray > 127) {
					data[y * bytesPerLine + x/8] |= 1 << x%8;
				}
			}
		}
	}
}

void Image::writeJpeg(int quality, uint8_t*& buf, unsigned long& bufLen)
{
	buf = nullptr;
	bufLen = 0;
	if(format == Format_Mono) {
		return; // not supported by jpeg
	}

	// https://github.com/LuaDist/libjpeg/blob/master/example.c
	jpeg_compress_struct cinfo;
	jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_mem_dest(&cinfo, &buf, &bufLen);
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = format == Format_RGB24 ? 3 : 1;
	cinfo.in_color_space = format == Format_RGB24 ? JCS_RGB : JCS_GRAYSCALE;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, true);
	jpeg_start_compress(&cinfo, true);
	JSAMPROW row_pointer[1];
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &data[cinfo.next_scanline * bytesPerLine];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
}

Cairo::RefPtr<Cairo::ImageSurface> Image::simulateFormat(Cairo::RefPtr<Cairo::ImageSurface> src, Format format)
{
	int imgw = src->get_width();
	int imgh = src->get_height();
	int stride = src->get_stride();

	if(format == Format_Gray8) {
		Cairo::RefPtr<Cairo::ImageSurface> dst = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, imgw, imgh);
		dst->flush();
		#pragma omp parallel for schedule(static)
		for(int y = 0; y < imgh; ++y) {
			for(int x = 0; x < imgw; ++x) {
				int offset = y * stride + 4 * x;
				uint8_t* srcpx = &src->get_data()[offset];
				uint8_t* dstpx = &dst->get_data()[offset];
				uint8_t gray = 0.21 * PIXEL_R(srcpx) + 0.72 * PIXEL_G(srcpx) + 0.07 * PIXEL_B(srcpx);
				PIXEL_A(dstpx) = 255;
				PIXEL_R(dstpx) = gray;
				PIXEL_G(dstpx) = gray;
				PIXEL_B(dstpx) = gray;
			}
		}
		dst->mark_dirty();
		return dst;
	} else if(format == Format_Mono) {
		// TODO: Dithering
		Cairo::RefPtr<Cairo::ImageSurface> dst = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, imgw, imgh);
		dst->flush();
		#pragma omp parallel for schedule(static)
		for(int y = 0; y < imgh; ++y) {
			for(int x = 0; x < imgw; ++x) {
				int offset = y * stride + 4 * x;
				uint8_t* srcpx = &src->get_data()[offset];
				uint8_t* dstpx = &dst->get_data()[offset];
				uint8_t bw = 0.21 * PIXEL_R(srcpx) + 0.72 * PIXEL_G(srcpx) + 0.07 * PIXEL_B(srcpx) > 127 ? 255 : 0;
				PIXEL_A(dstpx) = 255;
				PIXEL_R(dstpx) = bw;
				PIXEL_G(dstpx) = bw;
				PIXEL_B(dstpx) = bw;
			}
		}
		dst->mark_dirty();
		return dst;
	}
	return src;
}

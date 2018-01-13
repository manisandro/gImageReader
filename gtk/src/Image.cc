/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Geometry.hh
 * Copyright (C) (\d+)-2018 Sandro Mani <manisandro@gmail.com>
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

static inline uint8_t clamp(int val) {
	return val > 255 ? 255 : val < 0 ? 0 : val;
}

Image::Image(Cairo::RefPtr<Cairo::ImageSurface> src, Format targetFormat, ConversionFlags flags) {
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
	} else if(format == Format_Gray8 || format == Format_Mono) {
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
		if(format == Format_Mono) {
			sampleSize = 1;
			bytesPerLine = width / 8 + (width % 8 != 0);
			uint8_t* newdata = new uint8_t[height * bytesPerLine];
			std::memset(newdata, 0, height * bytesPerLine);
			// Dithering: https://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering
			for(int y = 0; y < height; ++y) {
				for(int x = 0; x < width; ++x) {
					uint8_t oldpixel = data[y * width + x];
					uint8_t newpixel = oldpixel > 127 ? 255 : 0;
					int err = int(oldpixel) - int(newpixel);
					if(newpixel == 255) {
						newdata[y * bytesPerLine + x/8] |= 0x80 >> x%8;
					}
					if(flags == DiffuseDithering) {
						if(x + 1 < width) { // right neighbor
							uint8_t& pxr = data[y * width + (x + 1)];
							pxr = clamp(pxr + ((err * 7) >> 4));
						}
						if(y + 1 == height) {
							continue; // last line
						}
						if(x > 0) { // bottom left and bottom neighbor
							uint8_t& pxbl = data[(y + 1) * width + (x - 1)];
							pxbl = clamp(pxbl + ((err * 3) >> 4));
							uint8_t& pxb = data[(y + 1) * width + x];
							pxb = clamp(pxb + ((err * 5) >> 4));
						}
						if(x + 1 < width) { // bottom right neighbor
							uint8_t& pxbr = data[(y + 1) * width + (x + 1)];
							pxbr = clamp(pxbr + ((err * 1) >> 4));
						}
					}
				}
			}
			delete[] data;
			data = newdata;
		}
	}
}

void Image::writeJpeg(int quality, uint8_t*& buf, unsigned long& bufLen) {
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

Cairo::RefPtr<Cairo::ImageSurface> Image::simulateFormat(Cairo::RefPtr<Cairo::ImageSurface> src, Format format, ConversionFlags flags) {
	int imgw = src->get_width();
	int imgh = src->get_height();
	int stride = src->get_stride();

	if(format == Format_Gray8 || format == Format_Mono) {
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
		if(format == Format_Mono) {
			// Dithering: https://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering
			for(int y = 0; y < imgh; ++y) {
				for(int x = 0; x < imgw; ++x) {
					uint8_t* px = &dst->get_data()[y * stride + 4 * x];
					uint8_t newpixel = PIXEL_R(px) > 127 ? 255 : 0;
					int err = int(PIXEL_R(px)) - int(newpixel);
					PIXEL_R(px) = PIXEL_G(px) = PIXEL_B(px) = newpixel;
					if(flags == DiffuseDithering) {
						if(x + 1 < imgw) { // right neighbor
							px = &dst->get_data()[y * stride + 4 * (x + 1)];
							PIXEL_R(px) = PIXEL_G(px) = PIXEL_B(px) = clamp(PIXEL_R(px) + ((err * 7) >> 4));
						}
						if(y + 1 == imgh) {
							continue; // last line
						}
						if(x > 0) { // bottom left and bottom neighbor
							px = &dst->get_data()[(y + 1) * stride + 4 * (x - 1)];
							PIXEL_R(px) = PIXEL_G(px) = PIXEL_B(px) = clamp(PIXEL_R(px) + ((err * 3) >> 4));
							px = &dst->get_data()[(y + 1) * stride + 4 * x];
							PIXEL_R(px) = PIXEL_G(px) = PIXEL_B(px) = clamp(PIXEL_R(px) + ((err * 5) >> 4));
						}
						if(x + 1 < imgw) { // bottom right neighbor
							px = &dst->get_data()[(y + 1) * stride + 4 * (x + 1)];
							PIXEL_R(px) = PIXEL_G(px) = PIXEL_B(px) = clamp(PIXEL_R(px) + ((err * 1) >> 4));
						}
					}
				}
			}
		}
		dst->mark_dirty();
		return dst;
	}
	return src;
}

Cairo::RefPtr<Cairo::ImageSurface> Image::scale(Cairo::RefPtr<Cairo::ImageSurface> src, double scaleFactor) {
	if(scaleFactor == 1.0) {
		return src;
	}
	Cairo::RefPtr<Cairo::ImageSurface> dst = Cairo::ImageSurface::create(src->get_format(), std::ceil(src->get_width() * scaleFactor), std::ceil(src->get_height() * scaleFactor));
	Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(dst);
	ctx->scale(scaleFactor, scaleFactor);
	ctx->set_source(src, 0, 0);
	Cairo::RefPtr<Cairo::SurfacePattern>::cast_static(ctx->get_source())->set_filter(Cairo::FILTER_BEST);
	ctx->paint();
	dst->flush();
	return dst;
}

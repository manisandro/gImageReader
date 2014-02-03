/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayManipulator.cc
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#include "DisplayManipulator.hh"
#include "Geometry.hh"

#include <set>

void Manipulators::adjustBrightness(Cairo::RefPtr<Cairo::ImageSurface> surf, int adjustment)
{
	if(adjustment == 0){
		return;
	}
	float k = 1.f - std::abs(adjustment / 200.f);
	float d = adjustment > 0 ? 255.f : 0.f;
	int n = surf->get_height() * surf->get_width();
	uint8_t* data = surf->get_data();
	for(int i = 0; i < n; ++i){
		uint8_t& r = data[4*i + 2];
		uint8_t& g = data[4*i + 1];
		uint8_t& b = data[4*i + 0];
		r = d * (1.f - k) + r * k;
		g = d * (1.f - k) + g * k;
		b = d * (1.f - k) + b * k;
	}
}

void Manipulators::adjustContrast(Cairo::RefPtr<Cairo::ImageSurface> surf, int adjustment)
{
	if(adjustment == 0){
		return;
	}
	float k = adjustment * 2.55f;
	int n = surf->get_height() * surf->get_width();
	uint8_t* data = surf->get_data();
	// http://thecryptmag.com/Online/56/imgproc_5.html
	float F = (259.f * (k + 255.f)) / (255.f * (259.f - k));
	for(int i = 0; i < n; ++i){
		uint8_t& r = data[4*i + 2];
		uint8_t& g = data[4*i + 1];
		uint8_t& b = data[4*i + 0];
		r = std::max(0.f, std::min(F * (r - 128.f) + 128.f, 255.f));
		g = std::max(0.f, std::min(F * (g - 128.f) + 128.f, 255.f));
		b = std::max(0.f, std::min(F * (b - 128.f) + 128.f, 255.f));
	}
}

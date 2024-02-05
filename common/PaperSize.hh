/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * PaperSize.hh
 * Copyright (C) 2017-2018 Alexander Zaitsev <zamazan4ik@tut.by>
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

#ifndef GIMAGEREADER_PAPERSIZE_HH
#define GIMAGEREADER_PAPERSIZE_HH

#include <string>
#include <utility>
#include <vector>

class PaperSize {
public:
	template<typename T>
	struct Size {
		Size(T width_ = 0, T height_ = 0) : width(width_), height(height_) {}
		bool operator<(const Size& val);

		T width, height;
	};

	enum Unit {
		cm,
		inch
	};

	static Size<double> getSize(Unit unit, const std::string& format, bool landscape);
	static const double CMtoInch;
	static const std::vector<std::pair<std::string, Size<int>>> paperSizes;

}; // PaperSize

#endif //GIMAGEREADER_PAPERSIZE_HH

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRTextExporter.hh
 * Copyright (C) 2013-2025 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRTEXTEXPORTER_HH
#define HOCRTEXTEXPORTER_HH

#include "common.hh"
#include "HOCRExporter.hh"

class HOCRItem;


class HOCRTextExporter : public HOCRExporter {
public:
	bool run(const HOCRDocument* hocrdocument, const std::string& outname, const ExporterSettings* settings = nullptr) override;

private:
	void printItem(std::ofstream& outputStream, const HOCRItem* item, bool lastChild = false);
};

#endif // HOCRTEXTEXPORTER_HH

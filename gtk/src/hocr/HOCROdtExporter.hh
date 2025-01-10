/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCROdtExporter.hh
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRODTEXPORTER_HH
#define HOCRODTEXPORTER_HH

#include "common.hh"
#include "Geometry.hh"
#include "HOCRExporter.hh"


class HOCRItem;
namespace xmlpp {
class Element;
}
struct zip;


class HOCROdtExporter : public HOCRExporter {
public:
	bool run(const HOCRDocument* hocrdocument, const std::string& outname, const ExporterSettings* settings = nullptr) override;

private:

	void writeImage(zip* fzip, std::map<const HOCRItem*, Glib::ustring>& images, const HOCRItem* item);
	void writeFontFaceDecls(std::set<Glib::ustring>& families, const HOCRItem* item, xmlpp::Element* parentEl);
	void writeFontStyles(std::map<Glib::ustring, std::map<double, Glib::ustring >> & styles, const HOCRItem* item, xmlpp::Element* parentEl, int& counter);
	void printItem(xmlpp::Element* parentEl, const HOCRItem* item, int pageNr, int dpi, std::map<Glib::ustring, std::map<double, Glib::ustring >>& fontStyleNames, std::map<const HOCRItem*, Glib::ustring>& images);

	bool setSource(const Glib::ustring& sourceFile, int page, int dpi, double angle);
	Cairo::RefPtr<Cairo::ImageSurface> getSelection(const Geometry::Rectangle& bbox);
};

#endif // HOCRODTEXPORTER_HH

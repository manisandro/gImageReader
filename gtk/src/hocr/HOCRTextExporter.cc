/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRTextExporter.cc
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

#include "HOCRDocument.hh"
#include "HOCRTextExporter.hh"
#include "MainWindow.hh"
#include "FileDialogs.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <fstream>

bool HOCRTextExporter::run(const Glib::RefPtr<HOCRDocument>& hocrdocument, std::string &filebasename)
{
	std::string suggestion = filebasename;
	if(suggestion.empty()) {
		std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.empty() ? Utils::split_filename(sources.front()->displayname).first : _("output");
	}

	FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
	std::string outname = FileDialogs::save_dialog(_("Save Text Output..."), suggestion + ".txt", "outputdir", filter);
	if(outname.empty()) {
		return false;
	}
	filebasename = Utils::split_filename(outname).first;

	std::ofstream file(outname);
	if(!file.is_open()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Export failed"), _("The text export failed: unable to write output file."));
		return false;
	}

	for(int i = 0, n = hocrdocument->pageCount(); i < n; ++i) {
		const HOCRPage* page = hocrdocument->page(i);
		if(!page->isEnabled()) {
			continue;
		}
		printItem(file, page);
	}
	return true;
}

void HOCRTextExporter::printItem(std::ofstream& outputStream, const HOCRItem* item, bool lastChild)
{
	if(!item->isEnabled()) {
		return;
	}
	Glib::ustring itemClass = item->itemClass();
	if(itemClass == "ocrx_word") {
		outputStream << item->text();
		if(!lastChild) {
			outputStream << " ";
		}
	} else {
		for(int i = 0, n = item->children().size(); i < n; ++i) {
			printItem(outputStream, item->children()[i], i == n - 1);
		}
	}
	if(itemClass == "ocr_line" || itemClass == "ocr_par") {
		outputStream << "\n";
	}
}

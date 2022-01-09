/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditor.hh
 * Copyright (C) 2013-2020 Sandro Mani <manisandro@gmail.com>
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

#ifndef OUTPUTEDITOR_HH
#define OUTPUTEDITOR_HH

#include <gtkmm.h>
#include <string>

#include "Config.hh"

namespace tesseract {
class TessBaseAPI;
}

class OutputEditor {
public:
	struct PageInfo {
		std::string filename;
		int page;
		double angle;
		int resolution;
	};
	struct ReadSessionData {
		virtual ~ReadSessionData() = default;
		bool prependFile;
		bool prependPage;
		PageInfo pageInfo;
	};
	class BatchProcessor {
	public:
		virtual ~BatchProcessor() = default;
		virtual std::string fileSuffix() const = 0;
		virtual void writeHeader(std::ostream& /*dev*/, tesseract::TessBaseAPI* /*tess*/, const PageInfo& /*pageInfo*/) const {}
		virtual void writeFooter(std::ostream& /*dev*/) const {}
		virtual void appendOutput(std::ostream& dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo, bool firstArea) const = 0;
	};

	OutputEditor() {}
	virtual ~OutputEditor() {}

	virtual Gtk::Box* getUI() const = 0;
	virtual ReadSessionData* initRead(tesseract::TessBaseAPI& tess) = 0;
	virtual void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) = 0;
	virtual void readError(const Glib::ustring& errorMsg, ReadSessionData* data) = 0;
	virtual void finalizeRead(ReadSessionData* data) {
		delete data;
	}
	virtual BatchProcessor* createBatchProcessor(const std::map<Glib::ustring, Glib::ustring>& options) const = 0;

	virtual bool getModified(Gtk::Widget* widget = nullptr) const = 0;
	virtual bool containsSource(const std::string& source, int sourcePage) const { return false; }


	virtual void onVisibilityChanged(bool /*visible*/) {}
	virtual bool open(const std::string& filename) = 0;
	virtual bool clear(bool hide = true, Gtk::Widget* widget = nullptr) = 0;
	virtual bool save(const std::string& filename = "", Gtk::Widget* widget = nullptr) = 0;
	virtual void setLanguage(const Config::Lang& /*lang*/) {}
};

#endif // OUTPUTEDITOR_HH

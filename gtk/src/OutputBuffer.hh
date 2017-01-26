/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputBuffer.hh
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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

#ifndef OUTPUTBUFFER_HH
#define OUTPUTBUFFER_HH

#include "common.hh"
#include <gtksourceviewmm/buffer.h>

class OutputBuffer : public Gsv::Buffer {
public:
	void save_region_bounds(bool viewSelected);
	void get_region_bounds(Gtk::TextIter& start, Gtk::TextIter& stop);
	Gtk::TextIter replace_range(const Glib::ustring& text, const Gtk::TextIter& start, const Gtk::TextIter& end);
	bool findReplace(bool backwards, bool replace, bool matchCase, const Glib::ustring& searchstr, const Glib::ustring& replacestr, Gtk::TextView* view);
	bool replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase);

	static Glib::RefPtr<OutputBuffer> create() {
		return Glib::RefPtr<OutputBuffer>(new OutputBuffer());
	}

private:
	OutputBuffer();

	Glib::RefPtr<Gtk::TextTag> m_regionTag;
	Glib::RefPtr<Gtk::TextMark> m_regionBeginMark;
	Glib::RefPtr<Gtk::TextMark> m_regionEndMark;
};

#endif // OUTPUTBUFFER_HH

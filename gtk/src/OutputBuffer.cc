/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputBuffer.cc
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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

#include "OutputBuffer.hh"

OutputBuffer::OutputBuffer()
	: Gsv::Buffer()
{
	m_regionBeginMark = Gtk::TextMark::create(true);
	m_regionEndMark = Gtk::TextMark::create(false);
	add_mark(m_regionBeginMark, begin());
	add_mark(m_regionEndMark, end());
	set_highlight_matching_brackets(false);

	m_regionTag = create_tag("selection");
	Gdk::RGBA selColor = Gtk::Label().get_style_context()->get_background_color(Gtk::STATE_FLAG_SELECTED);
	selColor.set_red(std::min(1.0, selColor.get_red() * 1.6));
	selColor.set_green(std::min(1.0, selColor.get_green() * 1.6));
	selColor.set_blue(std::min(1.0, selColor.get_blue() * 1.6));
	m_regionTag->property_background_rgba() = selColor;
}

void OutputBuffer::save_region_bounds(bool viewSelected)
{
	Gtk::TextIter start = get_iter_at_mark(get_insert());
	Gtk::TextIter stop = get_iter_at_mark(get_selection_bound());
	if(viewSelected){
		bool entireRegion = false;
		if(start.get_offset() > stop.get_offset()){
			std::swap(start, stop);
		}
		// If nothing or only one word is selected, set region to entire document
		if(start.get_offset() == stop.get_offset() || !Glib::Regex::create("\\s")->match(get_text(start, stop))){
			start = begin();
			stop = end();
			entireRegion = true;
		}
		remove_tag(m_regionTag, get_iter_at_mark(m_regionBeginMark), get_iter_at_mark(m_regionEndMark));
		move_mark(m_regionBeginMark, start);
		move_mark(m_regionEndMark, stop);
		if(!entireRegion){
			apply_tag(m_regionTag, start, stop);
		}
	}
}

void OutputBuffer::get_region_bounds(Gtk::TextIter& start, Gtk::TextIter& stop)
{
	start = get_iter_at_mark(m_regionBeginMark);
	stop = get_iter_at_mark(m_regionEndMark);
}

Gtk::TextIter OutputBuffer::replace_range(const Glib::ustring &text, const Gtk::TextIter& start, const Gtk::TextIter& end)
{
	return insert(erase(start, end), text);
}

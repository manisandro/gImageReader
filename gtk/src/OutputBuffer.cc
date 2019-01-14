/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputBuffer.cc
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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
	: Gsv::Buffer() {
	m_regionBeginMark = Gtk::TextMark::create(true);
	m_regionEndMark = Gtk::TextMark::create(false);
	add_mark(m_regionBeginMark, begin());
	add_mark(m_regionEndMark, end());
	set_highlight_matching_brackets(false);

	m_regionTag = create_tag("selection");
	Gtk::Label label;
	Glib::RefPtr<Gtk::StyleContext> styleContext = label.get_style_context();
	styleContext->set_state(Gtk::STATE_FLAG_SELECTED);
	Gdk::RGBA selColor = styleContext->get_background_color(styleContext->get_state());
	selColor.set_red(std::min(1.0, selColor.get_red() * 1.6));
	selColor.set_green(std::min(1.0, selColor.get_green() * 1.6));
	selColor.set_blue(std::min(1.0, selColor.get_blue() * 1.6));
	m_regionTag->property_background_rgba() = selColor;
}

void OutputBuffer::save_region_bounds(bool viewSelected) {
	Gtk::TextIter start = get_iter_at_mark(get_insert());
	Gtk::TextIter stop = get_iter_at_mark(get_selection_bound());
	if(viewSelected) {
		bool entireRegion = false;
		if(start.get_offset() > stop.get_offset()) {
			std::swap(start, stop);
		}
		// If nothing or only one word is selected, set region to entire document
		if(start.get_offset() == stop.get_offset() || !Glib::Regex::create("\\s")->match(get_text(start, stop))) {
			start = begin();
			stop = end();
			entireRegion = true;
		}
		remove_tag(m_regionTag, get_iter_at_mark(m_regionBeginMark), get_iter_at_mark(m_regionEndMark));
		move_mark(m_regionBeginMark, start);
		move_mark(m_regionEndMark, stop);
		if(!entireRegion) {
			apply_tag(m_regionTag, start, stop);
		}
	}
}

void OutputBuffer::get_region_bounds(Gtk::TextIter& start, Gtk::TextIter& stop) {
	start = get_iter_at_mark(m_regionBeginMark);
	stop = get_iter_at_mark(m_regionEndMark);
}

Gtk::TextIter OutputBuffer::replace_range(const Glib::ustring& text, const Gtk::TextIter& start, const Gtk::TextIter& end) {
	return insert(erase(start, end), text);
}

bool OutputBuffer::findReplace(bool backwards, bool replace, bool matchCase, const Glib::ustring& searchstr, const Glib::ustring& replacestr, Gtk::TextView* view) {
	if(searchstr.empty()) {
		return false;
	}
	Gtk::TextSearchFlags flags = Gtk::TEXT_SEARCH_VISIBLE_ONLY | Gtk::TEXT_SEARCH_TEXT_ONLY;
	auto comparator = matchCase ?
	[](const Glib::ustring & s1, const Glib::ustring & s2) {
		return s1 == s2;
	}
:
	[](const Glib::ustring & s1, const Glib::ustring & s2) {
		return s1.lowercase() == s2.lowercase();
	};
	if(!matchCase) {
		flags |= Gtk::TEXT_SEARCH_CASE_INSENSITIVE;
	}

	Gtk::TextIter rstart, rend;
	get_region_bounds(rstart, rend);

	Gtk::TextIter start, end;
	get_selection_bounds(start, end);
	if(comparator(get_text(start, end, false), searchstr)) {
		if(replace) {
			start = end = insert(erase(start, end), replacestr);
			start.backward_chars(replacestr.length());
			select_range(start, end);
			view->scroll_to(end);
			return true;
		}
		if(backwards) {
			end.backward_char();
		} else {
			start.forward_char();
		}
	}
	Gtk::TextIter matchStart, matchEnd;
	if(backwards) {
		if(!end.backward_search(searchstr, flags, matchStart, matchEnd, rstart) &&
		        !rend.backward_search(searchstr, flags, matchStart, matchEnd, rstart)) {
			return false;
		}
	} else {
		if(!start.forward_search(searchstr, flags, matchStart, matchEnd, rend) &&
		        !rstart.forward_search(searchstr, flags, matchStart, matchEnd, rend)) {
			return false;
		}
	}
	// FIXME: backward_search appears to be buggy?
	matchEnd = matchStart;
	matchEnd.forward_chars(searchstr.length());
	select_range(matchStart, matchEnd);
	view->scroll_to(matchStart);
	return true;
}

bool OutputBuffer::replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase) {
	Gtk::TextIter start, end;
	get_region_bounds(start, end);
	int startpos = start.get_offset();
	int endpos = end.get_offset();
	Gtk::TextSearchFlags flags = Gtk::TEXT_SEARCH_VISIBLE_ONLY | Gtk::TEXT_SEARCH_TEXT_ONLY;
	if(!matchCase) {
		flags |= Gtk::TEXT_SEARCH_CASE_INSENSITIVE;
	}
	int diff = replacestr.length() - searchstr.length();
	int count = 0;
	Gtk::TextIter it = get_iter_at_offset(startpos);
	while(true) {
		Gtk::TextIter matchStart, matchEnd;
		if(!it.forward_search(searchstr, flags, matchStart, matchEnd) || matchEnd.get_offset() > endpos) {
			break;
		}
		it = insert(erase(matchStart, matchEnd), replacestr);
		endpos += diff;
		++count;
		while(Gtk::Main::events_pending()) {
			Gtk::Main::iteration();
		}
	}
	if(count == 0) {
		return false;
	}
	return true;
}

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.hh
 * Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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

#ifndef UTILS_HH
#define UTILS_HH

#include <gtkmm.h>
#include <type_traits>
#include <iterator>
#include <utility>

namespace Utils {
	extern Glib::ustring FALLBACK_COLOR_SCHEME;

	void popup_positioner(int& x, int& y, bool& push_in, Gtk::Widget* ref, Gtk::Menu* menu, bool alignRight, bool alignBottom);

	void error_dialog(const Glib::ustring& title, const Glib::ustring& text, Gtk::Window* parent = 0);
	int question_dialog(const Glib::ustring& title, const Glib::ustring& text, Gtk::Window* parent = 0);

	void configure_spin(Gtk::SpinButton* spin, double value, double min, double max, double step, double page, sigc::connection* block = 0);
	void set_error_state(Gtk::Entry* entry);
	void clear_error_state(Gtk::Entry* entry);

	Glib::ustring get_content_type(const std::string& filename);
	void get_filename_parts(const std::string& filename, std::string& base, std::string& ext);

	std::vector<Glib::ustring> string_split(const Glib::ustring& text, char delim, bool keepEmpty = false);

	template<typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
	T round(T x){
		return std::floor(x + T(0.5));
	}

	template<class It>
	struct rev_iters {
		It _begin, _end;
		It begin(){ return _begin; }
		It end(){ return _end; }
	};

	template<class T, class It = std::reverse_iterator<typename T::iterator>>
	rev_iters<It> reverse(T& t){
		return { t.rbegin(), t.rend() };
	}

	template <class T, class It = std::reverse_iterator<typename T::const_iterator>>
	rev_iters<It> reverse(const T& t){
		return { t.crbegin(), t.crend() };
	}

	bool busyTask(const std::function<bool()>& f, const Glib::ustring& msg);
}

#endif

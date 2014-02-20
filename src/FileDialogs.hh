/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FileDialogs.hh
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

#ifndef FILEDIALOGS_HH
#define FILEDIALOGS_HH

#include "common.hh"

namespace FileDialogs {

struct FileFilter {
	std::string name;
	std::string mime_type;
	std::string pattern;
};

std::vector<Glib::RefPtr<Gio::File>> open_sources_dialog(const std::string& initialDirectory, Gtk::Window* parent = nullptr);
std::string save_dialog(const Glib::ustring &title, const std::string& suggestedFile, const FileFilter& filter, Gtk::Window *parent = nullptr);


} // FileDialogs

#endif // FILEDIALOGS_HH

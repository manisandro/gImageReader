/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FileDialogs.cc
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

#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "Utils.hh"
#include <sstream>

#ifdef G_OS_WIN32
#include <gdk/gdkwin32.h>
#include <windows.h>
#include <locale>
#else
#include <gdk/gdkx.h>
#endif

#ifdef G_OS_WIN32

static std::wstring s2ws(const std::string& s)
{
	g_assert(sizeof(wchar_t) == sizeof(gunichar2));
	const char* in = s.c_str();
	wchar_t* buf = reinterpret_cast<wchar_t*>(g_utf8_to_utf16(in, -1, nullptr, nullptr, nullptr));
	std::wstring out(buf);
	g_free(buf);
	return out;
}

static std::string ws2s(const std::wstring& s)
{
	g_assert(sizeof(wchar_t) == sizeof(gunichar2));
	const gunichar2* in = reinterpret_cast<const gunichar2*>(s.c_str());
	char* buf = g_utf16_to_utf8(in, -1, nullptr, nullptr, nullptr);
	std::string out(buf);
	g_free(buf);
	return out;
}


std::wstring FileDialogs::FileFilter::to_win32_filter() const
{
	std::wstring filterstr = s2ws(name);
	filterstr += L'\0';
	filterstr += s2ws(pattern);
	filterstr += L'\0';
	return filterstr;
}

static std::vector<Glib::RefPtr<Gio::File>> win32_open_dialog(const Glib::ustring &title, const std::string& initialDirectory, const std::wstring& filter, bool multiple, Gtk::Window *parent)
{
	wchar_t wfile[1024] = {};
	std::wstring winitialDirectory = s2ws(initialDirectory);
	std::wstring wtitle = s2ws(title);

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = gdk_win32_window_get_impl_hwnd(parent->get_window()->gobj());
	ofn.lpstrFilter = filter.c_str();
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = wfile;
	ofn.nMaxFile = sizeof(wfile);
	ofn.lpstrInitialDir = winitialDirectory.c_str();
	ofn.lpstrTitle = wtitle.c_str();
	ofn.Flags = OFN_NONETWORKBUTTON|OFN_EXPLORER;
	if(multiple) ofn.Flags |= OFN_ALLOWMULTISELECT;
	bool ok = GetOpenFileNameW(&ofn);

	std::vector<Glib::RefPtr<Gio::File>> files;
	if(ok){
		if(ofn.nFileOffset == 0) {
			// One file only
			std::string filename = ws2s(ofn.lpstrFile);
			files.push_back(Gio::File::create_for_path(filename));
		} else {
			std::string directory = ws2s(ofn.lpstrFile);
			size_t offset = ofn.nFileOffset;
			while(true) {
				size_t len = wcslen(ofn.lpstrFile + offset);
				if(len == 0) {
					break;
				} else {
					std::string filename = ws2s(ofn.lpstrFile + offset);
					files.push_back(Gio::File::create_for_path(filename));
					offset += len + 1; // +1: string length + terminator, end of string is doubly terminated
				}
			}
		}
	}
	return files;
}

static std::vector<Glib::RefPtr<Gio::File>> win32_open_sources_dialog(const std::string& initialDirectory, Gtk::Window* parent)
{
	std::string title = _("Select Files");

	// Get formats
	std::wstring filter = s2ws(_("Images and PDFs"));
	filter += L'\0';
	for(const Gdk::PixbufFormat& format : Gdk::Pixbuf::get_formats()) {
		for(const Glib::ustring& ext : format.get_extensions()) {
			filter += s2ws(Glib::ustring::compose("*.%1;", ext));
		}
	}
	filter += L"*.pdf\0";

	return win32_open_dialog(title, initialDirectory, filter, true, parent);
}

static std::string win32_save_dialog(const Glib::ustring &title, const std::string &suggestedFile, const FileDialogs::FileFilter& filter, Gtk::Window *parent)
{
	// Get formats
	std::wstring filterstr = filter.to_win32_filter();

	std::wstring wsuggestedFile = s2ws(suggestedFile);
	std::wstring wtitle = s2ws(title);

	std::wstring suggestedName = s2ws(Glib::path_get_basename(suggestedFile));

	wchar_t wfile[MAX_PATH] = {};
	wcsncpy(wfile, suggestedName.c_str(), sizeof(wfile) / sizeof(wfile[0]));

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = gdk_win32_window_get_impl_hwnd(parent->get_window()->gobj());
	ofn.lpstrFilter = filterstr.c_str();
	ofn.lpstrFile = wfile;
	ofn.nMaxFile = sizeof(wfile);
	ofn.lpstrInitialDir = wsuggestedFile.c_str();
	ofn.lpstrTitle = wtitle.c_str();
	ofn.Flags = OFN_NONETWORKBUTTON|OFN_EXPLORER|OFN_OVERWRITEPROMPT;
	bool ok = GetSaveFileNameW(&ofn);

	std::string filename;
	if(ok){
		filename = ws2s(ofn.lpstrFile);
	}
	return filename;
}

#else

Glib::RefPtr<Gtk::FileFilter> FileDialogs::FileFilter::to_gnome_filter() const
{
	Glib::RefPtr<Gtk::FileFilter> filefilter = Gtk::FileFilter::create();
	filefilter->set_name(name);
	filefilter->add_mime_type(mime_type);
	filefilter->add_pattern(pattern);
	return filefilter;
}

static std::vector<Glib::RefPtr<Gio::File>> gnome_open_dialog(const std::string& title, const std::string& initialDirectory, Glib::RefPtr<Gtk::FileFilter> filter, bool multiple, Gtk::Window* parent)
{
	Gtk::FileChooserDialog dialog(*MAIN->getWindow(), _("Select Files"));
	dialog.add_button(_("Cancel"), Gtk::RESPONSE_CANCEL);
	dialog.add_button(_("OK"), Gtk::RESPONSE_OK);
	dialog.set_select_multiple(multiple);
	dialog.set_local_only(false);
	dialog.set_filter(filter);
	dialog.set_current_folder(initialDirectory);

	if(dialog.run() == Gtk::RESPONSE_OK){
		return dialog.get_files();
	}
	return std::vector<Glib::RefPtr<Gio::File>>();
}

static std::vector<Glib::RefPtr<Gio::File>> gnome_open_sources_dialog(const std::string& initialDirectory, Gtk::Window* parent)
{
	Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
	filter->set_name(_("Images and PDFs"));
	filter->add_pixbuf_formats();
	filter->add_mime_type("application/pdf");
	return gnome_open_dialog(_("Select Files"), initialDirectory, filter, true, parent);
}

static std::string gnome_save_dialog(const Glib::ustring &title, const std::string &suggestedFile, const FileDialogs::FileFilter& filter, Gtk::Window *parent)
{
	Gtk::FileChooserDialog dialog(*parent, title, Gtk::FILE_CHOOSER_ACTION_SAVE);
	dialog.add_button("gtk-cancel", Gtk::RESPONSE_CANCEL);
	dialog.add_button("gtk-ok", Gtk::RESPONSE_OK);
	dialog.set_select_multiple(false);
	dialog.set_create_folders(true);
	dialog.set_do_overwrite_confirmation(true);
	dialog.set_local_only(false);
	dialog.set_current_folder(Glib::path_get_dirname(suggestedFile));
	dialog.set_current_name(Glib::path_get_basename(suggestedFile));

	dialog.set_filter(filter.to_gnome_filter());

	if(dialog.run() == Gtk::RESPONSE_OK){
		return dialog.get_filename();
	}
	return "";
}

Glib::ustring FileDialogs::FileFilter::to_kde_filter() const
{
	return mime_type;
}

static std::vector<Glib::RefPtr<Gio::File>> kde_open_dialog(const Glib::ustring& title, const std::string& initialDirectory, const Glib::ustring& filter, bool multiple, Gtk::Window* parent)
{
	// Command line
	std::vector<Glib::ustring> argv = {
		"/usr/bin/kdialog",
		"--getopenfilename", initialDirectory, filter,
		multiple ? "--multiple" : "", "--separate-output",
		"--attach", Glib::ustring::compose("%1", gdk_x11_window_get_xid(parent->get_window()->gobj())),
		"--title", title,
		"--caption", PACKAGE_NAME
	};

	// Spawn process
	std::string stdout;
	int exit_status = -1;
	Glib::spawn_sync("", argv, /*Glib::SPAWN_DEFAULT*/Glib::SpawnFlags(0), sigc::slot<void>(), &stdout, nullptr, &exit_status);
	std::vector<Glib::RefPtr<Gio::File>> files;
	if(exit_status == 0) {
		std::istringstream iss(stdout);
		std::string filename;
		while(std::getline(iss, filename)) {
			files.push_back(Gio::File::create_for_path(filename));
		}
	}
	return files;
}

static std::vector<Glib::RefPtr<Gio::File>> kde_open_sources_dialog(const std::string& initialDirectory, Gtk::Window* parent)
{
	// Get formats
	Glib::ustring filter;
	for(const Gdk::PixbufFormat& format : Gdk::Pixbuf::get_formats()) {
		for(const Glib::ustring& mime : format.get_mime_types()) {
			filter += Glib::ustring::compose(" %1", mime);
		}
	}
	filter += " application/pdf";

	return kde_open_dialog(_("Select Files"), initialDirectory, filter, true, parent);
}

static std::string kde_save_dialog(const Glib::ustring &title, const std::string &suggestedFile, const FileDialogs::FileFilter& filter, Gtk::Window *parent)
{
	std::string filename = suggestedFile;
	do {
		std::vector<Glib::ustring> argv = {
			"/usr/bin/kdialog",
			"--getsavefilename", filename, filter.mime_type,
			"--attach", Glib::ustring::compose("%1", gdk_x11_window_get_xid(parent->get_window()->gobj())),
			"--title", title,
			"--caption", PACKAGE_NAME
		};
		int exit_status = -1;
		std::string stdout;
		Glib::spawn_sync("", argv, /*Glib::SPAWN_DEFAULT*/Glib::SpawnFlags(0), sigc::slot<void>(), &stdout, nullptr, &exit_status);
		if(exit_status != 0) {
			break;
		}
		std::istringstream iss(stdout);
		std::getline(iss, filename);
		if(Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
			std::vector<Glib::ustring> argv = {
				"/usr/bin/kdialog",
				"--yesno", Glib::ustring::compose(_("A file named \"%1\" already exists. Are you sure you want to overwrite it?"), Glib::path_get_basename(filename)),
				"--attach", Glib::ustring::compose("%1", gdk_x11_window_get_xid(parent->get_window()->gobj())),
				"--title", _("Overwrite File?"),
				"--caption", PACKAGE_NAME
			};
			Glib::spawn_sync("", argv, /*Glib::SPAWN_DEFAULT*/Glib::SpawnFlags(0), sigc::slot<void>(), nullptr, nullptr, &exit_status);
			if(exit_status != 0){
				continue;
			}
		}
		return filename;
	} while(true);
	return std::string();
}

static bool is_kde()
{
	static bool is_kde =
			Glib::ustring(Glib::getenv("XDG_CURRENT_DESKTOP")).lowercase() == "kde" &&
			Glib::file_test("/usr/bin/kdialog", Glib::FILE_TEST_EXISTS);
	return is_kde;
}

#endif

static void add_to_recent_items(const std::vector<Glib::RefPtr<Gio::File>>& files)
{
	Glib::RefPtr<Gtk::RecentManager> manager = Gtk::RecentManager::get_default();
	for(const Glib::RefPtr<Gio::File>& file : files) {
		manager->add_item(file->get_uri());
	}
}

namespace FileDialogs {

std::vector<Glib::RefPtr<Gio::File>> open_sources_dialog(const std::string& initialDirectory, Gtk::Window* parent)
{
	std::string initialDir = initialDirectory.empty() ? Glib::get_home_dir() : initialDirectory;
	parent = parent == nullptr ? MAIN->getWindow() : parent;
	std::vector<Glib::RefPtr<Gio::File>> files;
#ifdef G_OS_WIN32
	files = win32_open_sources_dialog(initialDir, parent);
	add_to_recent_items(files);
#else
	if(is_kde()) {
		files = kde_open_sources_dialog(initialDir, parent);
		add_to_recent_items(files);
	} else {
		files = gnome_open_sources_dialog(initialDir, parent);
	}
#endif
	return files;
}

std::vector<Glib::RefPtr<Gio::File>> open_dialog(const Glib::ustring &title, const std::string& initialDirectory, const FileFilter &filter, bool multiple, Gtk::Window *parent)
{
	std::vector<Glib::RefPtr<Gio::File>> filenames;
	parent = parent == nullptr ? MAIN->getWindow() : parent;
#ifdef G_OS_WIN32
	filenames = win32_open_dialog(title, initialDirectory, filter.to_win32_filter(), multiple, parent);
#else
	if(is_kde()) {
		filenames = kde_open_dialog(title, initialDirectory, filter.to_kde_filter(), multiple, parent);
	} else {
		filenames = gnome_open_dialog(title, initialDirectory, filter.to_gnome_filter(), multiple, parent);
	}
#endif
	return filenames;
}

std::string save_dialog(const Glib::ustring &title, const std::string& suggestedFile, const FileFilter& filter, Gtk::Window *parent)
{
	std::string filename;
	parent = parent == nullptr ? MAIN->getWindow() : parent;
#ifdef G_OS_WIN32
	filename = win32_save_dialog(title, suggestedFile, filter, parent);
#else
	if(is_kde()) {
		filename = kde_save_dialog(title, suggestedFile, filter, parent);
	} else {
		filename = gnome_save_dialog(title, suggestedFile, filter, parent);
	}
#endif
	if(!filename.empty()) {
		Utils::ensure_extension(filename, suggestedFile.substr(suggestedFile.length() - 4));
	}
	return filename;
}

} // FileDialogs

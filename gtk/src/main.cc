/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.cc
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

#include <gtkmm.h>
#include <gtkspellmm.h>
#include <gtksourceviewmm.h>
#include <iostream>
#include <cstring>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include "common.hh"
#include "Application.hh"
#include "Config.hh"
#include "CrashHandler.hh"

std::string pkgDir;
std::string pkgExePath;

static std::string get_application_dir(char* argv0) {
#ifdef G_OS_WIN32
	gchar* dir = g_win32_get_package_installation_directory_of_module(0);
	std::string pathstr = dir;
	g_free(dir);
#else
	pid_t pid = getpid();
	std::string exe = Glib::ustring::compose("/proc/%1/exe", pid);
	GError* err = nullptr;
	char* path = g_file_read_link(exe.c_str(), &err);
	std::string pathstr = Glib::build_filename(Glib::path_get_dirname(path), "..");
	g_free(path);
	if(err) {
		if(Glib::path_is_absolute(argv0)) {
			pathstr = Glib::build_filename(Glib::path_get_dirname(argv0), "..");
		} else {
			pathstr = Glib::build_filename(Glib::get_current_dir(), Glib::path_get_dirname(argv0), "..");
		}
	}
#endif
	return pathstr;
}

static std::string get_application_exec_path(char* argv0) {
#ifdef G_OS_WIN32
	char buf[MAX_PATH];
	bool success = GetModuleFileName(0, buf, MAX_PATH) > 0;
	std::string pathstr = buf;
#else
	pid_t pid = getpid();
	std::string exe = Glib::ustring::compose("/proc/%1/exe", pid);
	GError* err = nullptr;
	char* path = g_file_read_link(exe.c_str(), &err);
	std::string pathstr = path;
	g_free(path);
	bool success = err == nullptr;
#endif
	if(!success) {
		if(Glib::path_is_absolute(argv0)) {
			pathstr = Glib::path_get_dirname(argv0);
		} else {
			pathstr = Glib::build_filename(Glib::get_current_dir(), argv0);
		}
	}
	return pathstr;
}

int main (int argc, char* argv[]) {
	pkgDir = get_application_dir(argv[0]);
	pkgExePath = get_application_exec_path(argv[0]);

#ifdef G_OS_WIN32
	if(Glib::getenv("LANG").empty()) {
		gchar* locale = g_win32_getlocale();
		Glib::setenv("LANG", locale);
		g_free(locale);
	}
#endif

	std::string localeDir = Glib::build_filename(pkgDir, "share", "locale");

	bindtextdomain(GETTEXT_PACKAGE, localeDir.c_str());
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	if(argc > 2 && std::strcmp("crashhandle", argv[1]) == 0) {
		// Run the crash handler
		CrashHandler app(argc, argv);
		return app.run();
	} else if(argc >= 2 && std::strcmp("tessdatadir", argv[1]) == 0) {
		Config::openTessdataDir();
		return 0;
	} else if(argc >= 2 && std::strcmp("spellingdir", argv[1]) == 0) {
		Config::openSpellingDir();
		return 0;
	} else {
		// Run the normal application

#ifdef G_OS_WIN32
		Glib::setenv("TWAINDSM_LOG", Glib::build_filename(pkgDir, "twain.log"));
		std::freopen(Glib::build_filename(pkgDir, "gimagereader.log").c_str(), "w", stderr);
#endif

		GtkSpell::init();
		Gsv::init();
		Application app(argc, argv, APPLICATION_ID, Gio::APPLICATION_HANDLES_OPEN);
		return app.run();
	}
}


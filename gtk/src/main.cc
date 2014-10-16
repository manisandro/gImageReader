/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.cc
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

#include <gtkmm.h>
#include <gtkspellmm.h>
#include <iostream>
#include <cstring>

#include "common.hh"
#include "Application.hh"
#include "CrashHandler.hh"

Glib::RefPtr<Gtk::Builder> Builder::builder;
std::string pkgExePath;
std::string pkgDataDir;

static std::string get_application_dir(char* argv0)
{
	pid_t pid = getpid();
	std::string exe = Glib::ustring::compose("/proc/%1/exe", pid);
	GError* err;
	char* path = g_file_read_link(exe.c_str(), &err);
	std::string pathstr = path;
	g_free(path);
	if(!err){
		return pathstr;
	}else{
		if(Glib::path_is_absolute(argv0)){
			return Glib::path_get_dirname(argv0);
		}else{
			return Glib::build_path("/", std::vector<std::string>{Glib::get_current_dir(), Glib::path_get_dirname(argv0)});
		}
	}
}

int main (int argc, char *argv[])
{
#ifdef G_OS_WIN32
	gchar* dir = g_win32_get_package_installation_directory_of_module(0);
	std::string dataDir = Glib::build_filename(dir, "share");
	pkgExePath = Glib::build_path("/", std::vector<std::string>{"/", dir, "bin", Glib::path_get_basename(argv[0])});
	g_free(dir);
	if(Glib::getenv("LANG").empty()) {
		gchar* locale = g_win32_getlocale();
		Glib::setenv("LANG", locale);
		g_free(locale);
	}
#else
	std::string execDir = get_application_dir(argv[0]);
	pkgExePath = Glib::build_path("/", std::vector<std::string>{"/", execDir, Glib::path_get_basename(argv[0])});
	std::string dataDir = Glib::build_path("/", std::vector<std::string>{execDir, "..", "share"});
#endif

	pkgDataDir = Glib::build_path("/", std::vector<std::string>{dataDir, GETTEXT_PACKAGE});
	std::string localeDir = Glib::build_path("/", std::vector<std::string>{dataDir, "locale"});

	bindtextdomain(GETTEXT_PACKAGE, localeDir.c_str());
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	if(argc > 2 && std::strcmp("crashhandle", argv[1]) == 0) {
		// Run the crash handler

		CrashHandler app(argc, argv);

		try {
			Builder::builder = Gtk::Builder::create_from_file(Glib::build_filename(pkgDataDir, "crashhandler.ui"));
			Builder::builder->set_translation_domain(GETTEXT_PACKAGE);
		} catch (const Glib::Error & ex) {
			std::cerr << ex.what() << std::endl;
			return 1;
		}

		return app.run();
	} else {
		// Run the normal application

#ifdef G_OS_WIN32
		gchar* dir = g_win32_get_package_installation_directory_of_module(0);
		Glib::setenv("TESSDATA_PREFIX", Glib::build_filename(dir, "share"));
		Glib::setenv("TWAINDSM_LOG", Glib::build_filename(dir, "twain.log"));
		freopen(Glib::build_filename(dir, "gimagereader.log").c_str(), "w", stderr);
		g_free(dir);
#endif

		GtkSpell::init();
		Application app(argc, argv);

		try {
			Builder::builder = Gtk::Builder::create_from_file(Glib::build_filename(pkgDataDir, "gimagereader.ui"));
			Builder::builder->set_translation_domain(GETTEXT_PACKAGE);
		} catch (const Glib::Error & ex) {
			std::cerr << ex.what() << std::endl;
			return 1;
		}

		return app.run();
	}
}


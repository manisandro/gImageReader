/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.cc
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

#include <gtkmm.h>
#include <gtkspellmm.h>
#include <iostream>
#include <cstring>

#include "common.hh"
#include "MainWindow.hh"
#include "CrashHandler.hh"

Glib::RefPtr<Gtk::Builder> Builder::builder;

class Application : public Gtk::Application {
public:
	Application(int argc, char* argv[])
		: Gtk::Application(argc, argv, APPLICATION_ID, Gio::APPLICATION_HANDLES_OPEN)
	{
		Glib::set_application_name(PACKAGE_NAME);
	}
	~Application()
	{
		delete m_mainWindow;
	}
	void on_startup()
	{
		Gtk::Application::on_startup();
		m_mainWindow = new MainWindow;
		add_window(*m_mainWindow->getWindow());
	}
	void on_open(const type_vec_files& files, const Glib::ustring& /*hint*/)
	{
		if(m_mainWindow == nullptr){
			activate();
		}
		m_mainWindow->openFiles(files);
	}

private:
	MainWindow* m_mainWindow = nullptr;
};

int main (int argc, char *argv[])
{
	std::string dataDir;
	std::string localeDir;

#ifdef G_OS_WIN32
	gchar* dir = g_win32_get_package_installation_directory_of_module(0);
	dataDir = Glib::build_filename(dir, "share", PACKAGE);
	localeDir = Glib::build_filename(dir, "share", "locale");
	g_free(dir);
#else
	dataDir = PACKAGE_DATA_DIR;
	localeDir = PACKAGE_LOCALE_DIR;
#endif

	bindtextdomain(GETTEXT_PACKAGE, localeDir.c_str());
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	if(argc > 2 && std::strcmp("crashhandle", argv[1]) == 0) {
		// Run the crash handler

		CrashHandler app(argc, argv);

		try {
			Builder::builder = Gtk::Builder::create_from_file(Glib::build_filename(dataDir, "crashhandler.ui"));
			Builder::builder->set_translation_domain(GETTEXT_PACKAGE);
		} catch (const Glib::FileError & ex) {
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
			Builder::builder = Gtk::Builder::create_from_file(Glib::build_filename(dataDir, "gimagereader.ui"));
			Builder::builder->set_translation_domain(GETTEXT_PACKAGE);
		} catch (const Glib::FileError & ex) {
			std::cerr << ex.what() << std::endl;
			return 1;
		}

		return app.run();
	}
}


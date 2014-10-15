/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Application.hh
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

#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "common.hh"
#include "MainWindow.hh"

class Application : public Gtk::Application {
public:
	Application(int argc, char* argv[])
		: Gtk::Application(argc, argv, APPLICATION_ID, Gio::APPLICATION_HANDLES_OPEN)
	{
		Glib::set_application_name(PACKAGE_NAME);

		m_executablePath = argv[0];
		if(!Glib::path_is_absolute(m_executablePath)) {
			m_executablePath = Glib::build_filename(Glib::get_current_dir(), argv[0]);
		}
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
	const std::string& get_executable_path() const {
		return m_executablePath;
	}

private:
	MainWindow* m_mainWindow = nullptr;
	std::string m_executablePath;
};

#endif // APPLICATION_HPP

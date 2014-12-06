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
	Application(int &argc, char **&argv, const Glib::ustring &application_id, Gio::ApplicationFlags flags)
		: Gtk::Application(argc, argv, application_id, flags)
	{
		Glib::set_application_name(PACKAGE_NAME);
	}

private:
	void on_activate()
	{
		if(m_mainWindow){
			m_mainWindow->getWindow()->present();
		}
	}

	void on_startup()
	{
		Gtk::Application::on_startup();

		add_action("redetectLangs", [this]{ m_mainWindow->redetectLanguages(); });
		add_action("preferences", [this]{ m_mainWindow->showConfig(); });
		add_action("help", [this]{ m_mainWindow->showHelp(); });
		add_action("about", [this]{ m_mainWindow->showAbout(); });
		add_action("quit", [this]{ on_quit(); });

		Glib::RefPtr<Gtk::Builder> appMenuBuilder = Gtk::Builder::create_from_resource("/org/gnome/gimagereader/appmenu.ui");
		Glib::RefPtr<Gio::MenuModel> menuModel = Glib::RefPtr<Gio::MenuModel>::cast_static(appMenuBuilder->get_object("appmenu"));

		bool appMenu = false;
#if GTK_CHECK_VERSION(3,14,0)
		appMenu = prefers_app_menu();
#endif
		if(appMenu){
			set_app_menu(menuModel);
		}

		g_assert(m_mainWindow == nullptr);
		m_mainWindow = new MainWindow();
		CONNECT(m_mainWindow->getWindow(), hide, [this]{ on_quit(); });
		if(!appMenu){
			m_mainWindow->setMenuModel(menuModel);
		}
		add_window(*m_mainWindow->getWindow());
		m_mainWindow->getWindow()->show();
	}

	void on_open(const type_vec_files& files, const Glib::ustring& /*hint*/)
	{
		m_mainWindow->openFiles(files);
		m_mainWindow->getWindow()->present();
	}

	void on_quit(){
		if(m_mainWindow){
			remove_window(*m_mainWindow->getWindow());
			delete m_mainWindow;
			m_mainWindow = nullptr;
		}
	}

	MainWindow* m_mainWindow = nullptr;
};

#endif // APPLICATION_HPP

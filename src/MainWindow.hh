/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.hh
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

#ifndef MAINWINDOW_HH
#define MAINWINDOW_HH

#include "common.hh"

#define MAIN MainWindow::getInstance()

class Config;
class Acquirer;
class Displayer;
class OutputManager;
class Recognizer;
class SourceManager;

class MainWindow {
public:
	enum class State { Idle, Normal, Busy };

	static MainWindow* getInstance(){ return s_instance; }

	MainWindow();
	~MainWindow();

	void openFiles(const std::vector<Glib::RefPtr<Gio::File>>& files);
	void pushState(State state, const Glib::ustring& msg);
	void popState();

	Config* getConfig(){ return m_config; }
	Displayer* getDisplayer(){ return m_displayer; }
	OutputManager* getOutputManager(){ return m_outputManager; }
	Recognizer* getRecognizer(){ return m_recognizer; }
	SourceManager* getSourceManager(){ return m_sourceManager; }
	Gtk::Window* getWindow() const{ return m_window; }
	void showHelp();

private:
	static MainWindow* s_instance;

	Gtk::Window* m_window;
	Gtk::Menu* m_menu;
	Gtk::AboutDialog* m_aboutdialog;
	Gtk::Statusbar* m_statusbar;
	Gtk::MenuButton m_menubutton;

	Config* m_config;
	Acquirer* m_acquirer;
	Displayer* m_displayer;
	OutputManager* m_outputManager;
	Recognizer* m_recognizer;
	SourceManager* m_sourceManager;

	Glib::Threads::Thread* m_newVerThread = nullptr;

	std::vector<Gtk::Widget*> m_idlegroup;
	std::vector<State> m_stateStack;

	void setState(State state);
	bool quit(GdkEventAny*);
	void showAbout();
	void setOutputPaneOrientation(Gtk::ComboBoxText* combo);
	void getNewestVersion();
	void checkVersion(const Glib::ustring& newver);
};

#endif

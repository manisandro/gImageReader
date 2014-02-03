/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * CrashHandler.cc
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

#ifndef CRASHHANDLER_HH
#define CRASHHANDLER_HH

#include "common.hh"

class CrashHandler : public Gtk::Application {
public:
	CrashHandler(int argc, char* argv[]);
	void on_startup();

private:
	Gtk::Dialog* m_dialog;
	Gtk::ProgressBar* m_progressBar;
	Gtk::Button* m_refreshButton;
	Gtk::TextView* m_textview;
	sigc::connection m_progressConnection;
	std::string m_saveFile;
	int m_pid = 0;

	void generate_backtrace();
	void generate_backtrace_end(bool success);
	bool pulse_progress();
	bool handle_stdout(Glib::IOCondition cond, Glib::RefPtr<Glib::IOChannel> ch);
	static void handle_child_exit(GPid pid, gint status, void* data);
};

#endif // CRASH_HANDLER_HH

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * CrashHandler.cc
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

#include "CrashHandler.hh"

CrashHandler::CrashHandler(int argc, char* argv[])
	: Gtk::Application(argc, argv, APPLICATION_ID".crashhandler", Gio::APPLICATION_HANDLES_COMMAND_LINE)
{
	Glib::set_application_name(Glib::ustring::compose("%1 %2", PACKAGE_NAME, _("Crash Handler")));

	if(argc > 2) {
		m_pid = std::atoi(argv[2]);
	}
	if(argc > 3) {
		m_saveFile = argv[3];
	}
}

void CrashHandler::on_startup()
{
	Gtk::Application::on_startup();
	m_dialog = Builder("dialog");
	m_progressBar = Builder("progressbar");
	m_textview = Builder("textview");
	m_refreshButton = Builder("button:regenerate");
	m_dialog->set_title(Glib::ustring::compose("%1 %2", PACKAGE_NAME, _("Crash Handler")));
	if(!m_saveFile.empty()) {
		Builder("label:autosave").as<Gtk::Label>()->set_markup(Glib::ustring::compose("Your work has been saved under <b>%1</b>.", m_saveFile));
	}else{
		Builder("label:autosave").as<Gtk::Label>()->set_text("There was no usaved work.");
	}
	CONNECT(m_dialog, delete_event, [this](GdkEventAny* ev) { quit(); return true; });
	CONNECT(Builder("button:close").as<Gtk::Button>(), clicked, [this]{ quit(); });
	CONNECT(m_refreshButton, clicked, [this]{ generate_backtrace(); });

	add_window(*m_dialog);
	m_dialog->show_all();

	generate_backtrace();
}

void CrashHandler::generate_backtrace()
{
	m_progressConnection = Glib::signal_timeout().connect([this]{ return pulse_progress(); }, 200);
	m_refreshButton->set_sensitive(false);
	m_textview->set_sensitive(false);
	m_textview->get_buffer()->set_text(Glib::ustring::compose("%1 %2 (rev %3)\n\n", PACKAGE_NAME, PACKAGE_VERSION, PACKAGE_REVISION));
	m_progressBar->show();

	int child_pid;
	int child_stdin;
	int child_stdout;
	try {
		Glib::spawn_async_with_pipes("", std::vector<std::string>{"gdb", "-p", Glib::ustring::compose("%1", m_pid)}, Glib::SPAWN_DO_NOT_REAP_CHILD|Glib::SPAWN_SEARCH_PATH|Glib::SPAWN_STDERR_TO_DEV_NULL, sigc::slot<void>(), &child_pid, &child_stdin, &child_stdout);
	} catch(Glib::Error&) {
		generate_backtrace_end(false);
		return;
	}

	g_child_watch_add(child_pid, handle_child_exit, this);

	Glib::RefPtr<Glib::IOChannel> ch_out = Glib::IOChannel::create_from_fd(child_stdout);
	Glib::RefPtr<Glib::IOSource> src_out = ch_out->create_watch(Glib::IO_IN|Glib::IO_HUP);
	src_out->connect([this, ch_out](Glib::IOCondition cond){ return handle_stdout(cond, ch_out); });
	src_out->attach(Glib::MainContext::get_default());

	Glib::RefPtr<Glib::IOChannel> ch_in = Glib::IOChannel::create_from_fd(child_stdin);
	ch_in->write("set pagination off\n");
	ch_in->write("bt\n");
	ch_in->write("thread apply all bt full\n");
	ch_in->write("quit\n");
}

void CrashHandler::generate_backtrace_end(bool success)
{
	m_progressConnection.disconnect();
	m_progressBar->hide();
	m_textview->set_sensitive(true);
	m_refreshButton->set_sensitive(true);
	if(!success) {
		m_textview->get_buffer()->set_text(_("Failed to obtain backtrace. Is gdb installed?"));
	}
	auto begin = m_textview->get_buffer()->begin();
	m_textview->scroll_to(begin);
}

bool CrashHandler::pulse_progress()
{
	m_progressBar->pulse();
	return true;
}

bool CrashHandler::handle_stdout(Glib::IOCondition cond, Glib::RefPtr<Glib::IOChannel> ch)
{
	if(cond == Glib::IO_HUP){
		return false;
	}
	Glib::ustring text;
	ch->read_line(text);
	m_textview->get_buffer()->insert(m_textview->get_buffer()->end(), text);
	auto end = m_textview->get_buffer()->end();
	m_textview->scroll_to(end);
	return true;
}

void CrashHandler::handle_child_exit(GPid pid, gint status, void* data)
{
	CrashHandler* instance = reinterpret_cast<CrashHandler*>(data);
	bool success = g_spawn_check_exit_status(status, nullptr);
	instance->generate_backtrace_end(success);
	Glib::spawn_close_pid(pid);
}

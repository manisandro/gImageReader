/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * CrashHandler.cc
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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
#include "Utils.hh"

CrashHandler::CrashHandler(int argc, char* argv[])
	: Gtk::Application(argc, argv, APPLICATION_ID ".crashhandler", Gio::APPLICATION_HANDLES_COMMAND_LINE) {
	Glib::set_application_name(Glib::ustring::compose("%1 - %2", PACKAGE_NAME, _("Crash Handler")));

	if(argc > 2) {
		m_pid = std::atoi(argv[2]);
	}
	if(argc > 3) {
		m_tesseractCrash = std::atoi(argv[3]);
	}
	if(argc > 4) {
		m_saveFile = argv[4];
	}
}

void CrashHandler::on_startup() {
	Gtk::Application::on_startup();
	ui.setupUi();
	ui.dialogCrashhandler->set_title(Glib::ustring::compose("%1 %2", PACKAGE_NAME, _("Crash Handler")));
	if(!m_saveFile.empty()) {
		ui.labelAutosave->set_markup(Glib::ustring::compose(_("Your work has been saved under <b>%1</b>."), m_saveFile));
	} else {
		ui.labelAutosave->set_text(_("There was no unsaved work."));
	}
	CONNECT(ui.dialogCrashhandler, delete_event, [this](GdkEventAny* /*ev*/) {
		quit();
		return true;
	});
	CONNECT(ui.buttonClose, clicked, [this] { quit(); });
	CONNECT(ui.buttonRegenerate, clicked, [this] { generate_backtrace(); });

	add_window(*ui.dialogCrashhandler);
	ui.dialogCrashhandler->show_all();
	ui.labelIntro->set_visible(!m_tesseractCrash);
	ui.labelIntroTesseract->set_visible(m_tesseractCrash);

#ifndef G_OS_WIN32
	generate_backtrace();
#else
	ui.buttonRegenerate->hide();
	ui.progressbar->hide();
#endif
}

void CrashHandler::generate_backtrace() {
	m_progressConnection = Glib::signal_timeout().connect([this] { return pulse_progress(); }, 200);
	ui.buttonRegenerate->set_sensitive(false);
	ui.textview->set_sensitive(false);
	ui.textview->get_buffer()->set_text(Glib::ustring::compose("%1 %2 (rev %3)\n\n", PACKAGE_NAME, PACKAGE_VERSION, PACKAGE_REVISION));
	ui.progressbar->show();

	GPid child_pid;
	int child_stdin;
	int child_stdout;
	try {
		Glib::spawn_async_with_pipes("", std::vector<std::string> {"gdb", "-q", "-p", Glib::ustring::compose("%1", m_pid)}, Glib::SPAWN_DO_NOT_REAP_CHILD | Glib::SPAWN_SEARCH_PATH | Glib::SPAWN_STDERR_TO_DEV_NULL, sigc::slot<void>(), &child_pid, &child_stdin, &child_stdout);
	} catch(Glib::Error&) {
		generate_backtrace_end(false);
		return;
	}

	g_child_watch_add(child_pid, handle_child_exit, this);

	Glib::RefPtr<Glib::IOChannel> ch_out = Glib::IOChannel::create_from_fd(child_stdout);
	Glib::RefPtr<Glib::IOSource> src_out = ch_out->create_watch(Glib::IO_IN | Glib::IO_HUP);
	src_out->connect([this, ch_out](Glib::IOCondition cond) {
		return handle_stdout(cond, ch_out);
	});
	src_out->attach(Glib::MainContext::get_default());

	Glib::RefPtr<Glib::IOChannel> ch_in = Glib::IOChannel::create_from_fd(child_stdin);
	ch_in->write("set pagination off\n");
	ch_in->write("bt\n");
	ch_in->write("thread apply all bt full\n");
	ch_in->write("quit\n");
}

void CrashHandler::generate_backtrace_end(bool success) {
	m_progressConnection.disconnect();
	ui.progressbar->hide();
	ui.textview->set_sensitive(true);
	ui.buttonRegenerate->set_sensitive(true);
	if(!success) {
		ui.textview->get_buffer()->set_text(_("Failed to obtain backtrace. Is GDB installed?"));
	} else {
		std::vector<Glib::ustring> lines = Utils::string_split(ui.textview->get_buffer()->get_text(false), '\n', false);
		Glib::ustring text = Glib::ustring::compose("%1\n\n", lines[0]);
		for(int i = 1, n = lines.size(); i < n; ++i) {
			if(lines[i].substr(0, 6) == "Thread") {
				text += Glib::ustring::compose("\n%1\n", lines[i]);
			} else if(lines[i].substr(0, 1) == "#") {
				text += Glib::ustring::compose("%1\n", lines[i]);
			}
		}
		ui.textview->get_buffer()->set_text(text);
	}
	auto begin = ui.textview->get_buffer()->begin();
	ui.textview->scroll_to(begin);
}

bool CrashHandler::pulse_progress() {
	ui.progressbar->pulse();
	return true;
}

bool CrashHandler::handle_stdout(Glib::IOCondition cond, Glib::RefPtr<Glib::IOChannel> ch) {
	if(cond == Glib::IO_HUP) {
		return false;
	}
	Glib::ustring text;
	ch->read_line(text);
	ui.textview->get_buffer()->insert(ui.textview->get_buffer()->end(), text);
	auto end = ui.textview->get_buffer()->end();
	ui.textview->scroll_to(end);
	return true;
}

void CrashHandler::handle_child_exit(GPid pid, gint status, void* data) {
	CrashHandler* instance = reinterpret_cast<CrashHandler*>(data);
	bool success = g_spawn_check_exit_status(status, nullptr);
	instance->generate_backtrace_end(success);
	Glib::spawn_close_pid(pid);
}

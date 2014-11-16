/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.cc
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

#include "Utils.hh"
#include "MainWindow.hh"

#include <clocale>
#include <tesseract/baseapi.h>

void Utils::popup_positioner(int& x, int& y, bool& push_in, Gtk::Widget* ref, Gtk::Menu* menu, bool alignRight, bool alignBottom)
{
	ref->get_window()->get_origin(x, y);
	x += ref->get_allocation().get_x();
	if(alignRight){
		x += ref->get_allocation().get_width();
		x -= menu->get_width();
	}
	y += ref->get_allocation().get_y();
	if(alignBottom){
		y += ref->get_allocation().get_height();
	}
	push_in = true;
}

void Utils::message_dialog(Gtk::MessageType message, const Glib::ustring &title, const Glib::ustring &text, Gtk::Window *parent)
{
	if(!parent){ parent = MAIN->getWindow(); }
	Gtk::MessageDialog dialog(*parent, title, false, message, Gtk::BUTTONS_OK, true);
	dialog.set_secondary_text(text);
	dialog.run();
}

int Utils::question_dialog(const Glib::ustring &title, const Glib::ustring &text, Gtk::Window *parent)
{
	if(!parent){ parent = MAIN->getWindow(); }
	Gtk::MessageDialog dialog(*parent, title, false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
	dialog.set_secondary_text(text);
	int response = dialog.run();
	if(response == Gtk::RESPONSE_NO){
		return 0;
	}else if(response == Gtk::RESPONSE_YES){
		return 1;
	}else{
		return 2;
	}
}

void Utils::set_spin_blocked(Gtk::SpinButton *spin, double value, sigc::connection &conn)
{
	conn.block(true);
	spin->set_value(value);
	conn.block(false);
}

static Glib::RefPtr<Gtk::CssProvider> createErrorStyleProvider()
{
	Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
	provider->load_from_data("GtkEntry { background: #FF7777; color: #FFFFFF; }");
	return provider;
}

static Glib::RefPtr<Gtk::CssProvider> getErrorStyleProvider()
{
	static Glib::RefPtr<Gtk::CssProvider> provider = createErrorStyleProvider();
	return provider;
}

void Utils::set_error_state(Gtk::Entry *entry)
{
	entry->get_style_context()->add_provider(getErrorStyleProvider(), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void Utils::clear_error_state(Gtk::Entry *entry)
{
	entry->get_style_context()->remove_provider(getErrorStyleProvider());
}

Glib::ustring Utils::get_content_type(const std::string &filename)
{
	gboolean uncertain;
	gchar* type = g_content_type_guess(filename.c_str(), 0, 0, &uncertain);
	Glib::ustring contenttype(type);
	g_free(type);
	return contenttype;
}

void Utils::get_filename_parts(const std::string& filename, std::string& base, std::string& ext)
{
	std::string::size_type pos = filename.rfind('.');
	if(pos == std::string::npos){
		base = filename;
		ext = "";
		return;
	}
	base = filename.substr(0, pos);
	ext = filename.substr(pos + 1);
	if(base.size() > 3 && base.substr(pos - 4) == ".tar"){
		base = base.substr(0, pos - 4);
		ext = "tar." + ext;
	}
}

void Utils::ensure_extension(std::string &filename, const std::string &ext)
{
	std::string fext = filename.substr(filename.length() - 4);
	std::transform(fext.begin(), fext.end(), fext.begin(), ::tolower);
	if(fext != ext){
		filename += ext;
	}
}

std::string Utils::make_absolute_path(const std::string& path)
{
	if(Glib::path_is_absolute(path)){
		return path;
	}
	return Glib::build_path("/", std::vector<std::string>{Glib::get_current_dir(), path});
}

std::string Utils::get_documents_dir()
{
	std::string dir = Glib::get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
	if(Glib::file_test(dir, Glib::FILE_TEST_IS_DIR)) {
		return dir;
	}
	return Glib::get_home_dir();
}

std::string Utils::make_output_filename(const std::string& filename)
{
	// Ensure directory exists
	std::string dirname = Glib::path_get_dirname(filename);
	std::string basename = Glib::path_get_basename(filename);
	if(!Glib::file_test(dirname, Glib::FILE_TEST_IS_DIR)){
		dirname = get_documents_dir();
	}
	std::string newfilename = Glib::build_filename(dirname, basename);
	// Generate non-existing file
	int i = 0;
	std::string base, ext;
	Utils::get_filename_parts(newfilename, base, ext);
	base = Glib::Regex::create("_[0-9]+$")->replace(base, 0, "", static_cast<Glib::RegexMatchFlags>(0));
	newfilename = Glib::ustring::compose("%1.%2", base, ext);
	while(Glib::file_test(newfilename, Glib::FILE_TEST_EXISTS)){
		newfilename = Glib::ustring::compose("%1_%2.%3", base, ++i, ext);
	}
	return newfilename;
}

std::vector<Glib::ustring> Utils::string_split(const Glib::ustring &text, char delim, bool keepEmpty)
{
	std::vector<Glib::ustring> parts;
	Glib::ustring::size_type startPos = 0, endPos = 0;
	Glib::ustring::size_type npos = Glib::ustring::npos;
	while(true){
		startPos = endPos;
		endPos = text.find(delim, startPos);
		Glib::ustring::size_type n = endPos == npos ? npos : endPos - startPos;
		if(n > 0 || keepEmpty){
			parts.push_back(text.substr(startPos, n));
		}
		if(endPos == npos) break;
		++endPos;
	}
	return parts;
}

bool Utils::busyTask(const std::function<bool()> &f, const Glib::ustring &msg)
{
	enum class TaskState { Waiting, Succeeded, Failed };
	TaskState taskState = TaskState::Waiting;
	Glib::Threads::Mutex mutex;
	MAIN->pushState(MainWindow::State::Busy, msg);

	Glib::Threads::Thread* thread = Glib::Threads::Thread::create([&]{
		bool success = f();
		mutex.lock();
		taskState = success ? TaskState::Succeeded : TaskState::Failed;
		mutex.unlock();
	});
	mutex.lock();
	while(taskState  == TaskState::Waiting){
		mutex.unlock();
		Gtk::Main::iteration(false);
		mutex.lock();
	}
	mutex.unlock();
	thread->join();
	MAIN->popState();
	return taskState  == TaskState::Succeeded;
}

bool Utils::initTess(tesseract::TessBaseAPI& tess, const char* datapath, const char* language)
{
	std::string current = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	int ret = tess.Init(datapath, language);
	setlocale(LC_NUMERIC, current.c_str());
	return ret != -1;
}

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.cc
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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
#include "Config.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"

#include <clocale>
#include <fontconfig/fontconfig.h>
#include <tesseract/baseapi.h>

void Utils::popup_positioner(int& x, int& y, bool& push_in, Gtk::Widget* ref, Gtk::Menu* menu, bool alignRight, bool alignBottom) {
	ref->get_window()->get_origin(x, y);
	x += ref->get_allocation().get_x();
	if(alignRight) {
		x += ref->get_allocation().get_width();
		x -= menu->get_width();
	}
	y += ref->get_allocation().get_y();
	if(alignBottom) {
		y += ref->get_allocation().get_height();
	}
	push_in = true;
}

void Utils::message_dialog(Gtk::MessageType message, const Glib::ustring &title, const Glib::ustring &text, Gtk::Window *parent) {
	if(!parent) {
		parent = MAIN->getWindow();
	}
	Gtk::MessageDialog dialog(*parent, title, false, message, Gtk::BUTTONS_OK, true);
	dialog.set_secondary_text(text);
	dialog.run();
}

Utils::Button::Type Utils::question_dialog(const Glib::ustring &title, const Glib::ustring &text, int buttons, Gtk::Window *parent) {
	if(!parent) {
		parent = MAIN->getWindow();
	}
	Gtk::MessageDialog dialog(*parent, title, false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE, true);
	dialog.set_position(Gtk::WIN_POS_CENTER_ON_PARENT);
	if((buttons & Button::Ok) != 0) {
		dialog.add_button(_("OK"), Button::Type::Ok);
	}
	if((buttons & Button::Yes) != 0) {
		dialog.add_button(_("Yes"), Button::Type::Yes);
	}
	if((buttons & Button::No) != 0) {
		dialog.add_button(_("No"), Button::Type::No);
	}
	if((buttons & Button::Cancel) != 0) {
		dialog.add_button(_("Cancel"), Button::Type::Cancel);
	}
	if((buttons & Button::Save) != 0) {
		dialog.add_button(_("Save"), Button::Type::Save);
	}
	if((buttons & Button::Discard) != 0) {
		dialog.add_button(_("Discard"), Button::Type::Discard);
	}
	dialog.set_secondary_text(text);
	return static_cast<Button::Type>(dialog.run());
}

void Utils::set_spin_blocked(Gtk::SpinButton *spin, double value, sigc::connection &conn) {
	conn.block(true);
	spin->set_value(value);
	conn.block(false);
}

static Glib::RefPtr<Gtk::CssProvider> createErrorStyleProvider() {
	Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
	provider->load_from_data("GtkEntry { background: #FF7777; color: #FFFFFF; }");
	return provider;
}

static Glib::RefPtr<Gtk::CssProvider> getErrorStyleProvider() {
	static Glib::RefPtr<Gtk::CssProvider> provider = createErrorStyleProvider();
	return provider;
}

void Utils::set_error_state(Gtk::Entry *entry) {
	entry->get_style_context()->add_provider(getErrorStyleProvider(), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void Utils::clear_error_state(Gtk::Entry *entry) {
	entry->get_style_context()->remove_provider(getErrorStyleProvider());
}

Glib::ustring Utils::get_content_type(const std::string &filename) {
	gboolean uncertain;
	gchar* type = g_content_type_guess(filename.c_str(), 0, 0, &uncertain);
	Glib::ustring contenttype(type);
	g_free(type);
	return contenttype;
}

void Utils::get_filename_parts(const std::string& filename, std::string& base, std::string& ext) {
	std::string::size_type pos = filename.rfind('.');
	if(pos == std::string::npos) {
		base = filename;
		ext = "";
		return;
	}
	base = filename.substr(0, pos);
	ext = filename.substr(pos + 1);
	if(base.size() > 3 && base.substr(pos - 4) == ".tar") {
		base = base.substr(0, pos - 4);
		ext = "tar." + ext;
	}
}

std::string Utils::make_absolute_path(const std::string& path) {
	if(Glib::path_is_absolute(path)) {
		return path;
	}
	return Glib::build_path("/", std::vector<std::string> {Glib::get_current_dir(), path});
}

std::string Utils::get_documents_dir() {
	std::string dir = Glib::get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
	if(Glib::file_test(dir, Glib::FILE_TEST_IS_DIR)) {
		return dir;
	}
	return Glib::get_home_dir();
}

std::string Utils::make_output_filename(const std::string& filename) {
	// Ensure directory exists
	std::string dirname = Glib::path_get_dirname(filename);
	std::string basename = Glib::path_get_basename(filename);
	if(!Glib::file_test(dirname, Glib::FILE_TEST_IS_DIR)) {
		dirname = get_documents_dir();
	}
	std::string newfilename = Glib::build_filename(dirname, basename);
	// Generate non-existing file
	int i = 0;
	std::string base, ext;
	Utils::get_filename_parts(newfilename, base, ext);
	base = Glib::Regex::create("_[0-9]+$")->replace(base, 0, "", static_cast<Glib::RegexMatchFlags>(0));
	newfilename = Glib::ustring::compose("%1.%2", base, ext);
	while(Glib::file_test(newfilename, Glib::FILE_TEST_EXISTS)) {
		newfilename = Glib::ustring::compose("%1_%2.%3", base, ++i, ext);
	}
	return newfilename;
}

std::vector<Glib::ustring> Utils::string_split(const Glib::ustring &text, char delim, bool keepEmpty) {
	std::vector<Glib::ustring> parts;
	Glib::ustring::size_type startPos = 0, endPos = 0;
	Glib::ustring::size_type npos = Glib::ustring::npos;
	while(true) {
		startPos = endPos;
		endPos = text.find(delim, startPos);
		Glib::ustring::size_type n = endPos == npos ? npos : endPos - startPos;
		if(n > 0 || keepEmpty) {
			parts.push_back(text.substr(startPos, n));
		}
		if(endPos == npos) break;
		++endPos;
	}
	return parts;
}

Glib::ustring Utils::string_join(const std::vector<Glib::ustring>& strings, const Glib::ustring& joiner ) {
	Glib::ustring result;
	if(!strings.empty()) {
		result = strings.front();
		for(std::size_t i = 1, n = strings.size(); i < n; ++i) {
			result += joiner + strings[i];
		}
	}
	return result;
}

Glib::ustring Utils::string_trim(const Glib::ustring &str) {
	Glib::ustring ret = str;
	ret.erase(0, ret.find_first_not_of(' '));
	std::size_t rpos = ret.find_last_not_of(' ');
	if(rpos != Glib::ustring::npos) {
		ret.erase(rpos + 1);
	}
	return ret;
}

void Utils::handle_drag_drop(const Glib::RefPtr<Gdk::DragContext> &context, int /*x*/, int /*y*/, const Gtk::SelectionData &selection_data, guint /*info*/, guint time) {
	if ((selection_data.get_length() >= 0) && (selection_data.get_format() == 8)) {
		std::vector<Glib::RefPtr<Gio::File>> files;
		for(const Glib::ustring& uri : selection_data.get_uris()) {
			files.push_back(Gio::File::create_for_uri(uri));
		}
		if(!files.empty()) {
			MAIN->getSourceManager()->addSources(files);
		}
	}
	context->drag_finish(false, false, time);
}

Glib::RefPtr<Glib::ByteArray> Utils::download(const std::string &url, Glib::ustring& messages, unsigned timeout) {
	enum Status { Waiting, Ready, Failed, Eos } status = Waiting;

	Glib::RefPtr<Glib::ByteArray> result = Glib::ByteArray::create();
	Glib::RefPtr<Gio::FileInputStream> stream;
	Glib::RefPtr<Gio::Cancellable> cancellable = Gio::Cancellable::create();
	sigc::connection timeoutConnection;

	try {
		Glib::RefPtr<Gio::File> file = Gio::File::create_for_uri(url);
		file->read_async([&](Glib::RefPtr<Gio::AsyncResult>& asyncResult) {
			try {
				stream = file->read_finish(asyncResult);
				status = stream ? Ready : Failed;
			} catch(Glib::Error&) {
				status = Failed;
			}
		}, cancellable);
		timeoutConnection = Glib::signal_timeout().connect([&] { cancellable->cancel(); return false; }, timeout);

		while(status == Waiting) {
			Gtk::Main::iteration();
		}
		timeoutConnection.disconnect();
		if(status == Failed) {
			return Glib::RefPtr<Glib::ByteArray>();
		}

		while(true) {
			status = Waiting;
			stream->read_bytes_async(4096, [&](Glib::RefPtr<Gio::AsyncResult>& asyncResult) {
				try {
					Glib::RefPtr<Glib::Bytes> bytes = stream->read_bytes_finish(asyncResult);
					if(!bytes) {
						status = Eos;
					}
					gsize size;
					result->append(reinterpret_cast<const guint8*>(bytes->get_data(size)), bytes->get_size());
					status = size == 0 ? Eos : Ready;
				} catch(Glib::Error&) {
					status = Failed;
				}
			}, cancellable);
			timeoutConnection = Glib::signal_timeout().connect([&] { cancellable->cancel(); return false; }, timeout);

			while(status == Waiting) {
				Gtk::Main::iteration();
			}
			timeoutConnection.disconnect();
			if(status == Failed) {
				return Glib::RefPtr<Glib::ByteArray>();
			} else if(status == Eos) {
				break;
			}
		}
	} catch (const Glib::Error& e) {
		messages = e.what();
		return Glib::RefPtr<Glib::ByteArray>();
	}
	return result;
}


Glib::ustring Utils::getSpellingLanguage(const Glib::ustring& lang) {
	// Look in the lang cultures table if a language hint is provided
	Config::Lang langspec = {lang};
	if(!lang.empty() && MAIN->getConfig()->searchLangSpec(langspec)) {
		std::vector<Glib::ustring> langCultures = MAIN->getConfig()->searchLangCultures(langspec.code);
		if(!langCultures.empty()) {
			return langCultures.front();
		}
	}
	// Use the application locale, if specified, otherwise fall back to en
	Glib::ustring syslocale = g_getenv ("LANG");
	if(syslocale == "c" || syslocale == "C" || syslocale.empty()) {
		return "en_US";
	}
	return syslocale;
}

Glib::ustring Utils::resolveFontName(const Glib::ustring& family) {
	Glib::ustring resolvedName = family;

	FcPattern *pat = FcNameParse(reinterpret_cast<const FcChar8*>(family.c_str()));
	FcConfigSubstitute (0, pat, FcMatchPattern);
	FcFontSet *fs = FcFontSetCreate();

	FcResult result;
	FcPattern *match = FcFontMatch (0, pat, &result);
	if (match)
		FcFontSetAdd (fs, match);
	FcPatternDestroy (pat);

	FcObjectSet *os = 0;
	if(fs->nfont > 0) {
		FcPattern *font = FcPatternFilter (fs->fonts[0], os);
		FcChar8 *s = FcPatternFormat (font, reinterpret_cast<const FcChar8*>("%{family}"));
		resolvedName = Glib::ustring(reinterpret_cast<const char*>(s));
		FcStrFree (s);
		FcPatternDestroy (font);
	}
	FcFontSetDestroy (fs);

	if (os)
		FcObjectSetDestroy (os);
	return resolvedName;
}

void Utils::openUri(const std::string& uri) {
#ifdef G_OS_WIN32
	ShellExecute(nullptr, "open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
	gtk_show_uri(nullptr, uri.c_str(), GDK_CURRENT_TIME, 0);
#endif
}

bool Utils::busyTask(const std::function<bool()> &f, const Glib::ustring &msg) {
	enum class TaskState { Waiting, Succeeded, Failed };
	TaskState taskState = TaskState::Waiting;
	Glib::Threads::Mutex mutex;
	MAIN->pushState(MainWindow::State::Busy, msg);

	Glib::Threads::Thread* thread = Glib::Threads::Thread::create([&] {
		bool success = f();
		mutex.lock();
		taskState = success ? TaskState::Succeeded : TaskState::Failed;
		mutex.unlock();
	});
	mutex.lock();
	while(taskState  == TaskState::Waiting) {
		mutex.unlock();
		Gtk::Main::iteration(false);
		mutex.lock();
	}
	mutex.unlock();
	thread->join();
	MAIN->popState();
	return taskState  == TaskState::Succeeded;
}

void Utils::runInMainThreadBlocking(const std::function<void()>& f) {
	Glib::Threads::Mutex mutex;
	Glib::Threads::Cond cond;
	bool finished = false;
	Glib::signal_idle().connect_once([&] {
		f();
		mutex.lock();
		finished = true;
		cond.signal();
		mutex.unlock();
	});
	mutex.lock();
	while(!finished) {
		cond.wait(mutex);
	}
	mutex.unlock();
}

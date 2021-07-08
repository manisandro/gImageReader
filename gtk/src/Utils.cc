/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.cc
 * Copyright (C) 2013-2020 Sandro Mani <manisandro@gmail.com>
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
#include <csignal>
#include <fontconfig/fontconfig.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

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

Utils::Button::Type Utils::messageBox(Gtk::MessageType type, const Glib::ustring& title, const Glib::ustring& text, const Glib::ustring& body, int buttons, Gtk::Window* parent) {
	if(!parent) {
		parent = MAIN->getWindow();
	}

	Gtk::Dialog dialog;
	dialog.set_transient_for(*parent);
	dialog.set_title(title);
	dialog.get_content_area()->set_orientation(Gtk::ORIENTATION_VERTICAL);
	dialog.get_content_area()->set_spacing(6);
	dialog.get_content_area()->set_margin_left(6);
	dialog.get_content_area()->set_margin_right(6);
	dialog.get_content_area()->set_margin_top(6);
	dialog.get_content_area()->set_margin_bottom(6);

	Gtk::Grid grid;
	dialog.get_content_area()->pack_start(grid, true, true);

	Glib::ustring iconName;
	switch (type) {
	case Gtk::MESSAGE_INFO:
		iconName = "dialog-information-symbolic";
		break;
	case Gtk::MESSAGE_WARNING:
		iconName = "dialog-warning-symbolic";
		break;
	case Gtk::MESSAGE_QUESTION:
		iconName = "dialog-question-symbolic";
		break;
	case Gtk::MESSAGE_ERROR:
		iconName = "dialog-error-symbolic";
		break;
	case Gtk::MESSAGE_OTHER:
		break;
	}
	Gtk::Image icon(iconName, Gtk::ICON_SIZE_DIALOG);
	grid.attach(icon, 0, 0, 1, body.empty() ? 1 : 2);


	Gtk::Label label(text);
	label.set_xalign(0);
	grid.attach(label, 1, 0);
	grid.set_column_spacing(6);
	grid.set_row_spacing(6);

	Gtk::ScrolledWindow scrollArea;
	Gtk::TextView textView;
	scrollArea.add(textView);
	if(!body.empty()) {
		textView.set_editable(false);
		Glib::RefPtr<Gtk::TextBuffer> buffer = Gtk::TextBuffer::create();
		buffer->set_text(body);
		textView.set_buffer(buffer);
		scrollArea.set_hexpand(true);
		scrollArea.set_vexpand(true);
		scrollArea.set_shadow_type(Gtk::SHADOW_IN);
		scrollArea.set_min_content_width(320);
		scrollArea.set_min_content_height(160);
		grid.attach(scrollArea, 1, 1);
	}

	if((buttons & Button::Ok) != 0) {
		dialog.add_button(_("OK"), Button::Type::Ok);
	}
	if((buttons & Button::Yes) != 0) {
		dialog.add_button(_("Yes"), Button::Type::Yes);
	}
	if((buttons & Button::YesAll) != 0) {
		dialog.add_button(_("Yes to all"), Button::Type::YesAll);
	}
	if((buttons & Button::No) != 0) {
		dialog.add_button(_("No"), Button::Type::No);
	}
	if((buttons & Button::NoAll) != 0) {
		dialog.add_button(_("No to all"), Button::Type::NoAll);
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
	if((buttons & Button::Text) != 0) {
		dialog.add_button(_("Text"), Button::Type::Text);
	}
	if((buttons & Button::HOCR) != 0) {
		dialog.add_button(_("hOCR"), Button::Type::HOCR);
	}

	dialog.show_all();
	return static_cast<Button::Type>(dialog.run());
}

void Utils::set_spin_blocked(Gtk::SpinButton* spin, double value, sigc::connection& conn) {
	conn.block(true);
	spin->set_value(value);
	conn.block(false);
}

static Glib::RefPtr<Gtk::CssProvider> createErrorStyleProvider() {
	Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
	provider->load_from_data(".error { background-color: #FF7777; color: #FFFFFF; }");
	return provider;
}

static Glib::RefPtr<Gtk::CssProvider> getErrorStyleProvider() {
	static Glib::RefPtr<Gtk::CssProvider> provider = createErrorStyleProvider();
	return provider;
}

void Utils::set_error_state(Gtk::Entry* entry) {
	entry->get_style_context()->add_provider(getErrorStyleProvider(), 10000);
	entry->get_style_context()->add_class("error");
}

void Utils::clear_error_state(Gtk::Entry* entry) {
	entry->get_style_context()->remove_class("error");
	entry->get_style_context()->remove_provider(getErrorStyleProvider());
}

Glib::ustring Utils::get_content_type(const std::string& filename) {
	gboolean uncertain;
	gchar* type = g_content_type_guess(filename.c_str(), 0, 0, &uncertain);
	Glib::ustring contenttype(type);
	g_free(type);
	return contenttype;
}

std::pair<std::string, std::string> Utils::split_filename(const std::string& filename) {
	std::string::size_type pos = filename.rfind('.');
	if(pos == std::string::npos) {
		return std::make_pair(filename, std::string());
	}
	std::pair<std::string, std::string> parts = std::make_pair(filename.substr(0, pos), filename.substr(pos + 1));
	if(parts.first.size() > 3 && parts.first.substr(pos - 4) == ".tar") {
		parts.first = parts.first.substr(0, pos - 4);
		parts.second = "tar." + parts.second;
	}
	return parts;
}

std::string Utils::make_absolute_path(const std::string& path, const std::string& basepath) {
	if(Glib::path_is_absolute(path)) {
		return path;
	}
	std::string abspath = Glib::build_filename(basepath, path);
#ifdef G_OS_UNIX
	char* realabspath = realpath(abspath.c_str(), nullptr);
	abspath = std::string(realabspath);
	free(realabspath);
#endif
	return abspath;
}

std::string Utils::make_relative_path(const std::string& path, const std::string& basepath) {
	if(!Glib::path_is_absolute(path)) {
		return path;
	}
	int pos = path.find(basepath);
	if(pos != 0) {
		return path;
	}
	return Glib::build_filename(".", path.substr(basepath.size()));
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
	std::pair<std::string, std::string> parts = split_filename(newfilename);
	parts.first = Glib::Regex::create(Glib::ustring::compose("_[0-9]+.%1$", parts.second))->replace(parts.first, 0, "", static_cast<Glib::RegexMatchFlags>(0));
	newfilename = Glib::ustring::compose("%1.%2", parts.first, parts.second);
	while(Glib::file_test(newfilename, Glib::FILE_TEST_EXISTS)) {
		newfilename = Glib::ustring::compose("%1_%2.%3", parts.first, ++i, parts.second);
	}
	return newfilename;
}

void Utils::list_dir(const std::string& path, const std::set<std::string>& filters, std::vector<Glib::RefPtr<Gio::File>>& output) {
	try {
		Glib::Dir dir(path);
		std::vector<Glib::RefPtr<Gio::File>> files;
		for(const std::string& filename : dir) {
			std::string subpath = Glib::build_filename(path, filename);
			if(Glib::file_test(subpath, Glib::FILE_TEST_IS_DIR)) {
				list_dir(subpath, filters, output);
			} else {
				std::pair<std::string, std::string> parts = Utils::split_filename(filename);
				if(filters.find(parts.second) != filters.end()) {
					output.push_back(Gio::File::create_for_path(subpath));
				}
			}
		}
	} catch(const Glib::FileError& e) { }
}

std::vector<Glib::ustring> Utils::string_split(const Glib::ustring& text, char delim, bool keepEmpty) {
	std::vector<Glib::ustring> parts;
	Glib::ustring::size_type startPos = 0, endPos = 0;
	Glib::ustring::size_type npos = Glib::ustring::npos;
	if(text.empty()) {
		if(keepEmpty) {
			parts.push_back("");
		}
		return parts;
	}
	while(true) {
		startPos = endPos;
		endPos = text.find(delim, startPos);
		Glib::ustring::size_type n = endPos == npos ? npos : endPos - startPos;
		if(n > 0 || keepEmpty) {
			parts.push_back(text.substr(startPos, n));
		}
		if(endPos == npos) { break; }
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

Glib::ustring Utils::string_trim(const Glib::ustring& str, const Glib::ustring& what) {
	Glib::ustring ret = str;
	ret.erase(0, ret.find_first_not_of(what));
	std::size_t rpos = ret.find_last_not_of(what);
	if(rpos != Glib::ustring::npos) {
		ret.erase(rpos + 1);
	}
	return ret;
}

bool Utils::strings_equal(const Glib::ustring& str1, const Glib::ustring& str2, bool matchCase) {
	return matchCase ? (str1 == str2) : (str1.casefold() == str2.casefold());
}

bool Utils::string_endswith(const Glib::ustring& str, const Glib::ustring& what) {
	int n = str.length();
	int m = what.length();
	return n >= m && str.substr(n - m) == what;
}

std::size_t Utils::string_firstIndex(const Glib::ustring& str, const Glib::ustring& search, int pos, bool matchCase) {
	std::size_t res = Glib::ustring::npos;
	if(matchCase) {
		res = str.find(search, pos);
	} else {
		res = str.lowercase().find(search.lowercase(), pos);
	}
	return res == Glib::ustring::npos ? -1 : res;
}

std::size_t Utils::string_lastIndex(const Glib::ustring& str, const Glib::ustring& search, int pos, bool matchCase) {
	std::size_t res = Glib::ustring::npos;
	if(matchCase) {
		res = str.rfind(search, pos);
	} else {
		res = str.lowercase().rfind(search.lowercase(), pos);
	}
	return res == Glib::ustring::npos ? -1 : res;
}

int Utils::string_replace(Glib::ustring& str, const Glib::ustring& search, const Glib::ustring& replace, bool matchCase) {
	int pos = 0;
	int count = 0;
	while(true) {
		pos = Utils::string_firstIndex(str, search, pos, matchCase);
		if(pos == -1) {
			break;
		}
		str = str.replace(pos, search.size(), replace);
		pos += replace.size();
		++count;
	}
	return count;
}

Glib::ustring Utils::string_html_escape(const Glib::ustring& str) {
	std::string result;
	result.reserve(str.bytes());
	for(size_t pos = 0; pos != str.bytes(); ++pos) {
		switch(str[pos]) {
		case '&':
			result.append("&amp;");
			break;
		case '"':
			result.append("&quot;");
			break;
		case '\'':
			result.append("&apos;");
			break;
		case '<':
			result.append("&lt;");
			break;
		case '>':
			result.append("&gt;");
			break;
		default:
			result.append(str.data() + pos, 1);
			break;
		}
	}
	return result;
}

std::vector<std::pair<Glib::ustring, int>> Utils::string_split_pos(const Glib::ustring& str, const Glib::RefPtr<Glib::Regex>& splitRe) {
	Glib::ustring::size_type strpos = 0;
	Glib::MatchInfo info;
	std::vector<std::pair<Glib::ustring, int>> result;

	Glib::ustring matchstr = str;
	while(splitRe->match(matchstr, 0, info)) {
		// Fetch pos returns positions in bytes, no characters... Need to extract substring on raw string, and then deduce the new position by comparing the lengths
		int start = 0, end = 0;
		info.fetch_pos(0, start, end);
		result.push_back(std::make_pair(str.raw().substr(0, start), strpos));

		Glib::ustring newmatchstr = matchstr.raw().substr(end);
		strpos += matchstr.length() - newmatchstr.length();
		matchstr = newmatchstr;
	}
	if(strpos < str.length()) {
		result.push_back(std::make_pair(str.substr(strpos), strpos));
	}
	return result;
}

int Utils::parseInt(const Glib::ustring& str, bool* ok) {
	static Glib::RefPtr<Glib::Regex> nrRegEx = Glib::Regex::create("^\\d+$");
	bool match = nrRegEx->match(str);
	if(ok) { *ok = match; }
	return match ? std::atoi(str.c_str()) : 0;
}

void Utils::handle_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int /*x*/, int /*y*/, const Gtk::SelectionData& selection_data, guint /*info*/, guint time) {
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

Glib::RefPtr<Glib::ByteArray> Utils::download(const std::string& url, Glib::ustring& messages, unsigned timeout) {
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
	// If it is already a valid code, return it
	static Glib::RefPtr<Glib::Regex> langPattern = Glib::Regex::create("^[a-z]{2,}(_[A-Z]{2,})?$");
	if(langPattern->match(lang)) {
		return lang;
	}
	// Treat the language as a tesseract lang spec and try to find a matching code
	Config::Lang langspec = {lang};
	if(!lang.empty() && MAIN->getConfig()->searchLangSpec(langspec)) {
		return langspec.code;
	}
	// Use the application locale, if specified, otherwise fall back to en
	Glib::ustring syslocale = Glib::getenv("LANG");
	if(syslocale == "c" || syslocale == "C" || syslocale.empty()) {
		return "en_US";
	}
	return syslocale;
}

Glib::ustring Utils::resolveFontName(const Glib::ustring& family) {
	Glib::ustring resolvedName = family;

	FcPattern* pat = FcNameParse(reinterpret_cast<const FcChar8*>(family.c_str()));
	FcConfigSubstitute (0, pat, FcMatchPattern);
	FcFontSet* fs = FcFontSetCreate();

	FcResult result;
	FcPattern* match = FcFontMatch (0, pat, &result);
	if (match) {
		FcFontSetAdd (fs, match);
	}
	FcPatternDestroy (pat);

	FcObjectSet* os = 0;
	if(fs->nfont > 0) {
		FcPattern* font = FcPatternFilter (fs->fonts[0], os);
		FcChar8* s = FcPatternFormat (font, reinterpret_cast<const FcChar8*>("%{family}"));
		resolvedName = Glib::ustring(reinterpret_cast<const char*>(s));
		FcStrFree (s);
		FcPatternDestroy (font);
	}
	FcFontSetDestroy (fs);

	if (os) {
		FcObjectSetDestroy (os);
	}
	return resolvedName;
}

void Utils::openUri(const std::string& uri) {
#ifdef G_OS_WIN32
	ShellExecute(nullptr, "open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
	gtk_show_uri_on_window(MAIN->getWindow()->gobj(), uri.c_str(), GDK_CURRENT_TIME, 0);
#endif
}

bool Utils::busyTask(const std::function<bool()>& f, const Glib::ustring& msg) {
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

std::unique_ptr<tesseract::TessBaseAPI> Utils::initTesseract(const char* language) {
	// unfortunately tesseract creates deliberate segfaults when an error occurs
	std::signal(SIGABRT, MainWindow::tesseractCrash);
	std::string current = setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "C");
	std::unique_ptr<tesseract::TessBaseAPI> tess(new tesseract::TessBaseAPI());
	int ret = tess->Init(nullptr, language);
	std::signal(SIGSEGV, MainWindow::signalHandler);
	setlocale(LC_ALL, current.c_str());

	if(ret == -1) {
		return nullptr;
	}
	return tess;
}

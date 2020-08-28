/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.hh
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

#ifndef UTILS_HH
#define UTILS_HH

#include <gtkmm.h>
#include <condition_variable>
#include <iterator>
#include <queue>
#include <type_traits>
#include <utility>

#ifdef MAKE_VERSION
#define TESSERACT_MAKE_VERSION(maj,min,patch) MAKE_VERSION((maj),(min),(patch))
#else
#define TESSERACT_MAKE_VERSION(maj,min,patch) ((maj) << 16 | (min) << 8 | (patch))
#endif

namespace tesseract {
class TessBaseAPI;
}

namespace Utils {
void popup_positioner(int& x, int& y, bool& push_in, Gtk::Widget* ref, Gtk::Menu* menu, bool alignRight, bool alignBottom);

void message_dialog(Gtk::MessageType message, const Glib::ustring& title, const Glib::ustring& text, Gtk::Window* parent = 0);

struct Button {
	enum Type {
		Ok = 1,
		Yes = 2,
		No = 4,
		Cancel = 8,
		Save = 16,
		Discard = 32,
		YesAll = 64,
		NoAll = 128
	};
};
Button::Type question_dialog(const Glib::ustring& title, const Glib::ustring& text, int buttons, Gtk::Window* parent = 0);

void set_spin_blocked(Gtk::SpinButton* spin, double value, sigc::connection& conn);
void set_error_state(Gtk::Entry* entry);
void clear_error_state(Gtk::Entry* entry);

Glib::ustring get_content_type(const std::string& filename);
std::pair<std::string, std::string> split_filename(const std::string& filename);
std::string make_absolute_path(const std::string& path, const std::string& basepath);
std::string make_relative_path(const std::string& path, const std::string& basepath);
std::string get_documents_dir();
std::string make_output_filename(const std::string& filename);

std::vector<Glib::ustring> string_split(const Glib::ustring& text, char delim, bool keepEmpty = true);
Glib::ustring string_join(const std::vector<Glib::ustring>& strings, const Glib::ustring& joiner);
Glib::ustring string_trim(const Glib::ustring& str, const Glib::ustring& what = " \t\n\r");
bool strings_equal(const Glib::ustring& str1, const Glib::ustring& str2, bool matchCase);
std::size_t string_firstIndex(const Glib::ustring& str, const Glib::ustring& search, int pos, bool matchCase);
std::size_t string_lastIndex(const Glib::ustring& str, const Glib::ustring& search, int pos, bool matchCase);
int string_replace(Glib::ustring& str, const Glib::ustring& search, const Glib::ustring& replace, bool matchCase);
Glib::ustring string_html_escape(const Glib::ustring& str);
std::vector<std::pair<Glib::ustring, int>> string_split_pos(const Glib::ustring& str, const Glib::RefPtr<Glib::Regex>& splitRe);

int parseInt(const Glib::ustring& str, bool* ok = nullptr);

void handle_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time);

Glib::RefPtr<Glib::ByteArray> download(const std::string& url, Glib::ustring& messages, unsigned timeout = 60000);

Glib::ustring getSpellingLanguage(const Glib::ustring& lang = Glib::ustring());

Glib::ustring resolveFontName(const Glib::ustring& family);

void openUri(const std::string& uri);

template<typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
T round(T x) {
	return std::floor(x + T(0.5));
}

template<class It>
struct rev_iters {
	It _begin, _end;
	It begin() {
		return _begin;
	}
	It end() {
		return _end;
	}
};

template<class T, class It = std::reverse_iterator<typename T::iterator>>
rev_iters<It> reverse(T& t) {
	return { t.rbegin(), t.rend() };
}

template<class T, class It = std::reverse_iterator<typename T::const_iterator>>
rev_iters<It> reverse(const T& t) {
	return { t.crbegin(), t.crend() };
}

bool busyTask(const std::function<bool()>& f, const Glib::ustring& msg);
void runInMainThreadBlocking(const std::function<void()>& f);
std::unique_ptr<tesseract::TessBaseAPI> initTesseract(const char* language = nullptr, bool* ok = nullptr);

template<typename T, typename S = std::deque<T>>
class AsyncQueue {
	std::queue<T, S> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_cond;
public:
	bool empty() {
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_queue.empty();
	}
	void enqueue(const T& item) {
		std::unique_lock<std::mutex> lock(m_mutex);
		m_queue.push(item);
		lock.unlock();
		m_cond.notify_one();
	}
	T dequeue() {
		std::unique_lock<std::mutex> lock(m_mutex);
		while ( m_queue.empty() ) {
			m_cond.wait(lock);
		}
		T result(m_queue.front());
		m_queue.pop();
		return result;
	}
};

}

#endif

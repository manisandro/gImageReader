/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.hh
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

#ifndef UTILS_HH
#define UTILS_HH

#include <gtkmm.h>
#include <iterator>
#include <queue>
#include <type_traits>
#include <utility>

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
		Discard = 32
	};
};
Button::Type question_dialog(const Glib::ustring& title, const Glib::ustring& text, int buttons, Gtk::Window *parent = 0);

void set_spin_blocked(Gtk::SpinButton* spin, double value, sigc::connection& conn);
void set_error_state(Gtk::Entry* entry);
void clear_error_state(Gtk::Entry* entry);

Glib::ustring get_content_type(const std::string& filename);
void get_filename_parts(const std::string& filename, std::string& base, std::string& ext);
std::string make_absolute_path(const std::string& path);
std::string get_documents_dir();
std::string make_output_filename(const std::string& filename);

std::vector<Glib::ustring> string_split(const Glib::ustring& text, char delim, bool keepEmpty = false);
Glib::ustring string_join(const std::vector<Glib::ustring>& strings, const Glib::ustring& joiner);
Glib::ustring string_trim(const Glib::ustring& str);

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

template<typename T, typename S = std::deque<T>>
class AsyncQueue {
	std::queue<T,S>   queue_;
	Glib::Mutex       mutex_;
	Glib::Cond        cond_;
public:
	bool empty() {
		Glib::Mutex::Lock queue_guard(mutex_);
		return queue_.empty();
	}
	void enqueue(const T& item) {
		Glib::Mutex::Lock queue_guard(mutex_);
		queue_.push(item);
		cond_.signal();
	}
	T dequeue() {
		Glib::Mutex::Lock queue_guard(mutex_);
		if(queue_.empty()) {
			while ( queue_.empty() )
				cond_.wait(mutex_);
		}
		T result(queue_.front());
		queue_.pop();
		return result;
	}
};
}

#endif

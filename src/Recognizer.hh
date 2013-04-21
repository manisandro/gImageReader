/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.hh
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

#ifndef RECOGNIZER_HH
#define RECOGNIZER_HH

#include "common.hh"
#include "AsyncQueue.hh"

#include <cairomm/cairomm.h>

class Recognizer
{
public:
	enum class OutputDestination { Buffer, Clipboard };
	Recognizer();
	bool recognizeImage(const Cairo::RefPtr<Cairo::ImageSurface>& img, OutputDestination dest);

private:
	enum class PageSelection { Prompt, Current, All, Selected };
	enum class TaskState { Waiting, Succeeded, Failed };
	Gtk::Menu* m_pagesMenu;
	Gtk::Dialog* m_pagesDialog;
	Gtk::Entry* m_pagesEntry;
	Gtk::ToolButton* m_recognizeBtn;
	Glib::Threads::Mutex m_mutex;
	Glib::Threads::Cond m_cond;
	Glib::Threads::Thread* m_thread;
	TaskState m_taskState;
	Glib::Dispatcher m_textDispatcher;
	AsyncQueue<Glib::ustring> m_textQueue;
	bool m_textInsert;

	void selectPages(std::vector<int> &pages);
	void recognizeStart(PageSelection pagessel = PageSelection::Prompt);
	void recognizeDo(const std::vector<int>& pages, const Glib::ustring &lang);
	void recognizeDone(const Glib::ustring &errors);
	void setPage(int page);
	void addText();
};

#endif // RECOGNIZER_HH

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputManager.hh
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

#ifndef OUTPUTMANAGER_HH
#define OUTPUTMANAGER_HH

#include "common.hh"
#include "UndoableBuffer.hh"
#include "Config.hh"
#include "Notifier.hh"

#include <gtkspellmm.h>

class OutputManager {
public:
	OutputManager();
	void addText(const Glib::ustring& text, bool insert = false);
	bool saveBuffer(std::string filename = "");
	bool clearBuffer();
	void setLanguage(const Config::Lang &lang);
	bool getModified() const{ return m_textBuffer->get_modified(); }

private:
	enum class InsertMode { Append, Cursor, Replace };

	Gtk::ToggleButton* m_insButton;
	Gtk::Menu* m_insMenu;
	Gtk::Image* m_insImage;
	Gtk::EventBox* m_replaceBox;
	Gtk::Box* m_outputBox;
	Gtk::TextView* m_textView;
	Gtk::Entry* m_searchEntry;
	Gtk::Entry* m_replaceEntry;
	Gtk::CheckMenuItem* m_filterKeepIfDot;
	Gtk::CheckMenuItem* m_filterKeepIfQuote;
	Gtk::CheckMenuItem* m_filterJoinHyphen;
	Gtk::CheckMenuItem* m_filterJoinSpace;
	Gtk::ToolButton* m_undoButton;
	Gtk::ToolButton* m_redoButton;

	Glib::RefPtr<UndoableBuffer> m_textBuffer;
	InsertMode m_insertMode;
	Gtk::TextIter m_insertIter;
	Gtk::TextIter m_selectIter;
	GtkSpell::Checker m_spell;
	Notifier::Handle m_notifierHandle = nullptr;

	void showInsertMenu();
	void setInsertMode(InsertMode mode, const std::string& iconName);
	void filterBuffer();
	void toggleReplaceBox(Gtk::ToggleToolButton *button);
	void historyChanged();
	void saveIters();
	void findInBuffer(bool replace = false);
#ifdef G_OS_UNIX
	void dictionaryAutoinstall(Glib::RefPtr<Gio::DBus::Proxy> proxy, const Glib::ustring& lang);
	void dictionaryAutoinstallDone(Glib::RefPtr<Gio::DBus::Proxy> proxy, Glib::RefPtr<Gio::AsyncResult>& result);
#endif
};

#endif // OUTPUTMANAGER_HH

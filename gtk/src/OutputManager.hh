/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputManager.hh
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

#ifndef OUTPUTMANAGER_HH
#define OUTPUTMANAGER_HH

#include "common.hh"
#include "Config.hh"
#include "MainWindow.hh"
#include "UndoableBuffer.hh"

#include <gtkspellmm.h>

class SubstitutionsManager;

class OutputManager {
public:
	OutputManager();
	~OutputManager();
	void addText(const Glib::ustring& text, bool insert = false);
	bool getBufferModified() const;
	bool clearBuffer();
	bool saveBuffer(const std::string& filename = "");
	void setLanguage(const Config::Lang &lang, bool force = false);

private:
	enum class InsertMode { Append, Cursor, Replace };

	Gtk::ToggleToolButton* m_togglePaneButton;
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
	Gtk::CheckButton* m_csCheckBox;

	Glib::RefPtr<UndoableBuffer> m_textBuffer;
	InsertMode m_insertMode;
	GtkSpell::Checker m_spell;
	MainWindow::Notification m_notifierHandle = nullptr;
	SubstitutionsManager* m_substitutionsManager;

	void completeTextViewMenu(Gtk::Menu* menu);
	void filterBuffer();
	void findReplace(bool backwards, bool replace);
	void replaceAll();
	void scrollCursorIntoView();
	void setFont();
	void setInsertMode(InsertMode mode, const std::string& iconName);
	void showInsertMenu();
	void toggleReplaceBox(Gtk::ToggleToolButton *button);
#ifdef G_OS_UNIX
	void dictionaryAutoinstall(Glib::RefPtr<Gio::DBus::Proxy> proxy, const Glib::ustring& lang);
	void dictionaryAutoinstallDone(Glib::RefPtr<Gio::DBus::Proxy> proxy, Glib::RefPtr<Gio::AsyncResult>& result);
#endif
};

#endif // OUTPUTMANAGER_HH

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.hh
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

#ifndef SOURCEMANAGER_HH
#define SOURCEMANAGER_HH

#include "common.hh"


struct Source {
	Source(const Glib::RefPtr<Gio::File>& _file, const Glib::RefPtr<Gio::FileMonitor>& _monitor, bool _isTemp = false)
		: file(_file), monitor(_monitor), isTemp(_isTemp) {}
	Glib::RefPtr<Gio::File> file;
	Glib::RefPtr<Gio::FileMonitor> monitor;
	bool isTemp;
	int brightness = 0;
	int contrast = 0;
	int resolution = -1;
	int page = 1;
	double angle = 0.;
};

class SourceManager {
public:
	SourceManager();
	~SourceManager();

	void addSources(const std::vector<Glib::RefPtr<Gio::File>>& files);
	Source* getSelectedSource() const;
	sigc::signal<void,Source*> signal_sourceChanged(){ return m_signal_sourceChanged; }

private:
	class ListViewColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		Gtk::TreeModelColumn<std::string> filename;
		Gtk::TreeModelColumn<Source*> source;
		ListViewColumns(){ add(filename); add(source); }
	};

	Gtk::Notebook* m_notebook;
	Gtk::TreeView* m_listView;
	Gtk::MenuToolButton* m_addButton;
	Gtk::ToolButton* m_removeButton;
	Gtk::ToolButton* m_deleteButton;
	Gtk::ToolButton* m_clearButton;
	Gtk::MenuItem* m_pasteItem;

	Glib::Threads::Thread* m_thread;

	Glib::RefPtr<Gtk::Clipboard> m_clipboard;
	ListViewColumns m_listViewCols;
	int m_screenshotCount = 0;
	int m_pasteCount = 0;
	sigc::signal<void,Source*> m_signal_sourceChanged;
	sigc::connection m_connectionSelectionChanged;

	void addSourcesBrowse();
	void pasteClipboard();
	void takeScreenshot();
	void savePixbuf(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, const std::string& displayname);
	void savePixbufDone(const std::string& path, const std::string& displayname);
	void removeItem(bool deleteFile);
	void clearItems();
	void selectionChanged();
	void sourceChanged(const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& otherFile, Gio::FileMonitorEvent event, Gtk::TreeIter it);
};

#endif // SOURCEMANAGER_HH

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.hh
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

#ifndef SOURCEMANAGER_HH
#define SOURCEMANAGER_HH

#include "common.hh"

namespace Ui {
class MainWindow;
}
class DisplayRenderer;

struct Source {
	Source(const Glib::RefPtr<Gio::File>& _file, const std::string& _displayname, const Glib::RefPtr<Gio::FileMonitor>& _monitor, bool _isTemp = false)
		: file(_file), displayname(_displayname), monitor(_monitor), isTemp(_isTemp) {}
	Glib::RefPtr<Gio::File> file;
	std::string displayname;
	Glib::ustring password;
	Glib::RefPtr<Gio::FileMonitor> monitor;
	bool isTemp;
	int brightness = 0;
	int contrast = 0;
	int resolution = -1;
	int page = 1;
	std::vector<double> angle;
	bool invert = false;
	DisplayRenderer* renderer = nullptr;

	//Additional info from original file
	Glib::ustring author, title, creator, producer, keywords, subject;
	guint pdfVersionMajor = -1, pdfVersionMinor = -1;
};

class SourceManager {
public:
	SourceManager(const Ui::MainWindow& _ui);
	~SourceManager();

	int addSources(const std::vector<Glib::RefPtr<Gio::File>>& files, bool suppressTextWarning = false);
	bool addSource(Glib::RefPtr<Gio::File> file, bool suppressTextWarning) {
		return addSources({file}, suppressTextWarning) == 1;
	}
	std::vector<Source*> getSelectedSources() const;
	sigc::signal<void> signal_sourceChanged() {
		return m_signal_sourceChanged;
	}

private:
	class ListViewColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		Gtk::TreeModelColumn<std::string> filename;
		Gtk::TreeModelColumn<Source*> source;
		Gtk::TreeModelColumn<std::string> path;
		ListViewColumns() {
			add(filename);
			add(source);
			add(path);
		}
	} m_listViewCols;

	enum class PdfWithTextAction {Ask, Add, Skip};
	const Ui::MainWindow& ui;
	ClassData m_classdata;
	Glib::RefPtr<Gtk::Clipboard> m_clipboard;
	int m_screenshotCount = 0;
	int m_pasteCount = 0;
	sigc::signal<void> m_signal_sourceChanged;
	sigc::connection m_connectionSelectionChanged;

	void clearSources();
	void fileChanged(const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& otherFile, Gio::FileMonitorEvent event, Gtk::TreeIter it);
	void openSources();
	void pasteClipboard();
	bool checkPdfSource(Source* source, PdfWithTextAction& textAction, std::vector<Glib::ustring>& failed) const;
	void removeSource(bool deleteFile);
	void savePixbuf(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, const std::string& displayname);
	void selectionChanged();
	void takeScreenshot();
};

#endif // SOURCEMANAGER_HH

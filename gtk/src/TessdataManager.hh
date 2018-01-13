/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * TessdataManager.hh
 * Copyright (C) 2013-2018 Sandro Mani <manisandro@gmail.com>
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

#ifndef TESSDATAMANAGER_HH
#define TESSDATAMANAGER_HH

#include "ui_TessdataManager.hh"

class TessdataManager {
public:
	static void exec();

private:
	TessdataManager();
	void run();
	struct LangFile {
		Glib::ustring name;
		Glib::ustring url;
	};

	struct ViewColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<bool> selected;
		Gtk::TreeModelColumn<Glib::ustring> label;
		Gtk::TreeModelColumn<Glib::ustring> prefix;
		ViewColumns() {
			add(selected);
			add(label);
			add(prefix);
		}
	} m_viewCols;

	Ui::TessdataManager ui;
	Glib::RefPtr<Gtk::ListStore> m_languageListStore;
	std::map<Glib::ustring,std::vector<LangFile>> m_languageFiles;
	Glib::RefPtr<Gio::DBus::Proxy> m_dbusProxy;

	bool fetchLanguageList(Glib::ustring& messages);
	void applyChanges();
	void refresh();
};

#endif // TESSDATAMANAGER_HH

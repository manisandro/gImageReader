/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SubstitutionsManager.hh
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

#ifndef SUBSTITUTIONS_MANAGER_HH
#define SUBSTITUTIONS_MANAGER_HH

#include "common.hh"

class OutputBuffer;

class SubstitutionsManager {
public:
	SubstitutionsManager(const Builder& builder, const Glib::RefPtr<OutputBuffer>& buffer);
	~SubstitutionsManager();
	void set_visible(bool visible);

private:
	struct ReplaceListColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> search;
		Gtk::TreeModelColumn<Glib::ustring> replace;
		ReplaceListColumns() { add(search); add(replace); }
	};

	std::string m_currentFile;
	ReplaceListColumns m_viewCols;
	Gtk::Window* m_dialog;
	Gtk::TreeView* m_listView;
	Gtk::Button* m_removeButton;
	Glib::RefPtr<Gtk::ListStore> m_listStore;
	Glib::RefPtr<OutputBuffer> m_buffer;
	Gtk::CheckButton* m_csCheckBox;

	void addRow();
	void applySubstitutions();
	bool clearList();
	void dialogClosed();
	void openList();
	void removeRows();
	bool saveList();
};

#endif // SUBSTITUTIONS_MANAGER_HH

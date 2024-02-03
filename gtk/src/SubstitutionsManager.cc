/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SubstitutionsManager.cc
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#include "SubstitutionsManager.hh"
#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "OutputBuffer.hh"
#include "Utils.hh"

#include <fstream>

SubstitutionsManager::SubstitutionsManager() {
	ui.setupUi();

	m_listStore = Gtk::ListStore::create(m_viewCols);
	ui.treeview->set_model(m_listStore);
	ui.treeview->append_column_editable(_("Search for"), m_viewCols.search);
	ui.treeview->append_column_editable(_("Replace with"), m_viewCols.replace);
	ui.treeview->get_column(0)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	ui.treeview->get_column(1)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	ui.treeview->get_column(0)->set_expand(true);
	ui.treeview->get_column(1)->set_expand(true);
	ui.treeview->set_fixed_height_mode();

	CONNECT(ui.toolbuttonOpen, clicked, [this] { openList(); });
	CONNECT(ui.toolbuttonSave, clicked, [this] { saveList(); });
	CONNECT(ui.toolbuttonClear, clicked, [this] { clearList(); });
	CONNECT(ui.toolbuttonAdd, clicked, [this] { addRow(); });
	CONNECT(ui.toolbuttonRemove, clicked, [this] { removeRows(); });
	CONNECT(ui.buttonApply, clicked, [this] { applySubstitutions(); });
	CONNECT(ui.buttonClose, clicked, [this] { ui.windowSubstitutions->hide(); });
	CONNECT(ui.treeview->get_selection(), changed, [this] { ui.toolbuttonRemove->set_sensitive(ui.treeview->get_selection()->count_selected_rows() != 0); });

	ADD_SETTING(ListStoreSetting("replacelist", Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeview->get_model())));
}

SubstitutionsManager::~SubstitutionsManager() {
	ConfigSettings::get<ListStoreSetting>("replacelist")->serialize();
}

void SubstitutionsManager::set_visible(bool visible) {
	ui.windowSubstitutions->set_visible(visible);
	if(visible) {
		ui.windowSubstitutions->raise();
	}
}

void SubstitutionsManager::openList() {
	if(!clearList()) {
		return;
	}
	std::string dir = !m_currentFile.empty() ? Glib::path_get_dirname(m_currentFile) : "";
	FileDialogs::FileFilter filter = { _("Substitutions List"), {"text/plain"}, {"*.txt"}};
	std::vector<Glib::RefPtr<Gio::File>> files = FileDialogs::open_dialog(_("Open Substitutions List"), dir, "auxdir", filter, false, ui.windowSubstitutions);

	if(!files.empty()) {
		std::ifstream file(files.front()->get_path());
		if(!file.is_open()) {
			Utils::messageBox(Gtk::MESSAGE_ERROR, _("Error Reading File"), Glib::ustring::compose(_("Unable to read '%1'."), files.front()->get_path()));
			return;
		}
		m_currentFile = files.front()->get_path();

		std::string line;
		bool errors = false;
		while(std::getline(file, line)) {
			Glib::ustring text = line.data();
			if(text.empty()) {
				continue;
			}
			std::vector<Glib::ustring> fields = Utils::string_split(text, '\t', true);
			if(fields.size() < 2) {
				errors = true;
				continue;
			}
			Gtk::TreeIter it = m_listStore->append();
			(*it)[m_viewCols.search] = fields[0];
			(*it)[m_viewCols.replace] = fields[1];
		}
		if(errors) {
			Utils::messageBox(Gtk::MESSAGE_WARNING, _("Errors Occurred Reading File"), _("Some entries of the substitutions list could not be read."));
		}
	}
}

bool SubstitutionsManager::saveList() {
	FileDialogs::FileFilter filter = { _("Substitutions List"), {"text/plain"}, {"*.txt"} };
	std::string filename = FileDialogs::save_dialog(_("Save Substitutions List"), m_currentFile, "auxdir", filter, false, ui.windowSubstitutions);
	if(filename.empty()) {
		return false;
	}
	std::ofstream file(filename);
	if(!file.is_open()) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Error Saving File"), Glib::ustring::compose(_("Unable to write to '%1'."), filename));
		return false;
	}
	m_currentFile = filename;
	Glib::ustring str;
	for(const Gtk::TreeModel::Row& row : m_listStore->children()) {
		Glib::ustring search, replace;
		row.get_value(0, search);
		row.get_value(1, replace);
		str += Glib::ustring::compose("%1\t%2\n", search, replace);
	}
	file.write(str.data(), str.bytes());
	return true;
}

bool SubstitutionsManager::clearList() {
	if(m_listStore->children().size() > 0) {
		int response = Utils::messageBox(Gtk::MESSAGE_QUESTION, _("Save List?"), _("Do you want to save the current list?"), "", Utils::Button::Save | Utils::Button::Discard | Utils::Button::Cancel, ui.windowSubstitutions);
		if(response == Utils::Button::Save) {
			if(!saveList()) {
				return false;
			}
		} else if(response != Utils::Button::Discard) {
			return false;
		}
	}
	m_listStore->clear();
	return true;
}

void SubstitutionsManager::addRow() {
	Gtk::TreeIter it = m_listStore->append();
	ui.treeview->set_cursor(m_listStore->get_path(it), *ui.treeview->get_column(0), true);
}

void SubstitutionsManager::removeRows() {
	if(ui.treeview->get_selection()->count_selected_rows() != 0) {
		std::vector<Gtk::ListStore::RowReference> rows;
		for(const Gtk::ListStore::TreeModel::Path& path : ui.treeview->get_selection()->get_selected_rows()) {
			rows.push_back(Gtk::ListStore::RowReference(m_listStore, path));
		}
		for(const Gtk::ListStore::RowReference& row : rows) {
			m_listStore->erase(m_listStore->get_iter(row.get_path()));
		}
	}
}

void SubstitutionsManager::applySubstitutions() {
	std::map<Glib::ustring, Glib::ustring> substitutions;
	for(const Gtk::TreeModel::Row& row : m_listStore->children()) {
		Glib::ustring search, replace;
		row.get_value(0, search);
		row.get_value(1, replace);
		substitutions.insert(std::make_pair(search, replace));
	}
	m_signal_apply_substitutions.emit(substitutions);
}

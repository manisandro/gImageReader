/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ReplaceListManager.cc
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

#include "ReplaceListManager.hh"
#include "MainWindow.hh"
#include "Config.hh"
#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "Utils.hh"

#include <fstream>
#include <cstring>

ReplaceListManager::ReplaceListManager()
{
	m_dialog = Builder("window:postproc");
	m_listView = Builder("treeview:postproc");
	m_removeButton = Builder("toolbutton:postproc.remove");

	m_listStore = Gtk::ListStore::create(m_viewCols);
	m_listView->set_model(m_listStore);
	m_listView->append_column_editable(_("Search for"), m_viewCols.search);
	m_listView->append_column_editable(_("Replace with"), m_viewCols.replace);
	m_listView->get_column(0)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	m_listView->get_column(1)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	m_listView->get_column(0)->set_expand(true);
	m_listView->get_column(1)->set_expand(true);
	m_listView->set_fixed_height_mode();


	CONNECT(Builder("toolbutton:postproc.open").as<Gtk::Button>(), clicked, [this]{ openList(); });
	CONNECT(Builder("toolbutton:postproc.save").as<Gtk::Button>(), clicked, [this]{ saveList(); });
	CONNECT(Builder("toolbutton:postproc.clear").as<Gtk::Button>(), clicked, [this]{ clearList(); });
	CONNECT(Builder("toolbutton:postproc.add").as<Gtk::Button>(), clicked, [this]{ addRow(); });
	CONNECT(m_removeButton, clicked, [this]{ removeRows(); });
	CONNECT(m_listView->get_selection(), changed, [this]{ m_removeButton->set_sensitive(m_listView->get_selection()->count_selected_rows() != 0); });
	CONNECT(m_dialog, hide, [this] { dialogClosed(); });

	MAIN->getConfig()->addSetting("replacelist", new ListStoreSetting(Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model())));
	MAIN->getConfig()->addSetting("replacelistfile", new VarSetting<Glib::ustring>());

	m_currentFile = MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("replacelistfile")->getValue();
	if(m_currentFile.empty()) {
		m_currentFile = Glib::build_filename(Utils::get_documents_dir(), _("replace_list.txt"));
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("replacelistfile")->setValue(m_currentFile);
	}
}

void ReplaceListManager::openList()
{
	if(!clearList()) {
		return;
	}
	std::string dir = Glib::path_get_dirname(m_currentFile);
	FileDialogs::FileFilter filter = { _("Replacement list"), "text/plain", "*.txt"};
	std::vector<Glib::RefPtr<Gio::File>> files = FileDialogs::open_dialog(_("Open replacement list"), dir, filter, false, m_dialog);

	if(!files.empty()) {
		std::ifstream file(files.front()->get_path());
		if(!file.is_open()) {
			Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error reading file"), Glib::ustring::compose(_("Unable to read from the file %1."), files.front()->get_path()));
			return;
		}
		m_currentFile = files.front()->get_path();
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("replacelistfile")->setValue(m_currentFile);

		std::string line;
		bool errors = false;
		while(std::getline(file, line)) {
			Glib::ustring text = line.data();
			std::vector<Glib::ustring> fields = Utils::string_split(text, '\t');
			if(fields.size() < 2) {
				errors = true;
				continue;
			}
			Gtk::TreeIter it = m_listStore->append();
			(*it)[m_viewCols.search] = fields[0];
			(*it)[m_viewCols.replace] = fields[1];
		}
		if(errors) {
			Utils::message_dialog(Gtk::MESSAGE_WARNING, _("Errors occurred reading file"), _("Some entries of the replacement list could not be read."));
		}
	}
}

bool ReplaceListManager::saveList()
{
	FileDialogs::FileFilter filter = { _("Replacement list"), "text/plain", "*.txt" };
	std::string filename = FileDialogs::save_dialog(_("Save replacement list"), m_currentFile, filter, m_dialog);
	if(filename.empty()) {
		return false;
	}
	std::ofstream file(filename);
	if(!file.is_open()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error saving file"), Glib::ustring::compose(_("Unable to write to the file %1."), filename));
		return false;
	}
	m_currentFile = filename;
	MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("replacelistfile")->setValue(m_currentFile);
	Glib::ustring str;
	for(const Gtk::TreeModel::Row& row : m_listStore->children()){
		Glib::ustring search, replace;
		row.get_value(0, search);
		row.get_value(1, replace);
		str += Glib::ustring::compose("%1\t%2\n", search, replace);
	}
	file.write(str.data(), std::strlen(str.data()));
	return true;
}

bool ReplaceListManager::clearList()
{
	if(m_listStore->children().size() > 0) {
		int ret = Utils::question_dialog(_("Save the current list?"), _("Do you want to save the current list?"), m_dialog);
		if(ret == 2) {
			return false;
		} else if(ret == 1) {
			if(!saveList()) {
				return false;
			}
		}
	}
	m_listStore->clear();
	return true;
}

void ReplaceListManager::addRow()
{
	Gtk::TreeIter it = m_listStore->append();
	m_listView->set_cursor(m_listStore->get_path(it), *m_listView->get_column(0), true);
}

void ReplaceListManager::removeRows()
{
	if(m_listView->get_selection()->count_selected_rows() != 0){
		std::vector<Gtk::ListStore::RowReference> rows;
		for(const Gtk::ListStore::TreeModel::Path& path : m_listView->get_selection()->get_selected_rows()) {
			rows.push_back(Gtk::ListStore::RowReference(m_listStore, path));
		}
		for(const Gtk::ListStore::RowReference& row : rows) {
			m_listStore->erase(m_listStore->get_iter(row.get_path()));
		}
	}
}

void ReplaceListManager::dialogClosed()
{
	MAIN->getConfig()->getSetting<ListStoreSetting>("replacelist")->serialize();
}

void ReplaceListManager::show()
{
	m_dialog->show();
}

void ReplaceListManager::hide()
{
	m_dialog->hide();
}

void ReplaceListManager::apply(Glib::RefPtr<UndoableBuffer> buffer)
{
	Gtk::TextIter start, end;
	if(!buffer->get_selection_bounds(start, end)){
		start = buffer->begin();
		end = buffer->end();
	}
	Glib::ustring text = buffer->get_text(start, end);

	Utils::busyTask([this,&text]{
		for(const Gtk::TreeModel::Row& row : m_listStore->children()){
			Glib::ustring search, replace;
			row.get_value(0, search);
			row.get_value(1, replace);
			std::size_t pos = 0;
			std::size_t search_len = search.length();
			while(true) {
				pos = text.find(search, pos);
				if(pos == std::string::npos) {
					break;
				}
				text.replace(pos, search_len, replace);
				pos += 1;
			}
		}
		return true;
	}, _("Applying replacement list..."));

	buffer->replace_range(text, start, end);
	start = end = buffer->get_iter_at_mark(buffer->get_insert());
	start.backward_chars(text.size());
	buffer->select_range(start, end);
}

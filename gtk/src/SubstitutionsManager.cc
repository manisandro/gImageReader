/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SubstitutionsManager.cc
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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
#include "MainWindow.hh"
#include "Config.hh"
#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "Utils.hh"

#include <fstream>
#include <cstring>

SubstitutionsManager::SubstitutionsManager(const Glib::RefPtr<UndoableBuffer>& buffer, Gtk::CheckButton* csCheckBox)
	: m_buffer(buffer), m_csCheckBox(csCheckBox)
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
	CONNECT(Builder("button:postproc.apply").as<Gtk::Button>(), clicked, [this]{ applySubstitutions(); });
	CONNECT(Builder("button:postproc.close").as<Gtk::Button>(), clicked, [this]{ m_dialog->hide(); });
	CONNECT(m_removeButton, clicked, [this]{ removeRows(); });
	CONNECT(m_listView->get_selection(), changed, [this]{ m_removeButton->set_sensitive(m_listView->get_selection()->count_selected_rows() != 0); });
	CONNECT(m_dialog, hide, [this] { dialogClosed(); });

	MAIN->getConfig()->addSetting(new ListStoreSetting("replacelist", Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model())));
	MAIN->getConfig()->addSetting(new VarSetting<Glib::ustring>("replacelistfile"));

	m_currentFile = MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("replacelistfile")->getValue();
	if(m_currentFile.empty()) {
		m_currentFile = Glib::build_filename(Utils::get_documents_dir(), _("substitution_list.txt"));
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("replacelistfile")->setValue(m_currentFile);
	}
}

void SubstitutionsManager::set_visible(bool visible)
{
	m_dialog->set_visible(visible);
	if(visible && m_dialog->is_visible()){
		m_dialog->raise();
	}
}

void SubstitutionsManager::openList()
{
	if(!clearList()) {
		return;
	}
	std::string dir = Glib::path_get_dirname(m_currentFile);
	FileDialogs::FileFilter filter = { _("Substitutions List"), "text/plain", "*.txt"};
	std::vector<Glib::RefPtr<Gio::File>> files = FileDialogs::open_dialog(_("Open Substitutions List"), dir, filter, false, m_dialog);

	if(!files.empty()) {
		std::ifstream file(files.front()->get_path());
		if(!file.is_open()) {
			Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error Reading File"), Glib::ustring::compose(_("Unable to read '%1'."), files.front()->get_path()));
			return;
		}
		m_currentFile = files.front()->get_path();
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("replacelistfile")->setValue(m_currentFile);

		std::string line;
		bool errors = false;
		while(std::getline(file, line)) {
			Glib::ustring text = line.data();
			if(text.empty()){
				continue;
			}
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
			Utils::message_dialog(Gtk::MESSAGE_WARNING, _("Errors Occurred Reading File"), _("Some entries of the substitutions list could not be read."));
		}
	}
}

bool SubstitutionsManager::saveList()
{
	FileDialogs::FileFilter filter = { _("Substitutions List"), "text/plain", "*.txt" };
	std::string filename = FileDialogs::save_dialog(_("Save Substitutions List"), m_currentFile, filter, m_dialog);
	if(filename.empty()) {
		return false;
	}
	std::ofstream file(filename);
	if(!file.is_open()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error Saving File"), Glib::ustring::compose(_("Unable to write to '%1'."), filename));
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

bool SubstitutionsManager::clearList()
{
	if(m_listStore->children().size() > 0) {
		int response = Utils::question_dialog(_("Save List?"), _("Do you want to save the current list?"), Utils::Button::Save|Utils::Button::Discard|Utils::Button::Cancel, m_dialog);
		if(response == Utils::Button::Save){
			if(!saveList()){
				return false;
			}
		}else if(response != Utils::Button::Discard){
			return false;
		}
	}
	m_listStore->clear();
	return true;
}

void SubstitutionsManager::addRow()
{
	Gtk::TreeIter it = m_listStore->append();
	m_listView->set_cursor(m_listStore->get_path(it), *m_listView->get_column(0), true);
}

void SubstitutionsManager::removeRows()
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

void SubstitutionsManager::dialogClosed()
{
	MAIN->getConfig()->getSetting<ListStoreSetting>("replacelist")->serialize();
}

void SubstitutionsManager::applySubstitutions()
{
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	Gtk::TextIter start, end;
	if(!m_buffer->get_selection_bounds(start, end)){
		start = m_buffer->begin();
		end = m_buffer->end();
	}
	int startpos = start.get_offset();
	int endpos = end.get_offset();
	Gtk::TextSearchFlags flags = Gtk::TEXT_SEARCH_VISIBLE_ONLY|Gtk::TEXT_SEARCH_TEXT_ONLY;
	if(!m_csCheckBox->get_active()){
		flags |= Gtk::TEXT_SEARCH_CASE_INSENSITIVE;
	}
	for(const Gtk::TreeModel::Row& row : m_listStore->children()){
		Glib::ustring search, replace;
		row.get_value(0, search);
		row.get_value(1, replace);
		int diff = replace.length() - search.length();
		Gtk::TextIter it = m_buffer->get_iter_at_offset(startpos);
		while(true){
			Gtk::TextIter matchStart, matchEnd;
			if(!it.forward_search(search, flags, matchStart, matchEnd) || matchEnd.get_offset() > endpos){
				break;
			}
			it = m_buffer->replace_range(replace, matchStart, matchEnd);
			endpos += diff;
		}
		while(Gtk::Main::events_pending()){
			Gtk::Main::iteration();
		}
	}
	m_buffer->select_range(m_buffer->get_iter_at_offset(startpos), m_buffer->get_iter_at_offset(endpos));
	MAIN->popState();
}

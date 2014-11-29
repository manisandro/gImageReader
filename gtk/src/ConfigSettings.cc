/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ConfigSettings.cc
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

#include "ConfigSettings.hh"
#include "Utils.hh"

Glib::RefPtr<Gio::Settings> get_default_settings(){
	static Glib::RefPtr<Gio::Settings> settings = Gio::Settings::create(APPLICATION_ID);
	return settings;
}

ListStoreSetting::ListStoreSetting(const Glib::ustring &key, Glib::RefPtr<Gtk::ListStore> liststore)
	: AbstractSetting(key), m_liststore(liststore)
{
	Glib::ustring str = get_default_settings()->get_string(m_key);
	m_liststore->clear();
	std::size_t nCols = m_liststore->get_n_columns();

	for(const Glib::ustring& row : Utils::string_split(str, ';', false)){
		int colidx = 0;
		std::vector<Glib::ustring> cols = Utils::string_split(row, ',', true);
		if(cols.size() != nCols){
			continue;
		}
		Gtk::TreeModel::Row treerow = *(m_liststore->append());
		for(const Glib::ustring& col : cols){
			treerow.set_value(colidx++, col);
		}
	}
}

void ListStoreSetting::serialize()
{
	// Serialized string has format a11,a12,a13;a21,a22,a23;...
	Glib::ustring str;
	int nCols = m_liststore->get_n_columns();
	for(const Gtk::TreeModel::Row& row : m_liststore->children()){
		for(int col = 0; col < nCols; ++col){
			Glib::ustring field;
			row.get_value(col, field);
			str += field + (col == nCols - 1 ? ";" : ",");
		}
	}
	Glib::Variant<Glib::ustring> v = Glib::Variant<Glib::ustring>::create(str);
	get_default_settings()->set_value(m_key, v);
}

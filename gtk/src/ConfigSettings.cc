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

void ListStoreSetting::reread(){
	Glib::Variant<Glib::ustring> v;
	m_settings->get_value(m_key, v);
	Glib::ustring str = v.get();

	// Split string into table, string has format a11,a12,a13;a21,a22,a23;...
	m_liststore->clear();
	int nCols = m_liststore->get_n_columns();

	std::vector<Glib::ustring>&& rows = Utils::string_split(str, ';', false);
	for(const Glib::ustring& row : rows){
		std::vector<Glib::ustring>&& cols = Utils::string_split(row, ',', true);
		if(int(cols.size()) != nCols){ continue; }

		Gtk::TreeModel::Row treerow = *(m_liststore->append());
		int colidx = 0;
		for(const Glib::ustring& col : cols){
			treerow.set_value(colidx++, col);
		}
	}
}

void ListStoreSetting::serialize(){
	Glib::ustring str;
	int nCols = m_liststore->get_n_columns();
	for(const Gtk::TreeModel::Row& row : m_liststore->children()){
		for(int i = 0; i < nCols; ++i){
			Glib::ustring field;
			row.get_value(i, field);
			str += field + (i == nCols - 1 ? ";" : ",");
		}
	}
	Glib::Variant<Glib::ustring> v = Glib::Variant<Glib::ustring>::create(str);
	m_settings->set_value(m_key, v);
}

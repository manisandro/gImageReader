/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FontComboBox.hh
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

#ifndef FONTCOMBOBOX_HH
#define FONTCOMBOBOX_HH

#include "common.hh"

class FontComboBox : public Gtk::ComboBox {
public:
	FontComboBox();
	FontComboBox(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder);
	Glib::ustring get_active_font() const;
	void set_active_font(const Glib::ustring& font);

	sigc::signal<void, Glib::ustring> signal_font_changed() {
		return m_signal_font_changed;
	}

private:
	ClassData m_classdata;
	sigc::signal<void, Glib::ustring> m_signal_font_changed;

	struct FontComboColums : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> fontFamily;
		FontComboColums() {
			add(fontFamily);
		}
	};

	void init();

	static FontComboColums s_fontComboCols;
	static Glib::RefPtr<Gtk::ListStore> getModel();
};

#endif // FONTCOMBOBOX_HH

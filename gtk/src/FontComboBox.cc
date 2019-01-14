/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FontComboBox.cc
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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

#include "FontComboBox.hh"

FontComboBox::FontComboColums FontComboBox::s_fontComboCols;

FontComboBox::FontComboBox()
	: Gtk::ComboBox(true), Glib::ObjectBase("FontComboBox") {
	init();
}

FontComboBox::FontComboBox(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
	: Gtk::ComboBox(cobject), Glib::ObjectBase("FontComboBox") {
	init();
}

void FontComboBox::init() {
	set_model(getModel());
	set_entry_text_column(s_fontComboCols.fontFamily);
	add_attribute(*get_first_cell(), "family", s_fontComboCols.fontFamily);
	Glib::RefPtr<Gtk::EntryCompletion> completion = Gtk::EntryCompletion::create();
	completion->set_model(getModel());
	completion->set_text_column(s_fontComboCols.fontFamily);
	get_entry()->set_completion(completion);
	CONNECT(this, changed, [this] { m_signal_font_changed.emit(get_active_font()); });
}

Glib::ustring FontComboBox::get_active_font() const {
	return get_entry()->get_text();
}

void FontComboBox::set_active_font(const Glib::ustring& font) {
	get_entry()->set_text(font);
}

Glib::RefPtr<Gtk::ListStore> FontComboBox::getModel() {
	static Glib::RefPtr<Gtk::ListStore> fontComboModel;
	if(!fontComboModel) {
		std::vector<Glib::ustring> fontFamilies;
		Glib::RefPtr<Pango::FontMap> fontMap = Glib::wrap(pango_cairo_font_map_get_default(),  true);
		for(const Glib::RefPtr<Pango::FontFamily>& family : fontMap->list_families()) {
			fontFamilies.push_back(family->get_name());
		}
		std::sort(fontFamilies.begin(), fontFamilies.end(), [](const Glib::ustring & a, const Glib::ustring & b) {
			return a.casefold().compare(b.casefold()) < 0;
		});
		fontComboModel = Gtk::ListStore::create(s_fontComboCols);
		for(const Glib::ustring& family : fontFamilies) {
			(*fontComboModel->append())[s_fontComboCols.fontFamily] = family;
		}
	}
	return fontComboModel;
}

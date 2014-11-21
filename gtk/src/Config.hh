/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.hh
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

#ifndef CONFIG_HH
#define CONFIG_HH

#include <cassert>
#include <map>
#include <vector>

#include "common.hh"
#include "ConfigSettings.hh"

class Config {
public:
	struct Lang {
		Glib::ustring prefix, code, name;
	};

	Config();
	~Config();

	void addSetting(AbstractSetting* setting) {
		m_settings.insert(std::make_pair(setting->key(), setting));
	}
	template<class T>
	T* getSetting(const Glib::ustring& key) const{
		auto it = m_settings.find(key);
		return it == m_settings.end() ? nullptr : static_cast<T*>(it->second);
	}

	bool searchLangSpec(Lang& lang) const;
	void showDialog();

private:
	struct LangViewColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> prefix;
		Gtk::TreeModelColumn<Glib::ustring> code;
		Gtk::TreeModelColumn<Glib::ustring> name;
		LangViewColumns() { add(prefix); add(code); add(name); }
	};

	static const std::vector<Lang> LANGUAGES;

	Gtk::Dialog* m_dialog;
	Gtk::Box* m_addLangBox;
	Gtk::Entry* m_addLangPrefix;
	Gtk::Entry* m_addLangName;
	Gtk::Entry* m_addLangCode;
	Gtk::ButtonBox* m_editLangBox;
	Gtk::Button* m_removeLangButton;
	Gtk::TreeView* m_predefLangView;
	Gtk::TreeView* m_customLangView;
	Gtk::Button* m_dialogOkButton;

	LangViewColumns m_langViewCols;
	std::map<Glib::ustring,AbstractSetting*> m_settings;

	void addLanguage();
	void removeLanguage();
	void langTableSelectionChanged();
	void toggleAddLanguage(bool forceHide = false);
};

#endif // CONFIG_HH

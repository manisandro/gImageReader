/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.hh
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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
#include "ui_ConfigDialog.hh"

class Config {
public:
	struct Lang {
		Glib::ustring prefix, code, name;
		Lang(const Glib::ustring& _prefix = Glib::ustring(), const Glib::ustring& _code = Glib::ustring(), const Glib::ustring& _name = Glib::ustring())
			: prefix(_prefix), code(_code), name(_name) {}
	};

	Config();
	~Config();

	void addSetting(AbstractSetting* setting) {
		auto it = m_settings.find(setting->key());
		if(it != m_settings.end()) {
			delete it->second;
			it->second = setting;
		} else {
			m_settings.insert(std::make_pair(setting->key(), setting));
		}
	}
	template<class T>
	T* getSetting(const Glib::ustring& key) const {
		auto it = m_settings.find(key);
		return it == m_settings.end() ? nullptr : static_cast<T*>(it->second);
	}
	void removeSetting(const Glib::ustring& key) {
		auto it = m_settings.find(key);
		if(it != m_settings.end()) {
			delete it->second;
			m_settings.erase(it);
		}
	}

	bool searchLangSpec(Lang& lang) const;
	std::vector<Glib::ustring> searchLangCultures(const Glib::ustring& code) const;
	void showDialog();

	bool useSystemDataLocations() const;
	std::string tessdataLocation() const;
	std::string spellingLocation() const;

	static void openTessdataDir();
	static void openSpellingDir();

private:
	struct LangViewColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> prefix;
		Gtk::TreeModelColumn<Glib::ustring> code;
		Gtk::TreeModelColumn<Glib::ustring> name;
		LangViewColumns() {
			add(prefix);
			add(code);
			add(name);
		}
	};

	static const std::vector<Lang> LANGUAGES;
	static const std::multimap<Glib::ustring,Glib::ustring> LANGUAGE_CULTURES ;

	Ui::ConfigDialog ui;
	ConnectionsStore m_connections;

	LangViewColumns m_langViewCols;
	std::map<Glib::ustring,AbstractSetting*> m_settings;

	static std::multimap<Glib::ustring,Glib::ustring> buildLanguageCultureTable();

	void addLanguage();
	void removeLanguage();
	void langTableSelectionChanged();
	void setDataLocations(int idx);
	void toggleAddLanguage(bool forceHide = false);
};

#endif // CONFIG_HH

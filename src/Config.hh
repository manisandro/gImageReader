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

	void addSetting(const Glib::ustring& key, AbstractSetting* setting) {
		m_settings.insert(std::make_pair(key, setting));
		setting->setSettingsAndKey(key, m_gioSettings);
		setting->reread();
	}
	template<class T>
	T* getSetting(const Glib::ustring& key) const{
		auto it = m_settings.find(key);
		assert(it != m_settings.end());
		return (T*)(it->second);
	}
	const Lang& getSelectedLanguage() const{ return m_curlang; }
	sigc::signal<void, const Lang&> signal_languageChanged(){ return m_signal_languageChanged; }

	void showDialog();
	void updateLanguagesMenu();

private:
	struct LangViewColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> prefix;
		Gtk::TreeModelColumn<Glib::ustring> code;
		Gtk::TreeModelColumn<Glib::ustring> name;
		LangViewColumns() { add(prefix); add(code); add(name); }
	};

	static const std::vector<Lang> LANGUAGES;

	Gtk::Dialog* m_dialog;
	Gtk::Label* m_langLabel;
	Gtk::Box* m_addLangBox;
	Gtk::Entry* m_addLangPrefix;
	Gtk::Entry* m_addLangName;
	Gtk::Entry* m_addLangCode;
	Gtk::ButtonBox* m_editLangBox;
	Gtk::Button* m_removeLangButton;
	Gtk::TreeView* m_predefLangView;
	Gtk::TreeView* m_customLangView;
	Gtk::Button* m_dialogOkButton;
	Gtk::Menu m_langsMenu;
	Gtk::RadioButtonGroup m_radioGroup;

	LangViewColumns m_langViewCols;

	Lang m_curlang;
	std::map<Glib::ustring,AbstractSetting*> m_settings;
	Glib::RefPtr<Gio::Settings> m_gioSettings;

	sigc::signal<void, const Lang&> m_signal_languageChanged;

	void setLanguage(const Gtk::RadioMenuItem *item, const Lang& lang, const Glib::ustring& prettyname);
	void toggleAddLanguage();
	void addLanguage();
	void removeLanguage();
	bool searchLangSpec(const Glib::RefPtr<Gtk::TreeModel> model, Lang& lang) const;
};

#endif // CONFIG_HH

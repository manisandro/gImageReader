/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.hh
 * Copyright (C) 2013-2018 Sandro Mani <manisandro@gmail.com>
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
#include "ui_ConfigDialog.hh"

class Config {
public:
	struct Lang {
		Glib::ustring prefix, code, name;
		Lang(const Glib::ustring& _prefix = Glib::ustring(), const Glib::ustring& _code = Glib::ustring(), const Glib::ustring& _name = Glib::ustring())
			: prefix(_prefix), code(_code), name(_name) {}
	};

	Config();

	bool searchLangSpec(Lang& lang) const;
	std::vector<Glib::ustring> searchLangCultures(const Glib::ustring& code) const;
	void showDialog();

	bool useSystemDataLocations() const;
	std::string tessdataLocation() const;
	std::string spellingLocation() const;

	static void openTessdataDir();
	static void openSpellingDir();

private:
	enum Location {SystemLocation = 0, UserLocation = 1};

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
	static const std::multimap<Glib::ustring, Glib::ustring> LANGUAGE_CULTURES ;

	Ui::ConfigDialog ui;
	ClassData m_classdata;

	LangViewColumns m_langViewCols;

	void addLanguage();
	void removeLanguage();
	void langTableSelectionChanged();
	void setDataLocations(int idx);
	void toggleAddLanguage(bool forceHide = false);

	static std::string spellingLocation(Location location);
	static std::string tessdataLocation(Location location);
};

#endif // CONFIG_HH

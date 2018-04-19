/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.cc
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

#include "Config.hh"
#include "ConfigSettings.hh"
#include "LangTables.hh"
#include "MainWindow.hh"
#include "Utils.hh"

#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

const std::vector<Config::Lang> Config::LANGUAGES = LangTables::languages<std::vector<Config::Lang>, Glib::ustring>([](const char* str) { return Glib::ustring(str); });
const std::multimap<Glib::ustring, Glib::ustring> Config::LANGUAGE_CULTURES = LangTables::languageCultures<std::multimap<Glib::ustring, Glib::ustring>>();

Config::Config() {
	ui.setupUi();
	ui.dialogConfig->set_transient_for(*MAIN->getWindow());

	for(Gtk::TreeView* view : {
	            ui.treeviewLangsPredef, ui.treeviewLangsCustom
	        }) {
		view->set_model(Gtk::ListStore::create(m_langViewCols));
		view->append_column_editable(_("Filename prefix"), m_langViewCols.prefix);
		view->append_column_editable(_("Code"), m_langViewCols.code);
		view->append_column_editable(_("Native name"), m_langViewCols.name);
		for(int i = 0; i < 3; ++i) {
			view->get_column(i)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
			view->get_column(i)->set_expand(true);
		}
		view->set_fixed_height_mode(true);
	}

	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewLangsPredef->get_model());
	for(const auto& lang : LANGUAGES) {
		Gtk::TreeIter it = store->append();
		(*it)[m_langViewCols.prefix] = lang.prefix;
		(*it)[m_langViewCols.code] = lang.code;
		(*it)[m_langViewCols.name] = lang.name;
	}

	CONNECT(ui.checkbuttonDefaultoutputfont, toggled, [this] { ui.fontbuttonCustomoutputfont->set_sensitive(!ui.checkbuttonDefaultoutputfont->get_active()); });
	CONNECT(ui.buttonLangsEditAdd, clicked, [this] { toggleAddLanguage(); });
	CONNECT(ui.buttonLangsEditRemove, clicked, [this] { removeLanguage(); });
	CONNECT(ui.buttonLangsAddOk, clicked, [this] { addLanguage(); });
	CONNECT(ui.buttonLangsAddCancel, clicked, [this] { toggleAddLanguage(); });
	CONNECT(ui.treeviewLangsCustom->get_selection(), changed, [this] { langTableSelectionChanged(); });
	CONNECT(ui.buttonHelp, clicked, [] { MAIN->showHelp("#Usage_Options"); });
	CONNECT(ui.entryLangsAddPrefix, focus_in_event, [this](GdkEventFocus*) {
		Utils::clear_error_state(ui.entryLangsAddPrefix);
		return false;
	});
	CONNECT(ui.entryLangsAddName, focus_in_event, [this](GdkEventFocus*) {
		Utils::clear_error_state(ui.entryLangsAddName);
		return false;
	});
	CONNECT(ui.entryLangsAddCode, focus_in_event, [this](GdkEventFocus*) {
		Utils::clear_error_state(ui.entryLangsAddCode);
		return false;
	});
	CONNECT(ui.comboDatadirs, changed, [this] { setDataLocations(ui.comboDatadirs->get_active_row_number()); });

	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("dictinstall", ui.checkDictinstall));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("openafterexport", ui.checkOpenExported));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("updatecheck", ui.checkUpdate));
	ADD_SETTING(ListStoreSetting("customlangs", Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewLangsCustom->get_model())));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("systemoutputfont", ui.checkbuttonDefaultoutputfont));
	ADD_SETTING(FontSetting("customoutputfont", ui.fontbuttonCustomoutputfont));
	ADD_SETTING(ComboSetting("outputorient", ui.comboPaneorient));
	ADD_SETTING(ComboSetting("datadirs", ui.comboDatadirs));
	ADD_SETTING(VarSetting<Glib::ustring>("sourcedir"));
	ADD_SETTING(VarSetting<Glib::ustring>("outputdir"));
	ADD_SETTING(VarSetting<Glib::ustring>("auxdir"));

#if !ENABLE_VERSIONCHECK
	ui.checkUpdate->hide();
#endif
}

bool Config::searchLangSpec(Lang& lang) const {
	// Tesseract 4.0.0-beta.1 and previous had Script tessdatas on same level as language tessdatas, but they are distinguishable in that they begin with an upper case character
	if(lang.prefix.substr(0, 6).lowercase() == "script" || lang.prefix.substr(0, 1).uppercase() == lang.prefix.substr(0, 1)) {
		Glib::ustring name = lang.prefix.substr(0, 6).lowercase() == "script" ? lang.prefix.substr(7) : lang.prefix;
		lang.name = Glib::ustring::compose("%1 [%2]", name, _("Script"));
		return true;
	}
	for(const Glib::RefPtr<Gtk::TreeModel>& model : {
	            ui.treeviewLangsPredef->get_model(), ui.treeviewLangsCustom->get_model()
	        }) {
		Gtk::TreeIter it = std::find_if(model->children().begin(), model->children().end(),
		[this, &lang](const Gtk::TreeRow & row) {
			return row[m_langViewCols.prefix] == lang.prefix;
		});
		if(it) {
			lang = {lang.prefix, (*it)[m_langViewCols.code], Glib::ustring::compose("%1 [%2]", (*it)[m_langViewCols.name], lang.prefix)};
			return true;
		}
	}
	return false;
}

std::vector<Glib::ustring> Config::searchLangCultures(const Glib::ustring& code) const {
	std::vector<Glib::ustring> result;
	auto ii = LANGUAGE_CULTURES.equal_range(code);
	for(auto it = ii.first; it != ii.second; ++it) {
		result.push_back(it->second);
	}
	return result;
}

bool Config::useSystemDataLocations() const {
	return ui.comboDatadirs->get_active_row_number() == 0;
}

std::string Config::tessdataLocation() const {
	return ui.entryTessdatadir->get_text();
}

std::string Config::spellingLocation() const {
	return ui.entrySpelldir->get_text();
}

void Config::showDialog() {
	toggleAddLanguage(true);
	while(ui.dialogConfig->run() == Gtk::RESPONSE_HELP);
	ConfigSettings::get<ListStoreSetting>("customlangs")->serialize();
	ui.dialogConfig->hide();
}

void Config::setDataLocations(int idx) {
	if(idx == 0) {
#ifdef G_OS_WIN32
		std::string dataDir = Glib::build_filename(pkgDir, "share");
		Glib::setenv("TESSDATA_PREFIX", dataDir);
#else
		std::string dataDir("/usr/share");
		Glib::unsetenv("TESSDATA_PREFIX");
#endif
		Glib::RefPtr<Gio::File> dictDir = Gio::File::create_for_path(Glib::build_filename(dataDir, "myspell", "dicts"));
		if(!dictDir->query_exists()) {
			dictDir = Gio::File::create_for_path(Glib::build_filename(dataDir, "myspell"));
		}
		ui.entrySpelldir->set_text(dictDir->get_path());
	} else {
		std::string configDir = Glib::get_user_config_dir();
		Glib::setenv("TESSDATA_PREFIX", Glib::build_filename(configDir, "tessdata"));
		ui.entrySpelldir->set_text(Glib::build_filename(configDir, "enchant", "myspell"));
	}
	tesseract::TessBaseAPI tess;
	std::string current = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	tess.Init(nullptr, nullptr);
	setlocale(LC_NUMERIC, current.c_str());
	ui.entryTessdatadir->set_text(tess.GetDatapath());
}

void Config::openTessdataDir() {
	int idx = Gio::Settings::create(APPLICATION_ID)->get_int("datadirs");
	if(idx == 0) {
#ifdef G_OS_WIN32
		std::string dataDir = Glib::build_filename(pkgDir, "share");
		Glib::setenv("TESSDATA_PREFIX", dataDir);
#else
		Glib::unsetenv("TESSDATA_PREFIX");
#endif
	} else {
		std::string configDir = Glib::get_user_config_dir();
		Glib::setenv("TESSDATA_PREFIX", Glib::build_filename(configDir, "tessdata"));
	}
	tesseract::TessBaseAPI tess;
	std::string current = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	tess.Init(nullptr, nullptr);
	setlocale(LC_NUMERIC, current.c_str());
	Utils::openUri(Glib::filename_to_uri(tess.GetDatapath()));
}

void Config::openSpellingDir() {
	int idx = Gio::Settings::create(APPLICATION_ID)->get_int("datadirs");
	if(idx == 0) {
#ifdef G_OS_WIN32
		std::string dataDir = Glib::build_filename(pkgDir, "share");
#else
		std::string dataDir("/usr/share");
#endif
		Glib::RefPtr<Gio::File> dictDir = Gio::File::create_for_path(Glib::build_filename(dataDir, "myspell", "dicts"));
		if(!dictDir->query_exists()) {
			dictDir = Gio::File::create_for_path(Glib::build_filename(dataDir, "myspell"));
		}
		Utils::openUri(Glib::filename_to_uri(dictDir->get_path()));
	} else {
		std::string configDir = Glib::get_user_config_dir();
		Utils::openUri(Glib::filename_to_uri(Glib::build_filename(configDir, "enchant", "myspell")));
	}
}

void Config::toggleAddLanguage(bool forceHide) {
	bool addVisible = forceHide ? true : ui.boxLangsAdd->get_visible();
	ui.boxLangsAdd->set_visible(!addVisible);
	ui.buttonboxLangsEdit->set_visible(addVisible);
	if(addVisible) {
		ui.buttonLangsEditAdd->grab_focus();
	} else {
		ui.buttonLangsAddOk->grab_focus();
	}
	ui.entryLangsAddPrefix->set_text("");
	ui.entryLangsAddCode->set_text("");
	ui.entryLangsAddName->set_text("");
	Utils::clear_error_state(ui.entryLangsAddPrefix);
	Utils::clear_error_state(ui.entryLangsAddCode);
	Utils::clear_error_state(ui.entryLangsAddName);
}

void Config::addLanguage() {
	bool invalid = false;
	if(!Glib::Regex::create("^\\w+$")->match(ui.entryLangsAddPrefix->get_text())) {
		invalid = true;
		Utils::set_error_state(ui.entryLangsAddPrefix);
	}
	if(!Glib::Regex::create("^.+$")->match(ui.entryLangsAddName->get_text())) {
		invalid = true;
		Utils::set_error_state(ui.entryLangsAddName);
	}
	if(!Glib::Regex::create("^[a-z]{2}$")->match(ui.entryLangsAddCode->get_text()) &&
	        !Glib::Regex::create("^[a-z]{2}_[A-Z]{2}$")->match(ui.entryLangsAddCode->get_text())) {
		invalid = true;
		Utils::set_error_state(ui.entryLangsAddCode);
	}
	if(!invalid) {
		Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewLangsCustom->get_model());
		Gtk::TreeIter it = store->append();
		(*it)[m_langViewCols.prefix] = ui.entryLangsAddPrefix->get_text();
		(*it)[m_langViewCols.code] = ui.entryLangsAddCode->get_text();
		(*it)[m_langViewCols.name] = ui.entryLangsAddName->get_text();
		ui.entryLangsAddPrefix->set_text("");
		ui.entryLangsAddCode->set_text("");
		ui.entryLangsAddName->set_text("");
		toggleAddLanguage();
	}
}

void Config::removeLanguage() {
	if(ui.treeviewLangsCustom->get_selection()->count_selected_rows() != 0) {
		Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewLangsCustom->get_model());
		store->erase(ui.treeviewLangsCustom->get_selection()->get_selected());
	}
}

void Config::langTableSelectionChanged() {
	ui.buttonLangsEditRemove->set_sensitive(ui.treeviewLangsCustom->get_selection()->count_selected_rows() != 0);
}

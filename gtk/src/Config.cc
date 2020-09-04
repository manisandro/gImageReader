/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.cc
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

#include "Config.hh"
#include "ConfigSettings.hh"
#include "LangTables.hh"
#include "MainWindow.hh"
#include "Utils.hh"

#include <enchant-provider.h>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#include <tesseract/genericvector.h>
#undef USE_STD_NAMESPACE

const std::vector<Config::Lang> Config::LANGUAGES = LangTables::languages<std::vector<Config::Lang>, Glib::ustring>([](const char* str) { return Glib::ustring(str); });
const std::map<Glib::ustring, Glib::ustring> Config::LANG_LOOKUP = [] {
	std::map<Glib::ustring, Glib::ustring> lookup;
	for(const Config::Lang& lang : LANGUAGES) {
		lookup.insert(std::make_pair(lang.prefix, lang.code));
	}
	return lookup;
}();
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
	// Tesseract 4.x up to beta.1 had script tessdatas on same level as language tessdatas, but they are distinguishable in that they begin with an upper case character
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

std::vector<Glib::ustring> Config::getAvailableLanguages() {
	auto tess = Utils::initTesseract();
	if(!tess) {
		return std::vector<Glib::ustring>();
	}
	GenericVector<STRING> availLanguages;
	tess->GetAvailableLanguagesAsVector(&availLanguages);
	std::vector<Glib::ustring> result;
	for(int i = 0; i < availLanguages.size(); ++i) {
		result.push_back(availLanguages[i].string());
	}
	std::sort(result.begin(), result.end(), [](const Glib::ustring & s1, const Glib::ustring & s2) {
		bool s1Script = s1.substr(0, 6) == "script" || s1.substr(0, 1) == s1.substr(0, 1).uppercase();
		bool s2Script = s2.substr(0, 6) == "script" || s2.substr(0, 1) == s2.substr(0, 1).uppercase();
		if(s1Script != s2Script) {
			return !s1Script;
		} else {
			return s1 < s2;
		}
	});
	return result;
}

void Config::showDialog() {
	toggleAddLanguage(true);
	while(ui.dialogConfig->run() == Gtk::RESPONSE_HELP);
	ConfigSettings::get<ListStoreSetting>("customlangs")->serialize();
	ui.dialogConfig->hide();
}

void Config::setDataLocations(int idx) {
	ui.entrySpelldir->set_text(spellingLocation(static_cast<Location>(idx)));
	ui.entryTessdatadir->set_text(tessdataLocation(static_cast<Location>(idx)));
}

void Config::openTessdataDir() {
	int idx = Gio::Settings::create(APPLICATION_ID)->get_int("datadirs");
	std::string tessdataDir = tessdataLocation(static_cast<Location>(idx));
	Gio::File::create_for_path(tessdataDir)->make_directory_with_parents();
	Utils::openUri(tessdataDir);
}

void Config::openSpellingDir() {
	int idx = Gio::Settings::create(APPLICATION_ID)->get_int("datadirs");
	std::string spellingDir = spellingLocation(static_cast<Location>(idx));
	Gio::File::create_for_path(spellingDir)->make_directory_with_parents();
	Utils::openUri(spellingDir);
}

std::string Config::spellingLocation(Location location) {
	if(location == SystemLocation) {
#ifdef G_OS_WIN32
		std::string dataDir = Glib::build_filename(pkgDir, "share");
#else
		char* prefix = enchant_get_prefix_dir();
		std::string dataDir = Glib::build_filename(prefix, "share");
		free(prefix);
#endif
#if HAVE_ENCHANT2
		if(Gio::File::create_for_path(Glib::build_filename(dataDir, "myspell"))->query_exists()) {
			return Glib::build_filename(dataDir, "myspell");
		} else {
			return Glib::build_filename(dataDir, "hunspell");
		}
#else
		return Glib::build_filename(dataDir, "myspell", "dicts");
#endif
	} else {
		std::string configDir = Glib::get_user_config_dir();
#if HAVE_ENCHANT2
		return Glib::build_filename(configDir, "enchant", "hunspell");
#else
		return Glib::build_filename(configDir, "enchant", "myspell");
#endif
	}
}

std::string Config::tessdataLocation(Location location) {
	if(location == SystemLocation) {
#ifdef G_OS_WIN32
		std::string dataDir = Glib::build_filename(pkgDir, "share");
		Glib::setenv("TESSDATA_PREFIX",  Glib::build_filename(dataDir, "tessdata");
#else
		Glib::unsetenv("TESSDATA_PREFIX");
#endif
	} else {
		std::string configDir = Glib::get_user_config_dir();
		Glib::setenv("TESSDATA_PREFIX", Glib::build_filename(configDir, "tessdata"));
	}
	std::string current = setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "C");
	tesseract::TessBaseAPI tess;
	tess.Init(nullptr, nullptr);
	setlocale(LC_ALL, current.c_str());
	return std::string(tess.GetDatapath());
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
	if(!Glib::Regex::create("^[a-z]{2,}(_[A-Z]{2,})?$")->match(ui.entryLangsAddCode->get_text())) {
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

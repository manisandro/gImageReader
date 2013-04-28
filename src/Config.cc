/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.cc
 * Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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
#include "MainWindow.hh"
#include "Utils.hh"

#include <enchant++.h>
#include <tesseract/baseapi.h>

static void dictDescribe(const char* const tag, const char* const providerName,
						 const char* const providerDesc, const char* const providerFile, void* data)
{
	((std::vector<Glib::ustring>*)data)->push_back(tag);
}

const std::vector<Config::Lang> Config::LANGUAGES = {
	{"ara", "\u0627\u0644\u0639\u0631\u0628\u064a\u0629", "ar_AE"},
	{"bul", "\u0431\u044a\u043b\u0433\u0430\u0440\u0441\u043a\u0438 \u0435\u0437\u0438\u043a", "bg_BG"},
	{"cat", "Catal\u00e0", "ca_ES"},
	{"ces", "\u010cesky", "cs_CS"},
	{"chi_sim", "\u7b80\u4f53\u5b57", "zh_CN"},
	{"chi_tra", "\u7c21\u9ad4\u5b57", "zh_CN"},
	{"dan-frak", "Dansk (Frak)", "da_DK"},
	{"dan", "Dansk", "da_DK"},
	{"deu-frak", "Deutsch (Frak)", "de_DE"},
	{"deu-f", "Deutsch (Frak)", "de_DE"},
	{"deu", "Deutsch", "de_DE"},
	{"ell", "\u0395\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac", "el_GR"},
	{"eng", "English", "en_US"},
	{"fin", "Suomi", "fi_FI"},
	{"fra", "Fran\u00e7ais", "fr_FR"},
	{"heb", "\u05e2\u05d1\u05e8\u05d9\u05ea", "he_IL"},
	{"hrv", "hrvatski", "hr_HR"},
	{"hun", "Magyar", "hu_HU"},
	{"ind", "Bahasa Indonesia", "id_ID"},
	{"ita", "Italiano", "it_IT"},
	{"jpn", "\u65e5\u672c\u8a9e", "ja_JP"},
	{"kan", "\u0c95\u0ca8\u0ccd\u0ca8\u0ca1", "kn_IN"},
	{"kor", "\ud55c\uad6d\ub9d0", "ko_KR"},
	{"lav", "Latvie\u0161u Valoda", "lv_LV"},
	{"lit", "Lietuvi\u0173 Kalba", "lt_LT"},
	{"nld", "Nederlands", "nl_NL"},
	{"nor", "Norsk", "no_NO"},
	{"pol", "Polski", "pl_PL"},
	{"por", "Portugu\u00eas", "pt_PT"},
	{"ron", "Rom\u00e2n\u0103", "ro_RO"},
	{"rus", "P\u0443\u0441\u0441\u043a\u0438\u0439 \u044f\u0437\u044b\u043a", "ru_RU"},
	{"slk-frak", "Sloven\u010dina", "sk_SK"},
	{"slk", "Sloven\u010dina", "sk_SK"},
	{"slv", "Sloven\u0161\u010dina", "sl_SI"},
	{"spa", "Espa\u00f1ol", "es_ES"},
	{"srp", "\u0441\u0440\u043f\u0441\u043a\u0438 \u0458\u0435\u0437\u0438\u043a", "sr_RS"},
	{"swe-frak", "Svenska (Frak)", "sv_SE"},
	{"swe", "Svenska", "sv_SE"},
	{"tgl", "Wikang Tagalog", "tl_PH"},
	{"tur", "T\u00fcrk\u00e7e", "tr_TR"},
	{"ukr", "Y\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430", "uk_UA"},
	{"vie", "Ti\u1ebfng Vi\u1ec7t", "vi_VN"}
};

Config::Config()
{
	m_dialog = Builder("dialog:config");
	m_langLabel = Builder("label:main.recognize.lang");
	m_addLangBox = Builder("box:config.langs.custom.add");
	m_addLangPrefix = Builder("entry:config.langs.custom.add.prefix");
	m_addLangName = Builder("entry:config.langs.custom.add.name");
	m_addLangCode = Builder("entry:config.langs.custom.add.code");
	m_editLangBox = Builder("buttonbox:config.langs.custom.edit");
	m_removeLangButton = Builder("button:config.langs.custom.edit.remove");
	m_predefLangView = Builder("treeview:config.langs.predef");
	m_customLangView = Builder("treeview:config.langs.custom");
	m_dialogOkButton = Builder("button:config.ok");

	m_addLangPrefix->set_placeholder_text(_("Prefix"));
	m_addLangName->set_placeholder_text(_("Name"));
	m_addLangCode->set_placeholder_text(_("ISO Code"));

	for(Gtk::TreeView* view : {m_predefLangView, m_customLangView})
	{
		view->set_model(Gtk::ListStore::create(m_langViewCols));
		CONNECT(view, size_allocate, [this,view](Gtk::Allocation& a){ viewResizeCols(view, a); });
		view->append_column(_("Filename prefix"), m_langViewCols.prefix);
		view->append_column(_("Native name"), m_langViewCols.name);
		view->append_column(_("ISO 639-1 code"), m_langViewCols.code);
		for(int i=0; i<3; ++i){
			view->get_column(i)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
		}
		view->set_fixed_height_mode();
	}

	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_predefLangView->get_model());
	for(const auto& lang : LANGUAGES){
		Gtk::TreeModel::Row row = *(store->append());
		row[m_langViewCols.prefix] = lang.prefix;
		row[m_langViewCols.name] = lang.name;
		row[m_langViewCols.code] = lang.code;
	}

	m_gioSettings = Gio::Settings::create(APPLICATION_ID);

	m_settings.insert(std::make_pair("outputorient", new ComboSetting("outputorient", m_gioSettings, "combo:config.settings.paneorient")));
	m_settings.insert(std::make_pair("showcontrols", new SwitchSettingT<Gtk::ToggleToolButton>("showcontrols", m_gioSettings, "tbbutton:main.controls")));
	m_settings.insert(std::make_pair("dictinstall", new SwitchSettingT<Gtk::CheckButton>("dictinstall", m_gioSettings, "check:config.settings.dictinstall")));
	m_settings.insert(std::make_pair("updatecheck", new SwitchSettingT<Gtk::CheckButton>("updatecheck", m_gioSettings, "check:config.settings.update")));
	m_settings.insert(std::make_pair("language", new VarSetting<Glib::ustring>("language", m_gioSettings)));
	m_settings.insert(std::make_pair("customlangs", new ListStoreSetting("customlangs", m_gioSettings, Glib::RefPtr<Gtk::ListStore>::cast_static(m_customLangView->get_model()))));
	m_settings.insert(std::make_pair("scanres", new ComboSetting("scanres", m_gioSettings, "combo:sources.acquire.resolution")));
	m_settings.insert(std::make_pair("scanmode", new ComboSetting("scanmode", m_gioSettings, "combo:sources.acquire.mode")));
	m_settings.insert(std::make_pair("scanoutput", new VarSetting<Glib::ustring>("scanoutput", m_gioSettings)));
	m_settings.insert(std::make_pair("scandev", new ComboSetting("scandev", m_gioSettings, "combo:input.acquire.device")));
	m_settings.insert(std::make_pair("wingeom", new VarSetting<std::vector<int>>("wingeom", m_gioSettings)));
	m_settings.insert(std::make_pair("keepdot", new SwitchSettingT<Gtk::CheckMenuItem>("keepdot", m_gioSettings, "menuitem:output.stripcrlf.keepdot")));
	m_settings.insert(std::make_pair("keepquote", new SwitchSettingT<Gtk::CheckMenuItem>("keepquote", m_gioSettings, "menuitem:output.stripcrlf.keepquote")));
	m_settings.insert(std::make_pair("joinhyphen", new SwitchSettingT<Gtk::CheckMenuItem>("joinhyphen", m_gioSettings, "menuitem:output.stripcrlf.joinhyphen")));
	m_settings.insert(std::make_pair("joinspace", new SwitchSettingT<Gtk::CheckMenuItem>("joinspace", m_gioSettings, "menuitem:output.stripcrlf.joinspace")));

	Builder("tbmenu:main.recognize").as<Gtk::MenuToolButton>()->set_menu(m_langsMenu);
	CONNECT(Builder("button:config.langs.custom.edit.add").as<Gtk::Button>(), clicked, [this]{ toggleAddLanguage(); });
	CONNECT(m_removeLangButton, clicked, [this]{ removeLanguage(); });
	CONNECT(Builder("button:config.langs.custom.add.ok").as<Gtk::Button>(), clicked, [this]{ addLanguage(); });
	CONNECT(Builder("button:config.langs.custom.add.cancel").as<Gtk::Button>(), clicked, [this]{ toggleAddLanguage(); });
	CONNECT(Builder("button:config.help").as<Gtk::Button>(), clicked, []{ MAIN->showHelp(); });
	CONNECT(m_customLangView->get_selection(), changed, [this]{ m_removeLangButton->set_sensitive(m_customLangView->get_selection()->count_selected_rows() != 0); });
	CONNECT(m_addLangPrefix, focus_in_event, [this](GdkEventFocus*){ Utils::clear_error_state(m_addLangPrefix); return false; });
	CONNECT(m_addLangName, focus_in_event, [this](GdkEventFocus*){ Utils::clear_error_state(m_addLangName); return false; });
	CONNECT(m_addLangCode, focus_in_event, [this](GdkEventFocus*){ Utils::clear_error_state(m_addLangCode); return false; });
	CONNECT(m_gioSettings, changed, [this](const Glib::ustring& key){ getSetting<AbstractSetting>(key)->reread(); });
}

Config::~Config()
{
	for(const auto& keyVal : m_settings){
		delete keyVal.second;
	}
}

void Config::readSettings()
{
	for(const auto& keyval : m_settings){
		keyval.second->reread();
	}
	updateLanguagesMenu();
}

bool Config::searchLangSpec(const Glib::RefPtr<Gtk::TreeModel> model, const Glib::ustring& prefix, Lang& lang) const
{
	Gtk::TreeIter it = std::find_if(model->children().begin(), model->children().end(),
		[this, &prefix](const Gtk::TreeRow& row){ return row[m_langViewCols.prefix] == prefix; });
	if(!it){
		return false;
	}
	lang = {(*it)[m_langViewCols.prefix], (*it)[m_langViewCols.name], (*it)[m_langViewCols.code]};
	return true;
}

void Config::updateLanguagesMenu()
{
	m_langsMenu.forall([this](Gtk::Widget& w){ m_langsMenu.remove(w); });
	m_radioGroup = Gtk::RadioButtonGroup();

	Gtk::RadioMenuItem* radioitem = nullptr;
	Glib::ustring curlang = getSetting<VarSetting<Glib::ustring>>("language")->getValue();

	std::vector<Glib::ustring> dicts;
	enchant::Broker::instance()->list_dicts(dictDescribe, &dicts);

	tesseract::TessBaseAPI tess;
	tess.Init(0, 0);
	GenericVector<STRING> availLanguages;
	tess.GetAvailableLanguagesAsVector(&availLanguages);

	for(int i = 0; i < availLanguages.size(); ++i){
		Lang lang;
		if(searchLangSpec(m_predefLangView->get_model(), availLanguages[i].string(), lang) ||
		   searchLangSpec(m_customLangView->get_model(), availLanguages[i].string(), lang))
		{
			std::vector<Glib::ustring> spelldicts;
			for(const Glib::ustring& dict : dicts){
				if(dict.substr(0, 2) == lang.code.substr(0, 2)){
					spelldicts.push_back(dict);
				}
			}
			std::sort(spelldicts.begin(), spelldicts.end());
			if(!spelldicts.empty()){
				Gtk::Image* image = Gtk::manage(new Gtk::Image());
				image->set_from_icon_name("tools-check-spelling", Gtk::ICON_SIZE_MENU);
				Gtk::ImageMenuItem* item = Gtk::manage(new Gtk::ImageMenuItem(*image, lang.name));
				Gtk::Menu* submenu = Gtk::manage(new Gtk::Menu);
				for(const Glib::ustring& dict : spelldicts){
					radioitem = Gtk::manage(new Gtk::RadioMenuItem(m_radioGroup, dict));
					Lang itemlang = {lang.prefix, lang.name, dict};
					Glib::ustring prettyname = lang.name + " (" + dict + ")";
					CONNECT(radioitem, toggled, [this, radioitem, itemlang, prettyname]{ setLanguage(radioitem, itemlang, prettyname); });
					if((curlang.size() == 2 && curlang == dict.substr(0, 2)) || curlang == dict){
						curlang = dict;
						radioitem->set_active(true);
					}
					submenu->append(*radioitem);
				}
				item->set_submenu(*submenu);
				m_langsMenu.append(*item);
			}else{
				radioitem = Gtk::manage(new Gtk::RadioMenuItem(m_radioGroup, lang.name));
				CONNECT(radioitem, toggled, [this, radioitem, lang]{ setLanguage(radioitem, lang, lang.name); });
				if(curlang.substr(0, 2) == lang.code.substr(0, 2)){
					curlang = lang.code;
					radioitem->set_active(true);
				}
				m_langsMenu.append(*radioitem);
			}
		}
	}
	m_langsMenu.show_all();
	if(curlang.empty() && radioitem){
		radioitem->set_active(true);
		radioitem->toggled();
	}
}

void Config::showDialog()
{
	m_gioSettings->delay();
	if(m_dialog->run() != Gtk::RESPONSE_OK){
		m_gioSettings->revert();
	}else{
		m_gioSettings->apply();
		updateLanguagesMenu();
	}
	m_dialog->hide();
}

void Config::setLanguage(const Gtk::RadioMenuItem* item, const Lang &lang, const Glib::ustring &prettyname)
{
	if(item->get_active()){
		m_langLabel->set_markup("<small>" + prettyname + "</small>");
		m_curlang = lang;
		getSetting<VarSetting<Glib::ustring>>("language")->setValue(lang.code);
		m_signal_languageChanged.emit(lang);
	}
}

void Config::viewResizeCols(Gtk::TreeView *view, const Gtk::Allocation& alloc)
{
	int width = alloc.get_width()/double(view->get_columns().size());
	for(Gtk::TreeViewColumn* col : view->get_columns()){
		col->set_fixed_width(width);
	}
}

void Config::toggleAddLanguage()
{
	bool addVisible = m_addLangBox->get_visible();
	m_addLangBox->set_visible(!addVisible);
	m_editLangBox->set_visible(addVisible);
	m_addLangPrefix->set_text("");
	m_addLangName->set_text("");
	m_addLangCode->set_text("");
	Utils::clear_error_state(m_addLangPrefix);
	Utils::clear_error_state(m_addLangName);
	Utils::clear_error_state(m_addLangCode);
}

void Config::addLanguage()
{
	bool invalid = false;
	if(!Glib::Regex::create("^\\w+$")->match(m_addLangPrefix->get_text())){
		invalid = true;
		Utils::set_error_state(m_addLangPrefix);
	}
	if(!Glib::Regex::create("^.+$")->match(m_addLangName->get_text())){
		invalid = true;
		Utils::set_error_state(m_addLangName);
	}
	if(!Glib::Regex::create("^[a-z]{2}_[A-Z]{2}$")->match(m_addLangCode->get_text())){
		invalid = true;
		Utils::set_error_state(m_addLangCode);
	}
	if(!invalid){
		Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_customLangView->get_model());
		Gtk::TreeRow row = *(store->append());
		row[m_langViewCols.prefix] = m_addLangPrefix->get_text();
		row[m_langViewCols.name] = m_addLangName->get_text();
		row[m_langViewCols.code] = m_addLangCode->get_text();
		m_addLangPrefix->set_text("");
		m_addLangName->set_text("");
		m_addLangCode->set_text("");
		updateLanguagesMenu();
	}
}

void Config::removeLanguage()
{
	if(m_customLangView->get_selection()->count_selected_rows() != 0){
		Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_customLangView->get_model());
		store->erase(m_customLangView->get_selection()->get_selected());
	}
}

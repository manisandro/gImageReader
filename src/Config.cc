/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.cc
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

#include "Config.hh"
#include "MainWindow.hh"
#include "Utils.hh"

#include <gtkspellmm.h>
#include <tesseract/baseapi.h>

const std::vector<Config::Lang> Config::LANGUAGES = {
	// {ISO 639-2, ISO 639-1, name}
	{"ara", "ar_AE", "\u0627\u0644\u0639\u0631\u0628\u064a\u0629"},
	{"bul", "bg_BG", "\u0431\u044a\u043b\u0433\u0430\u0440\u0441\u043a\u0438 \u0435\u0437\u0438\u043a"},
	{"cat", "ca_ES", "Catal\u00e0"},
	{"ces", "cs_CS", "\u010cesky"},
	{"chi_sim", "zh_CN", "\u7b80\u4f53\u5b57"},
	{"chi_tra", "zh_CN", "\u7c21\u9ad4\u5b57"},
	{"dan-frak", "da_DK", "Dansk (Frak)"},
	{"dan", "da_DK", "Dansk"},
	{"deu-frak", "de_DE", "Deutsch (Frak)"},
	{"deu-f", "de_DE", "Deutsch (Frak)"},
	{"deu", "de_DE", "Deutsch"},
	{"ell", "el_GR", "\u0395\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac"},
	{"eng", "en_US", "English"},
	{"fin", "fi_FI", "Suomi"},
	{"fra", "fr_FR", "Fran\u00e7ais"},
	{"heb", "he_IL", "\u05e2\u05d1\u05e8\u05d9\u05ea"},
	{"hrv", "hr_HR", "hrvatski"},
	{"hun", "hu_HU", "Magyar"},
	{"ind", "id_ID", "Bahasa Indonesia"},
	{"ita", "it_IT", "Italiano"},
	{"jpn", "ja_JP", "\u65e5\u672c\u8a9e"},
	{"kan", "kn_IN", "\u0c95\u0ca8\u0ccd\u0ca8\u0ca1"},
	{"kor", "ko_KR", "\ud55c\uad6d\ub9d0"},
	{"lav", "lv_LV", "Latvie\u0161u Valoda"},
	{"lit", "lt_LT", "Lietuvi\u0173 Kalba"},
	{"nld", "nl_NL", "Nederlands"},
	{"nor", "no_NO", "Norsk"},
	{"pol", "pl_PL", "Polski"},
	{"por", "pt_PT", "Portugu\u00eas"},
	{"ron", "ro_RO", "Rom\u00e2n\u0103"},
	{"rus", "ru_RU", "P\u0443\u0441\u0441\u043a\u0438\u0439 \u044f\u0437\u044b\u043a"},
	{"slk-frak", "sk_SK", "Sloven\u010dina"},
	{"slk", "sk_SK", "Sloven\u010dina"},
	{"slv", "sl_SI", "Sloven\u0161\u010dina"},
	{"spa", "es_ES", "Espa\u00f1ol"},
	{"srp", "sr_RS", "\u0441\u0440\u043f\u0441\u043a\u0438 \u0458\u0435\u0437\u0438\u043a"},
	{"swe-frak", "sv_SE", "Svenska (Frak)"},
	{"swe", "sv_SE", "Svenska"},
	{"tgl", "tl_PH", "Wikang Tagalog"},
	{"tur", "tr_TR", "T\u00fcrk\u00e7e"},
	{"ukr", "uk_UA", "Y\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430"},
	{"vie", "vi_VN", "Ti\u1ebfng Vi\u1ec7t"}
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

	for(Gtk::TreeView* view : {m_predefLangView, m_customLangView}){
		view->set_model(Gtk::ListStore::create(m_langViewCols));
		CONNECT(view, size_allocate, [this,view](Gtk::Allocation& a){ viewResizeCols(view, a); });
		view->append_column(_("Filename prefix"), m_langViewCols.prefix);
		view->append_column(_("Code"), m_langViewCols.code);
		view->append_column(_("Native name"), m_langViewCols.name);
		for(int i=0; i<3; ++i){
			view->get_column(i)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
		}
		view->set_fixed_height_mode();
	}

	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_predefLangView->get_model());
	for(const auto& lang : LANGUAGES){
		Gtk::TreeIter it = store->append();
		(*it)[m_langViewCols.prefix] = lang.prefix;
		(*it)[m_langViewCols.code] = lang.code;
		(*it)[m_langViewCols.name] = lang.name;
	}

	m_gioSettings = Gio::Settings::create(APPLICATION_ID);

	m_settings.insert(std::make_pair("outputorient", new ComboSetting("outputorient", m_gioSettings, "combo:config.settings.paneorient")));
	m_settings.insert(std::make_pair("systemoutputfont", new SwitchSettingT<Gtk::CheckButton>("systemoutputfont", m_gioSettings, "checkbutton:config.settings.defaultoutputfont")));
	m_settings.insert(std::make_pair("customoutputfont", new FontSetting("customoutputfont", m_gioSettings, "fontbutton:config.settings.customoutputfont")));
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
	CONNECTS(Builder("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>(), toggled, [](Gtk::CheckButton* btn){
		Builder("fontbutton:config.settings.customoutputfont").as<Gtk::FontButton>()->set_sensitive(!btn->get_active());
	});
	CONNECT(Builder("button:config.langs.custom.edit.add").as<Gtk::Button>(), clicked, [this]{ toggleAddLanguage(); });
	CONNECT(m_removeLangButton, clicked, [this]{ removeLanguage(); });
	CONNECT(Builder("button:config.langs.custom.add.ok").as<Gtk::Button>(), clicked, [this]{ addLanguage(); });
	CONNECT(Builder("button:config.langs.custom.add.cancel").as<Gtk::Button>(), clicked, [this]{ toggleAddLanguage(); });
	CONNECT(Builder("button:config.help").as<Gtk::Button>(), clicked, []{ MAIN->showHelp("#Usage_Options"); });
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

bool Config::searchLangSpec(const Glib::RefPtr<Gtk::TreeModel> model, Lang& lang) const
{
	Gtk::TreeIter it = std::find_if(model->children().begin(), model->children().end(),
		[this, &lang](const Gtk::TreeRow& row){ return row[m_langViewCols.prefix] == lang.prefix; });
	if(!it){
		return false;
	}
	lang = {(*it)[m_langViewCols.prefix], (*it)[m_langViewCols.code], (*it)[m_langViewCols.name]};
	return true;
}

void Config::updateLanguagesMenu()
{
	m_langsMenu.forall([this](Gtk::Widget& w){ m_langsMenu.remove(w); });
	m_curlang = Lang();
	m_radioGroup = Gtk::RadioButtonGroup();
	Gtk::RadioMenuItem* radioitem = nullptr;
	Gtk::RadioMenuItem* activeradio = nullptr;

	std::vector<Glib::ustring> parts = Utils::string_split(getSetting<VarSetting<Glib::ustring>>("language")->getValue(), ':');
	Lang curlang = {parts.empty() ? "eng" : parts[0], parts.size() < 2 ? "" : parts[1]};

	GtkSpell::Checker spell;
	std::vector<Glib::ustring> dicts = spell.get_language_list();

	tesseract::TessBaseAPI tess;
	Utils::initTess(tess, nullptr, nullptr);
	GenericVector<STRING> availLanguages;
	tess.GetAvailableLanguagesAsVector(&availLanguages);

	if(availLanguages.empty()){
		Utils::error_dialog(_("No languages available"), _("No tesseract languages are available for use. Recognition will not work."));
		m_langLabel->set_text("");
		return;
	}

	for(int i = 0; i < availLanguages.size(); ++i){
		Lang lang = {availLanguages[i].string()};
		if(!searchLangSpec(m_predefLangView->get_model(), lang) && !searchLangSpec(m_customLangView->get_model(), lang)){
			lang.name = lang.prefix;
		}
		std::vector<Glib::ustring> spelldicts;
		if(!lang.code.empty()){
			for(const Glib::ustring& dict : dicts){
				if(dict.substr(0, 2) == lang.code.substr(0, 2)){
					spelldicts.push_back(dict);
				}
			}
			std::sort(spelldicts.begin(), spelldicts.end());
		}
		if(!spelldicts.empty()){
			Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(lang.name));
			Gtk::Menu* submenu = Gtk::manage(new Gtk::Menu);
			for(const Glib::ustring& dict : spelldicts){
				radioitem = Gtk::manage(new Gtk::RadioMenuItem(m_radioGroup, spell.decode_language_code(dict)));
				Lang itemlang = {lang.prefix, dict, lang.name};
				Glib::ustring prettyname = lang.name + " (" + dict + ")";
				CONNECT(radioitem, toggled, [this, radioitem, itemlang, prettyname]{ setLanguage(radioitem, itemlang, prettyname); });
				if((curlang.prefix == lang.prefix) &&
				   (curlang.code.empty() || curlang.code == dict.substr(0, 2) || curlang.code == dict))
				{
					curlang = itemlang;
					radioitem->set_active(true);
					activeradio = radioitem;
				}
				submenu->append(*radioitem);
			}
			item->set_submenu(*submenu);
			m_langsMenu.append(*item);
		}else{
			radioitem = Gtk::manage(new Gtk::RadioMenuItem(m_radioGroup, lang.name));
			CONNECT(radioitem, toggled, [this, radioitem, lang]{ setLanguage(radioitem, lang, lang.name); });
			if(curlang.prefix == lang.prefix){
				curlang = lang;
				radioitem->set_active(true);
				activeradio = radioitem;
			}
			m_langsMenu.append(*radioitem);
		}
	}
	m_langsMenu.show_all();
	if(activeradio == nullptr){
		radioitem->set_active(true);
		activeradio = radioitem;
	}
	activeradio->toggled();
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
	if(m_addLangBox->get_visible()){
		toggleAddLanguage();
	}
}

void Config::setLanguage(const Gtk::RadioMenuItem* item, const Lang &lang, const Glib::ustring &prettyname)
{
	if(item->get_active()){
		m_langLabel->set_markup("<small>" + prettyname + "</small>");
		m_curlang = lang;
		getSetting<VarSetting<Glib::ustring>>("language")->setValue(lang.prefix + ":" + lang.code);
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
	m_addLangCode->set_text("");
	m_addLangName->set_text("");
	Utils::clear_error_state(m_addLangPrefix);
	Utils::clear_error_state(m_addLangCode);
	Utils::clear_error_state(m_addLangName);
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
		Gtk::TreeIter it = store->append();
		(*it)[m_langViewCols.prefix] = m_addLangPrefix->get_text();
		(*it)[m_langViewCols.code] = m_addLangCode->get_text();
		(*it)[m_langViewCols.name] = m_addLangName->get_text();
		getSetting<ListStoreSetting>("customlangs")->serialize();
		m_addLangPrefix->set_text("");
		m_addLangCode->set_text("");
		m_addLangName->set_text("");
		updateLanguagesMenu();
		toggleAddLanguage();
	}
}

void Config::removeLanguage()
{
	if(m_customLangView->get_selection()->count_selected_rows() != 0){
		Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_customLangView->get_model());
		store->erase(m_customLangView->get_selection()->get_selected());
		getSetting<ListStoreSetting>("customlangs")->serialize();
	}
}

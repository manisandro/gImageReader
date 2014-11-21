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
		view->append_column_editable(_("Filename prefix"), m_langViewCols.prefix);
		view->append_column_editable(_("Code"), m_langViewCols.code);
		view->append_column_editable(_("Native name"), m_langViewCols.name);
		for(int i=0; i<3; ++i){
			view->get_column(i)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
			view->get_column(i)->set_expand(true);
		}
		view->set_fixed_height_mode(true);
	}

	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_predefLangView->get_model());
	for(const auto& lang : LANGUAGES){
		Gtk::TreeIter it = store->append();
		(*it)[m_langViewCols.prefix] = lang.prefix;
		(*it)[m_langViewCols.code] = lang.code;
		(*it)[m_langViewCols.name] = lang.name;
	}

	CONNECTS(Builder("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>(), toggled, [](Gtk::CheckButton* btn){
		Builder("fontbutton:config.settings.customoutputfont").as<Gtk::FontButton>()->set_sensitive(!btn->get_active());
	});
	CONNECT(Builder("button:config.langs.custom.edit.add").as<Gtk::Button>(), clicked, [this]{ toggleAddLanguage(); });
	CONNECT(m_removeLangButton, clicked, [this]{ removeLanguage(); });
	CONNECT(Builder("button:config.langs.custom.add.ok").as<Gtk::Button>(), clicked, [this]{ addLanguage(); });
	CONNECT(Builder("button:config.langs.custom.add.cancel").as<Gtk::Button>(), clicked, [this]{ toggleAddLanguage(); });
	CONNECT(m_customLangView->get_selection(), changed, [this]{ langTableSelectionChanged(); });
	CONNECT(Builder("button:config.help").as<Gtk::Button>(), clicked, []{ MAIN->showHelp("#Usage_Options"); });
	CONNECT(m_addLangPrefix, focus_in_event, [this](GdkEventFocus*){ Utils::clear_error_state(m_addLangPrefix); return false; });
	CONNECT(m_addLangName, focus_in_event, [this](GdkEventFocus*){ Utils::clear_error_state(m_addLangName); return false; });
	CONNECT(m_addLangCode, focus_in_event, [this](GdkEventFocus*){ Utils::clear_error_state(m_addLangCode); return false; });

	addSetting(new SwitchSettingT<Gtk::CheckButton>("dictinstall", "check:config.settings.dictinstall"));
	addSetting(new SwitchSettingT<Gtk::CheckButton>("updatecheck", "check:config.settings.update"));
	addSetting(new ListStoreSetting("customlangs", Glib::RefPtr<Gtk::ListStore>::cast_static(m_customLangView->get_model())));
	addSetting(new SwitchSettingT<Gtk::CheckButton>("systemoutputfont", "checkbutton:config.settings.defaultoutputfont"));
	addSetting(new FontSetting("customoutputfont", "fontbutton:config.settings.customoutputfont"));
	addSetting(new ComboSetting("outputorient", "combo:config.settings.paneorient"));
}

Config::~Config()
{
	for(const auto& keyVal : m_settings){
		delete keyVal.second;
	}
}

bool Config::searchLangSpec(Lang& lang) const
{
	for(const Glib::RefPtr<Gtk::TreeModel>& model : {m_predefLangView->get_model(), m_customLangView->get_model()}){
		Gtk::TreeIter it = std::find_if(model->children().begin(), model->children().end(),
			[this, &lang](const Gtk::TreeRow& row){ return row[m_langViewCols.prefix] == lang.prefix; });
		if(it){
			lang = {(*it)[m_langViewCols.prefix], (*it)[m_langViewCols.code], (*it)[m_langViewCols.name]};
			return true;
		}
	}
	return false;
}

void Config::showDialog()
{
	toggleAddLanguage(true);
	while(m_dialog->run() == Gtk::RESPONSE_HELP);
	getSetting<ListStoreSetting>("customlangs")->serialize();
	m_dialog->hide();
}

void Config::toggleAddLanguage(bool forceHide)
{
	bool addVisible = forceHide ? true : m_addLangBox->get_visible();
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
		m_addLangPrefix->set_text("");
		m_addLangCode->set_text("");
		m_addLangName->set_text("");
		toggleAddLanguage();
	}
}

void Config::removeLanguage()
{
	if(m_customLangView->get_selection()->count_selected_rows() != 0){
		Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_customLangView->get_model());
		store->erase(m_customLangView->get_selection()->get_selected());
	}
}

void Config::langTableSelectionChanged()
{
	m_removeLangButton->set_sensitive(m_customLangView->get_selection()->count_selected_rows() != 0);
}

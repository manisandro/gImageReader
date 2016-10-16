/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.cc
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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

#include <tesseract/baseapi.h>

const std::vector<Config::Lang> Config::LANGUAGES = {
	// {ISO 639-2, ISO 639-1, name}
	{"afr",      "af", "Afrikaans"}, // Afrikaans
	{"amh",      "am", "\u12a0\u121b\u122d\u129b"}, // Amharic
	{"ara",      "ar", "\u0627\u0644\u0639\u0631\u0628\u064a\u0629"}, // Arabic
	{"asm",      "as", "\u0985\u09b8\u09ae\u09c0\u09af\u09bc\u09be"}, // Assamese
	{"aze",      "az", "\u0622\u0630\u0631\u0628\u0627\u06cc\u062c\u0627\u0646"}, // Azerbaijani
	{"aze_cyrl", "az", "Az\u0259rbaycan dili"}, // Azerbaijani (Cyrillic)
	{"bel",      "be", "\u0431\u0435\u043b\u0430\u0440\u0443\u0441\u043a\u0430\u044f \u043c\u043e\u0432\u0430"}, // Belarusian
	{"ben",      "bn", "\u09ac\u09be\u0982\u09b2\u09be"}, // Bengali
	{"bod",      "bo", "\u0f56\u0f7c\u0f51\u0f0b\u0f61\u0f72\u0f42"}, // "Tibetan (Standard)"
	{"bos",      "bs", "Bosanski jezik"}, // Bosnian
	{"bul",      "bg", "\u0431\u044a\u043b\u0433\u0430\u0440\u0441\u043a\u0438 \u0435\u0437\u0438\u043a"}, // Bulgarian
	{"cat",      "ca", "Catal\u00e0"}, // Catalan
	{"ceb",      "",   "Cebuano"}, // Cebuano
	{"ces",      "cs", "\u010ce\u0161tina"}, // Czech
	{"chi_sim",  "zh_CN", "\u7b80\u4f53\u5b57"}, // Chinese (Simplified)
	{"chi_tra",  "zh_TW", "\u7e41\u9ad4\u5b57"}, // Chinese (Traditional)
	{"chr",      "",   "\u13e3\u13b3\u13a9 \u13a6\u13ec\u13c2\u13af\u13cd\u13d7"}, // Cherokee
	{"cym",      "cy", "Cymraeg"}, // Welsh
	{"dan",      "da", "Dansk"}, // Danish
	{"dan_frak", "da", "Dansk (Fraktur)"}, // Danish (Fraktur)
	{"deu",      "de", "Deutsch"}, // German
	{"deu_frak", "de", "Deutsch (Fraktur)"}, // German (Fraktur)
	{"dzo",      "dz", "\u0f62\u0fab\u0f7c\u0f44\u0f0b\u0f41"}, // Dzongkha
	{"ell",      "el", "\u03b5\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac"}, // Greek
	{"eng",      "en", "English"},
	{"enm",      "en", "Middle English (1100-1500)"}, // Middle English (1100-1500)
	{"epo",      "eo", "Esperanto"}, // Esperanto
	{"equ",      "",   "Math / Equations"}, // Math / equation
	{"est",      "et", "Eesti keel"}, // Estonian
	{"eus",      "eu", "Euskara"}, // Basque
	{"fas",      "fa", "\u0641\u0627\u0631\u0633\u06cc"}, // Persian (Farsi)
	{"fin",      "fi", "Suomen kieli"}, // Finnish
	{"fra",      "fr", "Fran\u00e7ais"}, // French
	{"frk",      "",   "Frankish"}, // Frankish
	{"frm",      "fr", "Moyen fran\u00e7ais (ca. 1400-1600)"}, // Middle French (ca. 1400-1600)
	{"gle",      "ga", "Gaeilge"}, // Irish
	{"glg",      "gl", "Galego"}, // Galician
	{"grc",      "el", "\u1f19\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ae"}, // Ancient Greek
	{"guj",      "gu", "\u0a97\u0ac1\u0a9c\u0ab0\u0abe\u0aa4\u0ac0"}, // Gujarati
	{"hat",      "ht", "Krey\u00f2l ayisyen"}, // Haitian
	{"heb",      "he", "\u05e2\u05d1\u05e8\u05d9\u05ea"}, // Hebrew
	{"hin",      "hi", "\u0939\u093f\u0928\u094d\u0926\u0940"}, // Hindi
	{"hrv",      "hr", "Hrvatski"}, // Croatian
	{"hun",      "hu", "Magyar"}, // Hungarian
	{"iku",      "iu", "Inuktitut"}, // Inuktitut
	{"ind",      "id", "Bahasa indonesia"}, // Indonesian
	{"isl",      "is", "\u00cdslenska"}, // Icelandic
	{"ita",      "it", "Italiano"}, // Italian
	{"ita_old",  "it", "Italiano (Antico)"}, // Italian (Old)
	{"jav",      "jv", "Basa jawa"}, // Javanese
	{"jpn",      "ja", "\u65e5\u672c\u8a9e"}, // Japanese
	{"kan",      "kn", "\u0c95\u0ca8\u0ccd\u0ca8\u0ca1"}, // Kannada
	{"kat",      "ka", "\u10e5\u10d0\u10e0\u10d7\u10e3\u10da\u10d8"}, // Georgian
	{"kat_old",  "ka", "\u10d4\u10dc\u10d0\u10f2 \u10e5\u10d0\u10e0\u10d7\u10e3\u10da\u10d8"}, // Georgian (Old)
	{"kaz",      "kk", "\u049b\u0430\u0437\u0430\u049b \u0442\u0456\u043b\u0456"}, // Kazakh
	{"khm",      "km", "\u1781\u17d2\u1798\u17c2\u179a"}, // Khmer
	{"kir",      "ky", "\u041a\u044b\u0440\u0433\u044b\u0437\u0447\u0430"}, // Kyrgyz
	{"kor",      "ko", "\ud55c\uad6d\uc5b4"}, // Korean
	{"kur",      "ku", "\u0643\u0648\u0631\u062f\u06cc"}, // Kurdish
	{"lao",      "lo", "\u0e9e\u0eb2\u0eaa\u0eb2\u0ea5\u0eb2\u0ea7"}, // Lao
	{"lat",      "la", "Latina"}, // Latin
	{"lav",      "lv", "Latvie\u0161u valoda"}, // Latvian
	{"lit",      "lt", "Lietuvos"}, // Lithuanian
	{"mal",      "ml", "\u0d2e\u0d32\u0d2f\u0d3e\u0d33\u0d02"}, // Malayalam
	{"mar",      "mr", "\u092e\u0930\u093e\u0920\u0940"}, // Marathi
	{"mkd",      "mk", "\u043c\u0430\u043a\u0435\u0434\u043e\u043d\u0441\u043a\u0438 \u0458\u0430\u0437\u0438\u043a"}, // Macedonian
	{"mlt",      "mt", "Malti"}, // Maltese
	{"msa",      "ms", "Melayu"}, // Malay
	{"mya",      "my", "\u1017\u1019\u102c\u1005\u102c"}, // Burmese
	{"nep",      "ne", "\u0928\u0947\u092a\u093e\u0932\u0940"}, // Nepali
	{"nld",      "nl", "Nederlands"}, // Dutch
	{"nor",      "no", "Norsk"}, // Norwegian
	{"ori",      "or", "\u0b13\u0b21\u0b3c\u0b3f\u0b06"}, // Oriya
	{"pan",      "pa", "\u0a2a\u0a70\u0a1c\u0a3e\u0a2c\u0a40"}, // Panjabi
	{"pol",      "pl", "Polskie"}, // Polish
	{"por",      "pt", "Portugu\u00eas"}, // Portuguese
	{"pus",      "ps", "\u067e\u069a\u062a\u0648"}, // Pashto
	{"ron",      "ro", "Limba rom\u00e2n\u0103"}, // Romanian
	{"rus",      "ru", "\u0420\u0443\u0441\u0441\u043a\u0438\u0439"}, // Russian
	{"san",      "sa", "\u0938\u0902\u0938\u094d\u0915\u0943\u0924\u092e\u094d"}, // Sanskrit
	{"sin",      "si", "\u0dc3\u0dd2\u0d82\u0dc4\u0dbd"}, // Sinhala
	{"slk",      "sk", "Sloven\u010dina"}, // Slovak
	{"slk_frak", "sk", "Sloven\u010dina (Frakt\u00far)"}, // Slovak (Fraktur)
	{"slv",      "sl", "Sloven\u0161\u010dina"}, // Slovene
	{"spa",      "es", "Espa\u00f1ol"}, // Spanish
	{"spa_old",  "es", "Espa\u00f1ol (Antiguo)"}, // Spanish (Old)
	{"sqi",      "sq", "Shqip"}, // Albanian
	{"srp",      "sr", "\u0441\u0440\u043f\u0441\u043a\u0438 \u0458\u0435\u0437\u0438\u043a"}, // Serbian
	{"srp_latn", "sr", "Srpski"}, // Serbian (Latin)
	{"swa",      "sw", "Swahili"}, // Swahili
	{"swe",      "sv", "Svenska"}, // Swedish
	{"syr",      "",   "\u0720\u072b\u0722\u0710 \u0723\u0718\u072a\u071d\u071d\u0710"}, // Syriac
	{"tam",      "ta", "\u0ba4\u0bae\u0bbf\u0bb4\u0bcd"}, // Tamil
	{"tel",      "te", "\u0c24\u0c46\u0c32\u0c41\u0c17\u0c41"}, // Telugu
	{"tgk",      "tg", "\u0442\u043e\u04b7\u0438\u043a\u04e3"}, // Tajik
	{"tgl",      "tl", "Tagalog"}, // Tagalog
	{"tha",      "th", "\u0e44\u0e17\u0e22"}, // Thai
	{"tir",      "ti", "\u1275\u130d\u122d\u129b"}, // Tigrinya
	{"tur",      "tr", "T\u00fcrk\u00e7e"}, // Turkish
	{"uig",      "ug", "\u0626\u06c7\u064a\u063a\u06c7\u0631\u0686\u06d5\u200e"}, // Uyghur
	{"ukr",      "uk", "\u0443\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430 \u043c\u043e\u0432\u0430"}, // Ukrainian
	{"urd",      "ur", "\u0627\u0631\u062f\u0648"}, // Urdu
	{"uzb",      "uz", "O\u02bbzbek"}, // Uzbek
	{"uzb_cyrl", "uz", "\u040e\u0437\u0431\u0435\u043a"}, // Uzbek (Cyrillic)
	{"vie",      "vi", "Vi\u1ec7t Nam"}, // Vietnamese
	{"yid",      "yi", "\u05d9\u05d9\u05b4\u05d3\u05d9\u05e9"}, // Yiddish
};

std::multimap<Glib::ustring,Glib::ustring> Config::buildLanguageCultureTable() {
	std::multimap<Glib::ustring,Glib::ustring> map;
	map.insert(std::make_pair("af", "af_ZA"));
	map.insert(std::make_pair("ar", "ar_AE"));
	map.insert(std::make_pair("ar", "ar_BH"));
	map.insert(std::make_pair("ar", "ar_DZ"));
	map.insert(std::make_pair("ar", "ar_EG"));
	map.insert(std::make_pair("ar", "ar_IQ"));
	map.insert(std::make_pair("ar", "ar_JO"));
	map.insert(std::make_pair("ar", "ar_KW"));
	map.insert(std::make_pair("ar", "ar_LB"));
	map.insert(std::make_pair("ar", "ar_LY"));
	map.insert(std::make_pair("ar", "ar_MA"));
	map.insert(std::make_pair("ar", "ar_OM"));
	map.insert(std::make_pair("ar", "ar_QA"));
	map.insert(std::make_pair("ar", "ar_SA"));
	map.insert(std::make_pair("ar", "ar_SY"));
	map.insert(std::make_pair("ar", "ar_TN"));
	map.insert(std::make_pair("ar", "ar_YE"));
	map.insert(std::make_pair("az", "az_AZ"));
	map.insert(std::make_pair("az", "az_AZ"));
	map.insert(std::make_pair("be", "be_BY"));
	map.insert(std::make_pair("bg", "bg_BG"));
	map.insert(std::make_pair("ca", "ca_ES"));
	map.insert(std::make_pair("cs", "cs_CZ"));
	map.insert(std::make_pair("da", "da_DK"));
	map.insert(std::make_pair("de", "de_AT"));
	map.insert(std::make_pair("de", "de_CH"));
	map.insert(std::make_pair("de", "de_DE"));
	map.insert(std::make_pair("de", "de_LI"));
	map.insert(std::make_pair("de", "de_LU"));
	map.insert(std::make_pair("div", "div_MV)"));
	map.insert(std::make_pair("el", "el_GR"));
	map.insert(std::make_pair("en", "en_AU"));
	map.insert(std::make_pair("en", "en_BZ"));
	map.insert(std::make_pair("en", "en_CA"));
	map.insert(std::make_pair("en", "en_CB"));
	map.insert(std::make_pair("en", "en_GB"));
	map.insert(std::make_pair("en", "en_IE"));
	map.insert(std::make_pair("en", "en_JM"));
	map.insert(std::make_pair("en", "en_NZ"));
	map.insert(std::make_pair("en", "en_PH"));
	map.insert(std::make_pair("en", "en_TT"));
	map.insert(std::make_pair("en", "en_US"));
	map.insert(std::make_pair("en", "en_ZA"));
	map.insert(std::make_pair("en", "en_ZW"));
	map.insert(std::make_pair("es", "es_AR"));
	map.insert(std::make_pair("es", "es_BO"));
	map.insert(std::make_pair("es", "es_CL"));
	map.insert(std::make_pair("es", "es_CO"));
	map.insert(std::make_pair("es", "es_CR"));
	map.insert(std::make_pair("es", "es_DO"));
	map.insert(std::make_pair("es", "es_EC"));
	map.insert(std::make_pair("es", "es_ES"));
	map.insert(std::make_pair("es", "es_GT"));
	map.insert(std::make_pair("es", "es_HN"));
	map.insert(std::make_pair("es", "es_MX"));
	map.insert(std::make_pair("es", "es_NI"));
	map.insert(std::make_pair("es", "es_PA"));
	map.insert(std::make_pair("es", "es_PE"));
	map.insert(std::make_pair("es", "es_PR"));
	map.insert(std::make_pair("es", "es_PY"));
	map.insert(std::make_pair("es", "es_SV"));
	map.insert(std::make_pair("es", "es_UY"));
	map.insert(std::make_pair("es", "es_VE"));
	map.insert(std::make_pair("et", "et_EE"));
	map.insert(std::make_pair("eu", "eu_ES"));
	map.insert(std::make_pair("fa", "fa_IR"));
	map.insert(std::make_pair("fi", "fi_FI"));
	map.insert(std::make_pair("fo", "fo_FO"));
	map.insert(std::make_pair("fr", "fr_BE"));
	map.insert(std::make_pair("fr", "fr_CA"));
	map.insert(std::make_pair("fr", "fr_CH"));
	map.insert(std::make_pair("fr", "fr_FR"));
	map.insert(std::make_pair("fr", "fr_LU"));
	map.insert(std::make_pair("fr", "fr_MC"));
	map.insert(std::make_pair("gl", "gl_ES"));
	map.insert(std::make_pair("gu", "gu_IN"));
	map.insert(std::make_pair("he", "he_IL"));
	map.insert(std::make_pair("hi", "hi_IN"));
	map.insert(std::make_pair("hr", "hr_HR"));
	map.insert(std::make_pair("hu", "hu_HU"));
	map.insert(std::make_pair("hy", "hy_AM"));
	map.insert(std::make_pair("id", "id_ID"));
	map.insert(std::make_pair("is", "is_IS"));
	map.insert(std::make_pair("it", "it_CH"));
	map.insert(std::make_pair("it", "it_IT"));
	map.insert(std::make_pair("ja", "ja_JP"));
	map.insert(std::make_pair("ka", "ka_GE"));
	map.insert(std::make_pair("kk", "kk_KZ"));
	map.insert(std::make_pair("kn", "kn_IN"));
	map.insert(std::make_pair("kok", "kok_IN"));
	map.insert(std::make_pair("ko", "ko_KR"));
	map.insert(std::make_pair("ky", "ky_KZ"));
	map.insert(std::make_pair("lt", "lt_LT"));
	map.insert(std::make_pair("lv", "lv_LV"));
	map.insert(std::make_pair("mk", "mk_MK"));
	map.insert(std::make_pair("mn", "mn_MN"));
	map.insert(std::make_pair("mr", "mr_IN"));
	map.insert(std::make_pair("ms", "ms_BN"));
	map.insert(std::make_pair("ms", "ms_MY"));
	map.insert(std::make_pair("nb", "nb_NO"));
	map.insert(std::make_pair("nl", "nl_BE"));
	map.insert(std::make_pair("nl", "nl_NL"));
	map.insert(std::make_pair("nn", "nn_NO"));
	map.insert(std::make_pair("pa", "pa_IN"));
	map.insert(std::make_pair("pl", "pl_PL"));
	map.insert(std::make_pair("pt", "pt_BR"));
	map.insert(std::make_pair("pt", "pt_PT"));
	map.insert(std::make_pair("ro", "ro_RO"));
	map.insert(std::make_pair("ru", "ru_RU"));
	map.insert(std::make_pair("sa", "sa_IN"));
	map.insert(std::make_pair("sk", "sk_SK"));
	map.insert(std::make_pair("sl", "sl_SI"));
	map.insert(std::make_pair("sq", "sq_AL"));
	map.insert(std::make_pair("sr", "sr_SP"));
	map.insert(std::make_pair("sr", "sr_SP"));
	map.insert(std::make_pair("sv", "sv_FI"));
	map.insert(std::make_pair("sv", "sv_SE"));
	map.insert(std::make_pair("sw", "sw_KE"));
	map.insert(std::make_pair("syr", "syr_SY"));
	map.insert(std::make_pair("ta", "ta_IN"));
	map.insert(std::make_pair("te", "te_IN"));
	map.insert(std::make_pair("th", "th_TH"));
	map.insert(std::make_pair("tr", "tr_TR"));
	map.insert(std::make_pair("tt", "tt_RU"));
	map.insert(std::make_pair("uk", "uk_UA"));
	map.insert(std::make_pair("ur", "ur_PK"));
	map.insert(std::make_pair("uz", "uz_UZ"));
	map.insert(std::make_pair("uz", "uz_UZ"));
	map.insert(std::make_pair("vi", "vi_VN"));
	map.insert(std::make_pair("zh", "zh_CHS"));
	map.insert(std::make_pair("zh", "zh_CHT"));
	map.insert(std::make_pair("zh", "zh_CN"));
	map.insert(std::make_pair("zh", "zh_HK"));
	map.insert(std::make_pair("zh", "zh_MO"));
	map.insert(std::make_pair("zh", "zh_SG"));
	map.insert(std::make_pair("zh", "zh_TW"));
	return map;
}

const std::multimap<Glib::ustring,Glib::ustring> Config::LANGUAGE_CULTURES = Config::buildLanguageCultureTable();

Config::Config() {
	m_dialog = MAIN->getWidget("dialog:config");
	m_addLangBox = MAIN->getWidget("box:config.langs.custom.add");
	m_addLangPrefix = MAIN->getWidget("entry:config.langs.custom.add.prefix");
	m_addLangName = MAIN->getWidget("entry:config.langs.custom.add.name");
	m_addLangCode = MAIN->getWidget("entry:config.langs.custom.add.code");
	m_editLangBox = MAIN->getWidget("buttonbox:config.langs.custom.edit");
	m_addLangButton = MAIN->getWidget("button:config.langs.custom.edit.add");
	m_addLangButtonOk = MAIN->getWidget("button:config.langs.custom.add.ok");
	m_removeLangButton = MAIN->getWidget("button:config.langs.custom.edit.remove");
	m_predefLangView = MAIN->getWidget("treeview:config.langs.predef");
	m_customLangView = MAIN->getWidget("treeview:config.langs.custom");
	m_dialogOkButton = MAIN->getWidget("button:config.ok");

	for(Gtk::TreeView* view : {
	            m_predefLangView, m_customLangView
	        }) {
		view->set_model(Gtk::ListStore::create(m_langViewCols));
		view->append_column_editable(_("Filename prefix"), m_langViewCols.prefix);
		view->append_column_editable(_("Code"), m_langViewCols.code);
		view->append_column_editable(_("Native name"), m_langViewCols.name);
		for(int i=0; i<3; ++i) {
			view->get_column(i)->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
			view->get_column(i)->set_expand(true);
		}
		view->set_fixed_height_mode(true);
	}

	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_predefLangView->get_model());
	for(const auto& lang : LANGUAGES) {
		Gtk::TreeIter it = store->append();
		(*it)[m_langViewCols.prefix] = lang.prefix;
		(*it)[m_langViewCols.code] = lang.code;
		(*it)[m_langViewCols.name] = lang.name;
	}

	CONNECTS(MAIN->getWidget("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>(), toggled, [](Gtk::CheckButton* btn) {
		MAIN->getWidget("fontbutton:config.settings.customoutputfont").as<Gtk::FontButton>()->set_sensitive(!btn->get_active());
	});
	CONNECT(m_addLangButton, clicked, [this] { toggleAddLanguage(); });
	CONNECT(m_removeLangButton, clicked, [this] { removeLanguage(); });
	CONNECT(m_addLangButtonOk, clicked, [this] { addLanguage(); });
	CONNECT(MAIN->getWidget("button:config.langs.custom.add.cancel").as<Gtk::Button>(), clicked, [this] { toggleAddLanguage(); });
	CONNECT(m_customLangView->get_selection(), changed, [this] { langTableSelectionChanged(); });
	CONNECT(MAIN->getWidget("button:config.help").as<Gtk::Button>(), clicked, [] { MAIN->showHelp("#Usage_Options"); });
	CONNECT(m_addLangPrefix, focus_in_event, [this](GdkEventFocus*) {
		Utils::clear_error_state(m_addLangPrefix);
		return false;
	});
	CONNECT(m_addLangName, focus_in_event, [this](GdkEventFocus*) {
		Utils::clear_error_state(m_addLangName);
		return false;
	});
	CONNECT(m_addLangCode, focus_in_event, [this](GdkEventFocus*) {
		Utils::clear_error_state(m_addLangCode);
		return false;
	});
	CONNECTS(MAIN->getWidget("combo:config.datadirs").as<Gtk::ComboBox>(), changed, [this](Gtk::ComboBox* combo) {
		setDataLocations(combo->get_active_row_number());
	});

	addSetting(new SwitchSettingT<Gtk::CheckButton>("dictinstall", MAIN->getWidget("check:config.settings.dictinstall")));
	addSetting(new SwitchSettingT<Gtk::CheckButton>("updatecheck", MAIN->getWidget("check:config.settings.update")));
	addSetting(new ListStoreSetting("customlangs", Glib::RefPtr<Gtk::ListStore>::cast_static(m_customLangView->get_model())));
	addSetting(new SwitchSettingT<Gtk::CheckButton>("systemoutputfont", MAIN->getWidget("checkbutton:config.settings.defaultoutputfont")));
	addSetting(new FontSetting("customoutputfont", MAIN->getWidget("fontbutton:config.settings.customoutputfont")));
	addSetting(new ComboSetting("outputorient", MAIN->getWidget("combo:config.settings.paneorient")));
	addSetting(new ComboSetting("datadirs", MAIN->getWidget("combo:config.datadirs")));
}

Config::~Config() {
	for(const auto& keyVal : m_settings) {
		delete keyVal.second;
	}
}

bool Config::searchLangSpec(Lang& lang) const {
	for(const Glib::RefPtr<Gtk::TreeModel>& model : {
	m_predefLangView->get_model(), m_customLangView->get_model()
	}) {
		Gtk::TreeIter it = std::find_if(model->children().begin(), model->children().end(),
		[this, &lang](const Gtk::TreeRow& row) {
			return row[m_langViewCols.prefix] == lang.prefix;
		});
		if(it) {
			lang = {(*it)[m_langViewCols.prefix], (*it)[m_langViewCols.code], (*it)[m_langViewCols.name]};
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
	return MAIN->getWidget("combo:config.datadirs").as<Gtk::ComboBox>()->get_active_row_number() == 0;
}

std::string Config::tessdataLocation() const {
	return MAIN->getWidget("entry:config.tessdatadir").as<Gtk::Entry>()->get_text();
}

std::string Config::spellingLocation() const {
	return MAIN->getWidget("entry:config.spelldir").as<Gtk::Entry>()->get_text();
}

void Config::showDialog() {
	toggleAddLanguage(true);
	while(m_dialog->run() == Gtk::RESPONSE_HELP);
	getSetting<ListStoreSetting>("customlangs")->serialize();
	m_dialog->hide();
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
		MAIN->getWidget("entry:config.spelldir").as<Gtk::Entry>()->set_text(dictDir->get_path());
	} else {
		std::string configDir = Glib::get_user_config_dir();
		Glib::setenv("TESSDATA_PREFIX", Glib::build_filename(configDir, "tessdata"));
		MAIN->getWidget("entry:config.spelldir").as<Gtk::Entry>()->set_text(Glib::build_filename(configDir, "enchant", "myspell"));
	}
	tesseract::TessBaseAPI tess;
	std::string current = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	tess.Init(nullptr, nullptr);
	setlocale(LC_NUMERIC, current.c_str());
	MAIN->getWidget("entry:config.tessdatadir").as<Gtk::Entry>()->set_text(tess.GetDatapath());
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
	bool addVisible = forceHide ? true : m_addLangBox->get_visible();
	m_addLangBox->set_visible(!addVisible);
	m_editLangBox->set_visible(addVisible);
	if(addVisible) {
		m_addLangButton->grab_focus();
	} else {
		m_addLangButtonOk->grab_focus();
	}
	m_addLangPrefix->set_text("");
	m_addLangCode->set_text("");
	m_addLangName->set_text("");
	Utils::clear_error_state(m_addLangPrefix);
	Utils::clear_error_state(m_addLangCode);
	Utils::clear_error_state(m_addLangName);
}

void Config::addLanguage() {
	bool invalid = false;
	if(!Glib::Regex::create("^\\w+$")->match(m_addLangPrefix->get_text())) {
		invalid = true;
		Utils::set_error_state(m_addLangPrefix);
	}
	if(!Glib::Regex::create("^.+$")->match(m_addLangName->get_text())) {
		invalid = true;
		Utils::set_error_state(m_addLangName);
	}
	if(!Glib::Regex::create("^[a-z]{2}$")->match(m_addLangCode->get_text()) &&
	        !Glib::Regex::create("^[a-z]{2}_[A-Z]{2}$")->match(m_addLangCode->get_text())) {
		invalid = true;
		Utils::set_error_state(m_addLangCode);
	}
	if(!invalid) {
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

void Config::removeLanguage() {
	if(m_customLangView->get_selection()->count_selected_rows() != 0) {
		Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_customLangView->get_model());
		store->erase(m_customLangView->get_selection()->get_selected());
	}
}

void Config::langTableSelectionChanged() {
	m_removeLangButton->set_sensitive(m_customLangView->get_selection()->count_selected_rows() != 0);
}

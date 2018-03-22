/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.cc
 * Copyright (C) (\d+)-2018 Sandro Mani <manisandro@gmail.com>
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
#include "MainWindow.hh"
#include "Utils.hh"

#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

const std::vector<Config::Lang> Config::LANGUAGES = {
	// {ISO 639-2, ISO 639-1, name}
	{"afr",      "af", "Afrikaans"}, // Afrikaans
	{"amh",      "am", "\u12A3\u121B\u122D\u129B"}, // Amharic
	{"ara",      "ar", "\u0627\u0644\u0644\u063A\u0629 \u0627\u0644\u0639\u0631\u0628\u064A\u0629"}, // Arabic
	{"asm",      "as", "\u0985\u09B8\u09AE\u09C0\u09AF\u09BC\u09BE"}, // Assamese
	{"aze",      "az", "\u0622\u0630\u0631\u0628\u0627\u06CC\u062C\u0627\u0646 \u062F\u06CC\u0644\u06CC"}, // Azerbaijani
	{"aze_cyrl", "az", "Az\u0259rbaycanca"}, // Azerbaijani (Cyrillic)
	{"bel",      "be", "\u0411\u0435\u043B\u0430\u0440\u0443\u0441\u043A\u0430\u044F"}, // Belarusian
	{"ben",      "bn", "\u09AC\u09BE\u0982\u09B2\u09BE"}, // Bengali
	{"bod",      "bo", "\u0F51\u0F56\u0F74\u0F66\u0F0B\u0F66\u0F90\u0F51\u0F0B"}, // "Tibetan (Standard)"
	{"bos",      "bs", "Bosanski"}, // Bosnian
	{"bre",      "bs", "Brezhoneg"}, // Breton
	{"bul",      "bg", "\u0431\u044A\u043B\u0433\u0430\u0440\u0441\u043A\u0438 \u0435\u0437\u0438\u043A"}, // Bulgarian
	{"cat",      "ca", "Catal\u00E0"}, // Catalan
	{"ceb",      "",   "Bisaya"}, // Cebuano
	{"ces",      "cs", "\u010Ce\u0161tina"}, // Czech
	{"chi_sim",  "zh_CN", "\u7b80\u4f53\u5b57 "}, // Chinese (Simplified)
	{"chi_sim_vert",  "zh_CN", "\u7b80\u4f53\u5b57 \u7EB5"}, // Chinese (Simplified, Vertical)
	{"chi_tra",  "zh_TW", "\u7e41\u9ad4\u5b57"}, // Chinese (Traditional)
	{"chi_tra_vert",  "zh_TW", "\u7e41\u9ad4\u5b57 \u7EB5"}, // Chinese (Traditional, Vertical)
	{"chr",      "",   "\u13E3\u13B3\u13A9"}, // Cherokee
	{"cos",      "co", "Corsu"}, // Corsican
	{"cym",      "cy", "Cymraeg"}, // Welsh
	{"dan",      "da", "Dansk"}, // Danish
	{"dan_frak", "da", "Dansk (Fraktur)"}, // Danish (Fraktur)
	{"deu",      "de", "Deutsch"}, // German
	{"deu_frak", "de", "Deutsch (Fraktur)"}, // German (Fraktur)
	{"div",      "dv", "\u078B\u07A8\u0788\u07AC\u0780\u07A8"}, // Dhivehi
	{"dzo",      "dz", "\u0F62\u0FAB\u0F7C\u0F44\u0F0B\u0F41\u0F0B"}, // Dzongkha
	{"ell",      "el", "\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC"}, // Greek
	{"eng",      "en", "English"},
	{"enm",      "en", "Middle English (1100-1500)"}, // Middle English (1100-1500)
	{"epo",      "eo", "Esperanto"}, // Esperanto
	{"equ",      "",   "Math / Equations"}, // Math / equation
	{"est",      "et", "Eesti"}, // Estonian
	{"eus",      "eu", "Euskara"}, // Basque
	{"fas",      "fa", "\u0641\u0627\u0631\u0633\u06CC"}, // Persian (Farsi)
	{"fao",      "fo", "F\u00F8royskt"}, // Faroese
	{"fil",      "",   "Wikang Filipino"}, // Filipino
	{"fin",      "fi", "Suomi"}, // Finnish
	{"fra",      "fr", "Fran\u00E7ais"}, // French
	{"frk",      "",   "Frankish"}, // Frankish
	{"frm",      "fr", "Moyen fran\u00E7ais (ca. 1400-1600)"}, // Middle French (ca. 1400-1600)
	{"fry",      "fy", "Frysk"}, // Frisian (West)
	{"gla",      "gd", "G\u00E0idhlig"}, // Scottish Gaelic
	{"gle",      "ga", "Gaeilge"}, // Irish
	{"glg",      "gl", "Galego"}, // Galician
	{"grc",      "el", "\u1f19\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ae"}, // Ancient Greek
	{"guj",      "gu", "\u0A97\u0AC1\u0A9C\u0AB0\u0ABE\u0AA4\u0AC0"}, // Gujarati
	{"hat",      "ht", "Krey\u00F2l Ayisyen"}, // Haitian
	{"heb",      "he", "\u05E2\u05D1\u05E8\u05D9\u05EA\u202C"}, // Hebrew
	{"hin",      "hi", "\u0939\u093F\u0928\u094D\u0926\u0940"}, // Hindi
	{"hrv",      "hr", "Hrvatski"}, // Croatian
	{"hun",      "hu", "Magyar"}, // Hungarian
	{"hye",      "hy", "\u0540\u0561\u0575\u0565\u0580\u0565\u0576"}, // Armenian
	{"iku",      "iu", "\u1403\u14C4\u1483\u144E\u1450\u1466"}, // Inuktitut
	{"ind",      "id", "Bahasa Indonesia"}, // Indonesian
	{"isl",      "is", "\u00CDslenska"}, // Icelandic
	{"ita",      "it", "Italiano"}, // Italian
	{"ita_old",  "it", "Italiano (Antico)"}, // Italian (Old)
	{"jav",      "jv", "Basa jawa"}, // Javanese
	{"jpn",      "ja", "\uA9A7\uA9B1\uA997\uA9AE"}, // Japanese
	{"jpn_vert", "ja", "\uA9A7\uA9B1\uA997\uA9AE \u5782\u76F4"}, // Japanese (Vertical)
	{"kan",      "kn", "\u0C95\u0CA8\u0CCD\u0CA8\u0CA1"}, // Kannada
	{"kat",      "ka", "\u10E5\u10D0\u10E0\u10D7\u10E3\u10DA\u10D8"}, // Georgian
	{"kat_old",  "ka", "\u10d4\u10dc\u10d0\u10f2 \u10e5\u10d0\u10e0\u10d7\u10e3\u10da\u10d8"}, // Georgian (Old)
	{"kaz",      "kk", "\u049A\u0430\u0437\u0430\u049B T\u0456\u043B\u0456"}, // Kazakh
	{"khm",      "km", "\u1797\u17B6\u179F\u17B6\u1781\u17D2\u1798\u17C2\u179A"}, // Khmer
	{"kir",      "ky", "\u041a\u044b\u0440\u0433\u044b\u0437\u0447\u0430"}, // Kyrgyz
	{"kor",      "ko", "\uD55C\uAD6D\uC5B4"}, // Korean
	{"kor_vert", "ko", "\uD55C\uAD6D\uC5B4 \uC218\uC9C1\uC120"}, // Korean (Vertical)
	{"kur",      "ku", "Kurd\u00ED"}, // Kurdish
	{"kur_ara",  "ku", "\u06A9\u0648\u0631\u062F\u06CC"}, // Kurdish (Arabic)
	{"lao",      "lo", "\u0E9E\u0EB2\u0EAA\u0EB2\u0EA5\u0EB2\u0EA7"}, // Lao
	{"lat",      "la", "Latina"}, // Latin
	{"lav",      "lv", "Latvie\u0161u"}, // Latvian
	{"lit",      "lt", "Lietuvi\u0173"}, // Lithuanian
	{"ltz",      "lb", "L\u00EBtzebuergesch"}, // Luxembourgish
	{"mal",      "ml", "\u0D2E\u0D32\u0D2F\u0D3E\u0D33\u0D02"}, // Malayalam
	{"mar",      "mr", "\u092E\u0930\u093E\u0920\u0940"}, // Marathi
	{"mkd",      "mk", "M\u0430\u043A\u0435\u0434\u043E\u043D\u0441\u043A\u0438"}, // Macedonian
	{"mlt",      "mt", "Malti"}, // Maltese
	{"mon",      "mn", "\u041C\u043E\u043D\u0433\u043E\u043B \u0425\u044D\u043B"}, // Mongolian
	{"mri",      "mi", "te Reo M\u0101ori"}, // Maori
	{"msa",      "ms", "Bahasa Melayu"}, // Malay
	{"mya",      "my", "\u1019\u103C\u1014\u103A\u1019\u102C\u1005\u102C"}, // Burmese
	{"nep",      "ne", "\u0928\u0947\u092A\u093E\u0932\u0940"}, // Nepali
	{"nld",      "nl", "Nederlands"}, // Dutch
	{"nor",      "no", "Norsk"}, // Norwegian
	{"oci",      "oc", "Occitan"}, // Occitan
	{"ori",      "or", "\u0b13\u0b21\u0b3c\u0b3f\u0b06"}, // Oriya
	{"pan",      "pa", "\u0A2A\u0A70\u0A1C\u0A3E\u0A2C\u0A40"}, // Punjabi
	{"pol",      "pl", "Polski"}, // Polish
	{"por",      "pt", "Portugu\u00EAs"}, // Portuguese
	{"pus",      "ps", "\u067E\u069A\u062A\u0648"}, // Pashto
	{"que",      "qu", "Runasimi"}, // Quechuan
	{"ron",      "ro", "Rom\u00E2n\u0103"}, // Romanian
	{"rus",      "ru", "\u0420\u0443\u0441\u0441\u043A\u0438\u0439"}, // Russian
	{"san",      "sa", "\u0938\u0902\u0938\u094D\u0915\u0943\u0924\u092E\u094D"}, // Sanskrit
	{"sin",      "si", "\u0DC3\u0DD2\u0D82\u0DC4\u0DBD"}, // Sinhala
	{"slk",      "sk", "Sloven\u010Dina"}, // Slovak
	{"slk_frak", "sk", "Sloven\u010Dina (Frakt\u00far)"}, // Slovak (Fraktur)
	{"slv",      "sl", "Sloven\u0161\u010Dina"}, // Slovene
	{"snd",      "sd", "\u0633\u0646\u068C\u064A"}, // Sindhi
	{"spa",      "es", "Espa\u00F1ol"}, // Spanish
	{"spa_old",  "es", "Espa\u00F1ol (Antiguo)"}, // Spanish (Old)
	{"sqi",      "sq", "Shqip"}, // Albanian
	{"srp",      "sr", "\u0421\u0440\u043F\u0441\u043A\u0438"}, // Serbian
	{"srp_latn", "sr", "Srpski"}, // Serbian (Latin)
	{"sun",      "su", "Sudanese"}, // Sudanese
	{"swa",      "sw", "Kiswahili"}, // Swahili
	{"swe",      "sv", "Svenska"}, // Swedish
	{"syr",      "",   "\u0720\u072b\u0722\u0710 \u0723\u0718\u072a\u071d\u071d\u0710"}, // Syriac
	{"tam",      "ta", "\u0BA4\u0BAE\u0BBF\u0BB4\u0BCD"}, // Tamil
	{"tat",      "tt", "Tatar\u00E7a"}, // Tatar
	{"tel",      "te", "\u0C24\u0C46\u0C32\u0C41\u0C17\u0C41"}, // Telugu
	{"tgk",      "tg", "\u0442\u043e\u04b7\u0438\u043a\u04e3"}, // Tajik
	{"tgl",      "tl", "\u170A\u170A\u170C\u1712"}, // Tagalog
	{"tha",      "th", "\u0E20\u0E32\u0E29\u0E32\u0E44\u0E17\u0E22"}, // Thai
	{"tir",      "ti", "\u1275\u130D\u122D\u129B"}, // Tigrinya
	{"ton",      "to", "Lea faka-Tonga"}, // Tongan
	{"tur",      "tr", "T\u00FCrk\u00E7e"}, // Turkish
	{"uig",      "ug", "\u0626\u06C7\u064A\u063A\u06C7\u0631 \u062A\u0649\u0644\u0649"}, // Uyghur
	{"ukr",      "uk", "\u0423\u043A\u0440\u0430\u0457\u043D\u0441\u044C\u043A\u0430"}, // Ukrainian
	{"urd",      "ur", "\u0627\u064F\u0631\u062F\u064F\u0648"}, // Urdu
	{"uzb",      "uz", "\u0627\u0648\u0632\u0628\u06CC\u06A9"}, // Uzbek
	{"uzb_cyrl", "uz", "\u040E\u0437\u0431\u0435\u043A"}, // Uzbek (Cyrillic)
	{"vie",      "vi", "Ti\u1EBFng Vi\u1EC7t Nam"}, // Vietnamese
	{"yid",      "yi", "\u05D9\u05D9\u05B4\u05D3\u05D9\u05E9 \u05D9\u05D9\u05B4\u05D3\u05D9\u05E9\u202C"}, // Yiddish
	{"yor",      "yo", "\u00C8d\u00E8 Yor\u00F9b\u00E1"}, // Yoruba
};

std::multimap<Glib::ustring, Glib::ustring> Config::buildLanguageCultureTable() {
	std::multimap<Glib::ustring, Glib::ustring> map;
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

const std::multimap<Glib::ustring, Glib::ustring> Config::LANGUAGE_CULTURES = Config::buildLanguageCultureTable();

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
	for(const Glib::RefPtr<Gtk::TreeModel>& model : {
	            ui.treeviewLangsPredef->get_model(), ui.treeviewLangsCustom->get_model()
	        }) {
		Gtk::TreeIter it = std::find_if(model->children().begin(), model->children().end(),
		[this, &lang](const Gtk::TreeRow & row) {
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

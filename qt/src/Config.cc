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
#include "MainWindow.hh"
#include "Utils.hh"

#include <QDesktopServices>
#include <QDir>
#include <QMultiMap>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QStandardPaths>
#endif
#include <QUrl>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

// See https://en.wikipedia.org/wiki/List_of_language_names
const QList<Config::Lang> Config::LANGUAGES = {
	// {ISO 639-2, ISO 639-1, name}
	{"afr",      "af", QString::fromUtf8("Afrikaans")}, // Afrikaans
	{"amh",      "am", QString::fromUtf8("\u12A3\u121B\u122D\u129B")}, // Amharic
	{"ara",      "ar", QString::fromUtf8("\u0627\u0644\u0644\u063A\u0629 \u0627\u0644\u0639\u0631\u0628\u064A\u0629")}, // Arabic
	{"asm",      "as", QString::fromUtf8("\u0985\u09B8\u09AE\u09C0\u09AF\u09BC\u09BE")}, // Assamese
	{"aze",      "az", QString::fromUtf8("\u0622\u0630\u0631\u0628\u0627\u06CC\u062C\u0627\u0646 \u062F\u06CC\u0644\u06CC")}, // Azerbaijani
	{"aze_cyrl", "az", QString::fromUtf8("Az\u0259rbaycanca")}, // Azerbaijani (Cyrillic)
	{"bel",      "be", QString::fromUtf8("\u0411\u0435\u043B\u0430\u0440\u0443\u0441\u043A\u0430\u044F")}, // Belarusian
	{"ben",      "bn", QString::fromUtf8("\u09AC\u09BE\u0982\u09B2\u09BE")}, // Bengali
	{"bod",      "bo", QString::fromUtf8("\u0F51\u0F56\u0F74\u0F66\u0F0B\u0F66\u0F90\u0F51\u0F0B")}, // "Tibetan (Standard)"
	{"bos",      "bs", QString::fromUtf8("Bosanski")}, // Bosnian
	{"bre",      "bs", QString::fromUtf8("Brezhoneg")}, // Breton
	{"bul",      "bg", QString::fromUtf8("\u0431\u044A\u043B\u0433\u0430\u0440\u0441\u043A\u0438 \u0435\u0437\u0438\u043A")}, // Bulgarian
	{"cat",      "ca", QString::fromUtf8("Catal\u00E0")}, // Catalan
	{"ceb",      "",   QString::fromUtf8("Bisaya")}, // Cebuano
	{"ces",      "cs", QString::fromUtf8("\u010Ce\u0161tina")}, // Czech
	{"chi_sim",  "zh_CN", QString::fromUtf8("\u7b80\u4f53\u5b57 ")}, // Chinese (Simplified)
	{"chi_sim_vert",  "zh_CN", QString::fromUtf8("\u7b80\u4f53\u5b57 \u7EB5")}, // Chinese (Simplified, Vertical)
	{"chi_tra",  "zh_TW", QString::fromUtf8("\u7e41\u9ad4\u5b57")}, // Chinese (Traditional)
	{"chi_tra_vert",  "zh_TW", QString::fromUtf8("\u7e41\u9ad4\u5b57 \u7EB5")}, // Chinese (Traditional, Vertical)
	{"chr",      "",   QString::fromUtf8("\u13E3\u13B3\u13A9")}, // Cherokee
	{"cos",      "co", QString::fromUtf8("Corsu")}, // Corsican
	{"cym",      "cy", QString::fromUtf8("Cymraeg")}, // Welsh
	{"dan",      "da", QString::fromUtf8("Dansk")}, // Danish
	{"dan_frak", "da", QString::fromUtf8("Dansk (Fraktur)")}, // Danish (Fraktur)
	{"deu",      "de", QString::fromUtf8("Deutsch")}, // German
	{"deu_frak", "de", QString::fromUtf8("Deutsch (Fraktur)")}, // German (Fraktur)
	{"div",      "dv", QString::fromUtf8("\u078B\u07A8\u0788\u07AC\u0780\u07A8")}, // Dhivehi
	{"dzo",      "dz", QString::fromUtf8("\u0F62\u0FAB\u0F7C\u0F44\u0F0B\u0F41\u0F0B")}, // Dzongkha
	{"ell",      "el", QString::fromUtf8("\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC")}, // Greek
	{"eng",      "en", QString::fromUtf8("English")},
	{"enm",      "en", QString::fromUtf8("Middle English (1100-1500)")}, // Middle English (1100-1500)
	{"epo",      "eo", QString::fromUtf8("Esperanto")}, // Esperanto
	{"equ",      "",   QString::fromUtf8("Math / Equations")}, // Math / equation
	{"est",      "et", QString::fromUtf8("Eesti")}, // Estonian
	{"eus",      "eu", QString::fromUtf8("Euskara")}, // Basque
	{"fas",      "fa", QString::fromUtf8("\u0641\u0627\u0631\u0633\u06CC")}, // Persian (Farsi)
	{"fao",      "fo", QString::fromUtf8("F\u00F8royskt")}, // Faroese
	{"fil",      "",   QString::fromUtf8("Wikang Filipino")}, // Filipino
	{"fin",      "fi", QString::fromUtf8("Suomi")}, // Finnish
	{"fra",      "fr", QString::fromUtf8("Fran\u00E7ais")}, // French
	{"frk",      "",   QString::fromUtf8("Frankish")}, // Frankish
	{"frm",      "fr", QString::fromUtf8("Moyen fran\u00E7ais (ca. 1400-1600)")}, // Middle French (ca. 1400-1600)
	{"fry",      "fy", QString::fromUtf8("Frysk")}, // Frisian (West)
	{"gla",      "gd", QString::fromUtf8("G\u00E0idhlig")}, // Scottish Gaelic
	{"gle",      "ga", QString::fromUtf8("Gaeilge")}, // Irish
	{"glg",      "gl", QString::fromUtf8("Galego")}, // Galician
	{"grc",      "el", QString::fromUtf8("\u1f19\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ae")}, // Ancient Greek
	{"guj",      "gu", QString::fromUtf8("\u0A97\u0AC1\u0A9C\u0AB0\u0ABE\u0AA4\u0AC0")}, // Gujarati
	{"hat",      "ht", QString::fromUtf8("Krey\u00F2l Ayisyen")}, // Haitian
	{"heb",      "he", QString::fromUtf8("\u05E2\u05D1\u05E8\u05D9\u05EA\u202C")}, // Hebrew
	{"hin",      "hi", QString::fromUtf8("\u0939\u093F\u0928\u094D\u0926\u0940")}, // Hindi
	{"hrv",      "hr", QString::fromUtf8("Hrvatski")}, // Croatian
	{"hun",      "hu", QString::fromUtf8("Magyar")}, // Hungarian
	{"hye",      "hy", QString::fromUtf8("\u0540\u0561\u0575\u0565\u0580\u0565\u0576")}, // Armenian
	{"iku",      "iu", QString::fromUtf8("\u1403\u14C4\u1483\u144E\u1450\u1466")}, // Inuktitut
	{"ind",      "id", QString::fromUtf8("Bahasa Indonesia")}, // Indonesian
	{"isl",      "is", QString::fromUtf8("\u00CDslenska")}, // Icelandic
	{"ita",      "it", QString::fromUtf8("Italiano")}, // Italian
	{"ita_old",  "it", QString::fromUtf8("Italiano (Antico)")}, // Italian (Old)
	{"jav",      "jv", QString::fromUtf8("Basa jawa")}, // Javanese
	{"jpn",      "ja", QString::fromUtf8("\uA9A7\uA9B1\uA997\uA9AE")}, // Japanese
	{"jpn_vert", "ja", QString::fromUtf8("\uA9A7\uA9B1\uA997\uA9AE \u5782\u76F4")}, // Japanese (Vertical)
	{"kan",      "kn", QString::fromUtf8("\u0C95\u0CA8\u0CCD\u0CA8\u0CA1")}, // Kannada
	{"kat",      "ka", QString::fromUtf8("\u10E5\u10D0\u10E0\u10D7\u10E3\u10DA\u10D8")}, // Georgian
	{"kat_old",  "ka", QString::fromUtf8("\u10d4\u10dc\u10d0\u10f2 \u10e5\u10d0\u10e0\u10d7\u10e3\u10da\u10d8")}, // Georgian (Old)
	{"kaz",      "kk", QString::fromUtf8("\u049A\u0430\u0437\u0430\u049B T\u0456\u043B\u0456")}, // Kazakh
	{"khm",      "km", QString::fromUtf8("\u1797\u17B6\u179F\u17B6\u1781\u17D2\u1798\u17C2\u179A")}, // Khmer
	{"kir",      "ky", QString::fromUtf8("\u041a\u044b\u0440\u0433\u044b\u0437\u0447\u0430")}, // Kyrgyz
	{"kor",      "ko", QString::fromUtf8("\uD55C\uAD6D\uC5B4")}, // Korean
	{"kor_vert", "ko", QString::fromUtf8("\uD55C\uAD6D\uC5B4 \uC218\uC9C1\uC120")}, // Korean (Vertical)
	{"kur",      "ku", QString::fromUtf8("Kurd\u00ED")}, // Kurdish
	{"kur_ara",  "ku", QString::fromUtf8("\u06A9\u0648\u0631\u062F\u06CC")}, // Kurdish (Arabic)
	{"lao",      "lo", QString::fromUtf8("\u0E9E\u0EB2\u0EAA\u0EB2\u0EA5\u0EB2\u0EA7")}, // Lao
	{"lat",      "la", QString::fromUtf8("Latina")}, // Latin
	{"lav",      "lv", QString::fromUtf8("Latvie\u0161u")}, // Latvian
	{"lit",      "lt", QString::fromUtf8("Lietuvi\u0173")}, // Lithuanian
	{"ltz",      "lb", QString::fromUtf8("L\u00EBtzebuergesch")}, // Luxembourgish
	{"mal",      "ml", QString::fromUtf8("\u0D2E\u0D32\u0D2F\u0D3E\u0D33\u0D02")}, // Malayalam
	{"mar",      "mr", QString::fromUtf8("\u092E\u0930\u093E\u0920\u0940")}, // Marathi
	{"mkd",      "mk", QString::fromUtf8("M\u0430\u043A\u0435\u0434\u043E\u043D\u0441\u043A\u0438")}, // Macedonian
	{"mlt",      "mt", QString::fromUtf8("Malti")}, // Maltese
	{"mon",      "mn", QString::fromUtf8("\u041C\u043E\u043D\u0433\u043E\u043B \u0425\u044D\u043B")}, // Mongolian
	{"mri",      "mi", QString::fromUtf8("te Reo M\u0101ori")}, // Maori
	{"msa",      "ms", QString::fromUtf8("Bahasa Melayu")}, // Malay
	{"mya",      "my", QString::fromUtf8("\u1019\u103C\u1014\u103A\u1019\u102C\u1005\u102C")}, // Burmese
	{"nep",      "ne", QString::fromUtf8("\u0928\u0947\u092A\u093E\u0932\u0940")}, // Nepali
	{"nld",      "nl", QString::fromUtf8("Nederlands")}, // Dutch
	{"nor",      "no", QString::fromUtf8("Norsk")}, // Norwegian
	{"oci",      "oc", QString::fromUtf8("Occitan")}, // Occitan
	{"ori",      "or", QString::fromUtf8("\u0b13\u0b21\u0b3c\u0b3f\u0b06")}, // Oriya
	{"pan",      "pa", QString::fromUtf8("\u0A2A\u0A70\u0A1C\u0A3E\u0A2C\u0A40")}, // Punjabi
	{"pol",      "pl", QString::fromUtf8("Polski")}, // Polish
	{"por",      "pt", QString::fromUtf8("Portugu\u00EAs")}, // Portuguese
	{"pus",      "ps", QString::fromUtf8("\u067E\u069A\u062A\u0648")}, // Pashto
	{"que",      "qu", QString::fromUtf8("Runasimi")}, // Quechuan
	{"ron",      "ro", QString::fromUtf8("Rom\u00E2n\u0103")}, // Romanian
	{"rus",      "ru", QString::fromUtf8("\u0420\u0443\u0441\u0441\u043A\u0438\u0439")}, // Russian
	{"san",      "sa", QString::fromUtf8("\u0938\u0902\u0938\u094D\u0915\u0943\u0924\u092E\u094D")}, // Sanskrit
	{"sin",      "si", QString::fromUtf8("\u0DC3\u0DD2\u0D82\u0DC4\u0DBD")}, // Sinhala
	{"slk",      "sk", QString::fromUtf8("Sloven\u010Dina")}, // Slovak
	{"slk_frak", "sk", QString::fromUtf8("Sloven\u010Dina (Frakt\u00far)")}, // Slovak (Fraktur)
	{"slv",      "sl", QString::fromUtf8("Sloven\u0161\u010Dina")}, // Slovene
	{"snd",      "sd", QString::fromUtf8("\u0633\u0646\u068C\u064A")}, // Sindhi
	{"spa",      "es", QString::fromUtf8("Espa\u00F1ol")}, // Spanish
	{"spa_old",  "es", QString::fromUtf8("Espa\u00F1ol (Antiguo)")}, // Spanish (Old)
	{"sqi",      "sq", QString::fromUtf8("Shqip")}, // Albanian
	{"srp",      "sr", QString::fromUtf8("\u0421\u0440\u043F\u0441\u043A\u0438")}, // Serbian
	{"srp_latn", "sr", QString::fromUtf8("Srpski")}, // Serbian (Latin)
	{"sun",      "su", QString::fromUtf8("Sudanese")}, // Sudanese
	{"swa",      "sw", QString::fromUtf8("Kiswahili")}, // Swahili
	{"swe",      "sv", QString::fromUtf8("Svenska")}, // Swedish
	{"syr",      "",   QString::fromUtf8("\u0720\u072b\u0722\u0710 \u0723\u0718\u072a\u071d\u071d\u0710")}, // Syriac
	{"tam",      "ta", QString::fromUtf8("\u0BA4\u0BAE\u0BBF\u0BB4\u0BCD")}, // Tamil
	{"tat",      "tt", QString::fromUtf8("Tatar\u00E7a")}, // Tatar
	{"tel",      "te", QString::fromUtf8("\u0C24\u0C46\u0C32\u0C41\u0C17\u0C41")}, // Telugu
	{"tgk",      "tg", QString::fromUtf8("\u0442\u043e\u04b7\u0438\u043a\u04e3")}, // Tajik
	{"tgl",      "tl", QString::fromUtf8("\u170A\u170A\u170C\u1712")}, // Tagalog
	{"tha",      "th", QString::fromUtf8("\u0E20\u0E32\u0E29\u0E32\u0E44\u0E17\u0E22")}, // Thai
	{"tir",      "ti", QString::fromUtf8("\u1275\u130D\u122D\u129B")}, // Tigrinya
	{"ton",      "to", QString::fromUtf8("Lea faka-Tonga")}, // Tongan
	{"tur",      "tr", QString::fromUtf8("T\u00FCrk\u00E7e")}, // Turkish
	{"uig",      "ug", QString::fromUtf8("\u0626\u06C7\u064A\u063A\u06C7\u0631 \u062A\u0649\u0644\u0649")}, // Uyghur
	{"ukr",      "uk", QString::fromUtf8("\u0423\u043A\u0440\u0430\u0457\u043D\u0441\u044C\u043A\u0430")}, // Ukrainian
	{"urd",      "ur", QString::fromUtf8("\u0627\u064F\u0631\u062F\u064F\u0648")}, // Urdu
	{"uzb",      "uz", QString::fromUtf8("\u0627\u0648\u0632\u0628\u06CC\u06A9")}, // Uzbek
	{"uzb_cyrl", "uz", QString::fromUtf8("\u040E\u0437\u0431\u0435\u043A")}, // Uzbek (Cyrillic)
	{"vie",      "vi", QString::fromUtf8("Ti\u1EBFng Vi\u1EC7t Nam")}, // Vietnamese
	{"yid",      "yi", QString::fromUtf8("\u05D9\u05D9\u05B4\u05D3\u05D9\u05E9 \u05D9\u05D9\u05B4\u05D3\u05D9\u05E9\u202C")}, // Yiddish
	{"yor",      "yo", QString::fromUtf8("\u00C8d\u00E8 Yor\u00F9b\u00E1")}, // Yoruba
};

QMultiMap<QString, QString> Config::buildLanguageCultureTable() {
	QMultiMap<QString, QString> map;
	map.insert("af", "af_ZA");
	map.insert("ar", "ar_AE");
	map.insert("ar", "ar_BH");
	map.insert("ar", "ar_DZ");
	map.insert("ar", "ar_EG");
	map.insert("ar", "ar_IQ");
	map.insert("ar", "ar_JO");
	map.insert("ar", "ar_KW");
	map.insert("ar", "ar_LB");
	map.insert("ar", "ar_LY");
	map.insert("ar", "ar_MA");
	map.insert("ar", "ar_OM");
	map.insert("ar", "ar_QA");
	map.insert("ar", "ar_SA");
	map.insert("ar", "ar_SY");
	map.insert("ar", "ar_TN");
	map.insert("ar", "ar_YE");
	map.insert("az", "az_AZ");
	map.insert("az", "az_AZ");
	map.insert("be", "be_BY");
	map.insert("bg", "bg_BG");
	map.insert("ca", "ca_ES");
	map.insert("cs", "cs_CZ");
	map.insert("da", "da_DK");
	map.insert("de", "de_AT");
	map.insert("de", "de_CH");
	map.insert("de", "de_DE");
	map.insert("de", "de_LI");
	map.insert("de", "de_LU");
	map.insert("div", "div_MV");
	map.insert("el", "el_GR");
	map.insert("en", "en_AU");
	map.insert("en", "en_BZ");
	map.insert("en", "en_CA");
	map.insert("en", "en_CB");
	map.insert("en", "en_GB");
	map.insert("en", "en_IE");
	map.insert("en", "en_JM");
	map.insert("en", "en_NZ");
	map.insert("en", "en_PH");
	map.insert("en", "en_TT");
	map.insert("en", "en_US");
	map.insert("en", "en_ZA");
	map.insert("en", "en_ZW");
	map.insert("es", "es_AR");
	map.insert("es", "es_BO");
	map.insert("es", "es_CL");
	map.insert("es", "es_CO");
	map.insert("es", "es_CR");
	map.insert("es", "es_DO");
	map.insert("es", "es_EC");
	map.insert("es", "es_ES");
	map.insert("es", "es_GT");
	map.insert("es", "es_HN");
	map.insert("es", "es_MX");
	map.insert("es", "es_NI");
	map.insert("es", "es_PA");
	map.insert("es", "es_PE");
	map.insert("es", "es_PR");
	map.insert("es", "es_PY");
	map.insert("es", "es_SV");
	map.insert("es", "es_UY");
	map.insert("es", "es_VE");
	map.insert("et", "et_EE");
	map.insert("eu", "eu_ES");
	map.insert("fa", "fa_IR");
	map.insert("fi", "fi_FI");
	map.insert("fo", "fo_FO");
	map.insert("fr", "fr_BE");
	map.insert("fr", "fr_CA");
	map.insert("fr", "fr_CH");
	map.insert("fr", "fr_FR");
	map.insert("fr", "fr_LU");
	map.insert("fr", "fr_MC");
	map.insert("gl", "gl_ES");
	map.insert("gu", "gu_IN");
	map.insert("he", "he_IL");
	map.insert("hi", "hi_IN");
	map.insert("hr", "hr_HR");
	map.insert("hu", "hu_HU");
	map.insert("hy", "hy_AM");
	map.insert("id", "id_ID");
	map.insert("is", "is_IS");
	map.insert("it", "it_CH");
	map.insert("it", "it_IT");
	map.insert("ja", "ja_JP");
	map.insert("ka", "ka_GE");
	map.insert("kk", "kk_KZ");
	map.insert("kn", "kn_IN");
	map.insert("kok", "kok_IN");
	map.insert("ko", "ko_KR");
	map.insert("ky", "ky_KZ");
	map.insert("lt", "lt_LT");
	map.insert("lv", "lv_LV");
	map.insert("mk", "mk_MK");
	map.insert("mn", "mn_MN");
	map.insert("mr", "mr_IN");
	map.insert("ms", "ms_BN");
	map.insert("ms", "ms_MY");
	map.insert("nb", "nb_NO");
	map.insert("nl", "nl_BE");
	map.insert("nl", "nl_NL");
	map.insert("nn", "nn_NO");
	map.insert("pa", "pa_IN");
	map.insert("pl", "pl_PL");
	map.insert("pt", "pt_BR");
	map.insert("pt", "pt_PT");
	map.insert("ro", "ro_RO");
	map.insert("ru", "ru_RU");
	map.insert("sa", "sa_IN");
	map.insert("sk", "sk_SK");
	map.insert("sl", "sl_SI");
	map.insert("sq", "sq_AL");
	map.insert("sr", "sr_SP");
	map.insert("sr", "sr_SP");
	map.insert("sv", "sv_FI");
	map.insert("sv", "sv_SE");
	map.insert("sw", "sw_KE");
	map.insert("syr", "syr_SY");
	map.insert("ta", "ta_IN");
	map.insert("te", "te_IN");
	map.insert("th", "th_TH");
	map.insert("tr", "tr_TR");
	map.insert("tt", "tt_RU");
	map.insert("uk", "uk_UA");
	map.insert("ur", "ur_PK");
	map.insert("uz", "uz_UZ");
	map.insert("uz", "uz_UZ");
	map.insert("vi", "vi_VN");
	map.insert("zh", "zh_CHS");
	map.insert("zh", "zh_CHT");
	map.insert("zh", "zh_CN");
	map.insert("zh", "zh_HK");
	map.insert("zh", "zh_MO");
	map.insert("zh", "zh_SG");
	map.insert("zh", "zh_TW");
	return map;
}

const QMultiMap<QString, QString> Config::LANGUAGE_CULTURES = Config::buildLanguageCultureTable();


Config::Config(QWidget* parent)
	: QDialog(parent) {
	ui.setupUi(this);

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	ui.tableWidgetPredefLang->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
	ui.tableWidgetAdditionalLang->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#else
	ui.tableWidgetPredefLang->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	ui.tableWidgetAdditionalLang->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#endif
	ui.widgetAddLang->setVisible(false);

#if !ENABLE_VERSIONCHECK
	ui.checkBoxUpdateCheck->setVisible(false);
#endif

	for(const Lang& lang : LANGUAGES) {
		int row = ui.tableWidgetPredefLang->rowCount();
		ui.tableWidgetPredefLang->insertRow(row);
		ui.tableWidgetPredefLang->setItem(row, 0, new QTableWidgetItem(lang.prefix));
		ui.tableWidgetPredefLang->setItem(row, 1, new QTableWidgetItem(lang.code));
		ui.tableWidgetPredefLang->setItem(row, 2, new QTableWidgetItem(lang.name));
	}

	connect(ui.checkBoxDefaultOutputFont, SIGNAL(toggled(bool)), ui.pushButtonOutputFont, SLOT(setDisabled(bool)));
	connect(ui.pushButtonOutputFont, SIGNAL(clicked()), &m_fontDialog, SLOT(exec()));
	connect(&m_fontDialog, SIGNAL(fontSelected(QFont)), this, SLOT(updateFontButton(QFont)));
	connect(ui.pushButtonAddLang, SIGNAL(clicked()), this, SLOT(toggleAddLanguage()));
	connect(ui.pushButtonRemoveLang, SIGNAL(clicked()), this, SLOT(removeLanguage()));
	connect(ui.pushButtonAddLangOk, SIGNAL(clicked()), this, SLOT(addLanguage()));
	connect(ui.pushButtonAddLangCancel, SIGNAL(clicked()), this, SLOT(toggleAddLanguage()));
	connect(ui.tableWidgetAdditionalLang->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(langTableSelectionChanged(QItemSelection, QItemSelection)));
	connect(ui.buttonBox->button(QDialogButtonBox::Help), SIGNAL(clicked()), MAIN, SLOT(showHelp()));
	connect(ui.lineEditLangPrefix, SIGNAL(textChanged(QString)), this, SLOT(clearLineEditErrorState()));
	connect(ui.lineEditLangName, SIGNAL(textChanged(QString)), this, SLOT(clearLineEditErrorState()));
	connect(ui.lineEditLangCode, SIGNAL(textChanged(QString)), this, SLOT(clearLineEditErrorState()));
	connect(ui.comboBoxDataLocation, SIGNAL(currentIndexChanged(int)), this, SLOT(setDataLocations(int)));

	ADD_SETTING(SwitchSetting("dictinstall", ui.checkBoxDictInstall, true));
	ADD_SETTING(SwitchSetting("updatecheck", ui.checkBoxUpdateCheck, true));
	ADD_SETTING(SwitchSetting("openafterexport", ui.checkBoxOpenAfterExport, false));
	ADD_SETTING(TableSetting("customlangs", ui.tableWidgetAdditionalLang));
	ADD_SETTING(SwitchSetting("systemoutputfont", ui.checkBoxDefaultOutputFont, true));
	ADD_SETTING(FontSetting("customoutputfont", &m_fontDialog, QFont().toString()));
	ADD_SETTING(ComboSetting("textencoding", ui.comboBoxEncoding, 0));
	ADD_SETTING(ComboSetting("datadirs", ui.comboBoxDataLocation, 0));
	ADD_SETTING(VarSetting<QString>("sourcedir", Utils::documentsFolder()));
	ADD_SETTING(VarSetting<QString>("outputdir", Utils::documentsFolder()));
	ADD_SETTING(VarSetting<QString>("auxdir", Utils::documentsFolder()));

	updateFontButton(m_fontDialog.currentFont());
}

bool Config::searchLangSpec(Lang& lang) const {
	for(const QTableWidget* table : QList<QTableWidget*> {ui.tableWidgetPredefLang, ui.tableWidgetAdditionalLang}) {
		for(int row = 0, nRows = table->rowCount(); row < nRows; ++row) {
			if(table->item(row, 0)->text() == lang.prefix) {
				lang = {table->item(row, 0)->text(), table->item(row, 1)->text(), table->item(row, 2)->text()};
				return true;
			}
		}
	}
	return false;
}

QList<QString> Config::searchLangCultures(const QString& code) const {
	return LANGUAGE_CULTURES.values(code);
}

void Config::showDialog() {
	toggleAddLanguage(true);
	exec();
	ConfigSettings::get<TableSetting>("customlangs")->serialize();
}

bool Config::useUtf8() const {
	return ui.comboBoxEncoding->currentIndex() == 1;
}

bool Config::useSystemDataLocations() const {
	return ui.comboBoxDataLocation->currentIndex() == 0;
}

QString Config::tessdataLocation() const {
	return ui.lineEditTessdataLocation->text();
}

QString Config::spellingLocation() const {
	return ui.lineEditSpellLocation->text();
}

void Config::disableDictInstall() {
	ConfigSettings::get<SwitchSetting>("dictinstall")->setValue(false);
}

void Config::disableUpdateCheck() {
	ConfigSettings::get<SwitchSetting>("updatecheck")->setValue(false);
}

void Config::setDataLocations(int idx) {
	if(idx == 0) {
#ifdef Q_OS_WIN
		QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));
		qputenv("TESSDATA_PREFIX", dataDir.absolutePath().toLocal8Bit());
		ui.lineEditSpellLocation->setText(dataDir.absoluteFilePath("myspell"));
#else
		QDir dataDir("/usr/share");
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
		unsetenv("TESSDATA_PREFIX");
#else
		qunsetenv("TESSDATA_PREFIX");
#endif
		if(QDir(dataDir.absoluteFilePath("myspell/dicts")).exists()) {
			ui.lineEditSpellLocation->setText(dataDir.absoluteFilePath("myspell/dicts"));
		} else {
			ui.lineEditSpellLocation->setText(dataDir.absoluteFilePath("myspell"));
		}
#endif
	} else {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
# ifdef Q_OS_WIN
		QDir configDir = QDir(QDir::home().absoluteFilePath("Local Settings/Application Data"));
# else
		QDir configDir = QDir(QDir::home().absoluteFilePath(".config"));
# endif
#else
		QDir configDir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
#endif
		qputenv("TESSDATA_PREFIX", configDir.absoluteFilePath("tessdata").toLocal8Bit());
		ui.lineEditSpellLocation->setText(configDir.absoluteFilePath("enchant/myspell"));
	}
	tesseract::TessBaseAPI tess;
	QByteArray current = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	tess.Init(nullptr, nullptr);
	setlocale(LC_NUMERIC, current.constData());
	ui.lineEditTessdataLocation->setText(QString(tess.GetDatapath()));
}

void Config::openTessdataDir() {
	int idx = QSettings().value("datadirs").toInt();
	if(idx == 0) {
#ifdef Q_OS_WIN
		QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));
		qputenv("TESSDATA_PREFIX", dataDir.absolutePath().toLocal8Bit());
#else
# if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
		unsetenv("TESSDATA_PREFIX");
# else
		qunsetenv("TESSDATA_PREFIX");
# endif
#endif
	} else {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
# ifdef Q_OS_WIN
		QDir configDir = QDir(QDir::home().absoluteFilePath("Local Settings/Application Data"));
# else
		QDir configDir = QDir(QDir::home().absoluteFilePath(".config"));
# endif
#else
		QDir configDir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
#endif
		qputenv("TESSDATA_PREFIX", configDir.absoluteFilePath("tessdata").toLocal8Bit());
	}
	tesseract::TessBaseAPI tess;
	QByteArray current = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	tess.Init(nullptr, nullptr);
	setlocale(LC_NUMERIC, current.constData());
	QDir().mkpath(QString(tess.GetDatapath()));
	QDesktopServices::openUrl(QUrl::fromLocalFile(QString(tess.GetDatapath())));
}

void Config::openSpellingDir() {
	int idx = QSettings().value("datadirs").toInt();
	if(idx == 0) {
#ifdef Q_OS_WIN
		QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));
		QDir().mkpath(dataDir.absoluteFilePath("myspell"));
		QDesktopServices::openUrl(QUrl::fromLocalFile(dataDir.absoluteFilePath("myspell")));
#else
		QDir dataDir("/usr/share");
		if(QDir(dataDir.absoluteFilePath("myspell/dicts")).exists()) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(dataDir.absoluteFilePath("myspell/dicts")));
		} else {
			QDesktopServices::openUrl(QUrl::fromLocalFile(dataDir.absoluteFilePath("myspell")));
		}
#endif
	} else {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
# ifdef Q_OS_WIN
		QDir configDir = QDir(QDir::home().absoluteFilePath("Local Settings/Application Data"));
# else
		QDir configDir = QDir(QDir::home().absoluteFilePath(".config"));
# endif
#else
		QDir configDir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
#endif
		QDir().mkpath(configDir.absoluteFilePath("enchant/myspell"));
		QDesktopServices::openUrl(QUrl::fromLocalFile(configDir.absoluteFilePath("enchant/myspell")));
	}
}

void Config::toggleAddLanguage(bool forceHide) {
	bool addVisible = forceHide ? true : ui.widgetAddLang->isVisible();
	ui.widgetAddLang->setVisible(!addVisible);
	ui.widgetAddRemoveLang->setVisible(addVisible);
	if(addVisible) {
		ui.pushButtonAddLang->setFocus();
	} else {
		ui.pushButtonAddLangOk->setFocus();
	}
	ui.lineEditLangPrefix->setText("");
	ui.lineEditLangPrefix->setStyleSheet("");
	ui.lineEditLangCode->setText("");
	ui.lineEditLangCode->setStyleSheet("");
	ui.lineEditLangName->setText("");
	ui.lineEditLangName->setStyleSheet("");
}

void Config::addLanguage() {
	QString errorStyle = "background: #FF7777; color: #FFFFFF;";
	bool invalid = false;
	if(QRegExp("^\\w+$").indexIn(ui.lineEditLangPrefix->text()) == -1) {
		invalid = true;
		ui.lineEditLangPrefix->setStyleSheet(errorStyle);
	}
	if(QRegExp("^.+$").indexIn(ui.lineEditLangName->text()) == -1) {
		invalid = true;
		ui.lineEditLangName->setStyleSheet(errorStyle);
	}
	if(QRegExp("^[a-z]{2}$").indexIn(ui.lineEditLangCode->text()) == -1 &&
	        QRegExp("^[a-z]{2}_[A-Z]{2}$").indexIn(ui.lineEditLangCode->text()) == -1) {
		invalid = true;
		ui.lineEditLangCode->setStyleSheet(errorStyle);
	}
	if(!invalid) {
		int row = ui.tableWidgetAdditionalLang->rowCount();
		ui.tableWidgetAdditionalLang->insertRow(row);
		ui.tableWidgetAdditionalLang->setItem(row, 0, new QTableWidgetItem(ui.lineEditLangPrefix->text()));
		ui.tableWidgetAdditionalLang->setItem(row, 1, new QTableWidgetItem(ui.lineEditLangCode->text()));
		ui.tableWidgetAdditionalLang->setItem(row, 2, new QTableWidgetItem(ui.lineEditLangName->text()));
		ui.lineEditLangPrefix->setText("");
		ui.lineEditLangCode->setText("");
		ui.lineEditLangName->setText("");
		toggleAddLanguage();
	}
}

void Config::removeLanguage() {
	for(const QModelIndex& index : ui.tableWidgetAdditionalLang->selectionModel()->selectedRows()) {
		ui.tableWidgetAdditionalLang->removeRow(index.row());
	}
}

void Config::updateFontButton(const QFont& font) {
	ui.pushButtonOutputFont->setText(QString("%1 %2").arg(font.family()).arg(font.pointSize()));
}

void Config::langTableSelectionChanged(const QItemSelection& selected, const QItemSelection& /*deselected*/) {
	ui.pushButtonRemoveLang->setDisabled(selected.isEmpty());
}

void Config::clearLineEditErrorState() {
	static_cast<QLineEdit*>(QObject::sender())->setStyleSheet("");
}

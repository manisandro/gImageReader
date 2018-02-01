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

const QList<Config::Lang> Config::LANGUAGES = {
	// {ISO 639-2, ISO 639-1, name}
	{"afr",      "af", QString::fromUtf8("Afrikaans")}, // Afrikaans
	{"amh",      "am", QString::fromUtf8("\u12a0\u121b\u122d\u129b")}, // Amharic
	{"ara",      "ar", QString::fromUtf8("\u0627\u0644\u0639\u0631\u0628\u064a\u0629")}, // Arabic
	{"asm",      "as", QString::fromUtf8("\u0985\u09b8\u09ae\u09c0\u09af\u09bc\u09be")}, // Assamese
	{"aze",      "az", QString::fromUtf8("\u0622\u0630\u0631\u0628\u0627\u06cc\u062c\u0627\u0646")}, // Azerbaijani
	{"aze_cyrl", "az", QString::fromUtf8("Az\u0259rbaycan dili")}, // Azerbaijani (Cyrillic)
	{"bel",      "be", QString::fromUtf8("\u0431\u0435\u043b\u0430\u0440\u0443\u0441\u043a\u0430\u044f \u043c\u043e\u0432\u0430")}, // Belarusian
	{"ben",      "bn", QString::fromUtf8("\u09ac\u09be\u0982\u09b2\u09be")}, // Bengali
	{"bod",      "bo", QString::fromUtf8("\u0f56\u0f7c\u0f51\u0f0b\u0f61\u0f72\u0f42")}, // "Tibetan (Standard)"
	{"bos",      "bs", QString::fromUtf8("Bosanski jezik")}, // Bosnian
	{"bul",      "bg", QString::fromUtf8("\u0431\u044a\u043b\u0433\u0430\u0440\u0441\u043a\u0438 \u0435\u0437\u0438\u043a")}, // Bulgarian
	{"cat",      "ca", QString::fromUtf8("Catal\u00e0")}, // Catalan
	{"ceb",      "",   QString::fromUtf8("Cebuano")}, // Cebuano
	{"ces",      "cs", QString::fromUtf8("\u010ce\u0161tina")}, // Czech
	{"chi_sim",  "zh_CN", QString::fromUtf8("\u7b80\u4f53\u5b57")}, // Chinese (Simplified)
	{"chi_tra",  "zh_TW", QString::fromUtf8("\u7e41\u9ad4\u5b57")}, // Chinese (Traditional)
	{"chr",      "",   QString::fromUtf8("\u13e3\u13b3\u13a9 \u13a6\u13ec\u13c2\u13af\u13cd\u13d7")}, // Cherokee
	{"cym",      "cy", QString::fromUtf8("Cymraeg")}, // Welsh
	{"dan",      "da", QString::fromUtf8("Dansk")}, // Danish
	{"dan_frak", "da", QString::fromUtf8("Dansk (Fraktur)")}, // Danish (Fraktur)
	{"deu",      "de", QString::fromUtf8("Deutsch")}, // German
	{"deu_frak", "de", QString::fromUtf8("Deutsch (Fraktur)")}, // German (Fraktur)
	{"dzo",      "dz", QString::fromUtf8("\u0f62\u0fab\u0f7c\u0f44\u0f0b\u0f41")}, // Dzongkha
	{"ell",      "el", QString::fromUtf8("\u03b5\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac")}, // Greek
	{"eng",      "en", QString::fromUtf8("English")},
	{"enm",      "en", QString::fromUtf8("Middle English (1100-1500)")}, // Middle English (1100-1500)
	{"epo",      "eo", QString::fromUtf8("Esperanto")}, // Esperanto
	{"equ",      "",   QString::fromUtf8("Math / Equations")}, // Math / equation
	{"est",      "et", QString::fromUtf8("Eesti keel")}, // Estonian
	{"eus",      "eu", QString::fromUtf8("Euskara")}, // Basque
	{"fas",      "fa", QString::fromUtf8("\u0641\u0627\u0631\u0633\u06cc")}, // Persian (Farsi)
	{"fin",      "fi", QString::fromUtf8("Suomen kieli")}, // Finnish
	{"fra",      "fr", QString::fromUtf8("Fran\u00e7ais")}, // French
	{"frk",      "",   QString::fromUtf8("Frankish")}, // Frankish
	{"frm",      "fr", QString::fromUtf8("Moyen fran\u00e7ais (ca. 1400-1600)")}, // Middle French (ca. 1400-1600)
	{"gle",      "ga", QString::fromUtf8("Gaeilge")}, // Irish
	{"glg",      "gl", QString::fromUtf8("Galego")}, // Galician
	{"grc",      "el", QString::fromUtf8("\u1f19\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ae")}, // Ancient Greek
	{"guj",      "gu", QString::fromUtf8("\u0a97\u0ac1\u0a9c\u0ab0\u0abe\u0aa4\u0ac0")}, // Gujarati
	{"hat",      "ht", QString::fromUtf8("Krey\u00f2l ayisyen")}, // Haitian
	{"heb",      "he", QString::fromUtf8("\u05e2\u05d1\u05e8\u05d9\u05ea")}, // Hebrew
	{"hin",      "hi", QString::fromUtf8("\u0939\u093f\u0928\u094d\u0926\u0940")}, // Hindi
	{"hrv",      "hr", QString::fromUtf8("Hrvatski")}, // Croatian
	{"hun",      "hu", QString::fromUtf8("Magyar")}, // Hungarian
	{"iku",      "iu", QString::fromUtf8("Inuktitut")}, // Inuktitut
	{"ind",      "id", QString::fromUtf8("Bahasa indonesia")}, // Indonesian
	{"isl",      "is", QString::fromUtf8("\u00cdslenska")}, // Icelandic
	{"ita",      "it", QString::fromUtf8("Italiano")}, // Italian
	{"ita_old",  "it", QString::fromUtf8("Italiano (Antico)")}, // Italian (Old)
	{"jav",      "jv", QString::fromUtf8("Basa jawa")}, // Javanese
	{"jpn",      "ja", QString::fromUtf8("\u65e5\u672c\u8a9e")}, // Japanese
	{"kan",      "kn", QString::fromUtf8("\u0c95\u0ca8\u0ccd\u0ca8\u0ca1")}, // Kannada
	{"kat",      "ka", QString::fromUtf8("\u10e5\u10d0\u10e0\u10d7\u10e3\u10da\u10d8")}, // Georgian
	{"kat_old",  "ka", QString::fromUtf8("\u10d4\u10dc\u10d0\u10f2 \u10e5\u10d0\u10e0\u10d7\u10e3\u10da\u10d8")}, // Georgian (Old)
	{"kaz",      "kk", QString::fromUtf8("\u049b\u0430\u0437\u0430\u049b \u0442\u0456\u043b\u0456")}, // Kazakh
	{"khm",      "km", QString::fromUtf8("\u1781\u17d2\u1798\u17c2\u179a")}, // Khmer
	{"kir",      "ky", QString::fromUtf8("\u041a\u044b\u0440\u0433\u044b\u0437\u0447\u0430")}, // Kyrgyz
	{"kor",      "ko", QString::fromUtf8("\ud55c\uad6d\uc5b4")}, // Korean
	{"kur",      "ku", QString::fromUtf8("\u0643\u0648\u0631\u062f\u06cc")}, // Kurdish
	{"lao",      "lo", QString::fromUtf8("\u0e9e\u0eb2\u0eaa\u0eb2\u0ea5\u0eb2\u0ea7")}, // Lao
	{"lat",      "la", QString::fromUtf8("Latina")}, // Latin
	{"lav",      "lv", QString::fromUtf8("Latvie\u0161u valoda")}, // Latvian
	{"lit",      "lt", QString::fromUtf8("Lietuvos")}, // Lithuanian
	{"mal",      "ml", QString::fromUtf8("\u0d2e\u0d32\u0d2f\u0d3e\u0d33\u0d02")}, // Malayalam
	{"mar",      "mr", QString::fromUtf8("\u092e\u0930\u093e\u0920\u0940")}, // Marathi
	{"mkd",      "mk", QString::fromUtf8("\u043c\u0430\u043a\u0435\u0434\u043e\u043d\u0441\u043a\u0438 \u0458\u0430\u0437\u0438\u043a")}, // Macedonian
	{"mlt",      "mt", QString::fromUtf8("Malti")}, // Maltese
	{"msa",      "ms", QString::fromUtf8("Melayu")}, // Malay
	{"mya",      "my", QString::fromUtf8("\u1017\u1019\u102c\u1005\u102c")}, // Burmese
	{"nep",      "ne", QString::fromUtf8("\u0928\u0947\u092a\u093e\u0932\u0940")}, // Nepali
	{"nld",      "nl", QString::fromUtf8("Nederlands")}, // Dutch
	{"nor",      "no", QString::fromUtf8("Norsk")}, // Norwegian
	{"ori",      "or", QString::fromUtf8("\u0b13\u0b21\u0b3c\u0b3f\u0b06")}, // Oriya
	{"pan",      "pa", QString::fromUtf8("\u0a2a\u0a70\u0a1c\u0a3e\u0a2c\u0a40")}, // Panjabi
	{"pol",      "pl", QString::fromUtf8("Polskie")}, // Polish
	{"por",      "pt", QString::fromUtf8("Portugu\u00eas")}, // Portuguese
	{"pus",      "ps", QString::fromUtf8("\u067e\u069a\u062a\u0648")}, // Pashto
	{"ron",      "ro", QString::fromUtf8("Limba rom\u00e2n\u0103")}, // Romanian
	{"rus",      "ru", QString::fromUtf8("\u0420\u0443\u0441\u0441\u043a\u0438\u0439")}, // Russian
	{"san",      "sa", QString::fromUtf8("\u0938\u0902\u0938\u094d\u0915\u0943\u0924\u092e\u094d")}, // Sanskrit
	{"sin",      "si", QString::fromUtf8("\u0dc3\u0dd2\u0d82\u0dc4\u0dbd")}, // Sinhala
	{"slk",      "sk", QString::fromUtf8("Sloven\u010dina")}, // Slovak
	{"slk_frak", "sk", QString::fromUtf8("Sloven\u010dina (Frakt\u00far)")}, // Slovak (Fraktur)
	{"slv",      "sl", QString::fromUtf8("Sloven\u0161\u010dina")}, // Slovene
	{"spa",      "es", QString::fromUtf8("Espa\u00f1ol")}, // Spanish
	{"spa_old",  "es", QString::fromUtf8("Espa\u00f1ol (Antiguo)")}, // Spanish (Old)
	{"sqi",      "sq", QString::fromUtf8("Shqip")}, // Albanian
	{"srp",      "sr", QString::fromUtf8("\u0441\u0440\u043f\u0441\u043a\u0438 \u0458\u0435\u0437\u0438\u043a")}, // Serbian
	{"srp_latn", "sr", QString::fromUtf8("Srpski")}, // Serbian (Latin)
	{"swa",      "sw", QString::fromUtf8("Swahili")}, // Swahili
	{"swe",      "sv", QString::fromUtf8("Svenska")}, // Swedish
	{"syr",      "",   QString::fromUtf8("\u0720\u072b\u0722\u0710 \u0723\u0718\u072a\u071d\u071d\u0710")}, // Syriac
	{"tam",      "ta", QString::fromUtf8("\u0ba4\u0bae\u0bbf\u0bb4\u0bcd")}, // Tamil
	{"tel",      "te", QString::fromUtf8("\u0c24\u0c46\u0c32\u0c41\u0c17\u0c41")}, // Telugu
	{"tgk",      "tg", QString::fromUtf8("\u0442\u043e\u04b7\u0438\u043a\u04e3")}, // Tajik
	{"tgl",      "tl", QString::fromUtf8("Tagalog")}, // Tagalog
	{"tha",      "th", QString::fromUtf8("\u0e44\u0e17\u0e22")}, // Thai
	{"tir",      "ti", QString::fromUtf8("\u1275\u130d\u122d\u129b")}, // Tigrinya
	{"tur",      "tr", QString::fromUtf8("T\u00fcrk\u00e7e")}, // Turkish
	{"uig",      "ug", QString::fromUtf8("\u0626\u06c7\u064a\u063a\u06c7\u0631\u0686\u06d5\u200e")}, // Uyghur
	{"ukr",      "uk", QString::fromUtf8("\u0443\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430 \u043c\u043e\u0432\u0430")}, // Ukrainian
	{"urd",      "ur", QString::fromUtf8("\u0627\u0631\u062f\u0648")}, // Urdu
	{"uzb",      "uz", QString::fromUtf8("O\u02bbzbek")}, // Uzbek
	{"uzb_cyrl", "uz", QString::fromUtf8("\u040e\u0437\u0431\u0435\u043a")}, // Uzbek (Cyrillic)
	{"vie",      "vi", QString::fromUtf8("Vi\u1ec7t Nam")}, // Vietnamese
	{"yid",      "yi", QString::fromUtf8("\u05d9\u05d9\u05b4\u05d3\u05d9\u05e9")}, // Yiddish
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

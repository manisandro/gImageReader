/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * LangTables.hh
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

#ifndef LANGTABLES_HH
#define LANGTABLES_HH

namespace LangTables {

template<class Container, class String>
Container languages(const std::function<String(const char*)>& utf8str) {
	return Container {
		// See https://en.wikipedia.org/wiki/List_of_language_names
		// {ISO 639-2, ISO 639-1, name}
		//
		// An easy way to generate the strings below:
		// echo "עברית (רש״י)" | uni2ascii -a U -q
		// Verify:
		// echo "\u05E2\u05D1\u05E8\u05D9\u05EA (\u05E8\u05E9\u05F4\u05D9)" | ascii2uni  -a U -q
		{"afr",      "af", utf8str("Afrikaans")}, // Afrikaans
		{"amh",      "am", utf8str("\u12A3\u121B\u122D\u129B")}, // Amharic
		{"ara",      "ar", utf8str("\u0627\u0644\u0644\u063A\u0629 \u0627\u0644\u0639\u0631\u0628\u064A\u0629")}, // Arabic
		{"asm",      "as", utf8str("\u0985\u09B8\u09AE\u09C0\u09AF\u09BC\u09BE")}, // Assamese
		{"aze",      "az", utf8str("\u0622\u0630\u0631\u0628\u0627\u06CC\u062C\u0627\u0646 \u062F\u06CC\u0644\u06CC")}, // Azerbaijani
		{"aze_cyrl", "az", utf8str("Az\u0259rbaycanca")}, // Azerbaijani (Cyrillic)
		{"bel",      "be", utf8str("\u0411\u0435\u043B\u0430\u0440\u0443\u0441\u043A\u0430\u044F")}, // Belarusian
		{"ben",      "bn", utf8str("\u09AC\u09BE\u0982\u09B2\u09BE")}, // Bengali
		{"bod",      "bo", utf8str("\u0F51\u0F56\u0F74\u0F66\u0F0B\u0F66\u0F90\u0F51\u0F0B")}, // "Tibetan (Standard)"
		{"bos",      "bs", utf8str("Bosanski")}, // Bosnian
		{"bre",      "bs", utf8str("Brezhoneg")}, // Breton
		{"bul",      "bg", utf8str("\u0431\u044A\u043B\u0433\u0430\u0440\u0441\u043A\u0438 \u0435\u0437\u0438\u043A")}, // Bulgarian
		{"cat",      "ca", utf8str("Catal\u00E0")}, // Catalan
		{"ceb",      "",   utf8str("Bisaya")}, // Cebuano
		{"ces",      "cs", utf8str("\u010Ce\u0161tina")}, // Czech
		{"chi_sim",  "zh_CN", utf8str("\u7b80\u4f53\u5b57 ")}, // Chinese (Simplified)
		{"chi_sim_vert",  "zh_CN", utf8str("\u7b80\u4f53\u5b57 \u7EB5")}, // Chinese (Simplified, Vertical)
		{"chi_tra",  "zh_TW", utf8str("\u7e41\u9ad4\u5b57")}, // Chinese (Traditional)
		{"chi_tra_vert",  "zh_TW", utf8str("\u7e41\u9ad4\u5b57 \u7EB5")}, // Chinese (Traditional, Vertical)
		{"chr",      "",   utf8str("\u13E3\u13B3\u13A9")}, // Cherokee
		{"cos",      "co", utf8str("Corsu")}, // Corsican
		{"cym",      "cy", utf8str("Cymraeg")}, // Welsh
		{"dan",      "da", utf8str("Dansk")}, // Danish
		{"dan_frak", "da", utf8str("Dansk (Fraktur)")}, // Danish (Fraktur)
		{"deu",      "de", utf8str("Deutsch")}, // German
		{"deu_frak", "de", utf8str("Deutsch (Fraktur)")}, // German (Fraktur)
		{"div",      "dv", utf8str("\u078B\u07A8\u0788\u07AC\u0780\u07A8")}, // Dhivehi
		{"dzo",      "dz", utf8str("\u0F62\u0FAB\u0F7C\u0F44\u0F0B\u0F41\u0F0B")}, // Dzongkha
		{"ell",      "el", utf8str("\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC")}, // Greek
		{"eng",      "en", utf8str("English")},
		{"enm",      "en", utf8str("Middle English (1100-1500)")}, // Middle English (1100-1500)
		{"epo",      "eo", utf8str("Esperanto")}, // Esperanto
		{"equ",      "",   utf8str("Math / Equations")}, // Math / equation
		{"est",      "et", utf8str("Eesti")}, // Estonian
		{"eus",      "eu", utf8str("Euskara")}, // Basque
		{"fas",      "fa", utf8str("\u0641\u0627\u0631\u0633\u06CC")}, // Persian (Farsi)
		{"fao",      "fo", utf8str("F\u00F8royskt")}, // Faroese
		{"fil",      "",   utf8str("Wikang Filipino")}, // Filipino
		{"fin",      "fi", utf8str("Suomi")}, // Finnish
		{"fra",      "fr", utf8str("Fran\u00E7ais")}, // French
		{"frk",      "",   utf8str("Frankish")}, // Frankish
		{"frm",      "fr", utf8str("Moyen fran\u00E7ais (ca. 1400-1600)")}, // Middle French (ca. 1400-1600)
		{"fry",      "fy", utf8str("Frysk")}, // Frisian (West)
		{"gla",      "gd", utf8str("G\u00E0idhlig")}, // Scottish Gaelic
		{"gle",      "ga", utf8str("Gaeilge")}, // Irish
		{"glg",      "gl", utf8str("Galego")}, // Galician
		{"grc",      "el", utf8str("\u1f19\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ae")}, // Ancient Greek
		{"guj",      "gu", utf8str("\u0A97\u0AC1\u0A9C\u0AB0\u0ABE\u0AA4\u0AC0")}, // Gujarati
		{"hat",      "ht", utf8str("Krey\u00F2l Ayisyen")}, // Haitian
		{"heb",      "he", utf8str("\u05E2\u05D1\u05E8\u05D9\u05EA\u202C")}, // Hebrew
		{"heb_rashi", "he", utf8str("\u05E2\u05D1\u05E8\u05D9\u05EA (\u05E8\u05E9\u05F4\u05D9)")}, // Hebrew (Rashi)
		{"heb_toranit", "he", utf8str("\u05E2\u05D1\u05E8\u05D9\u05EA (\u05EA\u05D5\u05E8\u05E0\u05D9\u05EA)")}, // Hebrew (Toranit)
		{"hin",      "hi", utf8str("\u0939\u093F\u0928\u094D\u0926\u0940")}, // Hindi
		{"hrv",      "hr", utf8str("Hrvatski")}, // Croatian
		{"hun",      "hu", utf8str("Magyar")}, // Hungarian
		{"hye",      "hy", utf8str("\u0540\u0561\u0575\u0565\u0580\u0565\u0576")}, // Armenian
		{"iku",      "iu", utf8str("\u1403\u14C4\u1483\u144E\u1450\u1466")}, // Inuktitut
		{"ind",      "id", utf8str("Bahasa Indonesia")}, // Indonesian
		{"isl",      "is", utf8str("\u00CDslenska")}, // Icelandic
		{"ita",      "it", utf8str("Italiano")}, // Italian
		{"ita_old",  "it", utf8str("Italiano (Antico)")}, // Italian (Old)
		{"jav",      "jv", utf8str("Basa jawa")}, // Javanese
		{"jpn",      "ja", utf8str("\u65E5\u672C\u8A9E")}, // Japanese
		{"jpn_vert", "ja", utf8str("\u65E5\u672C\u8A9E \u7E26\u66F8\u304D")}, // Japanese (Vertical)
		{"kan",      "kn", utf8str("\u0C95\u0CA8\u0CCD\u0CA8\u0CA1")}, // Kannada
		{"kat",      "ka", utf8str("\u10E5\u10D0\u10E0\u10D7\u10E3\u10DA\u10D8")}, // Georgian
		{"kat_old",  "ka", utf8str("\u10d4\u10dc\u10d0\u10f2 \u10e5\u10d0\u10e0\u10d7\u10e3\u10da\u10d8")}, // Georgian (Old)
		{"kaz",      "kk", utf8str("\u049A\u0430\u0437\u0430\u049B T\u0456\u043B\u0456")}, // Kazakh
		{"khm",      "km", utf8str("\u1797\u17B6\u179F\u17B6\u1781\u17D2\u1798\u17C2\u179A")}, // Khmer
		{"kir",      "ky", utf8str("\u041a\u044b\u0440\u0433\u044b\u0437\u0447\u0430")}, // Kyrgyz
		{"kor",      "ko", utf8str("\uD55C\uAD6D\uC5B4")}, // Korean
		{"kor_vert", "ko", utf8str("\uD55C\uAD6D\uC5B4 \uC218\uC9C1\uC120")}, // Korean (Vertical)
		{"kur",      "ku", utf8str("Kurd\u00ED")}, // Kurdish
		{"kur_ara",  "ku", utf8str("\u06A9\u0648\u0631\u062F\u06CC")}, // Kurdish (Arabic)
		{"lao",      "lo", utf8str("\u0E9E\u0EB2\u0EAA\u0EB2\u0EA5\u0EB2\u0EA7")}, // Lao
		{"lat",      "la", utf8str("Latina")}, // Latin
		{"lav",      "lv", utf8str("Latvie\u0161u")}, // Latvian
		{"lit",      "lt", utf8str("Lietuvi\u0173")}, // Lithuanian
		{"ltz",      "lb", utf8str("L\u00EBtzebuergesch")}, // Luxembourgish
		{"mal",      "ml", utf8str("\u0D2E\u0D32\u0D2F\u0D3E\u0D33\u0D02")}, // Malayalam
		{"mar",      "mr", utf8str("\u092E\u0930\u093E\u0920\u0940")}, // Marathi
		{"mkd",      "mk", utf8str("M\u0430\u043A\u0435\u0434\u043E\u043D\u0441\u043A\u0438")}, // Macedonian
		{"mlt",      "mt", utf8str("Malti")}, // Maltese
		{"mon",      "mn", utf8str("\u041C\u043E\u043D\u0433\u043E\u043B \u0425\u044D\u043B")}, // Mongolian
		{"mri",      "mi", utf8str("te Reo M\u0101ori")}, // Maori
		{"msa",      "ms", utf8str("Bahasa Melayu")}, // Malay
		{"mya",      "my", utf8str("\u1019\u103C\u1014\u103A\u1019\u102C\u1005\u102C")}, // Burmese
		{"nep",      "ne", utf8str("\u0928\u0947\u092A\u093E\u0932\u0940")}, // Nepali
		{"nld",      "nl", utf8str("Nederlands")}, // Dutch
		{"nor",      "no", utf8str("Norsk")}, // Norwegian
		{"oci",      "oc", utf8str("Occitan")}, // Occitan
		{"ori",      "or", utf8str("\u0b13\u0b21\u0b3c\u0b3f\u0b06")}, // Oriya
		{"pan",      "pa", utf8str("\u0A2A\u0A70\u0A1C\u0A3E\u0A2C\u0A40")}, // Punjabi
		{"pol",      "pl", utf8str("Polski")}, // Polish
		{"por",      "pt", utf8str("Portugu\u00EAs")}, // Portuguese
		{"pus",      "ps", utf8str("\u067E\u069A\u062A\u0648")}, // Pashto
		{"que",      "qu", utf8str("Runasimi")}, // Quechuan
		{"ron",      "ro", utf8str("Rom\u00E2n\u0103")}, // Romanian
		{"rus",      "ru", utf8str("\u0420\u0443\u0441\u0441\u043A\u0438\u0439")}, // Russian
		{"san",      "sa", utf8str("\u0938\u0902\u0938\u094D\u0915\u0943\u0924\u092E\u094D")}, // Sanskrit
		{"sin",      "si", utf8str("\u0DC3\u0DD2\u0D82\u0DC4\u0DBD")}, // Sinhala
		{"slk",      "sk", utf8str("Sloven\u010Dina")}, // Slovak
		{"slk_frak", "sk", utf8str("Sloven\u010Dina (Frakt\u00far)")}, // Slovak (Fraktur)
		{"slv",      "sl", utf8str("Sloven\u0161\u010Dina")}, // Slovene
		{"snd",      "sd", utf8str("\u0633\u0646\u068C\u064A")}, // Sindhi
		{"spa",      "es", utf8str("Espa\u00F1ol")}, // Spanish
		{"spa_old",  "es", utf8str("Espa\u00F1ol (Antiguo)")}, // Spanish (Old)
		{"sqi",      "sq", utf8str("Shqip")}, // Albanian
		{"srp",      "sr", utf8str("\u0421\u0440\u043F\u0441\u043A\u0438")}, // Serbian
		{"srp_latn", "sr", utf8str("Srpski")}, // Serbian (Latin)
		{"sun",      "su", utf8str("\u1B98\u1B9E \u1B9E\u1BA5\u1B94\u1BAA\u1B93")}, // Sundanese
		{"swa",      "sw", utf8str("Kiswahili")}, // Swahili
		{"swe",      "sv", utf8str("Svenska")}, // Swedish
		{"syr",      "",   utf8str("\u0720\u072b\u0722\u0710 \u0723\u0718\u072a\u071d\u071d\u0710")}, // Syriac
		{"tam",      "ta", utf8str("\u0BA4\u0BAE\u0BBF\u0BB4\u0BCD")}, // Tamil
		{"tat",      "tt", utf8str("Tatar\u00E7a")}, // Tatar
		{"tel",      "te", utf8str("\u0C24\u0C46\u0C32\u0C41\u0C17\u0C41")}, // Telugu
		{"tgk",      "tg", utf8str("\u0442\u043e\u04b7\u0438\u043a\u04e3")}, // Tajik
		{"tgl",      "tl", utf8str("\u170A\u170A\u170C\u1712")}, // Tagalog
		{"tha",      "th", utf8str("\u0E20\u0E32\u0E29\u0E32\u0E44\u0E17\u0E22")}, // Thai
		{"tir",      "ti", utf8str("\u1275\u130D\u122D\u129B")}, // Tigrinya
		{"ton",      "to", utf8str("Lea faka-Tonga")}, // Tongan
		{"tur",      "tr", utf8str("T\u00FCrk\u00E7e")}, // Turkish
		{"uig",      "ug", utf8str("\u0626\u06C7\u064A\u063A\u06C7\u0631 \u062A\u0649\u0644\u0649")}, // Uyghur
		{"ukr",      "uk", utf8str("\u0423\u043A\u0440\u0430\u0457\u043D\u0441\u044C\u043A\u0430")}, // Ukrainian
		{"urd",      "ur", utf8str("\u0627\u064F\u0631\u062F\u064F\u0648")}, // Urdu
		{"uzb",      "uz", utf8str("\u0627\u0648\u0632\u0628\u06CC\u06A9")}, // Uzbek
		{"uzb_cyrl", "uz", utf8str("\u040E\u0437\u0431\u0435\u043A")}, // Uzbek (Cyrillic)
		{"vie",      "vi", utf8str("Ti\u1EBFng Vi\u1EC7t Nam")}, // Vietnamese
		{"yid",      "yi", utf8str("\u05D9\u05D9\u05B4\u05D3\u05D9\u05E9 \u05D9\u05D9\u05B4\u05D3\u05D9\u05E9\u202C")}, // Yiddish
		{"yor",      "yo", utf8str("\u00C8d\u00E8 Yor\u00F9b\u00E1")}, // Yoruba
	};
}

template<class Container>
Container languageCultures() {
	return Container{
		{"af", "af_ZA"},
		{"ar", "ar_AE"},
		{"ar", "ar_BH"},
		{"ar", "ar_DZ"},
		{"ar", "ar_EG"},
		{"ar", "ar_IQ"},
		{"ar", "ar_JO"},
		{"ar", "ar_KW"},
		{"ar", "ar_LB"},
		{"ar", "ar_LY"},
		{"ar", "ar_MA"},
		{"ar", "ar_OM"},
		{"ar", "ar_QA"},
		{"ar", "ar_SA"},
		{"ar", "ar_SY"},
		{"ar", "ar_TN"},
		{"ar", "ar_YE"},
		{"az", "az_AZ"},
		{"az", "az_AZ"},
		{"be", "be_BY"},
		{"bg", "bg_BG"},
		{"br", "br_FR"},
		{"ca", "ca_ES"},
		{"co", "co_FR"},
		{"co", "co_IT"},
		{"cs", "cs_CZ"},
		{"da", "da_DK"},
		{"de", "de_AT"},
		{"de", "de_CH"},
		{"de", "de_DE"},
		{"de", "de_LI"},
		{"de", "de_LU"},
		{"div", "div_MV"},
		{"dv", "dv_MV"},
		{"el", "el_GR"},
		{"en", "en_AU"},
		{"en", "en_BZ"},
		{"en", "en_CA"},
		{"en", "en_CB"},
		{"en", "en_GB"},
		{"en", "en_IE"},
		{"en", "en_JM"},
		{"en", "en_NZ"},
		{"en", "en_PH"},
		{"en", "en_TT"},
		{"en", "en_US"},
		{"en", "en_ZA"},
		{"en", "en_ZW"},
		{"es", "es_AR"},
		{"es", "es_BO"},
		{"es", "es_CL"},
		{"es", "es_CO"},
		{"es", "es_CR"},
		{"es", "es_DO"},
		{"es", "es_EC"},
		{"es", "es_ES"},
		{"es", "es_GT"},
		{"es", "es_HN"},
		{"es", "es_MX"},
		{"es", "es_NI"},
		{"es", "es_PA"},
		{"es", "es_PE"},
		{"es", "es_PR"},
		{"es", "es_PY"},
		{"es", "es_SV"},
		{"es", "es_UY"},
		{"es", "es_VE"},
		{"et", "et_EE"},
		{"eu", "eu_ES"},
		{"fa", "fa_IR"},
		{"fi", "fi_FI"},
		{"fo", "fo_FO"},
		{"fr", "fr_BE"},
		{"fr", "fr_CA"},
		{"fr", "fr_CH"},
		{"fr", "fr_FR"},
		{"fr", "fr_LU"},
		{"fr", "fr_MC"},
		{"fo", "fo_FO"},
		{"fo", "fo_DK"},
		{"fy", "fy_NL"},
		{"gd", "gd_GB"},
		{"gd", "gd_CA"},
		{"gl", "gl_ES"},
		{"gu", "gu_IN"},
		{"he", "he_IL"},
		{"he", "he_TORANIT"},
		{"hi", "hi_IN"},
		{"hr", "hr_HR"},
		{"hu", "hu_HU"},
		{"hy", "hy_AM"},
		{"id", "id_ID"},
		{"is", "is_IS"},
		{"it", "it_CH"},
		{"it", "it_IT"},
		{"ja", "ja_JP"},
		{"ka", "ka_GE"},
		{"kk", "kk_KZ"},
		{"kn", "kn_IN"},
		{"kok", "kok_IN"},
		{"ko", "ko_KR"},
		{"ku", "ku_IQ"},
		{"ky", "ky_KZ"},
		{"lb", "lb_LU"},
		{"lt", "lt_LT"},
		{"lv", "lv_LV"},
		{"mi", "mi_NZ"},
		{"mk", "mk_MK"},
		{"mn", "mn_MN"},
		{"mn", "mn_CN"},
		{"mr", "mr_IN"},
		{"ms", "ms_BN"},
		{"ms", "ms_MY"},
		{"nb", "nb_NO"},
		{"nl", "nl_BE"},
		{"nl", "nl_NL"},
		{"nn", "nn_NO"},
		{"oc", "oc_ES"},
		{"pa", "pa_IN"},
		{"pl", "pl_PL"},
		{"pt", "pt_BR"},
		{"pt", "pt_PT"},
		{"qu", "qu_PE"},
		{"ro", "ro_RO"},
		{"ru", "ru_RU"},
		{"sa", "sa_IN"},
		{"sd", "sd_PK"},
		{"sd", "sd_IN"},
		{"sk", "sk_SK"},
		{"sl", "sl_SI"},
		{"sq", "sq_AL"},
		{"sr", "sr_SP"},
		{"sr", "sr_SP"},
		{"su", "su_ID"},
		{"sv", "sv_FI"},
		{"sv", "sv_SE"},
		{"sw", "sw_KE"},
		{"syr", "syr_SY"},
		{"ta", "ta_IN"},
		{"te", "te_IN"},
		{"th", "th_TH"},
		{"to", "to_TO"},
		{"tr", "tr_TR"},
		{"tt", "tt_RU"},
		{"uk", "uk_UA"},
		{"ur", "ur_PK"},
		{"uz", "uz_UZ"},
		{"vi", "vi_VN"},
		{"yo", "yo_NR"},
		{"zh", "zh_CHS"},
		{"zh", "zh_CHT"},
		{"zh", "zh_CN"},
		{"zh", "zh_HK"},
		{"zh", "zh_MO"},
		{"zh", "zh_SG"},
		{"zh", "zh_TW"},
	};
}

}

#endif // LANGTABLES_HH

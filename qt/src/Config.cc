/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.cc
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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

const QList<Config::Lang> Config::LANGUAGES = {
	// {ISO 639-2, ISO 639-1, name}
	{"ara", "ar_AE", QString::fromUtf8("\u0627\u0644\u0639\u0631\u0628\u064a\u0629")},
	{"bul", "bg_BG", QString::fromUtf8("\u0431\u044a\u043b\u0433\u0430\u0440\u0441\u043a\u0438 \u0435\u0437\u0438\u043a")},
	{"cat", "ca_ES", QString::fromUtf8("Catal\u00e0")},
	{"ces", "cs_CS", QString::fromUtf8("\u010cesky")},
	{"chi_sim", "zh_CN", QString::fromUtf8("\u7b80\u4f53\u5b57")},
	{"chi_tra", "zh_TW", QString::fromUtf8("\u7e41\u9ad4\u5b57")},
	{"dan-frak", "da_DK", QString::fromUtf8("Dansk (Frak)")},
	{"dan", "da_DK", QString::fromUtf8("Dansk")},
	{"deu-frak", "de_DE", QString::fromUtf8("Deutsch (Frak)")},
	{"deu-f", "de_DE", QString::fromUtf8("Deutsch (Frak)")},
	{"deu", "de_DE", QString::fromUtf8("Deutsch")},
	{"ell", "el_GR", QString::fromUtf8("\u0395\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac")},
	{"eng", "en_US", QString::fromUtf8("English")},
	{"fin", "fi_FI", QString::fromUtf8("Suomi")},
	{"fra", "fr_FR", QString::fromUtf8("Fran\u00e7ais")},
	{"heb", "he_IL", QString::fromUtf8("\u05e2\u05d1\u05e8\u05d9\u05ea")},
	{"hrv", "hr_HR", QString::fromUtf8("hrvatski")},
	{"hun", "hu_HU", QString::fromUtf8("Magyar")},
	{"ind", "id_ID", QString::fromUtf8("Bahasa Indonesia")},
	{"ita", "it_IT", QString::fromUtf8("Italiano")},
	{"jpn", "ja_JP", QString::fromUtf8("\u65e5\u672c\u8a9e")},
	{"kan", "kn_IN", QString::fromUtf8("\u0c95\u0ca8\u0ccd\u0ca8\u0ca1")},
	{"kor", "ko_KR", QString::fromUtf8("\ud55c\uad6d\ub9d0")},
	{"lav", "lv_LV", QString::fromUtf8("Latvie\u0161u Valoda")},
	{"lit", "lt_LT", QString::fromUtf8("Lietuvi\u0173 Kalba")},
	{"nld", "nl_NL", QString::fromUtf8("Nederlands")},
	{"nor", "no_NO", QString::fromUtf8("Norsk")},
	{"pol", "pl_PL", QString::fromUtf8("Polski")},
	{"por", "pt_PT", QString::fromUtf8("Portugu\u00eas")},
	{"ron", "ro_RO", QString::fromUtf8("Rom\u00e2n\u0103")},
	{"rus", "ru_RU", QString::fromUtf8("P\u0443\u0441\u0441\u043a\u0438\u0439 \u044f\u0437\u044b\u043a")},
	{"slk-frak", "sk_SK", QString::fromUtf8("Sloven\u010dina")},
	{"slk", "sk_SK", QString::fromUtf8("Sloven\u010dina")},
	{"slv", "sl_SI", QString::fromUtf8("Sloven\u0161\u010dina")},
	{"spa", "es_ES", QString::fromUtf8("Espa\u00f1ol")},
	{"srp", "sr_RS", QString::fromUtf8("\u0441\u0440\u043f\u0441\u043a\u0438 \u0458\u0435\u0437\u0438\u043a")},
	{"swe-frak", "sv_SE", QString::fromUtf8("Svenska (Frak)")},
	{"swe", "sv_SE", QString::fromUtf8("Svenska")},
	{"tgl", "tl_PH", QString::fromUtf8("Wikang Tagalog")},
	{"tur", "tr_TR", QString::fromUtf8("T\u00fcrk\u00e7e")},
	{"ukr", "uk_UA", QString::fromUtf8("Y\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430")},
	{"vie", "vi_VN", QString::fromUtf8("Ti\u1ebfng Vi\u1ec7t")}
};

Config::Config(QWidget* parent)
	: QDialog(parent)
{
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

	for(const Lang& lang : LANGUAGES){
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
	connect(ui.tableWidgetAdditionalLang->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(langTableSelectionChanged(QItemSelection,QItemSelection)));
	connect(ui.buttonBox->button(QDialogButtonBox::Help), SIGNAL(clicked()), MAIN, SLOT(showHelp()));
	connect(ui.lineEditLangPrefix, SIGNAL(textChanged(QString)), this, SLOT(clearLineEditErrorState()));
	connect(ui.lineEditLangName, SIGNAL(textChanged(QString)), this, SLOT(clearLineEditErrorState()));
	connect(ui.lineEditLangCode, SIGNAL(textChanged(QString)), this, SLOT(clearLineEditErrorState()));

	addSetting(new SwitchSetting("dictinstall", ui.checkBoxDictInstall, true));
	addSetting(new SwitchSetting("updatecheck", ui.checkBoxUpdateCheck, true));
	addSetting(new TableSetting("customlangs", ui.tableWidgetAdditionalLang));
	addSetting(new SwitchSetting("systemoutputfont", ui.checkBoxDefaultOutputFont, true));
	addSetting(new FontSetting("customoutputfont", &m_fontDialog, QFont().toString()));

	updateFontButton(m_fontDialog.currentFont());
}

Config::~Config()
{
	qDeleteAll(m_settings.values());
}

bool Config::searchLangSpec(Lang& lang) const
{
	for(const QTableWidget* table : QList<QTableWidget*>{ui.tableWidgetPredefLang, ui.tableWidgetAdditionalLang}){
		for(int row = 0, nRows = table->rowCount(); row < nRows; ++row){
			if(table->item(row, 0)->text() == lang.prefix) {
				lang = {table->item(row, 0)->text(), table->item(row, 1)->text(), table->item(row, 2)->text()};
				return true;
			}
		}
	}
	return false;
}

void Config::showDialog()
{
	toggleAddLanguage(true);
	exec();
	getSetting<TableSetting>("customlangs")->serialize();
}

void Config::disableDictInstall()
{
	getSetting<SwitchSetting>("dictinstall")->setValue(false);
}

void Config::disableUpdateCheck()
{
	getSetting<SwitchSetting>("updatecheck")->setValue(false);
}

void Config::toggleAddLanguage(bool forceHide)
{
	bool addVisible = forceHide ? true : ui.widgetAddLang->isVisible();
	ui.widgetAddLang->setVisible(!addVisible);
	ui.widgetAddRemoveLang->setVisible(addVisible);
	if(addVisible){
		ui.pushButtonAddLang->setFocus();
	}else{
		ui.pushButtonAddLangOk->setFocus();
	}
	ui.lineEditLangPrefix->setText("");
	ui.lineEditLangPrefix->setStyleSheet("");
	ui.lineEditLangCode->setText("");
	ui.lineEditLangCode->setStyleSheet("");
	ui.lineEditLangName->setText("");
	ui.lineEditLangName->setStyleSheet("");
}

void Config::addLanguage()
{
	QString errorStyle = "background: #FF7777; color: #FFFFFF;";
	bool invalid = false;
	if(QRegExp("^\\w+$").indexIn(ui.lineEditLangPrefix->text()) == -1){
		invalid = true;
		ui.lineEditLangPrefix->setStyleSheet(errorStyle);
	}
	if(QRegExp("^.+$").indexIn(ui.lineEditLangName->text()) == -1){
		invalid = true;
		ui.lineEditLangName->setStyleSheet(errorStyle);
	}
	if(QRegExp("^[a-z]{2}_[A-Z]{2}$").indexIn(ui.lineEditLangCode->text()) == -1){
		invalid = true;
		ui.lineEditLangCode->setStyleSheet(errorStyle);
	}
	if(!invalid){
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

void Config::removeLanguage()
{
	for(const QModelIndex& index : ui.tableWidgetAdditionalLang->selectionModel()->selectedRows())
	{
		ui.tableWidgetAdditionalLang->removeRow(index.row());
	}
}

void Config::updateFontButton(const QFont &font)
{
	ui.pushButtonOutputFont->setText(QString("%1 %2").arg(font.family()).arg(font.pointSize()));
}

void Config::langTableSelectionChanged(const QItemSelection &selected, const QItemSelection &/*deselected*/)
{
	ui.pushButtonRemoveLang->setDisabled(selected.isEmpty());
}

void Config::clearLineEditErrorState()
{
	static_cast<QLineEdit*>(QObject::sender())->setStyleSheet("");
}

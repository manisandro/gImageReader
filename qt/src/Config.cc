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
#include "LangTables.hh"
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

const QList<Config::Lang> Config::LANGUAGES = LangTables::languages<QList<Config::Lang>, QString>([](const char* str) { return QString::fromUtf8(str); });
const QMultiMap<QString, QString> Config::LANGUAGE_CULTURES = LangTables::languageCultures<QMultiMap<QString, QString>>();


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
	// Tesseract 4.x up to beta.1 had script tessdatas on same level as language tessdatas, but they are distinguishable in that they begin with an upper case character
	if(lang.prefix.startsWith("script", Qt::CaseInsensitive) || lang.prefix.left(1).toUpper() == lang.prefix.left(1)) {
		QString name = lang.prefix.startsWith("script", Qt::CaseInsensitive) ? lang.prefix.mid(7) : lang.prefix;
		lang.name = QString("%1 [%2]").arg(name).arg(_("Script"));
		return true;
	}
	for(const QTableWidget* table : QList<QTableWidget*> {ui.tableWidgetPredefLang, ui.tableWidgetAdditionalLang}) {
		for(int row = 0, nRows = table->rowCount(); row < nRows; ++row) {
			if(table->item(row, 0)->text() == lang.prefix) {
				lang = {lang.prefix, table->item(row, 1)->text(), QString("%1 [%2]").arg(table->item(row, 2)->text(), lang.prefix)};
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
	ui.lineEditSpellLocation->setText(spellingLocation(static_cast<Location>(idx)));
	ui.lineEditTessdataLocation->setText(tessdataLocation(static_cast<Location>(idx)));
}

void Config::openTessdataDir() {
	int idx = QSettings().value("datadirs").toInt();
	QString tessdataDir = tessdataLocation(static_cast<Location>(idx));
	QDir().mkpath(tessdataDir);
	QDesktopServices::openUrl(QUrl::fromLocalFile(tessdataDir));
}

void Config::openSpellingDir() {
	int idx = QSettings().value("datadirs").toInt();
	QString spellingDir = spellingLocation(static_cast<Location>(idx));
	QDir().mkpath(spellingDir);
	QDesktopServices::openUrl(QUrl::fromLocalFile(spellingDir));
}

QString Config::spellingLocation(Location location) {
	if(location == SystemLocation) {
#ifdef Q_OS_WIN
		QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));
#else
		QDir dataDir("/usr/share");
#endif
#if HAVE_ENCHANT2
		if(QDir(dataDir.absoluteFilePath("myspell")).exists()) {
			return dataDir.absoluteFilePath("myspell");
		} else {
			return dataDir.absoluteFilePath("hunspell");
		}
#else
		return dataDir.absoluteFilePath("myspell/dicts");
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
#if HAVE_ENCHANT2
		return configDir.absoluteFilePath("enchant/hunspell");
#else
		return configDir.absoluteFilePath("enchant/myspell");
#endif
	}
}

QString Config::tessdataLocation(Location location) {
	if(location == SystemLocation) {
#ifdef Q_OS_WIN
		QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));
		qputenv("TESSDATA_PREFIX", dataDir.absoluteFilePath("tessdata").toLocal8Bit());
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
	QByteArray current = setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "C");
	tesseract::TessBaseAPI tess;
	tess.Init(nullptr, nullptr);
	setlocale(LC_ALL, current.constData());
	return QString(tess.GetDatapath());
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
	if(QRegExp("^[a-z]{2,}$").indexIn(ui.lineEditLangCode->text()) == -1 &&
	        QRegExp("^[a-z]{2,}_[A-Z]{2,}$").indexIn(ui.lineEditLangCode->text()) == -1) {
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

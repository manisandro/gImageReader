/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.cc
 * Copyright (C) 2013-2025 Sandro Mani <manisandro@gmail.com>
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
#include <QLibraryInfo>
#include <QMultiMap>
#include <QStandardPaths>
#include <QUrl>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#if TESSERACT_MAJOR_VERSION < 5
#include <tesseract/genericvector.h>
#endif
#undef USE_STD_NAMESPACE

const QList<Config::Lang> Config::LANGUAGES = LangTables::languages<QList<Config::Lang>, QString> ([](const char* str) { return QString::fromUtf8(str); });
const QMap<QString, QString> Config::LANG_LOOKUP = [] {
	QMap<QString, QString> lookup;
	for (const Config::Lang& lang : LANGUAGES) {
		lookup.insert(lang.prefix, lang.code);
	}
	return lookup;
}();

const QMultiMap<QString, QString> Config::LANGUAGE_CULTURES = LangTables::languageCultures<QMultiMap<QString, QString >> ();


Config::Config(QWidget* parent)
	: QDialog(parent) {
	ui.setupUi(this);

	ui.tableWidgetPredefLang->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	ui.tableWidgetAdditionalLang->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	ui.widgetAddLang->setVisible(false);

#if !ENABLE_VERSIONCHECK
	ui.checkBoxUpdateCheck->setVisible(false);
#endif

	for (const Lang& lang : LANGUAGES) {
		int row = ui.tableWidgetPredefLang->rowCount();
		ui.tableWidgetPredefLang->insertRow(row);
		ui.tableWidgetPredefLang->setItem(row, 0, new QTableWidgetItem(lang.prefix));
		ui.tableWidgetPredefLang->setItem(row, 1, new QTableWidgetItem(lang.code));
		ui.tableWidgetPredefLang->setItem(row, 2, new QTableWidgetItem(lang.name));
	}

	connect(ui.checkBoxDefaultOutputFont, &QCheckBox::toggled, ui.pushButtonOutputFont, &QPushButton::setDisabled);
	connect(ui.pushButtonOutputFont, &QPushButton::clicked, &m_fontDialog, &QFontDialog::exec);
	connect(&m_fontDialog, &QFontDialog::fontSelected, this, &Config::updateFontButton);
	connect(ui.pushButtonAddLang, &QPushButton::clicked, this, &Config::toggleAddLanguage);
	connect(ui.pushButtonRemoveLang, &QPushButton::clicked, this, &Config::removeLanguage);
	connect(ui.pushButtonAddLangOk, &QPushButton::clicked, this, &Config::addLanguage);
	connect(ui.pushButtonAddLangCancel, &QPushButton::clicked, this, &Config::toggleAddLanguage);
	connect(ui.tableWidgetAdditionalLang->selectionModel(), &QItemSelectionModel::selectionChanged, this, &Config::langTableSelectionChanged);
	connect(ui.buttonBox->button(QDialogButtonBox::Help), &QPushButton::clicked, MAIN, [] { MAIN->showHelp(); });
	connect(ui.lineEditLangPrefix, &QLineEdit::textChanged, this, &Config::clearLineEditErrorState);
	connect(ui.lineEditLangName, &QLineEdit::textChanged, this, &Config::clearLineEditErrorState);
	connect(ui.lineEditLangCode, &QLineEdit::textChanged, this, &Config::clearLineEditErrorState);
	connect(ui.comboBoxDataLocation, qOverload<int> (&QComboBox::currentIndexChanged), this, &Config::setDataLocations);

	ADD_SETTING(SwitchSetting("dictinstall", ui.checkBoxDictInstall, true));
	ADD_SETTING(SwitchSetting("updatecheck", ui.checkBoxUpdateCheck, true));
	ADD_SETTING(SwitchSetting("openafterexport", ui.checkBoxOpenAfterExport, false));
	ADD_SETTING(TableSetting("customlangs", ui.tableWidgetAdditionalLang));
	ADD_SETTING(SwitchSetting("systemoutputfont", ui.checkBoxDefaultOutputFont, true));
	ADD_SETTING(FontSetting("customoutputfont", &m_fontDialog, QFont().toString()));
	ADD_SETTING(ComboSetting("textencoding", ui.comboBoxEncoding, 0));
	ADD_SETTING(ComboSetting("datadirs", ui.comboBoxDataLocation, 0));
	ADD_SETTING(VarSetting<QString> ("sourcedir", Utils::documentsFolder()));
	ADD_SETTING(VarSetting<QString> ("outputdir", Utils::documentsFolder()));
	ADD_SETTING(VarSetting<QString> ("auxdir", Utils::documentsFolder()));

	updateFontButton(m_fontDialog.currentFont());
}

bool Config::searchLangSpec(Lang& lang) const {
	// Tesseract 4.x up to beta.1 had script tessdatas on same level as language tessdatas, but they are distinguishable in that they begin with an upper case character
	if (lang.prefix.startsWith("script", Qt::CaseInsensitive) || lang.prefix.left(1).toUpper() == lang.prefix.left(1)) {
		QString name = lang.prefix.startsWith("script", Qt::CaseInsensitive) ? lang.prefix.mid(7) : lang.prefix;
		lang.name = QString("%1 [%2]").arg(name).arg(_("Script"));
		return true;
	}
	for (const QTableWidget* table : QList<QTableWidget*> {ui.tableWidgetPredefLang, ui.tableWidgetAdditionalLang}) {
		for (int row = 0, nRows = table->rowCount(); row < nRows; ++row) {
			if (table->item(row, 0)->text() == lang.prefix) {
				lang = {lang.prefix, table->item(row, 1)->text(), QString("%1 [%2]").arg(table->item(row, 2)->text(), lang.prefix) };
				return true;
			}
		}
	}
	return false;
}

QStringList Config::searchLangCultures(const QString& code) const {
	return LANGUAGE_CULTURES.values(code);
}

void Config::showDialog() {
	toggleAddLanguage(true);
	exec();
	ConfigSettings::get<TableSetting> ("customlangs")->serialize();
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

QStringList Config::getAvailableLanguages() {
	Utils::TesseractHandle tess;
	if (!tess.get()) {
		return QStringList();
	}
#if TESSERACT_MAJOR_VERSION < 5
	GenericVector<STRING> availLanguages;
#else
	std::vector<std::string> availLanguages;
#endif
	try {
		tess.get()->GetAvailableLanguagesAsVector(&availLanguages);
	} catch (const std::exception&) {
		// Pass
	}
	QStringList result;
	for (std::size_t i = 0; i < availLanguages.size(); ++i) {
		result.append(availLanguages[i].c_str());
	}
	std::sort(result.begin(), result.end(), [](const QString & s1, const QString & s2) {
		bool s1Script = s1.startsWith("script") || s1.left(1) == s1.left(1).toUpper();
		bool s2Script = s2.startsWith("script") || s2.left(1) == s2.left(1).toUpper();
		if (s1Script != s2Script) {
			return !s1Script;
		} else {
			return s1 < s2;
		}
	});
	return result;
}

void Config::disableDictInstall() {
	ConfigSettings::get<SwitchSetting> ("dictinstall")->setValue(false);
}

void Config::disableUpdateCheck() {
	ConfigSettings::get<SwitchSetting> ("updatecheck")->setValue(false);
}

void Config::setDataLocations(int idx) {
	ui.lineEditSpellLocation->setText(spellingLocation(static_cast<Location> (idx)));
	ui.lineEditTessdataLocation->setText(tessdataLocation(static_cast<Location> (idx)));
}

void Config::openTessdataDir() {
	int idx = QSettings().value("datadirs").toInt();
	QString tessdataDir = tessdataLocation(static_cast<Location> (idx));
	QDir().mkpath(tessdataDir);
	QDesktopServices::openUrl(QUrl::fromLocalFile(tessdataDir));
}

void Config::openSpellingDir() {
	int idx = QSettings().value("datadirs").toInt();
	QString spellingDir = spellingLocation(static_cast<Location> (idx));
	QDir().mkpath(spellingDir);
	QDesktopServices::openUrl(QUrl::fromLocalFile(spellingDir));
}

QString Config::spellingLocation(Location location) {
	if (location == SystemLocation) {
#ifdef Q_OS_WIN
		QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));
#else
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QDir dataDir = QDir(QString("%1/share/").arg(QLibraryInfo::path(QLibraryInfo::PrefixPath)));
#else
		QDir dataDir = QDir(QString("%1/share/").arg(QLibraryInfo::location(QLibraryInfo::PrefixPath)));
#endif
#endif
#if HAVE_ENCHANT2
		if (QDir(dataDir.absoluteFilePath("myspell")).exists()) {
			return dataDir.absoluteFilePath("myspell");
		} else {
			return dataDir.absoluteFilePath("hunspell");
		}
#else
		return dataDir.absoluteFilePath("myspell/dicts");
#endif
	} else {
		QDir configDir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
#if HAVE_ENCHANT2
		return configDir.absoluteFilePath("enchant/hunspell");
#else
		return configDir.absoluteFilePath("enchant/myspell");
#endif
	}
}

QString Config::tessdataLocation(Location location) {
	if (location == SystemLocation) {
#ifdef Q_OS_WIN
		QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));
		qputenv("TESSDATA_PREFIX", dataDir.absoluteFilePath("tessdata").toLocal8Bit());
#else
		qunsetenv("TESSDATA_PREFIX");
#endif
	} else {
		QDir configDir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
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
	if (addVisible) {
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
	if (!QRegularExpression("^[\\w/]+$").match(ui.lineEditLangPrefix->text()).hasMatch()) {
		invalid = true;
		ui.lineEditLangPrefix->setStyleSheet(errorStyle);
	}
	if (!QRegularExpression("^.+$").match(ui.lineEditLangName->text()).hasMatch()) {
		invalid = true;
		ui.lineEditLangName->setStyleSheet(errorStyle);
	}
	if (!ui.lineEditLangCode->text().isEmpty() && !QRegularExpression("^[a-z]{2,}(_[A-Z]{2,})?$").match(ui.lineEditLangCode->text()).hasMatch()) {
		invalid = true;
		ui.lineEditLangCode->setStyleSheet(errorStyle);
	}
	if (!invalid) {
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
	for (const QModelIndex& index : ui.tableWidgetAdditionalLang->selectionModel()->selectedRows()) {
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
	static_cast<QLineEdit*> (QObject::sender())->setStyleSheet("");
}

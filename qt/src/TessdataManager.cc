/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * TessdataManager.hh
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

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <qjson/parser.h>
#else
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#endif
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>

#include "Config.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"
#include "TessdataManager.hh"
#include "Utils.hh"

TessdataManager::TessdataManager(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(_("Tessdata Manager"));
	setLayout(new QVBoxLayout());
	layout()->addWidget(new QLabel(_("Manage installed languages:")));

	m_languageList = new QListWidget(this);
	layout()->addWidget(m_languageList);

	QDialogButtonBox* bbox = new QDialogButtonBox();
	bbox->addButton(QDialogButtonBox::Close);
	bbox->addButton(QDialogButtonBox::Apply);
	connect(bbox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(applyChanges()));
	connect(bbox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(bbox, SIGNAL(rejected()), this, SLOT(reject()));
	layout()->addWidget(bbox);
	setFixedWidth(320);
}

bool TessdataManager::setup()
{
#ifdef Q_OS_LINUX
	if(MAIN->getConfig()->useSystemDataLocations()) {
		QDBusConnectionInterface* iface = QDBusConnection::sessionBus().interface();
		iface->startService("org.freedesktop.PackageKit");
		if(!iface->isServiceRegistered("org.freedesktop.PackageKit").value()){
			QMessageBox::critical(MAIN, _("Error"), _("PackageKit is required for managing system-wide tesseract language packs, but it was not found. Please use the system package management software to manage the tesseract language packs, or switch to use the user tessdata path in the configuration dialog."));
			return false;
		}
	}
#endif
	MAIN->pushState(MainWindow::State::Busy, _("Fetching available languages"));
	QString messages;
	bool success = fetchLanguageList(messages);
	MAIN->popState();
	if(!success)
	{
		QMessageBox::critical(MAIN, _("Error"), _("Failed to fetch list of available languages: %1").arg(messages));
		return false;
	}
	return true;
}

bool TessdataManager::fetchLanguageList(QString& messages)
{
	QByteArray data = Utils::download(QUrl("https://api.github.com/repos/tesseract-ocr/tessdata/contents"), messages);

	if(data.isEmpty()) {
		messages = _("Failed to fetch list of available languages: %1").arg(messages);
		return false;
	}

	QList<QPair<QString,QString>> extraFiles;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	QJson::Parser parser;
	bool ok = false;
	QVariantList json = parser.parse( data, &ok ).toList();
	if(!ok) {
		messages = _("Parsing error: %1").arg(parser.errorLine() + ": " + parser.errorString());
		return false;
	}
	for(const QVariant& value : json) {
		QVariantMap treeObj = value.toMap();
		QString name = treeObj.value("name").toString();
		QString url = treeObj.value("download_url").toString();
#else
	QJsonParseError err;
	QJsonDocument json = QJsonDocument::fromJson(data, &err);
	if(json.isNull()) {
		messages = _("Parsing error: %1").arg(err.errorString());
		return false;
	}
	for(const QJsonValue& value : json.array()) {
		QJsonObject treeObj = value.toObject();
		QString name = treeObj.find("name").value().toString();
		QString url = treeObj.find("download_url").value().toString();
#endif
		if(name.endsWith(".traineddata")) {
			m_languageFiles[name.left(name.indexOf("."))].append({name,url});
		} else {
			// Delay decision to determine whether file is a supplementary language file
			extraFiles.append(qMakePair(name, url));
		}
	}
	for(const QPair<QString,QString>& extraFile : extraFiles) {
		QString lang = extraFile.first.left(extraFile.first.indexOf("."));
		if(m_languageFiles.contains(lang)) {
			m_languageFiles[lang].append({extraFile.first,extraFile.second});
		}
	}

	QStringList availableLanguages = MAIN->getRecognizer()->getAvailableLanguages();

	QStringList languages = QStringList(m_languageFiles.keys());
	languages.sort();
	for(const QString& prefix : languages) {
		Config::Lang lang;
		lang.prefix = prefix;
		QString label;
		if(MAIN->getConfig()->searchLangSpec(lang)) {
			label = QString("%1 (%2)").arg(lang.name).arg(lang.prefix);
		} else {
			label = lang.prefix;
		}
		QListWidgetItem* item = new QListWidgetItem(label);
		item->setData(Qt::UserRole, prefix);
		item->setCheckState(availableLanguages.contains(prefix) ? Qt::Checked : Qt::Unchecked);
		m_languageList->addItem(item);
	}
	return true;
}

void TessdataManager::applyChanges()
{
	MAIN->pushState(MainWindow::State::Busy, _("Applying changes..."));
	setEnabled(false);
	setCursor(Qt::WaitCursor);
	QString errorMsg;
	QStringList availableLanguages = MAIN->getRecognizer()->getAvailableLanguages();
	QDir tessDataDir(MAIN->getConfig()->tessdataLocation());
#ifdef Q_OS_WIN
	bool isWindows = true;
#else
	bool isWindows = false;
#endif
	if(!isWindows && MAIN->getConfig()->useSystemDataLocations()) {
		// Place this in a ifdef since DBus stuff cannot be compiled on Windows
#ifdef Q_OS_LINUX
		QStringList installFiles;
		QStringList removeFiles;
		for(int row = 0, nRows = m_languageList->count(); row < nRows; ++row) {
			QListWidgetItem* item = m_languageList->item(row);
			QString prefix = item->data(Qt::UserRole).toString();
			if(item->checkState() == Qt::Checked && !availableLanguages.contains(prefix)) {
				installFiles.append(tessDataDir.absoluteFilePath(QString("%1.traineddata").arg(prefix)));
			} else if(item->checkState() != Qt::Checked && availableLanguages.contains(prefix)) {
				removeFiles.append(tessDataDir.absoluteFilePath(QString("%1.traineddata").arg(prefix)));
			}
		}

		if(!installFiles.isEmpty()) {
			QDBusMessage req = QDBusMessage::createMethodCall("org.freedesktop.PackageKit", "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify", "InstallProvideFiles");
			req.setArguments(QList<QVariant>() << QVariant::fromValue((quint32)winId()) << QVariant::fromValue(installFiles) << QVariant::fromValue(QString("always")));
			QDBusMessage reply = QDBusConnection::sessionBus().call(req, QDBus::BlockWithGui, 3600000);
			if(reply.type() == QDBusMessage::ErrorMessage) {
				errorMsg = reply.errorMessage();
			}
		}
		if(errorMsg.isEmpty() && !removeFiles.isEmpty()) {
			QDBusMessage req = QDBusMessage::createMethodCall("org.freedesktop.PackageKit", "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify", "RemovePackageByFiles");
			req.setArguments(QList<QVariant>() << QVariant::fromValue((quint32)winId()) << QVariant::fromValue(removeFiles) << QVariant::fromValue(QString("always")));
			QDBusMessage reply = QDBusConnection::sessionBus().call(req, QDBus::BlockWithGui, 3600000);
			if(reply.type() == QDBusMessage::ErrorMessage) {
				errorMsg = reply.errorMessage();
			}
		}
#endif
	} else {
		QStringList errors;
		if(!QDir().mkpath(tessDataDir.absolutePath())) {
			errors.append(_("Failed to create directory for tessdata files."));
		} else {
			for(int row = 0, nRows = m_languageList->count(); row < nRows; ++row) {
				QListWidgetItem* item = m_languageList->item(row);
				QString prefix = item->data(Qt::UserRole).toString();
				if(item->checkState() == Qt::Checked && !availableLanguages.contains(prefix)) {
					for(const LangFile& langFile : m_languageFiles.value(prefix)) {
						if(!QFile(tessDataDir.absoluteFilePath(langFile.name)).exists()) {
							MAIN->pushState(MainWindow::State::Busy, _("Downloading %1...").arg(langFile.name));
							QString messages;
							QByteArray data = Utils::download(QUrl(langFile.url), messages);
							QFile file(tessDataDir.absoluteFilePath(langFile.name));
							if(data.isEmpty() || !file.open(QIODevice::WriteOnly)) {
								errors.append(langFile.name);
							} else {
								file.write(data);
							}
							MAIN->popState();
						}
					}
				} else if(item->checkState() != Qt::Checked && availableLanguages.contains(prefix)) {
					foreach(const QString& file, tessDataDir.entryList(QStringList() << prefix + ".*")) {
						if(!QFile(tessDataDir.absoluteFilePath(file)).remove()) {
							errors.append(file);
						}
					}
				}
			}
		}
		if(!errors.isEmpty()) {
			errorMsg = _("The following files could not be downloaded or removed:\n%1\n\nCheck the connectivity and directory permissions.").arg(errors.join("\n"));
		}
	}
	unsetCursor();
	setEnabled(true);
	MAIN->popState();
	MAIN->getRecognizer()->updateLanguagesMenu();
	availableLanguages = MAIN->getRecognizer()->getAvailableLanguages();
	for(int row = 0, nRows = m_languageList->count(); row < nRows; ++row) {
		QListWidgetItem* item = m_languageList->item(row);
		QString prefix = item->data(Qt::UserRole).toString();
		item->setCheckState(availableLanguages.contains(prefix) ? Qt::Checked : Qt::Unchecked);
	}
	if(!errorMsg.isEmpty()) {
		QMessageBox::critical(this, _("Error"), errorMsg);
	}
}

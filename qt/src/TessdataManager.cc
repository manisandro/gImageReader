/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * TessdataManager.hh
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

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "ConfigSettings.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"
#include "TessdataManager.hh"
#include "Utils.hh"

TessdataManager::TessdataManager(QWidget* parent)
	: QDialog(parent) {
	setWindowTitle(_("Tessdata Manager"));
	setLayout(new QVBoxLayout());
	layout()->addWidget(new QLabel(_("Manage installed languages:")));

	m_languageList = new QListWidget(this);
	layout()->addWidget(m_languageList);

	QDialogButtonBox* bbox = new QDialogButtonBox();
	bbox->addButton(QDialogButtonBox::Close);
	bbox->addButton(QDialogButtonBox::Apply);
	QPushButton* refreshButton = bbox->addButton(_("Refresh"), QDialogButtonBox::ActionRole);
	connect(bbox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &TessdataManager::applyChanges);
	connect(bbox, &QDialogButtonBox::accepted, this, &TessdataManager::accept);
	connect(bbox, &QDialogButtonBox::rejected, this, &TessdataManager::reject);
	connect(refreshButton, &QPushButton::clicked, this, &TessdataManager::refresh);
	layout()->addWidget(bbox);
	setFixedWidth(320);
}

bool TessdataManager::setup() {
#ifdef Q_OS_LINUX
	if(MAIN->getConfig()->useSystemDataLocations()) {
		QDBusConnectionInterface* iface = QDBusConnection::sessionBus().interface();
		iface->startService("org.freedesktop.PackageKit");
		if(!iface->isServiceRegistered("org.freedesktop.PackageKit").value()) {
			QMessageBox::critical(MAIN, _("Error"), _("A session connection to the PackageKit backend is required for managing system-wide tesseract language packs, but it was not found. This service is usually provided by a software-management application such as Gnome Software. Please install software which provides the necessary PackageKit interface, use other system package management software to manage the tesseract language packs directly, or switch to using the user tessdata path in the configuration dialog."));
			return false;
		}
	}
#endif
	MAIN->pushState(MainWindow::State::Busy, _("Fetching available languages"));
	QString messages;
	bool success = fetchLanguageList(messages);
	MAIN->popState();
	if(!success) {
		QMessageBox::critical(MAIN, _("Error"), _("Failed to fetch list of available languages: %1").arg(messages));
		return false;
	}
	return true;
}

bool TessdataManager::fetchLanguageList(QString& messages) {
	m_languageList->clear();

	// Get newest tag older or equal to used tesseract version
#if TESSERACT_VERSION >= TESSERACT_MAKE_VERSION(4, 0, 0)
	QUrl url("https://api.github.com/repos/tesseract-ocr/tessdata_fast/tags");
#else
	QUrl url("https://api.github.com/repos/tesseract-ocr/tessdata/tags");
#endif
	QByteArray data = Utils::download(url, messages);
	if(data.isEmpty()) {
		messages = _("Failed to fetch list of available languages: %1").arg(messages);
		return false;
	}

	QString tessdataVer;
	int bestMatchDist = std::numeric_limits<int>::max();
	static const QRegExp verRegEx("^[vV]?(\\d+).(\\d+).(\\d+)-?(\\w?.*)$");
	QJsonParseError err;
	QJsonDocument json = QJsonDocument::fromJson(data, &err);
	if(json.isNull()) {
		messages = _("Parsing error: %1").arg(err.errorString());
		return false;
	}
	for(const QJsonValue& value : json.array()) {
		QString tag = value.toObject().value("name").toString();
		if(verRegEx.indexIn(tag) != -1) {
			int tagVer = TESSERACT_MAKE_VERSION(verRegEx.cap(1).toInt(), verRegEx.cap(2).toInt(), verRegEx.cap(3).toInt());
			int dist = TESSERACT_VERSION - tagVer;
			if(dist >= 0 && dist < bestMatchDist) {
				bestMatchDist = dist;
				tessdataVer = tag;
			}
		}
	}

	QVector<QPair<QString, QString>> extraFiles;
	QList<QUrl> dataUrls;
#if TESSERACT_VERSION >= TESSERACT_MAKE_VERSION(4, 0, 0)
	dataUrls.append(QUrl("https://api.github.com/repos/tesseract-ocr/tessdata_fast/contents?ref=" + tessdataVer));
	dataUrls.append(QUrl("https://api.github.com/repos/tesseract-ocr/tessdata_fast/contents/script?ref=" + tessdataVer));
#else

	dataUrls.append(QUrl("https://api.github.com/repos/tesseract-ocr/tessdata/contents?ref=" + tessdataVer));
#endif
	for(const QUrl& url : dataUrls) {
		data = Utils::download(url, messages);

		if(data.isEmpty()) {
			continue;
		}

		err = QJsonParseError();
		json = QJsonDocument::fromJson(data, &err);
		if(json.isNull()) {
			continue;
		}
		for(const QJsonValue& value : json.array()) {
			QJsonObject treeObj = value.toObject();
			QString fileName = treeObj.value("name").toString();
			QString url = treeObj.value("download_url").toString();
			QString subdir = "";
			// If filename starts with upper case letter, it is a script
			if(fileName.left(1) == fileName.left(1).toUpper()) {
				subdir = "script/";
			}
			if(fileName.endsWith(".traineddata")) {
				QString prefix = subdir + fileName.left(fileName.indexOf("."));
				m_languageFiles[prefix].append({subdir + fileName, url});
			} else {
				// Delay decision to determine whether file is a supplementary language file
				extraFiles.append(qMakePair(subdir + fileName, url));
			}
		}
		for(const QPair<QString, QString>& extraFile : extraFiles) {
			QString lang = extraFile.first.left(extraFile.first.indexOf("."));
			if(m_languageFiles.contains(lang)) {
				m_languageFiles[lang].append({extraFile.first, extraFile.second});
			}
		}
	}

	if(m_languageFiles.isEmpty()) {
		messages = _("Failed to fetch list of available languages: %1").arg(messages);
		return false;
	}

	QStringList availableLanguages = MAIN->getRecognizer()->getAvailableLanguages();

	QStringList languages = QStringList(m_languageFiles.keys());
	std::sort(languages.begin(), languages.end(), [](const QString & s1, const QString & s2) {
		bool s1Script = s1.startsWith("script") || s1.left(1) == s1.left(1).toUpper();
		bool s2Script = s2.startsWith("script") || s2.left(1) == s2.left(1).toUpper();
		if(s1Script != s2Script) {
			return !s1Script;
		} else {
			return s1 < s2;
		}
	});

	for(const QString& prefix : languages) {
		Config::Lang lang;
		lang.prefix = prefix;
		QString label;
		if(MAIN->getConfig()->searchLangSpec(lang)) {
			label = lang.name;
		} else {
			label = lang.prefix;
		}
		QListWidgetItem* item = new QListWidgetItem(label);
		item->setData(Qt::UserRole, lang.prefix);
		item->setCheckState(availableLanguages.contains(lang.prefix) ? Qt::Checked : Qt::Unchecked);
		m_languageList->addItem(item);
	}
	return true;
}

void TessdataManager::applyChanges() {
	MAIN->pushState(MainWindow::State::Busy, _("Applying changes..."));
	setEnabled(false);
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
		QDir scriptDir = QDir(tessDataDir.absoluteFilePath("script"));
		if(!QDir().mkpath(tessDataDir.absoluteFilePath("script"))) {
			errors.append(_("Failed to create directory for tessdata files."));
		} else {
			for(int row = 0, nRows = m_languageList->count(); row < nRows; ++row) {
				QListWidgetItem* item = m_languageList->item(row);
				QString prefix = item->data(Qt::UserRole).toString();
				if(item->checkState() == Qt::Checked && !availableLanguages.contains(prefix)) {
					for(const LangFile& langFile : m_languageFiles.value(prefix)) {
						QFile file(QDir(tessDataDir).absoluteFilePath(langFile.name));
						if(!file.exists()) {
							MAIN->pushState(MainWindow::State::Busy, _("Downloading %1...").arg(langFile.name));
							QString messages;
							QByteArray data = Utils::download(QUrl(langFile.url), messages);
							if(data.isEmpty() || !file.open(QIODevice::WriteOnly)) {
								errors.append(langFile.name);
							} else {
								file.write(data);
							}
							MAIN->popState();
						}
					}
				} else if(item->checkState() != Qt::Checked && availableLanguages.contains(prefix)) {
					for(const LangFile& langFile : m_languageFiles.value(prefix)) {
						if(!QFile(QDir(tessDataDir).absoluteFilePath(langFile.name)).remove()) {
							errors.append(langFile.name);
						}
					}
				}
			}
		}
		if(!errors.isEmpty()) {
			errorMsg = _("The following files could not be downloaded or removed:\n%1\n\nCheck the connectivity and directory permissions.\nHint: If you don't have write permissions in system folders, you can switch to user paths in the settings dialog.").arg(errors.join("\n"));
		}
	}
	setEnabled(true);
	MAIN->popState();
	refresh();
	if(!errorMsg.isEmpty()) {
		QMessageBox::critical(this, _("Error"), errorMsg);
	}
}

void TessdataManager::refresh() {
	MAIN->getRecognizer()->updateLanguagesMenu();
	QStringList availableLanguages = MAIN->getRecognizer()->getAvailableLanguages();
	for(int row = 0, nRows = m_languageList->count(); row < nRows; ++row) {
		QListWidgetItem* item = m_languageList->item(row);
		QString prefix = item->data(Qt::UserRole).toString();
		item->setCheckState(availableLanguages.contains(prefix) ? Qt::Checked : Qt::Unchecked);
	}
}

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.cc
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

#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QImageReader>
#include <QInputEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMimeData>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSslConfiguration>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#include "Utils.hh"
#include "Config.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"

QString Utils::documentsFolder() {
	QString documentsFolder;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	documentsFolder = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
	documentsFolder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
	return documentsFolder.isEmpty() ? QDir::homePath() : documentsFolder;
}

QString Utils::makeOutputFilename(const QString& filename) {
	// Ensure directory exists
	QFileInfo finfo(filename);
	QDir dir = finfo.absoluteDir();
	if(!dir.exists()) {
		dir = QDir(Utils::documentsFolder());
	}
	// Generate non-existing file
	QString base = finfo.baseName().replace(QRegExp("_[0-9]+$"), "");
	QString ext = finfo.completeSuffix();
	QString newfilename = dir.absoluteFilePath(base + "." + ext);
	for(int i = 1; QFile(newfilename).exists(); ++i) {
		newfilename = dir.absoluteFilePath(QString("%1_%2.%3").arg(base).arg(i).arg(ext));
	}
	return newfilename;
}

class BusyTaskThread : public QThread {
public:
	BusyTaskThread(const std::function<bool()> &f) : m_f(f), m_result(false) {}
	bool getResult() const {
		return m_result;
	}
private:
	std::function<bool()> m_f;
	bool m_result;

	void run() {
		m_result = m_f();
	}
};

class BusyEventFilter : public QObject {
public:
	bool eventFilter(QObject */*obj*/, QEvent *ev) {
		if(dynamic_cast<QMouseEvent*>(ev)) {
			QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
			QPoint evp = MAIN->m_progressCancelButton->mapFromGlobal(mev->globalPos());
			return !QRect(QPoint(0, 0), MAIN->m_progressCancelButton->size()).contains(evp);
		}
		return dynamic_cast<QInputEvent*>(ev);
	}
};

bool Utils::busyTask(const std::function<bool()> &f, const QString& msg) {
	MAIN->pushState(MainWindow::State::Busy, msg);
	QEventLoop evLoop;
	BusyTaskThread thread(f);
	QObject::connect(&thread, SIGNAL(finished()), &evLoop, SLOT(quit()));
	thread.start();
	BusyEventFilter filter;
	QApplication::instance()->installEventFilter(&filter);
	evLoop.exec();
	QApplication::instance()->removeEventFilter(&filter);

	MAIN->popState();
	return thread.getResult();
}

void Utils::setSpinBlocked(QSpinBox *spin, int value) {
	spin->blockSignals(true);
	spin->setValue(value);
	spin->blockSignals(false);
}

void Utils::setSpinBlocked(QDoubleSpinBox *spin, double value) {
	spin->blockSignals(true);
	spin->setValue(value);
	spin->blockSignals(false);
}

bool Utils::handleSourceDragEvent(const QMimeData* mimeData) {
	if(mimeData->hasImage()) {
		QTextStream(stdout) << "hasImage" << endl;
		return true;
	}
	QList<QByteArray> formats = QImageReader::supportedImageFormats();
	formats.append("pdf");
	for(const QUrl& url : mimeData->urls()) {
		QFile file(url.toLocalFile());
		if(!file.exists()) {
			continue;
		}
		for(const QByteArray& format : formats) {
			if(file.fileName().endsWith(format, Qt::CaseInsensitive)) {
				return true;
			}
		}
	}
	return false;
}

void Utils::handleSourceDropEvent(const QMimeData* mimeData) {
	if(mimeData->hasImage()) {
		QTextStream(stdout) << "hasImage" << endl;
		MAIN->getSourceManager()->addSourceImage(qvariant_cast<QImage>(mimeData->imageData()));
		return;
	}
	QList<QByteArray> formats = QImageReader::supportedImageFormats();
	formats.append("pdf");
	for(const QUrl& url : mimeData->urls()) {
		QFile file(url.toLocalFile());
		if(!file.exists()) {
			continue;
		}
		for(const QByteArray& format : formats) {
			if(file.fileName().endsWith(format, Qt::CaseInsensitive)) {
				MAIN->getSourceManager()->addSource(file.fileName());
				break;
			}
		}
	}
}

QByteArray Utils::download(QUrl url, QString& messages, int timeout) {
	QNetworkAccessManager networkMgr;
	QNetworkReply* reply = nullptr;

	while(true) {
		QNetworkRequest req(url);
		req.setRawHeader("User-Agent", "Wget/1.13.4");
		QSslConfiguration conf = req.sslConfiguration();
		conf.setPeerVerifyMode(QSslSocket::VerifyNone);
		req.setSslConfiguration(conf);
		reply = networkMgr.get(req);
		QTimer timer;
		QEventLoop loop;
		QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
		QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
		timer.setSingleShot(true);
		timer.start(timeout);
		loop.exec();
		if(reply->isRunning()) {
			// Timeout
			reply->close();
			delete reply;
			return QByteArray();
		}

		QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
		if(redirectUrl.isValid() && url != redirectUrl) {
			delete reply;
			url = redirectUrl;
		} else {
			break;
		}
	}

	QByteArray result = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
	messages = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
	delete reply;
	return result;
}

QString Utils::getSpellingLanguage(const QString& lang) {
	// If it is already a valid code, return it
	if(lang.length() == 2) {
		return lang;
	}
	if(QRegExp("^[a-z]{2}_[A-Z]{2}").indexIn(lang) != -1) {
		return lang;
	}
	// Treat the language as a tesseract lang spec and try to find a matching code
	Config::Lang langspec = {lang};
	if(!lang.isEmpty() && MAIN->getConfig()->searchLangSpec(langspec)) {
		return langspec.code;
	}
	// Use the application locale, if specified, otherwise fall back to en
	QString syslang = QLocale::system().name();
	if(syslang.toLower() == "c" || syslang.isEmpty()) {
		return "en_US";
	}
	return syslang;
}

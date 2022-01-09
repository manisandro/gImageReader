/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.cc
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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

#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTextCodec>
#include <QTranslator>
#include <libintl.h>
#include <cstring>

#include "MainWindow.hh"
#include "Config.hh"
#include "CrashHandler.hh"

int main (int argc, char* argv[]) {
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication app(argc, argv);

	QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));

	QApplication::setOrganizationName(PACKAGE_NAME);
	QApplication::setApplicationName(PACKAGE_NAME);
	QIcon::setFallbackSearchPaths(QIcon::fallbackSearchPaths() << ":/extra-theme-icons");

#ifdef Q_OS_WIN
	QIcon::setThemeSearchPaths({dataDir.absoluteFilePath("icons")});
	QIcon::setThemeName("hicolor");
	QDir packageDir = QDir(QString("%1/../").arg(QApplication::applicationDirPath()));
	if(qgetenv("LANG").isEmpty()) {
		qputenv("LANG", QLocale::system().name().toLocal8Bit());
	}
#endif

	QTranslator qtTranslator;
	QString translationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#ifdef Q_OS_WIN
	translationsPath = packageDir.absolutePath() + translationsPath.mid(QLibraryInfo::location(QLibraryInfo::PrefixPath).length());
#endif
	qtTranslator.load("qt_" + QLocale::system().name(), translationsPath);
	qtTranslator.load("qtbase_" + QLocale::system().name(), translationsPath);
	QApplication::instance()->installTranslator(&qtTranslator);

	bindtextdomain(GETTEXT_PACKAGE, dataDir.absoluteFilePath("locale").toLocal8Bit().data());
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	QWidget* window;
	if(argc >= 3 && std::strcmp("crashhandle", argv[1]) == 0) {
		int pid = std::atoi(argv[2]);
		int tesseractCrash = std::atoi(argv[3]);
		QString savefile = argc >= 5 ? argv[4] : "";
		window = new CrashHandler(pid, tesseractCrash, savefile);
	} else if(argc >= 2 && std::strcmp("tessdatadir", argv[1]) == 0) {
		Config::openTessdataDir();
		return 0;
	} else if(argc >= 2 && std::strcmp("spellingdir", argv[1]) == 0) {
		Config::openSpellingDir();
		return 0;
	} else {
#ifdef Q_OS_WIN
		qputenv("TWAINDSM_LOG", packageDir.absoluteFilePath("twain.log").toLocal8Bit());
		std::freopen(packageDir.absoluteFilePath("gimagereader.log").toLocal8Bit().data(), "w", stderr);
#endif
		QStringList files;
		for(int i = 1; i < argc; ++i) {
			if(QFile(argv[i]).exists()) {
				files.append(argv[i]);
			}
		}
		window = new MainWindow(files);
	}
	window->show();

	int exitcode = app.exec();
	delete window;
	return exitcode;
}


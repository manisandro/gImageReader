/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.cc
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

#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTextCodec>
#include <QTranslator>
#include <QCommandLineParser>
#include <libintl.h>
#include <cstring>

#include "MainWindow.hh"
#include "Config.hh"
#include "CrashHandler.hh"

int main(int argc, char* argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
	QApplication app(argc, argv);

	QDir dataDir = QDir(QString("%1/../share/").arg(QApplication::applicationDirPath()));

	QApplication::setOrganizationName(PACKAGE_NAME);
	QApplication::setApplicationName(PACKAGE_NAME);
	QApplication::setApplicationVersion(QString("%1 (%2)").arg(PACKAGE_VERSION, QString(PACKAGE_REVISION).left(6)));

	QIcon::setFallbackSearchPaths(QIcon::fallbackSearchPaths() << ":/extra-theme-icons");

#ifdef Q_OS_WIN
	QIcon::setThemeSearchPaths({dataDir.absoluteFilePath("icons") });
	QIcon::setThemeName("hicolor");
	QDir packageDir = QDir(QString("%1/../").arg(QApplication::applicationDirPath()));
	if (qgetenv("LANG").isEmpty()) {
		qputenv("LANG", QLocale::system().name().toLocal8Bit());
	}
#endif

	QTranslator qtTranslator;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QString translationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
	QString translationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	translationsPath = packageDir.absolutePath() + translationsPath.mid(QLibraryInfo::path(QLibraryInfo::PrefixPath).length());
#else
	translationsPath = packageDir.absolutePath() + translationsPath.mid(QLibraryInfo::location(QLibraryInfo::PrefixPath).length());
#endif
#endif
	qtTranslator.load("qt_" + QLocale::system().name(), translationsPath);
	qtTranslator.load("qtbase_" + QLocale::system().name(), translationsPath);
	QApplication::instance()->installTranslator(&qtTranslator);

	bindtextdomain(GETTEXT_PACKAGE, dataDir.absoluteFilePath("locale").toLocal8Bit().data());
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	QWidget* window;
	if (argc >= 3 && std::strcmp("crashhandle", argv[1]) == 0) {
		int pid = std::atoi(argv[2]);
		int tesseractCrash = std::atoi(argv[3]);
		QString savefile = argc >= 5 ? argv[4] : "";
		window = new CrashHandler(pid, tesseractCrash, savefile);
	} else if (argc >= 2 && std::strcmp("tessdatadir", argv[1]) == 0) {
		Config::openTessdataDir();
		return 0;
	} else if (argc >= 2 && std::strcmp("spellingdir", argv[1]) == 0) {
		Config::openSpellingDir();
		return 0;
	} else {
#ifdef Q_OS_WIN
		qputenv("TWAINDSM_LOG", packageDir.absoluteFilePath("twain.log").toLocal8Bit());
		std::freopen(packageDir.absoluteFilePath("gimagereader.log").toLocal8Bit().data(), "w", stderr);
#endif

		QCommandLineParser parser;
		parser.setApplicationDescription(PACKAGE_DESCRIPTION);
		parser.addHelpOption();
		parser.addVersionOption();

		parser.addPositionalArgument(
			"files",
			QCoreApplication::translate(
				"main",
				"Files to open, optionally."
				" These can be image files or hocr files."
				" Every image file is seen as one page."
				// TODO verify: these image paths are relative to the hocr dirname
				" Hocr files can reference image files for pages or graphics."
			),
			"[files...]"
		);
		parser.process(app);

		QStringList files;
		for (const QString arg : parser.positionalArguments()) {
			if (QFile(arg).exists()) {
				qDebug("adding file: %s", arg.toUtf8().data());
				files.append(arg);
			}
			else {
				qInfo("ignoring non-file argument: %s", arg.toUtf8().data());
			}
		}

		window = new MainWindow(files);
	}
	window->show();

	int exitcode = app.exec();
	delete window;
	return exitcode;
}


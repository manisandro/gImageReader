/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.hh
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#ifndef MAINWINDOW_HH
#define MAINWINDOW_HH

#include <QFutureWatcher>
#include <QList>
#include <QMainWindow>
#include <QStack>
#include <QStringList>

#include "common.hh"
#include "Ui_MainWindow.hh"

#define MAIN MainWindow::getInstance()

class Config;
class Acquirer;
class Displayer;
class OutputManager;
class Recognizer;
class SourceManager;
class Source;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	enum class State { Idle, Normal, Busy };

	struct NotificationAction {
		QString text;
		QObject* target;
		QByteArray slot;
		bool close;
	};

	typedef void* Notification;

	static MainWindow* getInstance(){ return s_instance; }

	MainWindow();
	~MainWindow();

	void openFiles(const QStringList& files);

	Config* getConfig(){ return m_config; }
	Displayer* getDisplayer(){ return m_displayer; }
	OutputManager* getOutputManager(){ return m_outputManager; }
	Recognizer* getRecognizer(){ return m_recognizer; }
	SourceManager* getSourceManager(){ return m_sourceManager; }
	Notification addNotification(const QString& title, const QString& message, const QList<NotificationAction>& actions);
	void hideNotification(Notification notification);

public slots:
	void popState();
	void pushState(MainWindow::State state, const QString& msg);
	void showHelp(const QString& chapter = "");

private:
	static MainWindow* s_instance;

	UI_MainWindow ui;

	Config* m_config = nullptr;
	Acquirer* m_acquirer = nullptr;
	Displayer* m_displayer = nullptr;
	OutputManager* m_outputManager = nullptr;
	Recognizer* m_recognizer = nullptr;
	SourceManager* m_sourceManager = nullptr;

	QActionGroup m_idleActions;
	QList<QWidget*> m_idleWidgets;
	QStack<QPair<State, QString>> m_stateStack;
	QFutureWatcher<QString> m_versionCheckWatcher;

	void closeEvent(QCloseEvent* ev);
	void setState(State state);

#if ENABLE_VERSIONCHECK
	QString getNewestVersion();
#endif

private slots:
	void checkVersion();
	void setWindowState(Source* source);
	void showAbout();
	void showConfig();
#if ENABLE_VERSIONCHECK
	void openDownloadUrl();
	void openChangeLogUrl();
#endif
};

Q_DECLARE_METATYPE(MainWindow::State)

#endif

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.hh
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

#ifndef MAINWINDOW_HH
#define MAINWINDOW_HH

#include <QList>
#include <QMainWindow>
#include <QStack>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include "common.hh"
#include "Ui_MainWindow.hh"

#define MAIN MainWindow::getInstance()

class Config;
class Acquirer;
class Displayer;
class DisplayerTool;
class OutputEditor;
class Recognizer;
class SourceManager;
class Source;
class QProgressBar;

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

	struct ProgressMonitor {
		virtual ~ProgressMonitor() {}
		virtual int getProgress() = 0;
		virtual void cancel() = 0;
	};

	typedef void* Notification;

	static MainWindow* getInstance(){ return s_instance; }

	MainWindow(const QStringList& files);
	~MainWindow();

	Config* getConfig(){ return m_config; }
	Displayer* getDisplayer(){ return m_displayer; }
	OutputEditor* getOutputEditor(){ return m_outputEditor; }
	Recognizer* getRecognizer(){ return m_recognizer; }
	SourceManager* getSourceManager(){ return m_sourceManager; }
	void addNotification(const QString& title, const QString& message, const QList<NotificationAction>& actions, Notification* handle = nullptr);
	void openFiles(const QStringList& files);
	void setOutputPaneVisible(bool visible);
	void showProgress(ProgressMonitor* monitor, int updateInterval = 500);
	void hideProgress();

public slots:
	void popState();
	void pushState(MainWindow::State state, const QString& msg);
	void showHelp(const QString& chapter = "");
	void hideNotification(Notification handle = nullptr);

private:
	friend class BusyEventFilter;

	static MainWindow* s_instance;

	UI_MainWindow ui;

	Config* m_config = nullptr;
	Acquirer* m_acquirer = nullptr;
	Displayer* m_displayer = nullptr;
	DisplayerTool* m_displayerTool = nullptr;
	OutputEditor* m_outputEditor = nullptr;
	Recognizer* m_recognizer = nullptr;
	SourceManager* m_sourceManager = nullptr;

	QActionGroup m_idleActions;
	QList<QWidget*> m_idleWidgets;
	QStack<QPair<State, QString>> m_stateStack;

	MainWindow::Notification m_notifierHandle = nullptr;

	QWidget* m_progressWidget = nullptr;
	QProgressBar* m_progressBar = nullptr;
	QToolButton* m_progressCancelButton = nullptr;
	QTimer m_progressTimer;
	ProgressMonitor* m_progressMonitor = nullptr;


	class VersionCheckThread : public QThread {
	public:
		const QString& getNewestVersion() const{ return m_newestVersion; }
	private:
		QString m_newestVersion;
		void run();
	};
	VersionCheckThread m_versionCheckThread;

	void closeEvent(QCloseEvent* ev);
	void setState(State state);

private slots:
	void checkVersion();
	void onSourceChanged();
	void showAbout();
	void showConfig();
	void openDownloadUrl();
	void openChangeLogUrl();
	void progressCancel();
	void progressUpdate();
	void setOCRMode(int idx);
	void languageChanged();
	void dictionaryAutoinstall();
};

Q_DECLARE_METATYPE(MainWindow::State)

#endif

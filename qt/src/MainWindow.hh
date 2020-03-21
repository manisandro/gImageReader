/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.hh
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

#ifndef MAINWINDOW_HH
#define MAINWINDOW_HH

#include <QFutureWatcher>
#include <QList>
#include <QMainWindow>
#include <QMutexLocker>
#include <QStack>
#include <QStringList>
#include <QTimer>

#include "common.hh"
#include "Config.hh"
#include "Ui_MainWindow.hh"

#define MAIN MainWindow::getInstance()

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
		std::function<void()> slot;
		bool close;
	};

	class ProgressMonitor {
	public:
		ProgressMonitor(int total) : mTotal(total) {}
		virtual ~ProgressMonitor() {}
		void increaseProgress() {
			QMutexLocker locker(&mMutex);
			++mProgress;
		}
		virtual int getProgress() const {
			QMutexLocker locker(&mMutex);
			return (mProgress * 100) / mTotal;
		}
		virtual void cancel() {
			QMutexLocker locker(&mMutex);
			mCancelled = true;
		}
		bool cancelled() const {
			QMutexLocker locker(&mMutex);
			return mCancelled;
		}

	protected:
		mutable QMutex mMutex;
		const int mTotal;
		int mProgress = 0;
		bool mCancelled = false;
	};

	typedef void* Notification;

	static MainWindow* getInstance() {
		return s_instance;
	}
	static void signalHandler(int signal);
	static void tesseractCrash(int signal);

	MainWindow(const QStringList& files);
	~MainWindow();

	Config* getConfig() {
		return m_config;
	}
	Displayer* getDisplayer() {
		return m_displayer;
	}
	OutputEditor* getOutputEditor() {
		return m_outputEditor;
	}
	Recognizer* getRecognizer() {
		return m_recognizer;
	}
	SourceManager* getSourceManager() {
		return m_sourceManager;
	}
	void addNotification(const QString& title, const QString& message, const QList<NotificationAction>& actions, Notification* handle = nullptr);
	void openFiles(const QStringList& files);
	void setOutputPaneVisible(bool visible);
	void showProgress(ProgressMonitor* monitor, int updateInterval = 500);
	void hideProgress();

public slots:
	void manageLanguages();
	void popState();
	void pushState(MainWindow::State state, const QString& msg);
	void showHelp(const QString& chapter = "");
	void hideNotification(Notification handle = nullptr);

private:
	static void signalHandlerExec(int signal, bool tesseractCrash);
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

	QFutureWatcher<QString> m_versionWatcher;

	void closeEvent(QCloseEvent* ev);
	void setState(State state);

private slots:
	void checkVersion(const QString& newver);
	void onSourceChanged();
	void showAbout();
	void showConfig();
	void openDownloadUrl();
	void openChangeLogUrl();
	void progressCancel();
	void progressUpdate();
	void setOCRMode(int idx);
	void languageChanged(const Config::Lang& lang);
	void dictionaryAutoinstall();
};

Q_DECLARE_METATYPE(MainWindow::State)

#endif

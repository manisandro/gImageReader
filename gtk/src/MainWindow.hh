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

#include "common.hh"
#include "Config.hh"
#include "ui_MainWindow.hh"

#define MAIN MainWindow::getInstance()

namespace std { class thread; }
class Acquirer;
class Displayer;
class DisplayerTool;
class OutputEditor;
class Recognizer;
class RecognitionMenu;
class Source;
class SourceManager;

class MainWindow {
public:
	enum class State { Idle, Normal, Busy };
	enum OutputMode {OutputModeText = 0, OutputModeHOCR = 1};

	struct NotificationAction {
		Glib::ustring label;
		std::function<bool()> action;
	};

	class ProgressMonitor {
	public:
		ProgressMonitor(int total) : m_total(total) {}
		virtual ~ProgressMonitor() {}
		void increaseProgress() {
			std::lock_guard<std::mutex> lock(m_mutex);
			++m_progress;
		}
		virtual int getProgress() const {
			std::lock_guard<std::mutex> lock(m_mutex);
			return (m_progress * 100) / m_total;
		}
		virtual void cancel() {
			std::lock_guard<std::mutex> lock(m_mutex);
			m_cancelled = true;
		}
		bool cancelled() const {
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_cancelled;
		}

	protected:
		mutable std::mutex m_mutex;
		const int m_total;
		int m_progress = 0;
		bool m_cancelled = false;
	};

	typedef void* Notification;

	static MainWindow* getInstance() {
		return s_instance;
	}
	static void signalHandler(int signal);
	static void tesseractCrash(int signal);

	MainWindow();
	~MainWindow();

	Config* getConfig() const {
		return m_config;
	}
	Displayer* getDisplayer() const {
		return m_displayer;
	}
	OutputEditor* getOutputEditor() const {
		return m_outputEditor;
	}
	Recognizer* getRecognizer() const {
		return m_recognizer;
	}
	SourceManager* getSourceManager() const {
		return m_sourceManager;
	}
	RecognitionMenu* getRecognitionMenu() const {
		return m_recognitionMenu;
	}
	Gtk::Window* getWindow() const {
		return ui.windowMain;
	}
	Gtk::HeaderBar* getHeaderBar() const {
		return ui.headerbar;
	}
	void setMenuModel(const Glib::RefPtr<Gio::MenuModel>& menuModel);
	void redetectLanguages();
	void manageLanguages();
	void showConfig();
	void showHelp(const std::string& chapter = "");
	void showAbout();
	void addNotification(const Glib::ustring& title, const Glib::ustring& message, const std::vector<NotificationAction>& actions, Notification* handle = nullptr);
	void hideNotification(Notification handle);
	void openFiles(const std::vector<Glib::RefPtr<Gio::File>>& files);
	void openOutput(const std::string& filename);
	bool setOutputMode(OutputMode mode);
	void setOutputPaneVisible(bool visible);
	void pushState(State state, const Glib::ustring& msg);
	void popState();
	void showProgress(ProgressMonitor* monitor, int updateInterval = 500);
	void hideProgress();

private:
	static void signalHandlerExec(int signal, bool tesseractCrash);

	static MainWindow* s_instance;

	Ui::MainWindow ui;
	ClassData m_classdata;

	Config* m_config = nullptr;
	Acquirer* m_acquirer = nullptr;
	Displayer* m_displayer = nullptr;
	DisplayerTool* m_displayerTool = nullptr;
	OutputEditor* m_outputEditor = nullptr;
	RecognitionMenu* m_recognitionMenu = nullptr;
	Recognizer* m_recognizer = nullptr;
	SourceManager* m_sourceManager = nullptr;

	std::thread* m_newVerThread = nullptr;

	MainWindow::Notification m_notifierHandle = nullptr;

	ProgressMonitor* m_progressMonitor = nullptr;

	std::vector<Gtk::Widget*> m_idlegroup;
	std::vector<State> m_stateStack;
	sigc::connection m_connection_setOCRMode;
	sigc::connection m_connection_progressUpdate;

	bool closeEvent(GdkEventAny*);
	void languageChanged(const Config::Lang& lang);
	void onSourceChanged();
	void setState(State state);
	void progressCancel();
	void progressUpdate();
#if ENABLE_VERSIONCHECK
	void getNewestVersion();
	void checkVersion(const Glib::ustring& newver);
#endif
#ifdef G_OS_UNIX
	void dictionaryAutoinstall(Glib::RefPtr<Gio::DBus::Proxy> proxy, const Glib::ustring& lang);
	void dictionaryAutoinstallDone(Glib::RefPtr<Gio::DBus::Proxy> proxy, Glib::RefPtr<Gio::AsyncResult>& result);
#endif
	void dictionaryAutoinstall(Glib::ustring lang);
};

#endif
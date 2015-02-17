/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.cc
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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

#include "Application.hh"
#include "MainWindow.hh"
#include "Acquirer.hh"
#include "Config.hh"
#include "Displayer.hh"
#include "OutputManager.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <unistd.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif
#ifdef G_OS_UNIX
#include <sys/prctl.h>
#include <sys/wait.h>
#endif


#if ENABLE_VERSIONCHECK
#define CHECKURL "http://sourceforge.net/projects/gimagereader/files/LATEST/download?use_mirror=autoselect"
#define DOWNLOADURL "http://sourceforge.net/projects/gimagereader/files"
#define CHANGELOGURL "http://sourceforge.net/projects/gimagereader/files/changelog.txt/download?use_mirror=autoselect"
#endif // ENABLE_VERSIONCHECK

static Glib::Quark notificationHandleKey("handle");

static void signalHandler(int sig)
{
	std::signal(sig, nullptr);
	std::string filename;
	if(MAIN->getOutputManager() && MAIN->getOutputManager()->getBufferModified()){
		filename = Glib::build_filename(g_get_home_dir(), Glib::ustring::compose("%1_crash-save.txt", PACKAGE_NAME));
		int i = 0;
		while(Glib::file_test(filename, Glib::FILE_TEST_EXISTS)){
			++i;
			filename = Glib::build_filename(g_get_home_dir(), Glib::ustring::compose("%1_crash-save_%2.txt", PACKAGE_NAME, i));
		}
		MAIN->getOutputManager()->saveBuffer(filename);
	}
	Glib::Pid pid;
	Glib::spawn_async("", std::vector<std::string>{pkgExePath, "crashhandle", Glib::ustring::compose("%1", getpid()), filename}, Glib::SPAWN_DO_NOT_REAP_CHILD, sigc::slot<void>(), &pid);
#ifdef G_OS_WIN32
	WaitForSingleObject((HANDLE)pid, 0);
#else
	// Allow crash handler spawned debugger to attach to the crashed process
	prctl(PR_SET_PTRACER, pid, 0, 0, 0);
	waitpid(pid, 0, 0);
#endif
	std::raise(sig);
}


static void terminateHandler()
{
	std::set_terminate(nullptr);
	std::exception_ptr exptr = std::current_exception();
	if (exptr != 0){
		try{
			std::rethrow_exception(exptr);
		}catch (std::exception &ex){
			std::cerr << "Terminated due to exception: " << ex.what() << std::endl;
		}catch (...){
			std::cerr << "Terminated due to unknown exception" << std::endl;
		}
	}else{
		std::cerr << "Terminated due to unknown reason:" << std::endl;
	}
	signalHandler(SIGABRT);
}

MainWindow* MainWindow::s_instance = nullptr;

MainWindow::MainWindow()
{
	s_instance = this;

	std::signal(SIGSEGV, signalHandler);
	std::signal(SIGABRT, signalHandler);
	std::set_terminate(terminateHandler);

	m_window = Builder("applicationwindow:main");
	m_aboutdialog = Builder("dialog:about");
	m_statusbar = Builder("statusbar:main");
	m_window->set_icon_name("gimagereader");
	m_aboutdialog->set_version(PACKAGE_VERSION);

	m_config = new Config;
	m_acquirer = new Acquirer;
	m_displayer = new Displayer;
	m_outputManager = new OutputManager;
	m_recognizer = new Recognizer;
	m_sourceManager = new SourceManager;

	m_idlegroup.push_back(Builder("tbbutton:main.zoomin"));
	m_idlegroup.push_back(Builder("tbbutton:main.zoomout"));
	m_idlegroup.push_back(Builder("tbbutton:main.normsize"));
	m_idlegroup.push_back(Builder("tbbutton:main.bestfit"));
	m_idlegroup.push_back(Builder("tbbutton:main.rotleft"));
	m_idlegroup.push_back(Builder("tbbutton:main.rotright"));
	m_idlegroup.push_back(Builder("spin:display.rotate"));
	m_idlegroup.push_back(Builder("spin:display.page"));
	m_idlegroup.push_back(Builder("spin:display.brightness"));
	m_idlegroup.push_back(Builder("spin:display.contrast"));
	m_idlegroup.push_back(Builder("spin:display.resolution"));
	m_idlegroup.push_back(Builder("tbbutton:main.autolayout"));
	m_idlegroup.push_back(Builder("tbmenu:main.recognize"));

	CONNECT(m_window, delete_event, [this](GdkEventAny* ev) { return closeEvent(ev); });
	CONNECTS(Builder("tbbutton:main.controls").as<Gtk::ToggleToolButton>(), toggled,
			 [this](Gtk::ToggleToolButton* b) { Builder("toolbar:display").as<Gtk::Toolbar>()->set_visible(b->get_active()); });
	CONNECT(m_displayer, selectionChanged, [this](bool haveSelection){ m_recognizer->setRecognizeMode(haveSelection); });
	CONNECT(m_recognizer, languageChanged, [this](const Config::Lang& lang){ m_outputManager->setLanguage(lang); });
	CONNECT(m_acquirer, scanPageAvailable, [this](const std::string& filename){ m_sourceManager->addSources({Gio::File::create_for_path(filename)}); });
	CONNECT(m_sourceManager, sourceChanged, [this](Source* source){ onSourceChanged(source); });

	m_config->addSetting(new VarSetting<std::vector<int>>("wingeom"));
	m_config->addSetting(new SwitchSettingT<Gtk::ToggleToolButton>("showcontrols", "tbbutton:main.controls"));

	m_recognizer->updateLanguagesMenu();

	pushState(State::Idle, _("Select an image to begin..."));

	const std::vector<int>& geom = m_config->getSetting<VarSetting<std::vector<int>>>("wingeom")->getValue();
	if(geom.size() == 4){
		m_window->resize(geom[2], geom[3]);
		m_window->move(geom[0], geom[1]);
	}
#if ENABLE_VERSIONCHECK
	if(m_config->getSetting<SwitchSetting>("updatecheck")->getValue()){
		m_newVerThread = Glib::Threads::Thread::create([this]{ getNewestVersion(); });
	}
#else
	Builder("check:config.settings.update").as<Gtk::Widget>()->hide();
#endif
}

MainWindow::~MainWindow()
{
	delete m_acquirer;
	delete m_sourceManager;
	delete m_displayer;
	delete m_recognizer;
	delete m_outputManager;
	delete m_config;
	s_instance = nullptr;
}

void MainWindow::setMenuModel(const Glib::RefPtr<Gio::MenuModel> &menuModel)
{
	Builder("menubutton:options").as<Gtk::MenuButton>()->set_menu_model(menuModel);
	Builder("tbbutton:options").as<Gtk::ToolItem>()->set_visible(true);
}

void MainWindow::openFiles(const std::vector<Glib::RefPtr<Gio::File>>& files)
{
	m_sourceManager->addSources(files);
}

void MainWindow::pushState(State state, const Glib::ustring &msg)
{
	m_stateStack.push_back(state);
	m_statusbar->push(msg);
	setState(state);
}

void MainWindow::popState()
{
	m_statusbar->pop();
	m_stateStack.pop_back();
	setState(m_stateStack.back());
}

void MainWindow::setState(State state)
{
	bool isIdle = state == State::Idle;
	bool isBusy = state == State::Busy;
	for(Gtk::Widget* w : m_idlegroup){ w->set_sensitive(!isIdle); }
	m_window->set_sensitive(!isBusy);
	if(m_window->get_window()){
		m_window->get_window()->set_cursor(isBusy ? Gdk::Cursor::create(Gdk::WATCH) : Glib::RefPtr<Gdk::Cursor>());
	}
}

bool MainWindow::closeEvent(GdkEventAny*)
{
	if(!m_outputManager->clearBuffer()){
		return true;
	}
	if((m_window->get_window()->get_state() & Gdk::WINDOW_STATE_MAXIMIZED) == 0){
		std::vector<int> geom(4);
		m_window->get_position(geom[0], geom[1]);
		m_window->get_size(geom[2], geom[3]);
		m_config->getSetting<VarSetting<std::vector<int>>>("wingeom")->setValue(geom);
	}
	m_window->hide();
	return false;
}

void MainWindow::onSourceChanged(Source* source)
{
	if(m_stateStack.back() == State::Normal){
		popState();
	}
	if(m_displayer->setSource(source)){
		m_window->set_title(Glib::ustring::compose("%1 - %2", source->displayname, PACKAGE_NAME));
		pushState(State::Normal, _("To recognize specific areas, drag rectangles over them."));
	}else{
		m_window->set_title(PACKAGE_NAME);
	}
}

void MainWindow::showAbout()
{
	m_aboutdialog->run();
	m_aboutdialog->hide();
}

void MainWindow::showHelp(const std::string& chapter)
{
#ifdef Q_OS_WIN32
	std::string manualDir = Glib::build_filename(pkgDir, "share", "doc", "gimagereader");
	char* locale = g_win32_getlocale();
	std::string language(locale, 2);
	g_free(locale);
#else
	std::string manualDir = MANUAL_DIR;
	std::string language(setlocale(LC_ALL, NULL), 2);
#endif
	std::string manualFile = Utils::make_absolute_path(Glib::build_filename(manualDir, Glib::ustring::compose("manual_%1.html", language)));
	if(!Glib::file_test(manualFile, Glib::FILE_TEST_EXISTS)){
		manualFile = Utils::make_absolute_path(Glib::build_filename(manualDir,"manual.html"));
	}
	std::string manualURI = Glib::filename_to_uri(manualFile) + chapter;
#ifdef G_OS_WIN32
	ShellExecute(nullptr, "open", manualURI.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
	gtk_show_uri(nullptr, manualURI.c_str(), GDK_CURRENT_TIME, 0);
#endif
}

void MainWindow::showConfig()
{
	m_config->showDialog();
	m_recognizer->updateLanguagesMenu();
}

void MainWindow::redetectLanguages()
{
	m_recognizer->updateLanguagesMenu();
}

void MainWindow::addNotification(const Glib::ustring &title, const Glib::ustring &message, const std::vector<NotificationAction> &actions, Notification* handle)
{
	Gtk::Frame* frame = Gtk::manage(new Gtk::Frame);
	frame->set_data(notificationHandleKey, handle);
	frame->set_shadow_type(Gtk::SHADOW_OUT);
	Gtk::Box* box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
	box->override_background_color(Gdk::RGBA("#FFD000"), Gtk::STATE_FLAG_NORMAL);
	frame->add(*box);
	Gtk::Label* titlelabel = Gtk::manage(new Gtk::Label);
	titlelabel->set_markup(Glib::ustring::compose("<b>%1:</b>", title));
	box->pack_start(*titlelabel, false, true);
	box->pack_start(*Gtk::manage(new Gtk::Label(message, Gtk::ALIGN_START)), true, true);
	Gtk::Button* closebtn = Gtk::manage(new Gtk::Button());
	closebtn->set_image_from_icon_name("window-close", Gtk::ICON_SIZE_MENU);
	CONNECT(closebtn, clicked, [this,frame]{ hideNotification(frame); });
	box->pack_end(*closebtn, false, true);
	for(const NotificationAction& action : actions){
		Gtk::Button* btn = Gtk::manage(new Gtk::Button(action.label));
		btn->set_relief(Gtk::RELIEF_NONE);
		CONNECT(btn, clicked, [this,frame,action]{ if(action.action()){ hideNotification(frame); }});
		box->pack_start(*btn, false, true);
		btn->get_child()->override_color(Gdk::RGBA("#0000FF"), Gtk::STATE_FLAG_NORMAL);
	}
	frame->show_all();
	Builder("box:main").as<Gtk::Box>()->pack_end(*frame, false, true);
	if(handle != nullptr){
		*handle = frame;
	}
}

void MainWindow::hideNotification(Notification handle)
{
	if(handle){
		Gtk::Frame* frame = static_cast<Gtk::Frame*>(handle);
		Notification* h = reinterpret_cast<Notification*>(frame->get_data(notificationHandleKey));
		if(h){
			*h = nullptr;
		}
		Builder("box:main").as<Gtk::Box>()->remove(*frame);
		delete frame;
	}
}

#if ENABLE_VERSIONCHECK
void MainWindow::getNewestVersion()
{
	char buf[16] = {};
	try {
		Glib::RefPtr<Gio::File> file = Gio::File::create_for_uri(CHECKURL);
		Glib::RefPtr<Gio::FileInputStream> stream = file->read();
		stream->read(buf, 16);
	} catch (const Glib::Error&) {
		return;
	}
	std::string newver(buf);
	newver.erase(std::remove_if(newver.begin(), newver.end(), ::isspace), newver.end());
	if(Glib::Regex::create(R"(^[\d+\.]+\d+$)")->match(newver, 0, Glib::RegexMatchFlags(0))){
		Glib::signal_idle().connect_once([this,newver]{ checkVersion(newver); });
	}
}

void MainWindow::checkVersion(const Glib::ustring& newver)
{
	m_newVerThread->join();
	Glib::ustring curver = PACKAGE_VERSION;

	if(newver.compare(curver) > 0){
		addNotification(_("New version"), Glib::ustring::compose(_("gImageReader %1 is available"), newver),
			{{_("Download"), []{ gtk_show_uri(Gdk::Screen::get_default()->gobj(), DOWNLOADURL, GDK_CURRENT_TIME, 0); return false; }},
			 {_("Changelog"), []{ gtk_show_uri(Gdk::Screen::get_default()->gobj(), CHANGELOGURL, GDK_CURRENT_TIME, 0); return false; }},
			 {_("Don't notify again"), [this]{ m_config->getSetting<SwitchSetting>("updatecheck")->setValue(false); return true; }}});
	}
}
#endif // ENABLE_VERSIONCHECK

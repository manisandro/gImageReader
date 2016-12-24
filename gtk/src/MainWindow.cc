/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.cc
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

#include "Application.hh"
#include "MainWindow.hh"
#include "Acquirer.hh"
#include "Config.hh"
#include "Displayer.hh"
#include "DisplayerToolHOCR.hh"
#include "DisplayerToolSelect.hh"
#include "OutputEditorHOCR.hh"
#include "OutputEditorText.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <gtkspellmm.h>
#include <algorithm>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif
#ifdef G_OS_UNIX
#include <gdk/gdkx.h>
#endif
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/wait.h>
#endif
#include <tesseract/baseapi.h>


#if ENABLE_VERSIONCHECK
#define CHECKURL "https://raw.githubusercontent.com/manisandro/gImageReader/master/LATEST"
#define DOWNLOADURL "https://github.com/manisandro/gImageReader/releases"
#define CHANGELOGURL "https://raw.githubusercontent.com/manisandro/gImageReader/master/NEWS"
#endif // ENABLE_VERSIONCHECK

static Glib::Quark notificationHandleKey("handle");

void MainWindow::signalHandler(int sig) {
	std::signal(sig, nullptr);
	std::string filename;
	if(MAIN->getOutputEditor() && MAIN->getOutputEditor()->getModified()) {
		filename = Glib::build_filename(g_get_home_dir(), Glib::ustring::compose("%1_crash-save.txt", PACKAGE_NAME));
		int i = 0;
		while(Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
			++i;
			filename = Glib::build_filename(g_get_home_dir(), Glib::ustring::compose("%1_crash-save_%2.txt", PACKAGE_NAME, i));
		}
		MAIN->getOutputEditor()->save(filename);
	}
	Glib::Pid pid;
	Glib::spawn_async("", std::vector<std::string> {pkgExePath, "crashhandle", Glib::ustring::compose("%1", getpid()), filename}, Glib::SPAWN_DO_NOT_REAP_CHILD, sigc::slot<void>(), &pid);
#ifdef G_OS_WIN32
	WaitForSingleObject((HANDLE)pid, 0);
#elif __linux__
	// Allow crash handler spawned debugger to attach to the crashed process
	prctl(PR_SET_PTRACER, pid, 0, 0, 0);
	waitpid(pid, 0, 0);
#endif
	std::raise(sig);
}

#ifndef __ARMEL__
static void terminateHandler() {
	std::set_terminate(nullptr);
	std::exception_ptr exptr = std::current_exception();
	if (exptr != 0) {
		try {
			std::rethrow_exception(exptr);
		} catch (std::exception &ex) {
			std::cerr << "Terminated due to exception: " << ex.what() << std::endl;
		} catch (...) {
			std::cerr << "Terminated due to unknown exception" << std::endl;
		}
	} else {
		std::cerr << "Terminated due to unknown reason:" << std::endl;
	}
	MainWindow::signalHandler(SIGABRT);
}
#endif

MainWindow* MainWindow::s_instance = nullptr;

MainWindow::MainWindow()
	: m_builder("/org/gnome/gimagereader/gimagereader.ui") {
	s_instance = this;

	std::signal(SIGSEGV, signalHandler);
	std::signal(SIGABRT, signalHandler);
#ifndef __ARMEL__
	std::set_terminate(terminateHandler);
#endif

	m_window = getWidget("applicationwindow:main");
	m_headerbar = getWidget("headerbar:main");
	m_aboutdialog = getWidget("dialog:about");
	m_statusbar = getWidget("statusbar:main");
	m_ocrModeCombo = getWidget("combo:main.ocrmode");
	m_outputPaneToggleButton = getWidget("button:main.outputpane");
	m_window->set_icon_name("gimagereader");
	m_aboutdialog->set_version(PACKAGE_VERSION);
	getWidget("label:about.tesseractver").as<Gtk::Label>()->set_markup(Glib::ustring::compose("<small>%1 %2</small>", _("Using tesseract"), TESSERACT_VERSION_STR));

	m_config = new Config;
	m_acquirer = new Acquirer;
	m_displayer = new Displayer;
	m_recognizer = new Recognizer;
	m_sourceManager = new SourceManager;

	m_idlegroup.push_back(getWidget("button:main.zoomin"));
	m_idlegroup.push_back(getWidget("button:main.zoomout"));
	m_idlegroup.push_back(getWidget("button:main.zoomnormsize"));
	m_idlegroup.push_back(getWidget("button:main.zoomfit"));
	m_idlegroup.push_back(getWidget("button:main.recognize"));
	m_idlegroup.push_back(getWidget("menubutton:display.rotate.mode"));
	m_idlegroup.push_back(getWidget("spin:display.rotate"));
	m_idlegroup.push_back(getWidget("spin:display.page"));
	m_idlegroup.push_back(getWidget("spin:display.brightness"));
	m_idlegroup.push_back(getWidget("spin:display.contrast"));
	m_idlegroup.push_back(getWidget("spin:display.resolution"));
	m_idlegroup.push_back(getWidget("button:main.autolayout"));
	m_idlegroup.push_back(getWidget("menubutton:main.languages"));

	CONNECT(m_window, delete_event, [this](GdkEventAny* ev) {
		return closeEvent(ev);
	});
	CONNECTS(getWidget("button:main.controls").as<Gtk::ToggleButton>(), toggled,
	[this](Gtk::ToggleButton* b) {
		getWidget("toolbar:display").as<Gtk::Toolbar>()->set_visible(b->get_active());
	});
	CONNECT(m_acquirer, scanPageAvailable, [this](const std::string& filename) {
		m_sourceManager->addSources({Gio::File::create_for_path(filename)});
	});
	CONNECT(m_sourceManager, sourceChanged, [this] { onSourceChanged(); });
	CONNECT(m_outputPaneToggleButton, toggled, [this] { m_outputEditor->getUI()->set_visible(m_outputPaneToggleButton->get_active()); });
	m_connection_setOCRMode = CONNECT(m_ocrModeCombo, changed, [this] { setOCRMode(m_ocrModeCombo->get_active_row_number()); });
	CONNECT(m_recognizer, languageChanged, [this] (const Config::Lang& /*lang*/ ) {
		languageChanged();
	});
	CONNECTS(getWidget("combo:config.settings.paneorient").as<Gtk::ComboBoxText>(), changed, [this](Gtk::ComboBoxText* combo) {
		getWidget("paned:output").as<Gtk::Paned>()->set_orientation(static_cast<Gtk::Orientation>(!combo->get_active_row_number()));
	});
	CONNECT(getWidget("button:main.progress.cancel").as<Gtk::Button>(), clicked, [this] { progressCancel(); });


	m_config->addSetting(new VarSetting<std::vector<int>>("wingeom"));
	m_config->addSetting(new SwitchSettingT<Gtk::ToggleButton>("showcontrols", m_builder("button:main.controls")));
	m_config->addSetting(new VarSetting<Glib::ustring>("outputdir"));
	m_config->addSetting(new ComboSetting("outputeditor", m_builder("combo:main.ocrmode")));

	m_recognizer->updateLanguagesMenu();

	pushState(State::Idle, _("Select an image to begin..."));

	const std::vector<int>& geom = m_config->getSetting<VarSetting<std::vector<int>>>("wingeom")->getValue();
	if(geom.size() == 4) {
		m_window->resize(geom[2], geom[3]);
		m_window->move(geom[0], geom[1]);
	}
#if ENABLE_VERSIONCHECK
	if(m_config->getSetting<SwitchSetting>("updatecheck")->getValue()) {
		m_newVerThread = Glib::Threads::Thread::create([this] { getNewestVersion(); });
	}
#else
	getWidget("check:config.settings.update")->hide();
#endif
}

MainWindow::~MainWindow() {
	delete m_acquirer;
	delete m_outputEditor;
	delete m_sourceManager;
	m_displayer->setTool(nullptr);
	delete m_displayerTool;
	delete m_displayer;
	delete m_recognizer;
	delete m_config;
	s_instance = nullptr;
}

void MainWindow::setMenuModel(const Glib::RefPtr<Gio::MenuModel> &menuModel) {
	getWidget("menubutton:main.options").as<Gtk::MenuButton>()->set_menu_model(menuModel);
	getWidget("menubutton:main.options").as<Gtk::MenuButton>()->set_visible(true);
}

void MainWindow::openFiles(const std::vector<Glib::RefPtr<Gio::File>>& files) {
	m_sourceManager->addSources(files);
}

void MainWindow::setOutputPaneVisible(bool visible) {
	m_outputPaneToggleButton->set_active(visible);
}

void MainWindow::pushState(State state, const Glib::ustring &msg) {
	m_stateStack.push_back(state);
	m_statusbar->push(msg);
	setState(state);
}

void MainWindow::popState() {
	m_statusbar->pop();
	m_stateStack.pop_back();
	setState(m_stateStack.back());
}

void MainWindow::setState(State state) {
	bool isIdle = state == State::Idle;
	bool isBusy = state == State::Busy;
	for(Gtk::Widget* w : m_idlegroup) {
		w->set_sensitive(!isIdle);
	}
	getWidget("paned:output").as<Gtk::Box>()->set_sensitive(!isBusy);
	m_headerbar->set_sensitive(!isBusy);
	if(m_window->get_window()) {
		m_window->get_window()->set_cursor(isBusy ? Gdk::Cursor::create(Gdk::WATCH) : Glib::RefPtr<Gdk::Cursor>());
	}
}

void MainWindow::showProgress(ProgressMonitor* monitor, int updateInterval) {
	m_progressMonitor = monitor;
	m_connection_progressUpdate = Glib::signal_timeout().connect([this] { progressUpdate(); return true; }, updateInterval);
	getWidget("progressbar:main.progress").as<Gtk::ProgressBar>()->set_fraction(0);
	getWidget("button:main.progress.cancel").as<Gtk::Button>()->set_sensitive(true);
	getWidget("box:main.progress").as<Gtk::Box>()->set_visible(true);
}

void MainWindow::hideProgress() {
	getWidget("box:main.progress").as<Gtk::Box>()->set_visible(false);
	m_connection_progressUpdate.disconnect();
	m_progressMonitor = nullptr;
}

void MainWindow::progressCancel() {
	if(m_progressMonitor) {
		getWidget("button:main.progress.cancel").as<Gtk::Button>()->set_sensitive(true);
		m_progressMonitor->cancel();
	}
}

void MainWindow::progressUpdate() {
	if(m_progressMonitor) {
		getWidget("progressbar:main.progress").as<Gtk::ProgressBar>()->set_fraction(m_progressMonitor->getProgress() / 100.);
	}
}


bool MainWindow::closeEvent(GdkEventAny*) {
	if(!m_outputEditor->clear()) {
		return true;
	}
	if((m_window->get_window()->get_state() & Gdk::WINDOW_STATE_MAXIMIZED) == 0) {
		std::vector<int> geom(4);
		m_window->get_position(geom[0], geom[1]);
		m_window->get_size(geom[2], geom[3]);
		m_config->getSetting<VarSetting<std::vector<int>>>("wingeom")->setValue(geom);
	}
	m_window->hide();
	return false;
}

void MainWindow::onSourceChanged() {
	if(m_stateStack.back() == State::Normal) {
		popState();
	}
	std::vector<Source*> sources = m_sourceManager->getSelectedSources();
	if(m_displayer->setSources(sources)) {
		m_headerbar->set_subtitle(sources.size() == 1 ? sources.front()->displayname : _("Multiple sources"));
		pushState(State::Normal, _("Ready"));
	} else {
		m_headerbar->set_subtitle("");
	}
}

void MainWindow::showAbout() {
	m_aboutdialog->run();
	m_aboutdialog->hide();
}

void MainWindow::showHelp(const std::string& chapter) {
#ifdef G_OS_WIN32
	std::string manualDir = Glib::build_filename(pkgDir, "share", "doc", "gimagereader");
	char* locale = g_win32_getlocale();
	std::string language(locale, 2);
	g_free(locale);
#else
	std::string manualDir = MANUAL_DIR;
	std::string language = Glib::getenv("LANG").substr(0, 2);
#endif
	std::string manualFile = Utils::make_absolute_path(Glib::build_filename(manualDir, Glib::ustring::compose("manual-%1.html", language)));
	if(!Glib::file_test(manualFile, Glib::FILE_TEST_EXISTS)) {
		manualFile = Utils::make_absolute_path(Glib::build_filename(manualDir,"manual.html"));
	}
	Utils::openUri(Glib::filename_to_uri(manualFile) + chapter);
}

void MainWindow::showConfig() {
	m_config->showDialog();
	m_recognizer->updateLanguagesMenu();
}

void MainWindow::setOCRMode(int idx) {
	if(m_outputEditor && !m_outputEditor->clear()) {
		m_connection_setOCRMode.block(true);
		if(dynamic_cast<OutputEditorText*>(m_outputEditor)) {
			m_ocrModeCombo->set_active(0);
		} else if(dynamic_cast<OutputEditorHOCR*>(m_outputEditor)) {
			m_ocrModeCombo->set_active(1);
		}
		m_connection_setOCRMode.block(false);
	} else {
		if(m_outputEditor) {
			m_connection_setOutputEditorLanguage.disconnect();
			m_connection_setOutputEditorVisibility.disconnect();
			getWidget("paned:output").as<Gtk::Paned>()->remove(*m_outputEditor->getUI());
			delete m_outputEditor;
		}
		delete m_displayerTool;
		if(idx == 0) {
			m_displayerTool = new DisplayerToolSelect(m_displayer);
			m_outputEditor = new OutputEditorText();
		} else { /*if(idx == 1)*/
			m_displayerTool = new DisplayerToolHOCR(m_displayer);
			m_outputEditor = new OutputEditorHOCR(static_cast<DisplayerToolHOCR*>(m_displayerTool));
		}
		m_displayer->setTool(m_displayerTool);
		m_connection_setOutputEditorLanguage = CONNECT(m_recognizer, languageChanged, [this](const Config::Lang& lang) {
			m_outputEditor->setLanguage(lang);
		});
		m_outputEditor->setLanguage(m_recognizer->getSelectedLanguage());
		m_connection_setOutputEditorVisibility = CONNECT(m_outputPaneToggleButton, toggled, [this] { m_outputEditor->onVisibilityChanged(m_outputPaneToggleButton->get_active()); });
		getWidget("paned:output").as<Gtk::Paned>()->pack2(*m_outputEditor->getUI(), true, false);
		m_outputEditor->getUI()->set_visible(m_outputPaneToggleButton->get_active());
	}
}

void MainWindow::redetectLanguages() {
	m_recognizer->updateLanguagesMenu();
}

void MainWindow::addNotification(const Glib::ustring &title, const Glib::ustring &message, const std::vector<NotificationAction> &actions, Notification* handle) {
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
	CONNECT(closebtn, clicked, [this,frame] { hideNotification(frame); });
	box->pack_end(*closebtn, false, true);
	for(const NotificationAction& action : actions) {
		Gtk::Button* btn = Gtk::manage(new Gtk::Button(action.label));
		btn->set_relief(Gtk::RELIEF_NONE);
		CONNECT(btn, clicked, [this,frame,action] { if(action.action()) {
		hideNotification(frame);
		}
		                                          });
		box->pack_start(*btn, false, true);
		btn->get_child()->override_color(Gdk::RGBA("#0000FF"), Gtk::STATE_FLAG_NORMAL);
	}
	frame->show_all();
	getWidget("box:main").as<Gtk::Box>()->pack_end(*frame, false, true);
	if(handle != nullptr) {
		*handle = frame;
	}
}

void MainWindow::hideNotification(Notification handle) {
	if(handle) {
		Gtk::Frame* frame = static_cast<Gtk::Frame*>(handle);
		Notification* h = reinterpret_cast<Notification*>(frame->get_data(notificationHandleKey));
		if(h) {
			*h = nullptr;
		}
		getWidget("box:main").as<Gtk::Box>()->remove(*frame);
		delete frame;
	}
}

#if ENABLE_VERSIONCHECK
void MainWindow::getNewestVersion() {
	char buf[16] = {};
	try {
		Glib::RefPtr<Gio::File> file = Gio::File::create_for_uri(CHECKURL);
		Glib::RefPtr<Gio::FileInputStream> stream = file->read();
		stream->read(buf, 16);
	} catch (const Glib::Error&) {
		return;
	}
	std::string newver(buf);
	g_debug("Newest version is: %s", newver.c_str());
	newver.erase(std::remove_if(newver.begin(), newver.end(), ::isspace), newver.end());
	if(Glib::Regex::create(R"(^[\d+\.]+\d+$)")->match(newver, 0, Glib::RegexMatchFlags(0))) {
		Glib::signal_idle().connect_once([this,newver] { checkVersion(newver); });
	}
}

void MainWindow::checkVersion(const Glib::ustring& newver) {
	m_newVerThread->join();
	Glib::ustring curver = PACKAGE_VERSION;

	if(newver.compare(curver) > 0) {
		addNotification(_("New version"), Glib::ustring::compose(_("gImageReader %1 is available"), newver), {
			{_("Download"), []{ gtk_show_uri(Gdk::Screen::get_default()->gobj(), DOWNLOADURL, GDK_CURRENT_TIME, 0); return false; }},
			{_("Changelog"), []{ gtk_show_uri(Gdk::Screen::get_default()->gobj(), CHANGELOGURL, GDK_CURRENT_TIME, 0); return false; }},
			{_("Don't notify again"), [this]{ m_config->getSetting<SwitchSetting>("updatecheck")->setValue(false); return true; }}
		});
	}
}
#endif // ENABLE_VERSIONCHECK

void MainWindow::languageChanged() {
	hideNotification(m_notifierHandle);
	m_notifierHandle = nullptr;
	const Config::Lang& lang =  m_recognizer->getSelectedLanguage();
	std::string code = lang.code;
	if(code.empty()) {
		return;
	}
	GtkSpell::Checker checker;
	try {
		checker.set_language(code);
	} catch(const GtkSpell::Error& /*e*/) {
		if(getConfig()->getSetting<SwitchSetting>("dictinstall")->getValue()) {
			NotificationAction actionDontShowAgain = {_("Don't show again"), [this]{ m_config->getSetting<SwitchSetting>("dictinstall")->setValue(false); return true; }};
			NotificationAction actionInstall = NotificationAction{_("Install"), [this,lang]{ dictionaryAutoinstall(lang.code); return false; }};
#ifdef G_OS_UNIX
			if(getConfig()->useSystemDataLocations()) {
				// Try initiating a DBUS connection for PackageKit
				Glib::RefPtr<Gio::DBus::Proxy> proxy;
				Glib::ustring service_owner;
				try {
					proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BUS_TYPE_SESSION, "org.freedesktop.PackageKit",
					        "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify");
					service_owner = proxy->get_name_owner();
				} catch(...) {
				}
				if(!service_owner.empty()) {
					actionInstall = MainWindow::NotificationAction{_("Install"), [this,proxy,lang]{ dictionaryAutoinstall(proxy, lang.code); return false; }};
				} else {
					actionInstall = {_("Help"), [this]{ showHelp("#InstallSpelling"); return false; }};
					g_warning("Could not find PackageKit on DBus, dictionary autoinstallation will not work");
				}
			}
#endif
			addNotification(_("Spelling dictionary missing"), Glib::ustring::compose(_("The spellcheck dictionary for %1 is not installed"), lang.name), {actionInstall, actionDontShowAgain}, &m_notifierHandle);
		}
	}
}

#ifdef G_OS_UNIX
void MainWindow::dictionaryAutoinstall(Glib::RefPtr<Gio::DBus::Proxy> proxy, const Glib::ustring &code) {
	pushState(State::Busy, Glib::ustring::compose(_("Installing spelling dictionary for '%1'"), code));
	std::uint32_t xid = gdk_x11_window_get_xid(getWindow()->get_window()->gobj());
	std::vector<Glib::ustring> files;
	for(const Glib::ustring& langCulture : m_config->searchLangCultures(code)) {
		files.push_back("/usr/share/myspell/" + langCulture + ".dic");
		files.push_back("/usr/share/hunspell/" + langCulture + ".dic");
	}
	std::vector<Glib::VariantBase> params = { Glib::Variant<std::uint32_t>::create(xid),
	                                          Glib::Variant<std::vector<Glib::ustring>>::create(files),
	                                          Glib::Variant<Glib::ustring>::create("always")
	                                        };
	proxy->call("InstallProvideFiles", [proxy,this](Glib::RefPtr<Gio::AsyncResult> r) {
		dictionaryAutoinstallDone(proxy, r);
	},
	Glib::VariantContainerBase::create_tuple(params), 3600000);
}

void MainWindow::dictionaryAutoinstallDone(Glib::RefPtr<Gio::DBus::Proxy> proxy, Glib::RefPtr<Gio::AsyncResult>& result) {
	try {
		proxy->call_finish(result);
	} catch (const Glib::Error& e) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("Failed to install spelling dictionary: %1"), e.what()));
	}
	getRecognizer()->updateLanguagesMenu();
	popState();
}
#endif

void MainWindow::dictionaryAutoinstall(Glib::ustring code) {
	std::vector<Glib::ustring> codes = m_config->searchLangCultures(code);
	code = codes.empty() ? code : codes.front();

	pushState(State::Busy, Glib::ustring::compose(_("Installing spelling dictionary for '%1'"), code));
	Glib::ustring url= "https://cgit.freedesktop.org/libreoffice/dictionaries/tree/";
	Glib::ustring plainurl = "https://cgit.freedesktop.org/libreoffice/dictionaries/plain/";
	Glib::ustring urlcode = code;
	std::string dictPath = getConfig()->spellingLocation();
	Glib::RefPtr<Gio::File> dictDir = Gio::File::create_for_path(dictPath);
	bool dirExists = false;
	try {
		dirExists = dictDir->make_directory_with_parents();
	} catch(...) {
		dirExists = dictDir->query_exists();
	}
	if(!dirExists) {
		popState();
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), _("Failed to create directory for spelling dictionaries."));
		return;
	}

	Glib::ustring messages;
	Glib::RefPtr<Glib::ByteArray> html = Utils::download(url, messages);
	if(!html) {
		popState();
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("Could not read %1: %2."), url, messages));
		return;
	}
	Glib::ustring htmls(reinterpret_cast<const char*>(html->get_data()), html->size());
	if(htmls.find(Glib::ustring::compose(">%1<", code)) != Glib::ustring::npos) {
		// Ok
	} else if(htmls.find(Glib::ustring::compose(">%1<", code.substr(0, 2))) != Glib::ustring::npos) {
		urlcode = code.substr(0, 2);
	} else {
		popState();
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("No spelling dictionaries found for '%1'."), code));
		return;
	}
	html = Utils::download(url + urlcode + "/", messages);
	if(!html) {
		popState();
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("Could not read %1: %2."), url + urlcode + "/", messages));
		return;
	}
	Glib::RefPtr<Glib::Regex> pat = Glib::Regex::create(Glib::ustring::compose(">(%1[^<]*\\.(dic|aff))<", code.substr(0, 2)));
	htmls = Glib::ustring(reinterpret_cast<const char*>(html->get_data()), html->size());

	Glib::ustring downloaded;
	int pos = 0;
	Glib::MatchInfo matchInfo;
	while(pat->match(htmls, pos, matchInfo)) {
		pushState(State::Busy, Glib::ustring::compose(_("Downloading '%1'..."), matchInfo.fetch(1)));
		Glib::RefPtr<Glib::ByteArray> data = Utils::download(plainurl + urlcode + "/" + matchInfo.fetch(1), messages);
		if(data) {
			std::ofstream file(Glib::build_filename(dictPath, matchInfo.fetch(1)), std::ios::binary);
			if(file.is_open()) {
				file.write(reinterpret_cast<char*>(data->get_data()), data->size());
				downloaded.append(Glib::ustring::compose("\n%1", matchInfo.fetch(1)));
			}
		}
		popState();
		int start;
		matchInfo.fetch_pos(0, start, pos);
	}
	if(!downloaded.empty()) {
		getRecognizer()->updateLanguagesMenu();
		popState();
		Utils::message_dialog(Gtk::MESSAGE_INFO, _("Dictionaries installed"), Glib::ustring::compose(_("The following dictionaries were installed:%1"), downloaded));
	} else {
		popState();
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("No spelling dictionaries found for '%1'."), code));
	}
}

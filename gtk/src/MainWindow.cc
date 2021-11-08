/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.cc
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

#include "Application.hh"
#include "MainWindow.hh"
#include "Acquirer.hh"
#include "Config.hh"
#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "DisplayerToolHOCR.hh"
#include "DisplayerToolSelect.hh"
#include "OutputEditorHOCR.hh"
#include "OutputEditorText.hh"
#include "RecognitionMenu.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "TessdataManager.hh"
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
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE


#if ENABLE_VERSIONCHECK
#define CHECKURL "https://raw.githubusercontent.com/manisandro/gImageReader/master/LATEST"
#define DOWNLOADURL "https://github.com/manisandro/gImageReader/releases"
#define CHANGELOGURL "https://raw.githubusercontent.com/manisandro/gImageReader/master/NEWS"
#endif // ENABLE_VERSIONCHECK

static Glib::Quark notificationHandleKey("handle");

void MainWindow::signalHandler(int sig) {
	signalHandlerExec(sig, false);
}

void MainWindow::tesseractCrash(int signal) {
	signalHandlerExec(signal, true);
}

void MainWindow::signalHandlerExec(int sig, bool tesseractCrash) {
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
	Glib::spawn_async("", std::vector<std::string> {pkgExePath, "crashhandle", Glib::ustring::compose("%1", getpid()), Glib::ustring::compose("%1", tesseractCrash), filename}, Glib::SPAWN_DO_NOT_REAP_CHILD, sigc::slot<void>(), &pid);
#ifdef G_OS_WIN32
	WaitForSingleObject((HANDLE)pid, 0);
#elif __linux__
	// Allow crash handler spawned debugger to attach to the crashed process
	prctl(PR_SET_PTRACER, pid, 0, 0, 0);
	waitpid(pid, 0, 0);
#endif
	std::raise(sig);
}

#if !(defined(__ARMEL__) || defined(__LCC__) && __LCC__ <= 121)
static void terminateHandler() {
	std::set_terminate(nullptr);
	std::exception_ptr exptr = std::current_exception();
	if (exptr != 0) {
		try {
			std::rethrow_exception(exptr);
		} catch (std::exception& ex) {
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

MainWindow::MainWindow() {
	s_instance = this;

	std::signal(SIGSEGV, signalHandler);
	std::signal(SIGABRT, signalHandler);
#if !(defined(__ARMEL__) || defined(__LCC__) && __LCC__ <= 121)
	std::set_terminate(terminateHandler);
#endif

	ui.setupUi();
	ui.windowMain->set_icon_name("gimagereader");
	ui.dialogAbout->set_version(Glib::ustring::compose("%1 (%2)", PACKAGE_VERSION, Glib::ustring(PACKAGE_REVISION).substr(0, 7)));
	ui.labelAboutTesseractver->set_markup(Glib::ustring::compose("<small>%1 %2</small>", _("Using tesseract"), TESSERACT_VERSION_STR));

	m_config = new Config;
	m_acquirer = new Acquirer(ui);
	m_displayer = new Displayer(ui);
	m_recognitionMenu = new RecognitionMenu();
	m_recognizer = new Recognizer(ui);
	m_sourceManager = new SourceManager(ui);

	m_idlegroup.push_back(ui.buttonZoomin);
	m_idlegroup.push_back(ui.buttonZoomout);
	m_idlegroup.push_back(ui.buttonZoomnorm);
	m_idlegroup.push_back(ui.buttonZoomfit);
	m_idlegroup.push_back(ui.buttonRecognize);
	m_idlegroup.push_back(ui.menubuttonRotateMode);
	m_idlegroup.push_back(ui.spinRotate);
	m_idlegroup.push_back(ui.spinPage);
	m_idlegroup.push_back(ui.spinBrightness);
	m_idlegroup.push_back(ui.spinContrast);
	m_idlegroup.push_back(ui.spinResolution);
	m_idlegroup.push_back(ui.buttonAutolayout);

	CONNECT(ui.windowMain, delete_event, [this](GdkEventAny * ev) {
		return closeEvent(ev);
	});
	CONNECT(ui.buttonControls, toggled, [this] { ui.toolbarDisplay->set_visible(ui.buttonControls->get_active()); });
	CONNECT(m_acquirer, scanPageAvailable, [this](const std::string & filename) {
		m_sourceManager->addSources({Gio::File::create_for_path(filename)});
	});
	CONNECT(m_sourceManager, sourceChanged, [this] { onSourceChanged(); });
	CONNECT(ui.buttonOutputpane, toggled, [this] { if(m_outputEditor) m_outputEditor->getUI()->set_visible(ui.buttonOutputpane->get_active()); });
	m_connection_setOCRMode = CONNECT(ui.comboOcrmode, changed, [this] { setOutputMode(static_cast<OutputMode>(ui.comboOcrmode->get_active_row_number())); });
	CONNECT(m_recognitionMenu, languageChanged, [this] (const Config::Lang & lang) {
		languageChanged(lang);
	});
	CONNECT(ConfigSettings::get<ComboSetting>("outputorient"), changed, [this] {
		ui.panedOutput->set_orientation(static_cast<Gtk::Orientation>(!ConfigSettings::get<ComboSetting>("outputorient")->getValue()));
	});
	CONNECT(ui.buttonProgressCancel, clicked, [this] { progressCancel(); });
	CONNECT(ui.buttonAutolayout, clicked, [this] { m_displayer->autodetectOCRAreas(); });

	ADD_SETTING(VarSetting<std::vector<int>>("wingeom"));
	ADD_SETTING(SwitchSettingT<Gtk::ToggleButton>("showcontrols", ui.buttonControls));
	ADD_SETTING(ComboSetting("outputeditor", ui.comboOcrmode));

	ui.menubuttonLanguages->set_menu(*m_recognitionMenu);
	m_recognitionMenu->rebuild();

	pushState(State::Idle, _("Select an image to begin..."));

	const std::vector<int>& geom = ConfigSettings::get<VarSetting<std::vector<int>>>("wingeom")->getValue();
	if(geom.size() == 4) {
		ui.windowMain->resize(geom[2], geom[3]);
		ui.windowMain->move(geom[0], geom[1]);
	}
	ui.panedOutput->set_orientation(static_cast<Gtk::Orientation>(!ConfigSettings::get<ComboSetting>("outputorient")->getValue()));
#if ENABLE_VERSIONCHECK
	if(ConfigSettings::get<SwitchSetting>("updatecheck")->getValue()) {
		m_newVerThread = new std::thread(&MainWindow::getNewestVersion, this);
	}
#endif
}

MainWindow::~MainWindow() {
#if ENABLE_VERSIONCHECK
	if(m_newVerThread) {
		m_newVerThread->join();
		delete m_newVerThread;
	}
#endif
	delete m_acquirer;
	delete m_outputEditor;
	delete m_sourceManager;
	m_displayer->setTool(nullptr);
	delete m_displayerTool;
	delete m_displayer;
	delete m_recognizer;
	delete m_config;
	delete m_recognitionMenu;
	s_instance = nullptr;
}

void MainWindow::setMenuModel(const Glib::RefPtr<Gio::MenuModel>& menuModel) {
	ui.menubuttonOptions->set_menu_model(menuModel);
	ui.menubuttonOptions->set_visible(true);
}

void MainWindow::openFiles(const std::vector<Glib::RefPtr<Gio::File>>& files) {
	std::vector<Glib::RefPtr<Gio::File>> hocrFiles;
	std::vector<Glib::RefPtr<Gio::File>> otherFiles;
	for(const Glib::RefPtr<Gio::File>& file : files) {
		std::string filename = file->get_path();
		if(Glib::ustring(filename.substr(filename.length() - 5)).lowercase() == ".html") {
			hocrFiles.push_back(file);
		} else {
			otherFiles.push_back(file);
		}
	}
	m_sourceManager->addSources(otherFiles);
	if(!hocrFiles.empty()) {
		if(setOutputMode(OutputModeHOCR)) {
			static_cast<OutputEditorHOCR*>(m_outputEditor)->open(OutputEditorHOCR::InsertMode::Append, hocrFiles);
			m_outputEditor->getUI()->set_visible(true);
		}
	}
}

void MainWindow::openOutput(const std::string& filename) {
	if(Utils::string_endswith(Glib::ustring(filename).lowercase(), ".txt")) {
		if(setOutputMode(OutputModeText)) {
			m_outputEditor->open(filename);
		}
	} else if(Utils::string_endswith(Glib::ustring(filename).lowercase(), ".html")) {
		if(setOutputMode(OutputModeHOCR)) {
			m_outputEditor->open(filename);
		}
	}
}

void MainWindow::setOutputPaneVisible(bool visible) {
	ui.buttonOutputpane->set_active(visible);
}

void MainWindow::pushState(State state, const Glib::ustring& msg) {
	m_stateStack.push_back(state);
	ui.statusbar->push(msg);
	setState(state);
}

void MainWindow::popState() {
	ui.statusbar->pop();
	m_stateStack.pop_back();
	setState(m_stateStack.back());
}

void MainWindow::setState(State state) {
	bool isIdle = state == State::Idle;
	bool isBusy = state == State::Busy;
	for(Gtk::Widget* w : m_idlegroup) {
		w->set_sensitive(!isIdle);
	}
	ui.panedOutput->set_sensitive(!isBusy);
	ui.headerbar->set_sensitive(!isBusy);
	if(ui.windowMain->get_window()) {
		ui.windowMain->get_window()->set_cursor(isBusy ? Gdk::Cursor::create(Gdk::WATCH) : Glib::RefPtr<Gdk::Cursor>());
	}
}

void MainWindow::showProgress(ProgressMonitor* monitor, int updateInterval) {
	m_progressMonitor = monitor;
	m_connection_progressUpdate = Glib::signal_timeout().connect([this] { progressUpdate(); return true; }, updateInterval);
	ui.progressbarProgress->set_fraction(0);
	ui.buttonProgressCancel->set_sensitive(true);
	ui.boxProgress->set_visible(true);
}

void MainWindow::hideProgress() {
	ui.boxProgress->set_visible(false);
	m_connection_progressUpdate.disconnect();
	m_progressMonitor = nullptr;
}

void MainWindow::progressCancel() {
	if(m_progressMonitor) {
		ui.buttonProgressCancel->set_sensitive(true);
		m_progressMonitor->cancel();
	}
}

void MainWindow::progressUpdate() {
	if(m_progressMonitor) {
		ui.progressbarProgress->set_fraction(m_progressMonitor->getProgress() / 100.);
	}
}


bool MainWindow::closeEvent(GdkEventAny*) {
	if(!m_outputEditor->clear()) {
		return true;
	}
	if((ui.windowMain->get_window()->get_state() & Gdk::WINDOW_STATE_MAXIMIZED) == 0) {
		std::vector<int> geom(4);
		ui.windowMain->get_position(geom[0], geom[1]);
		ui.windowMain->get_size(geom[2], geom[3]);
		ConfigSettings::get<VarSetting<std::vector<int>>>("wingeom")->setValue(geom);
	}
	ui.windowMain->hide();
	return false;
}

void MainWindow::onSourceChanged() {
	std::vector<Source*> sources = m_sourceManager->getSelectedSources();
	if(m_displayer->setSources(sources) && !sources.empty()) {
		// ui.headerbar->set_subtitle(sources.size() == 1 ? Glib::ustring(sources.front()->displayname) : Glib::ustring::compose(_("Multiple sources (%1)"), sources.size()));
		if(m_stateStack.back() == State::Idle) {
			pushState(State::Normal, _("Ready"));
		}
	} else {
		if(m_stateStack.back() == State::Normal) {
			popState();
		}
		// ui.headerbar->set_subtitle("");
	}
}

void MainWindow::showAbout() {
	ui.dialogAbout->run();
	ui.dialogAbout->hide();
}

void MainWindow::showHelp(const std::string& chapter) {
#ifdef G_OS_WIN32
	// Always use relative path on Windows
	std::string manualDirPath;
	char* locale = g_win32_getlocale();
	std::string language(locale, 2);
	g_free(locale);
#else
	std::string manualDirPath = MANUAL_DIR;
	std::string language = Glib::getenv("LANG").substr(0, 2);
#endif
	if(manualDirPath.empty()) {
		manualDirPath = Glib::build_filename(pkgDir, "share", "doc", "gimagereader");
	}
	std::string manualFile = Utils::make_absolute_path(Glib::build_filename(manualDirPath, Glib::ustring::compose("manual-%1.html", language)), Glib::get_current_dir());
	if(!Glib::file_test(manualFile, Glib::FILE_TEST_EXISTS)) {
		manualFile = Utils::make_absolute_path(Glib::build_filename(manualDirPath, "manual.html"), Glib::get_current_dir());
	}
	Utils::openUri(Glib::filename_to_uri(manualFile) + chapter);
}

void MainWindow::showConfig() {
	m_config->showDialog();
	m_recognitionMenu->rebuild();
}

bool MainWindow::setOutputMode(OutputMode mode) {
	if(m_outputEditor && !m_outputEditor->clear()) {
		m_connection_setOCRMode.block(true);
		if(dynamic_cast<OutputEditorText*>(m_outputEditor)) {
			ui.comboOcrmode->set_active(OutputModeText);
		} else if(dynamic_cast<OutputEditorHOCR*>(m_outputEditor)) {
			ui.comboOcrmode->set_active(OutputModeHOCR);
		}
		m_connection_setOCRMode.block(false);
		return false;
	} else {
		if(m_outputEditor) {
			ui.panedOutput->remove(*m_outputEditor->getUI());
			delete m_outputEditor;
		}
		delete m_displayerTool;
		if(mode == OutputModeText) {
			m_displayerTool = new DisplayerToolSelect(m_displayer);
			m_outputEditor = new OutputEditorText();
		} else { /*if(mode == OutputModeHOCR)*/
			m_displayerTool = new DisplayerToolHOCR(m_displayer);
			m_outputEditor = new OutputEditorHOCR(static_cast<DisplayerToolHOCR*>(m_displayerTool));
		}
		ui.buttonAutolayout->set_visible(m_displayerTool->allowAutodetectOCRAreas());
		m_displayer->setTool(m_displayerTool);
		m_outputEditor->setLanguage(m_recognitionMenu->getRecognitionLanguage());
		ui.panedOutput->pack2(*m_outputEditor->getUI(), true, false);
		// set default width of the left child (panedSources) to 1300px
		ui.panedOutput->set_position(1300);
		m_outputEditor->getUI()->set_visible(ui.buttonOutputpane->get_active());
		return true;
	}
}

void MainWindow::redetectLanguages() {
	m_recognitionMenu->rebuild();
}

void MainWindow::manageLanguages() {
	TessdataManager::exec();
}

void MainWindow::addNotification(const Glib::ustring& title, const Glib::ustring& message, const std::vector<NotificationAction>& actions, Notification* handle) {
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
	CONNECT(closebtn, clicked, [this, frame] { hideNotification(frame); });
	box->pack_end(*closebtn, false, true);
	for(const NotificationAction& action : actions) {
		Gtk::Button* btn = Gtk::manage(new Gtk::Button(action.label));
		btn->set_relief(Gtk::RELIEF_NONE);
		CONNECT(btn, clicked, [this, frame, action] { if(action.action()) {
		hideNotification(frame);
		}
		                                            });
		box->pack_start(*btn, false, true);
		btn->get_child()->override_color(Gdk::RGBA("#0000FF"), Gtk::STATE_FLAG_NORMAL);
	}
	frame->show_all();
	ui.boxMain->pack_end(*frame, false, true);
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
		ui.boxMain->remove(*frame);
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
	} catch (const Glib::Error& e) {
		return;
	}
	std::string newver(buf);
	g_debug("Newest version is: %s", newver.c_str());
	newver.erase(std::remove_if(newver.begin(), newver.end(), ::isspace), newver.end());
	if(Glib::Regex::create(R"(^[\d+\.]+\d+$)")->match(newver, 0, Glib::RegexMatchFlags(0))) {
		Glib::signal_idle().connect_once([this, newver] { checkVersion(newver); });
	}
}

void MainWindow::checkVersion(const Glib::ustring& newver) {
	m_newVerThread->join();
	m_newVerThread = nullptr;
	Glib::ustring curver = PACKAGE_VERSION;

	if(newver.compare(curver) > 0) {
		addNotification(_("New version"), Glib::ustring::compose(_("gImageReader %1 is available"), newver), {
			{_("Download"), [ = ]{ Utils::openUri(DOWNLOADURL); return false; }},
			{_("Changelog"), [ = ]{ Utils::openUri(CHANGELOGURL); return false; }},
			{_("Don't notify again"), []{ ConfigSettings::get<SwitchSetting>("updatecheck")->setValue(false); return true; }}
		});
	}
}
#endif // ENABLE_VERSIONCHECK

void MainWindow::languageChanged(const Config::Lang& lang) {
	if(m_outputEditor) {
		m_outputEditor->setLanguage(lang);
	}
	hideNotification(m_notifierHandle);
	m_notifierHandle = nullptr;
	std::string code = lang.code;
	if(code.empty()) {
		return;
	}
	GtkSpell::Checker checker;
	try {
		checker.set_language(code);
	} catch(const GtkSpell::Error& /*e*/) {
		if(ConfigSettings::get<SwitchSetting>("dictinstall")->getValue()) {
			NotificationAction actionDontShowAgain = {_("Don't show again"), []{ ConfigSettings::get<SwitchSetting>("dictinstall")->setValue(false); return true; }};
			NotificationAction actionInstall = NotificationAction{_("Install"), [this, lang]{ dictionaryAutoinstall(lang.code); return false; }};
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
					actionInstall = MainWindow::NotificationAction{_("Install"), [this, proxy, lang]{ dictionaryAutoinstall(proxy, lang.code); return false; }};
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
void MainWindow::dictionaryAutoinstall(Glib::RefPtr<Gio::DBus::Proxy> proxy, const Glib::ustring& code) {
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
	proxy->call("InstallProvideFiles", [proxy, this](Glib::RefPtr<Gio::AsyncResult> r) {
		dictionaryAutoinstallDone(proxy, r);
	},
	Glib::VariantContainerBase::create_tuple(params), 3600000);
}

void MainWindow::dictionaryAutoinstallDone(Glib::RefPtr<Gio::DBus::Proxy> proxy, Glib::RefPtr<Gio::AsyncResult>& result) {
	try {
		proxy->call_finish(result);
	} catch (const Glib::Error& e) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("Failed to install spelling dictionary: %1"), e.what()));
	}
	m_recognitionMenu->rebuild();
	popState();
}
#endif

void MainWindow::dictionaryAutoinstall(Glib::ustring code) {
	pushState(State::Busy, Glib::ustring::compose(_("Installing spelling dictionary for '%1'"), code));
	Glib::ustring url = "https://cgit.freedesktop.org/libreoffice/dictionaries/tree/";
	Glib::ustring plainurl = "https://cgit.freedesktop.org/libreoffice/dictionaries/plain/";
	std::string spellingDir = getConfig()->spellingLocation();
	Glib::RefPtr<Gio::File> dir = Gio::File::create_for_path(spellingDir);
	bool dirExists = false;
	try {
		dirExists = dir->make_directory_with_parents();
	} catch(...) {
		dirExists = dir->query_exists();
	}
	if(!dirExists) {
		popState();
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Error"), _("Failed to create directory for spelling dictionaries."));
		return;
	}

	Glib::ustring messages;
	Glib::RefPtr<Glib::ByteArray> htmlData = Utils::download(url, messages);
	if(!htmlData) {
		popState();
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("Could not read %1: %2."), url, messages));
		return;
	}
	Glib::ustring html(reinterpret_cast<const char*>(htmlData->get_data()), htmlData->size());
	std::string langCode = code.substr(0, 2);
	Glib::RefPtr<Glib::Regex> langPat = Glib::Regex::create(Glib::ustring::compose(">(%1_?[A-Z]*)<", langCode));
	Glib::RefPtr<Glib::Regex> dictPat = Glib::Regex::create(Glib::ustring::compose(">(%1_?[\\w_]*\\.(dic|aff))<", langCode));
	std::vector<Glib::ustring> downloaded;
	std::vector<Glib::ustring> failed;

	int pos = 0;
	Glib::MatchInfo langMatchInfo;
	while(langPat->match(html, pos, langMatchInfo)) {
		Glib::ustring lang = langMatchInfo.fetch(1);
		int start;
		langMatchInfo.fetch_pos(0, start, pos);

		Glib::RefPtr<Glib::ByteArray> dictHtmlData = Utils::download(url + lang + "/", messages);
		if(!dictHtmlData) {
			continue;
		}
		Glib::ustring dictHtml(reinterpret_cast<const char*>(dictHtmlData->get_data()), dictHtmlData->size());

		int dictPos = 0;
		Glib::MatchInfo dictMatchInfo;
		while(dictPat->match(dictHtml, dictPos, dictMatchInfo)) {
			Glib::ustring filename = dictMatchInfo.fetch(1);
			pushState(State::Busy, Glib::ustring::compose(_("Downloading '%1'..."), filename));
			Glib::RefPtr<Glib::ByteArray> data = Utils::download(plainurl + lang + "/" + filename, messages);
			if(data) {
				std::ofstream file(Glib::build_filename(spellingDir, filename), std::ios::binary);
				if(file.is_open()) {
					file.write(reinterpret_cast<char*>(data->get_data()), data->size());
					downloaded.push_back(filename);
				} else {
					failed.push_back(filename);
				}
			} else {
				failed.push_back(filename);
			}
			popState();
			int start;
			dictMatchInfo.fetch_pos(0, start, dictPos);
		}
	}

	popState();
	if(!failed.empty()) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("The following dictionaries could not be downloaded:\n%1\n\nCheck the connectivity and directory permissions.\nHint: If you don't have write permissions in system folders, you can switch to user paths in the settings dialog."), Utils::string_join(failed, "\n")));
	} else if(!downloaded.empty()) {
		m_recognitionMenu->rebuild();
		Utils::messageBox(Gtk::MESSAGE_INFO, _("Dictionaries installed"), Glib::ustring::compose(_("The following dictionaries were installed:\n%1"), Utils::string_join(downloaded, "\n")));
	} else {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("No spelling dictionaries found for '%1'."), code));
	}
}

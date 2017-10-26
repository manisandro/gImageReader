/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.cc
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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

#include <QCloseEvent>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QProcess>
#include <QProgressBar>
#include <QStatusBar>
#include <QUrl>
#include <csignal>
#include <iostream>
#ifdef Q_OS_LINUX
#include <sys/prctl.h>
#endif
#include <QtSpell.hpp>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "MainWindow.hh"
#include "Acquirer.hh"
#include "Config.hh"
#include "Displayer.hh"
#include "DisplayerToolSelect.hh"
#include "DisplayerToolHOCR.hh"
#include "ImageProcessor.hh"
#include "OutputEditorText.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include "ui_AboutDialog.h"


#define CHECKURL "https://raw.githubusercontent.com/manisandro/gImageReader/master/LATEST"
#define DOWNLOADURL "https://github.com/manisandro/gImageReader/releases"
#define CHANGELOGURL "https://raw.githubusercontent.com/manisandro/gImageReader/master/NEWS"

void MainWindow::signalHandler(int signal) {
	std::signal(signal, nullptr);

	QString filename;
	if(MAIN->getOutputEditor() && MAIN->getOutputEditor()->getModified()) {
		filename = QDir(Utils::documentsFolder()).absoluteFilePath(QString("%1_crash-save.txt").arg(PACKAGE_NAME));
		int i = 0;
		while(QFile(filename).exists()) {
			++i;
			filename = QDir(Utils::documentsFolder()).absoluteFilePath(QString("%1_crash-save_%2.txt").arg(PACKAGE_NAME).arg(i));
		}
		MAIN->getOutputEditor()->save(filename);
	}

	QProcess process;
	process.start(QApplication::applicationFilePath(), QStringList() << "crashhandle" << QString::number(QApplication::applicationPid()) << filename);
#ifdef Q_OS_LINUX
	// Allow crash handler spawned debugger to attach to the crashed process
	prctl(PR_SET_PTRACER, process.pid(), 0, 0, 0);
#endif
	process.waitForFinished(-1);
	std::raise(signal);
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

MainWindow::MainWindow(const QStringList& files)
	: m_idleActions(0) {
	s_instance = this;

	std::signal(SIGSEGV, signalHandler);
	std::signal(SIGABRT, signalHandler);
#ifndef __ARMEL__
	std::set_terminate(terminateHandler);
#endif

	QList<QNetworkProxy> listOfProxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl(CHECKURL)));
	if (listOfProxies.size())
		QNetworkProxy::setApplicationProxy(listOfProxies[0]);

	qRegisterMetaType<MainWindow::State>();

	ui.setupUi(this);

	m_config = new Config(this);
	m_acquirer = new Acquirer(ui);
	m_displayer = new Displayer(ui);
	m_recognizer = new Recognizer(ui);
	m_sourceManager = new SourceManager(ui);
    // TODO: Use own-written make_unique function
	m_imageProcessor.reset(new ImageProcessor(ui, *m_displayer));

	ui.centralwidget->layout()->addWidget(m_displayer);

	m_idleActions.setExclusive(false);
	m_idleActions.addAction(ui.actionZoomIn);
	m_idleActions.addAction(ui.actionZoomOut);
	m_idleActions.addAction(ui.actionOriginalSize);
	m_idleActions.addAction(ui.actionBestFit);
	m_idleActions.addAction(ui.actionRotateLeft);
	m_idleActions.addAction(ui.actionRotateRight);
	m_idleActions.addAction(ui.actionAutodetectLayout);
	m_idleWidgets.append(ui.spinBoxRotation);
	m_idleWidgets.append(ui.spinBoxPage);
	m_idleWidgets.append(ui.spinBoxBrightness);
	m_idleWidgets.append(ui.spinBoxContrast);
	m_idleWidgets.append(ui.spinBoxResolution);
	m_idleWidgets.append(ui.toolButtonRecognize);

	connect(ui.actionRedetectLanguages, SIGNAL(triggered()), m_recognizer, SLOT(updateLanguagesMenu()));
	connect(ui.actionPreferences, SIGNAL(triggered()), this, SLOT(showConfig()));
	connect(ui.actionHelp, SIGNAL(triggered()), this, SLOT(showHelp()));
	connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
	connect(ui.actionImageControls, SIGNAL(toggled(bool)), ui.widgetImageControls, SLOT(setVisible(bool)));
	connect(m_acquirer, SIGNAL(scanPageAvailable(QString)), m_sourceManager, SLOT(addSource(QString)));
	connect(m_sourceManager, SIGNAL(sourceChanged()), this, SLOT(onSourceChanged()));
	connect(ui.actionToggleOutputPane, SIGNAL(toggled(bool)), ui.dockWidgetOutput, SLOT(setVisible(bool)));
	connect(ui.comboBoxOCRMode, SIGNAL(currentIndexChanged(int)), this, SLOT(setOCRMode(int)));
	connect(m_recognizer, SIGNAL(languageChanged(Config::Lang)), this, SLOT(languageChanged()));


	m_config->addSetting(new VarSetting<QByteArray>("wingeom"));
	m_config->addSetting(new VarSetting<QByteArray>("winstate"));
	m_config->addSetting(new ActionSetting("showcontrols", ui.actionImageControls));
	m_config->addSetting(new ComboSetting("outputeditor", ui.comboBoxOCRMode, 0));

	m_recognizer->updateLanguagesMenu();

	m_progressWidget = new QWidget(this);
	m_progressWidget->setLayout(new QHBoxLayout());
	m_progressWidget->layout()->setContentsMargins(0, 0, 0, 0);
	m_progressWidget->layout()->setSpacing(2);
	m_progressWidget->layout()->addWidget(new QLabel());
	m_progressBar = new QProgressBar();
	m_progressBar->setRange(0, 100);
	m_progressBar->setMaximumWidth(100);
	m_progressTimer.setSingleShot(false);
	connect(&m_progressTimer, SIGNAL(timeout()), this, SLOT(progressUpdate()));
	m_progressWidget->layout()->addWidget(m_progressBar);
	m_progressCancelButton = new QToolButton();
	m_progressCancelButton->setIcon(QIcon::fromTheme("dialog-close"));
	connect(m_progressCancelButton, SIGNAL(clicked(bool)), this, SLOT(progressCancel()));
	m_progressWidget->layout()->addWidget(m_progressCancelButton);
	statusBar()->addPermanentWidget(m_progressWidget);
	m_progressWidget->setVisible(false);

	pushState(State::Idle, _("Select an image to begin..."));

	restoreGeometry(m_config->getSetting<VarSetting<QByteArray>>("wingeom")->getValue());
	restoreState(m_config->getSetting<VarSetting<QByteArray>>("winstate")->getValue());
	ui.dockWidgetOutput->setVisible(false);

	ui.actionSources->trigger();

#if ENABLE_VERSIONCHECK
	if(m_config->getSetting<SwitchSetting>("updatecheck")->getValue()) {
		connect(&m_versionCheckThread, SIGNAL(finished()), this, SLOT(checkVersion()));
		m_versionCheckThread.start();
	}
#endif

	m_sourceManager->addSources(files);
}

MainWindow::~MainWindow() {
#if ENABLE_VERSIONCHECK
	while(m_versionCheckThread.isRunning()) {
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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
	s_instance = nullptr;
}

void MainWindow::openFiles(const QStringList& files) {
	m_sourceManager->addSources(files);
}

void MainWindow::setOutputPaneVisible(bool visible) {
	ui.actionToggleOutputPane->setChecked(visible);
}

void MainWindow::pushState(MainWindow::State state, const QString& msg) {
	m_stateStack.push(QPair<State, QString>(state, msg));
	ui.statusbar->showMessage(msg);
	setState(state);
	if(state == State::Busy) {
		QApplication::setOverrideCursor(Qt::WaitCursor);
	}
}

void MainWindow::popState() {
	if(m_stateStack.top().first == State::Busy) {
		QApplication::restoreOverrideCursor();
	}
	m_stateStack.pop();
	const QPair<State, QString>& pair = m_stateStack.top();
	ui.statusbar->showMessage(pair.second);
	setState(pair.first);
}

void MainWindow::setState(State state) {
	bool isIdle = state == State::Idle;
	m_idleActions.setEnabled(!isIdle);
	for(QWidget* widget : m_idleWidgets)
		widget->setEnabled(!isIdle);
}

void MainWindow::closeEvent(QCloseEvent* ev) {
	if(m_stateStack.top().first == State::Busy) {
		ev->ignore();
	} else if(!m_outputEditor->clear()) {
		ev->ignore();
	} else if(!isMaximized()) {
		m_config->getSetting<VarSetting<QByteArray>>("wingeom")->setValue(saveGeometry());
		m_config->getSetting<VarSetting<QByteArray>>("winstate")->setValue(saveState());
	}
}

void MainWindow::onSourceChanged() {
	if(m_stateStack.top().first == State::Normal) {
		popState();
	}
	QList<Source*> sources = m_sourceManager->getSelectedSources();
	if(m_displayer->setSources(sources)) {
		setWindowTitle(QString("%1 - %2").arg(sources.size() == 1 ? sources.front()->displayname : _("Multiple sources")).arg(PACKAGE_NAME));
		pushState(State::Normal, _("Ready"));
	} else {
		setWindowTitle(PACKAGE_NAME);
	}
}

void MainWindow::showAbout() {
	QDialog d(this);
	Ui::AboutDialog aboutDialogUi;
	aboutDialogUi.setupUi(&d);
	aboutDialogUi.labelVersion->setText(PACKAGE_VERSION);
	aboutDialogUi.labelTesseractVer->setText(QString("<html><head/><body><p style=\"font-size:small;\">%1 %2</p></body></html>").arg(_("Using tesseract")).arg(TESSERACT_VERSION_STR));
	d.exec();
}

void MainWindow::showHelp(const QString& chapter) {
#ifdef Q_OS_WIN32
	QDir manualDir(QString("%1/../share/doc/gimagereader").arg(QApplication::applicationDirPath()));
#else
	QDir manualDir(MANUAL_DIR);
#endif
	QString language = QLocale::system().name().left(2);
	QString manualFile = manualDir.absoluteFilePath(QString("manual-%1.html").arg(language));
	if(!QFile(manualFile).exists()) {
		manualFile = manualDir.absoluteFilePath("manual.html");
	}
	QUrl manualUrl = QUrl::fromLocalFile(manualFile);
	manualUrl.setFragment(chapter);
	QDesktopServices::openUrl(manualUrl);
}

void MainWindow::showConfig() {
	m_config->showDialog();
	m_recognizer->updateLanguagesMenu();
}

void MainWindow::setOCRMode(int idx) {
	if(m_outputEditor && !m_outputEditor->clear()) {
		ui.comboBoxOCRMode->blockSignals(true);
		if(dynamic_cast<OutputEditorText*>(m_outputEditor)) {
			ui.comboBoxOCRMode->setCurrentIndex(0);
		} else { /*if(dynamic_cast<OutputEditorHOCR*>(m_outputEditor))*/
			ui.comboBoxOCRMode->setCurrentIndex(1);
		}
		ui.comboBoxOCRMode->blockSignals(false);
	} else {
		delete m_displayerTool;
		delete m_outputEditor;
		if(idx == 0) {
			m_displayerTool = new DisplayerToolSelect(ui.actionAutodetectLayout, m_displayer);
			m_outputEditor = new OutputEditorText();
		} else { /*if(idx == 1)*/
			m_displayerTool = new DisplayerToolHOCR(m_displayer);
			m_outputEditor = new OutputEditorHOCR(static_cast<DisplayerToolHOCR*>(m_displayerTool));
		}
		m_displayer->setTool(m_displayerTool);
		connect(m_recognizer, SIGNAL(languageChanged(Config::Lang)), m_outputEditor, SLOT(setLanguage(Config::Lang)));
		m_outputEditor->setLanguage(m_recognizer->getSelectedLanguage());
		connect(ui.actionToggleOutputPane, SIGNAL(toggled(bool)), m_outputEditor, SLOT(onVisibilityChanged(bool)));
		ui.dockWidgetOutput->setWidget(m_outputEditor->getUI());
	}
}

void MainWindow::addNotification(const QString& title, const QString& message, const QList<NotificationAction> &actions, MainWindow::Notification* handle) {
	QFrame* frame = new QFrame();
	frame->setFrameStyle(QFrame::StyledPanel|QFrame::Raised);
	frame->setStyleSheet("background: #FFD000;");
	QHBoxLayout* layout = new QHBoxLayout(frame);
	layout->addWidget(new QLabel(QString("<b>%1</b>").arg(title), frame));
	QLabel* msgLabel = new QLabel(message, frame);
	msgLabel->setWordWrap(true);
	layout->addWidget(msgLabel, 1);
	for(const NotificationAction& action : actions) {
		QToolButton* btn = new QToolButton(frame);
		btn->setText(action.text);
		connect(btn, SIGNAL(clicked()), action.target, action.slot);
		if(action.close) {
			btn->setProperty("handle", QVariant::fromValue(reinterpret_cast<void*>(handle)));
			btn->setProperty("frame", QVariant::fromValue(reinterpret_cast<void*>(frame)));
			connect(btn, SIGNAL(clicked()), this, SLOT(hideNotification()));
		}
		layout->addWidget(btn);
	}
	QToolButton* closeBtn = new QToolButton();
	closeBtn->setIcon(QIcon::fromTheme("dialog-close"));
	closeBtn->setProperty("handle", QVariant::fromValue(reinterpret_cast<void*>(handle)));
	closeBtn->setProperty("frame", QVariant::fromValue(reinterpret_cast<void*>(frame)));
	connect(closeBtn, SIGNAL(clicked()), this, SLOT(hideNotification()));
	layout->addWidget(closeBtn);
	ui.centralwidget->layout()->addWidget(frame);
	if(handle) {
		*handle = frame;
	}
}

void MainWindow::hideNotification(Notification handle) {
	if(!handle && QObject::sender()) {
		handle = static_cast<QFrame*>(static_cast<QToolButton*>(QObject::sender())->property("frame").value<void*>());
		Notification* h = reinterpret_cast<void**>(static_cast<QToolButton*>(QObject::sender())->property("handle").value<void*>());
		if(h) {
			*h = nullptr;
		}
	}
	if(handle) {
		static_cast<QFrame*>(handle)->deleteLater();
	}
}

void MainWindow::VersionCheckThread::run() {
	QString messages;
	QString newver = Utils::download(QUrl(CHECKURL), messages, 5000);
	newver.replace(QRegExp("\\s+"), "");
	QRegExp pat(R"(^[\d+\.]+\d+$)");
	if(pat.exactMatch(newver)) {
		m_newestVersion = newver;
	}
}

void MainWindow::checkVersion() {
	QString newver = m_versionCheckThread.getNewestVersion();
	qDebug("Newest version is: %s", qPrintable(newver));
	if(newver.isEmpty()) {
		return;
	}
	QString curver = PACKAGE_VERSION;

	if(newver.compare(curver) > 0) {
		addNotification(_("New version"), _("gImageReader %1 is available").arg(newver), {
			{_("Download"), this, SLOT(openDownloadUrl()), false},
			{_("Changelog"), this, SLOT(openChangeLogUrl()), false},
			{_("Don't notify again"), m_config, SLOT(disableUpdateCheck()), true}
		});
	}
}

void MainWindow::openDownloadUrl() {
	QDesktopServices::openUrl(QUrl(DOWNLOADURL));
}

void MainWindow::openChangeLogUrl() {
	QDesktopServices::openUrl(QUrl(CHANGELOGURL));
}

void MainWindow::showProgress(ProgressMonitor* monitor, int updateInterval) {
	m_progressMonitor = monitor;
	m_progressTimer.start(updateInterval);
	m_progressCancelButton->setEnabled(true);
	m_progressBar->setValue(0);
	m_progressWidget->show();
}

void MainWindow::hideProgress() {
	m_progressWidget->hide();
	m_progressTimer.stop();
	m_progressMonitor = nullptr;
}

void MainWindow::progressCancel() {
	if(m_progressMonitor) {
		m_progressCancelButton->setEnabled(false);
		m_progressMonitor->cancel();
	}
}

void MainWindow::progressUpdate() {
	if(m_progressMonitor) {
		m_progressBar->setValue(m_progressMonitor->getProgress());
	}
}

void MainWindow::languageChanged() {
	hideNotification(m_notifierHandle);
	m_notifierHandle = nullptr;
	const QString& code = m_recognizer->getSelectedLanguage().code;
	if(!code.isEmpty() && !QtSpell::checkLanguageInstalled(code) && m_config->getSetting<SwitchSetting>("dictinstall")->getValue()) {
		NotificationAction actionDontShowAgain = {_("Don't show again"), m_config, SLOT(disableDictInstall()), true};
		NotificationAction actionInstall = {_("Install"), this, SLOT(dictionaryAutoinstall()), false};
#ifdef Q_OS_LINUX
		if(getConfig()->useSystemDataLocations()) {
			QDBusConnectionInterface* iface = QDBusConnection::sessionBus().interface();
			iface->startService("org.freedesktop.PackageKit");
			if(!iface->isServiceRegistered("org.freedesktop.PackageKit").value()) {
				actionInstall = {_("Help"), this, SLOT(showHelp()), false}; // TODO #InstallSpelling
				qWarning("Could not find PackageKit on DBus, dictionary autoinstallation will not work");
			}
		}
#endif
		const QString& name = m_recognizer->getSelectedLanguage().name;
		addNotification(_("Spelling dictionary missing"), _("The spellcheck dictionary for %1 is not installed").arg(name), {actionInstall, actionDontShowAgain}, &m_notifierHandle);
	}
}

void MainWindow::dictionaryAutoinstall() {
	QList<QString> codes = m_config->searchLangCultures(m_recognizer->getSelectedLanguage().code);
	QString code = codes.isEmpty() ? m_recognizer->getSelectedLanguage().code : codes.front();
	pushState(State::Busy, _("Installing spelling dictionary for '%1'").arg(code));
#ifdef Q_OS_WIN
	bool isWindows = true;
#else
	bool isWindows = false;
#endif

	if(!isWindows && MAIN->getConfig()->useSystemDataLocations()) {
		// Place this in a ifdef since DBus stuff cannot be compiled on Windows
#ifdef Q_OS_LINUX
		QStringList files;
		for(const QString& langCulture : m_config->searchLangCultures(code)) {
			files.append("/usr/share/myspell/" + langCulture + ".dic");
			files.append("/usr/share/hunspell/" + langCulture + ".dic");
		}
		QDBusMessage req = QDBusMessage::createMethodCall("org.freedesktop.PackageKit", "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify", "InstallProvideFiles");
		req.setArguments(QList<QVariant>() << QVariant::fromValue((quint32)winId()) << QVariant::fromValue(files) << QVariant::fromValue(QString("always")));
		QDBusMessage reply = QDBusConnection::sessionBus().call(req, QDBus::BlockWithGui, 3600000);
		if(reply.type() == QDBusMessage::ErrorMessage) {
			if(QMessageBox::Help == QMessageBox::critical(this, _("Error"), _("Failed to install spelling dictionary: %1").arg(reply.errorMessage()), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)) {
				showHelp("#InstallSpelling");
			}
		}
		m_recognizer->updateLanguagesMenu();
		popState();
#endif
	} else {
		QString url = "https://cgit.freedesktop.org/libreoffice/dictionaries/tree/";
		QString plainurl = "https://cgit.freedesktop.org/libreoffice/dictionaries/plain/";
		QDir spellingDir(getConfig()->spellingLocation());
		if(!QDir().mkpath(spellingDir.absolutePath())) {
			popState();
			if(QMessageBox::Help == QMessageBox::critical(this, _("Error"), _("Failed to create directory for spelling dictionaries."), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)) {
				showHelp("#InstallSpelling");
			}
			return;
		}
		QString urlcode = code;
		QString messages;
		QByteArray html = Utils::download(url, messages);
		if(html.isNull()) {
			popState();
			if(QMessageBox::Help == QMessageBox::critical(this, _("Error"), _("Could not read %1: %2.").arg(url).arg(messages), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)) {
				showHelp("#InstallSpelling");
			}
			return;
		} else if(html.indexOf(QString(">%1<").arg(code)) != -1) {
			// Ok
		} else if(html.indexOf(QString(">%1<").arg(code.left(2))) != -1) {
			urlcode = code.left(2);
		} else {
			popState();
			if(QMessageBox::Help == QMessageBox::critical(this, _("Error"), _("No spelling dictionaries found for '%1'.").arg(code), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)) {
				showHelp("#InstallSpelling");
			}
			return;
		}
		html = Utils::download(url + urlcode + "/", messages);
		if(html.isNull()) {
			popState();
			if(QMessageBox::Help == QMessageBox::critical(this, _("Error"), _("Could not read %1: %2").arg(url + urlcode + "/").arg(messages), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)) {
				showHelp("#InstallSpelling");
			}
			return;
		}
		QRegExp pat(QString(">(%1[^<]*\\.(dic|aff))<").arg(code.left(2)));
		QString htmls = html;

		QString downloaded;
		int pos = 0;
		while((pos = htmls.indexOf(pat, pos)) != -1) {
			pushState(State::Busy, _("Downloading '%1'...").arg(pat.cap(1)));
			QByteArray data = Utils::download(plainurl + urlcode + "/" + pat.cap(1), messages);
			if(!data.isNull()) {
				QFile file(spellingDir.absoluteFilePath(pat.cap(1)));
				if(file.open(QIODevice::WriteOnly)) {
					file.write(data);
					downloaded.append(QString("\n%1").arg(pat.cap(1)));
				}
			}
			popState();
			pos += pat.matchedLength();
		}
		popState();
		if(!downloaded.isEmpty()) {
			QMessageBox::information(this, _("Dictionaries installed"), _("The following dictionary files were installed:%1").arg(downloaded));
			m_recognizer->updateLanguagesMenu();
		} else {
			if(QMessageBox::Help == QMessageBox::critical(this, _("Error"), _("No spelling dictionaries found for '%1'.").arg(code), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)) {
				showHelp("#InstallSpelling");
			}
		}
	}
}

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * MainWindow.cc
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

#include <QDesktopServices>
#include <QDir>
#include <QCloseEvent>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStatusBar>
#include <QtConcurrentRun>
#include <QUrl>

#include "AboutDialog.hh"
#include "MainWindow.hh"
#include "Acquirer.hh"
#include "Config.hh"
#include "Displayer.hh"
#include "OutputManager.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"


#if ENABLE_VERSIONCHECK
#define CHECKURL "http://sourceforge.net/projects/gimagereader/files/LATEST/download?use_mirror=autoselect"
#define DOWNLOADURL "http://sourceforge.net/projects/gimagereader/files"
#define CHANGELOGURL "http://sourceforge.net/projects/gimagereader/files/changelog.txt/download?use_mirror=autoselect"
#endif // ENABLE_VERSIONCHECK

MainWindow* MainWindow::s_instance = nullptr;

MainWindow::MainWindow()
	: m_idleActions(0)
{
	qRegisterMetaType<MainWindow::State>();

	s_instance = this;

	// Setup UI
	ui.setupUi(this);

	m_config = new Config();
	m_acquirer = new Acquirer(ui);
	m_displayer = new Displayer(ui);
	m_outputManager = new OutputManager(ui);
	m_recognizer = new Recognizer();
	m_sourceManager = new SourceManager(ui);

	ui.toolBarMain->insertWidget(ui.actionToggleOutputPane, m_recognizer);
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
	m_idleWidgets.append(m_recognizer);

	connect(ui.actionRedetectLanguages, SIGNAL(triggered()), m_recognizer, SLOT(updateLanguagesMenu()));
	connect(ui.actionPreferences, SIGNAL(triggered()), this, SLOT(showConfig()));
	connect(ui.actionHelp, SIGNAL(triggered()), this, SLOT(showHelp()));
	connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
	connect(ui.actionImageControls, SIGNAL(toggled(bool)), ui.widgetImageControls, SLOT(setVisible(bool)));
	connect(m_displayer, SIGNAL(selectionChanged(bool)), m_recognizer, SLOT(setRecognizeMode(bool)));
	connect(m_recognizer, SIGNAL(languageChanged(Config::Lang)), m_outputManager, SLOT(setLanguage(Config::Lang)));
	connect(m_acquirer, SIGNAL(scanPageAvailable(QString)), m_sourceManager, SLOT(addSource(QString)));
	connect(m_sourceManager, SIGNAL(sourceChanged(Source*)), m_displayer, SLOT(setSource(Source*)));
	connect(m_sourceManager, SIGNAL(sourceChanged(Source*)), this, SLOT(setWindowState(Source*)));

	m_config->addSetting(new VarSetting<QByteArray>("wingeom"));
	m_config->addSetting(new VarSetting<QByteArray>("winstate"));
	m_config->addSetting(new ActionSetting("showcontrols", ui.actionImageControls));

	m_acquirer->start();

	m_recognizer->updateLanguagesMenu();

	pushState(State::Idle, _("Select an image to begin..."));

	restoreGeometry(m_config->getSetting<VarSetting<QByteArray>>("wingeom")->getValue());
	restoreState(m_config->getSetting<VarSetting<QByteArray>>("winstate")->getValue());

	ui.actionSources->trigger();

#if ENABLE_VERSIONCHECK
	if(m_config->getSetting<SwitchSetting>("updatecheck")->getValue()){
		connect(&m_versionCheckWatcher, SIGNAL(finished()), this, SLOT(checkVersion()));
		m_versionCheckWatcher.setFuture(QtConcurrent::run(this, &MainWindow::getNewestVersion));
	}
#endif

//TODO	std::signal(SIGSEGV, crash_handler);
//TODO	std::signal(SIGABRT, crash_handler);
}

MainWindow::~MainWindow()
{
	delete m_acquirer;
	delete m_displayer;
	delete m_outputManager;
	delete m_sourceManager;
	delete m_recognizer;
	delete m_config;
	s_instance = nullptr;
}

void MainWindow::openFiles(const QStringList& files)
{
	m_sourceManager->addSources(files);
}

void MainWindow::pushState(MainWindow::State state, const QString& msg)
{
	m_stateStack.push(QPair<State, QString>(state, msg));
	ui.statusbar->showMessage(msg);
	setState(state);
	if(state == State::Busy){
		QApplication::setOverrideCursor(Qt::WaitCursor);
	}
}

void MainWindow::popState()
{
	if(m_stateStack.top().first == State::Busy){
		QApplication::restoreOverrideCursor();
	}
	m_stateStack.pop();
	const QPair<State, QString>& pair = m_stateStack.top();
	ui.statusbar->showMessage(pair.second);
	setState(pair.first);
}

void MainWindow::setState(State state)
{
	bool isIdle = state == State::Idle;
	m_idleActions.setEnabled(!isIdle);
	for(QWidget* widget : m_idleWidgets)
		widget->setEnabled(!isIdle);
}

void MainWindow::closeEvent(QCloseEvent* ev)
{
	if(!m_outputManager->clearBuffer()){
		ev->ignore();
	}else if(!isMaximized()){
		m_config->getSetting<VarSetting<QByteArray>>("wingeom")->setValue(saveGeometry());
		m_config->getSetting<VarSetting<QByteArray>>("winstate")->setValue(saveState());
	}
}

void MainWindow::setWindowState(Source* source)
{
	if(!source){
		setWindowTitle("gImageReader");
		popState();
	}else{
		setWindowTitle(QString("%1 - gImageReader").arg(source->displayname));
		pushState(State::Normal, _("To recognize specific areas, drag rectangles over them."));
	}
}

void MainWindow::showAbout()
{
	AboutDialog(this).exec();
}

void MainWindow::showHelp(const QString& chapter)
{
	QString manualFile = QDir(QString("%1/../share/gimagereader").arg(QApplication::applicationDirPath())).absoluteFilePath("manual.html");
	QUrl manualUrl = QUrl::fromLocalFile(manualFile);
	manualUrl.setFragment(chapter);
	QDesktopServices::openUrl(manualUrl);
}

void MainWindow::showConfig()
{
	m_config->showDialog();
	m_recognizer->updateLanguagesMenu();
}

MainWindow::Notification MainWindow::addNotification(const QString& title, const QString& message, const QList<NotificationAction> &actions)
{
	QFrame* frame = new QFrame();
	frame->setFrameStyle(QFrame::StyledPanel|QFrame::Raised);
	frame->setStyleSheet("background: #FFD000;");
	QHBoxLayout* layout = new QHBoxLayout(frame);
	layout->addWidget(new QLabel(QString("<b>%1</b>").arg(title), frame));
	layout->addWidget(new QLabel(message, frame), 1);
	for(const NotificationAction& action : actions){
		QToolButton* btn = new QToolButton(frame);
		btn->setText(action.text);
		connect(btn, SIGNAL(clicked()), action.target, action.slot);
		layout->addWidget(btn);
	}
	QToolButton* closeBtn = new QToolButton();
	closeBtn->setIcon(QIcon::fromTheme("dialog-close"));
	connect(closeBtn, SIGNAL(clicked()), frame, SLOT(deleteLater()));
	ui.centralwidget->layout()->addWidget(frame);
	return frame;
}

void MainWindow::hideNotification(Notification notification)
{
	delete static_cast<QFrame*>(notification);
}

#if ENABLE_VERSIONCHECK
QString MainWindow::getNewestVersion()
{
	QNetworkAccessManager networkMgr;
	QNetworkReply* reply = networkMgr.get(QNetworkRequest(QUrl(CHECKURL)));

	QEventLoop loop;
	connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
	loop.exec();

	QString newver = reply->readAll();
	delete reply;

	newver.replace(QRegExp("\\s+"), "");
	QRegExp pat("(^[\\d+\\.]+\\d+?$)");
	if(pat.exactMatch(newver)){
		return newver;
	}
	return QString();
}

void MainWindow::checkVersion()
{
	QString newver = m_versionCheckWatcher.result();
	if(newver.isEmpty()){
		return;
	}
	QString curver = PACKAGE_VERSION;

	if(newver.compare(curver) > 0){
		addNotification(_("New version"), _("gImageReader %1 is available").arg(newver),
			{{_("Download"), this, SLOT(openDownloadUrl()), false},
			 {_("Changelog"), this, SLOT(openChangeLogUrl()), false},
			 {_("Don't notify again"), m_config, SLOT(disableUpdateCheck()), true}});
	}
}

void MainWindow::openDownloadUrl()
{
	QDesktopServices::openUrl(QUrl(DOWNLOADURL));
}

void MainWindow::openChangeLogUrl()
{
	QDesktopServices::openUrl(QUrl(CHANGELOGURL));
}

#endif // ENABLE_VERSIONCHECK

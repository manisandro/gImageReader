/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputManager.cc
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

#include <QDBusInterface>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>

#include "OutputManager.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "SubstitutionsManager.hh"
#include "Utils.hh"


OutputManager::OutputManager(const UI_MainWindow& _ui)
	: ui(_ui)
{
	m_substitutionsManager = new SubstitutionsManager(ui.plainTextEditOutput, ui.checkBoxOutputSearchMatchCase, ui.dockWidgetOutput);

	m_insertMode = InsertMode::Append;

	ui.actionOutputModeAppend->setData(static_cast<int>(InsertMode::Append));
	ui.actionOutputModeCursor->setData(static_cast<int>(InsertMode::Cursor));
	ui.actionOutputModeReplace->setData(static_cast<int>(InsertMode::Replace));
	ui.frameOutputSearch->setVisible(false);

	ui.actionOutputReplace->setShortcut(Qt::CTRL + Qt::Key_F);
	ui.actionOutputSave->setShortcut(Qt::CTRL + Qt::Key_S);

	m_spell.setDecodeLanguageCodes(true);
	m_spell.setShowCheckSpellingCheckbox(true);
	m_spell.setTextEdit(ui.plainTextEditOutput);
	m_spell.setUndoRedoEnabled(true);

	connect(ui.actionToggleOutputPane, SIGNAL(toggled(bool)), ui.dockWidgetOutput, SLOT(setVisible(bool)));
	connect(ui.actionToggleOutputPane, SIGNAL(toggled(bool)), m_substitutionsManager, SLOT(hide()));
	connect(ui.menuOutputMode, SIGNAL(triggered(QAction*)), this, SLOT(setInsertMode(QAction*)));
	connect(ui.toolButtonOutputPostproc, SIGNAL(clicked()), this, SLOT(filterBuffer()));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.frameOutputSearch, SLOT(setVisible(bool)));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.lineEditOutputSearch, SLOT(clear()));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.lineEditOutputReplace, SLOT(clear()));
	connect(ui.actionOutputUndo, SIGNAL(triggered()), &m_spell, SLOT(undo()));
	connect(ui.actionOutputRedo, SIGNAL(triggered()), &m_spell, SLOT(redo()));
	connect(ui.actionOutputSave, SIGNAL(triggered()), this, SLOT(saveBuffer()));
	connect(ui.actionOutputClear, SIGNAL(triggered()), this, SLOT(clearBuffer()));
	connect(&m_spell, SIGNAL(undoAvailable(bool)), ui.actionOutputUndo, SLOT(setEnabled(bool)));
	connect(&m_spell, SIGNAL(redoAvailable(bool)), ui.actionOutputRedo, SLOT(setEnabled(bool)));
	connect(ui.checkBoxOutputSearchMatchCase, SIGNAL(toggled(bool)), this, SLOT(clearErrorState()));
	connect(ui.lineEditOutputSearch, SIGNAL(textChanged(QString)), this, SLOT(clearErrorState()));
	connect(ui.lineEditOutputSearch, SIGNAL(returnPressed()), this, SLOT(findNext()));
	connect(ui.lineEditOutputReplace, SIGNAL(returnPressed()), this, SLOT(replaceNext()));
	connect(ui.toolButtonOutputFindNext, SIGNAL(clicked()), this, SLOT(findNext()));
	connect(ui.toolButtonOutputFindPrev, SIGNAL(clicked()), this, SLOT(findPrev()));
	connect(ui.toolButtonOutputReplace, SIGNAL(clicked()), this, SLOT(replaceNext()));
	connect(ui.toolButtonOutputReplaceAll, SIGNAL(clicked()), this, SLOT(replaceAll()));
	connect(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(ui.pushButtonOutputReplacementList, SIGNAL(clicked()), m_substitutionsManager, SLOT(show()));
	connect(ui.pushButtonOutputReplacementList, SIGNAL(clicked()), m_substitutionsManager, SLOT(raise()));
	connect(ui.actionOutputPostprocDrawWhitespace, SIGNAL(toggled(bool)), ui.plainTextEditOutput, SLOT(setDrawWhitespace(bool)));

	MAIN->getConfig()->addSetting(new ActionSetting("keepdot", ui.actionOutputPostprocKeepDot, true));
	MAIN->getConfig()->addSetting(new ActionSetting("keepquote", ui.actionOutputPostprocKeepQuote));
	MAIN->getConfig()->addSetting(new ActionSetting("joinhyphen", ui.actionOutputPostprocJoinHyphen, true));
	MAIN->getConfig()->addSetting(new ActionSetting("joinspace", ui.actionOutputPostprocCollapseSpaces, true));
	MAIN->getConfig()->addSetting(new ActionSetting("keepparagraphs", ui.actionOutputPostprocKeepParagraphs, true));
	MAIN->getConfig()->addSetting(new ActionSetting("drawwhitespace", ui.actionOutputPostprocDrawWhitespace));
	MAIN->getConfig()->addSetting(new SwitchSetting("searchmatchcase", ui.checkBoxOutputSearchMatchCase));
	MAIN->getConfig()->addSetting(new VarSetting<QString>("outputdir", Utils::documentsFolder()));

	setFont();
}

OutputManager::~OutputManager()
{
	delete m_substitutionsManager;
}

void OutputManager::clearErrorState()
{
	ui.lineEditOutputSearch->setStyleSheet("");
}

void OutputManager::setFont()
{
	if(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont")->getValue()){
		ui.plainTextEditOutput->setFont(QFont());
	}else{
		ui.plainTextEditOutput->setFont(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont")->getValue());
	}
}

void OutputManager::setInsertMode(QAction* action)
{
	m_insertMode = static_cast<InsertMode>(action->data().value<int>());
	ui.toolButtonOutputMode->setIcon(action->icon());
}

void OutputManager::filterBuffer()
{
	QTextCursor cursor = ui.plainTextEditOutput->regionBounds();
	QString txt = cursor.selectedText();

	Utils::busyTask([this,&txt]{
		// Always remove trailing whitespace
		txt.replace(QRegExp("\\s+\u2029"), "\u2029");

		if(ui.actionOutputPostprocJoinHyphen->isChecked()){
			txt.replace(QRegExp("-\\s*\u2029\\s*"), "");
		}
		QString preChars, sucChars;
		if(ui.actionOutputPostprocKeepParagraphs->isChecked()){
			preChars += "\u2029"; // Keep if preceded by line break
		}
		if(ui.actionOutputPostprocKeepDot->isChecked()){
			preChars += "\\."; // Keep if preceded by dot
		}
		if(ui.actionOutputPostprocKeepQuote->isChecked()){
			preChars += "'\""; // Keep if preceded by dot
			sucChars += "'\""; // Keep if succeeded by dot
		}
		if(ui.actionOutputPostprocKeepParagraphs->isChecked()){
			sucChars += "\u2029"; // Keep if succeeded by line break
		}
		if(!preChars.isEmpty()){
			preChars = "([^" + preChars + "])";
		}
		if(!sucChars.isEmpty()){
			sucChars = "(?![" + sucChars + "])";
		}
		QString expr = preChars + "\u2029" + sucChars;
		txt.replace(QRegExp(expr), preChars.isEmpty() ? " " : "\\1 ");

		if(ui.actionOutputPostprocCollapseSpaces->isChecked()){
			txt.replace(QRegExp("[ \t]+"), " ");
		}
		return true;
	}, _("Stripping line breaks..."));
	ui.plainTextEditOutput->setTextCursor(cursor);
	cursor.insertText(txt);
	cursor.setPosition(cursor.position() - txt.length(), QTextCursor::KeepAnchor);
	ui.plainTextEditOutput->setTextCursor(cursor);
}

void OutputManager::findNext()
{
	findReplace(false, false);
}

void OutputManager::findPrev()
{
	findReplace(true, false);
}

void OutputManager::replaceNext()
{
	findReplace(false, true);
}

void OutputManager::replaceAll()
{
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	QTextCursor cursor =  ui.plainTextEditOutput->regionBounds();
	int end = cursor.position();
	QString cursel = cursor.selectedText();
	if(cursor.anchor() == cursor.position() ||
	   cursel == ui.lineEditOutputSearch->text() ||
	   cursel == ui.lineEditOutputReplace->text())
	{
		cursor.movePosition(QTextCursor::Start);
		QTextCursor tmp(cursor);
		tmp.movePosition(QTextCursor::End);
		end = tmp.position();
	}else{
		cursor.setPosition(qMin(cursor.anchor(), cursor.position()));
	}
	QTextDocument::FindFlags flags = 0;
	if(ui.checkBoxOutputSearchMatchCase->isChecked()){
		flags = QTextDocument::FindCaseSensitively;
	}
	QString search = ui.lineEditOutputSearch->text();
	QString replace = ui.lineEditOutputReplace->text();
	int diff = replace.length() - search.length();
	int count = 0;
	while(true){
		cursor = ui.plainTextEditOutput->document()->find(search, cursor, flags);
		if(cursor.isNull() || cursor.position() > end){
			break;
		}
		cursor.insertText(replace);
		end += diff;
		++count;
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}
	if(count == 0){
		ui.lineEditOutputSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
	}
	MAIN->popState();
}

void OutputManager::findReplace(bool backwards, bool replace)
{
	ui.plainTextEditOutput->clearFocus();
	clearErrorState();
	QString findStr = ui.lineEditOutputSearch->text();
	if(findStr.isEmpty()){
		return;
	}

	QTextDocument::FindFlags flags = 0;
	Qt::CaseSensitivity cs = Qt::CaseInsensitive;
	if(backwards){
		flags |= QTextDocument::FindBackward;
	}
	if(ui.checkBoxOutputSearchMatchCase->isChecked()){
		flags |= QTextDocument::FindCaseSensitively;
		cs = Qt::CaseSensitive;
	}

	QTextCursor regionCursor = ui.plainTextEditOutput->regionBounds();

	QTextCursor selCursor = ui.plainTextEditOutput->textCursor();
	if(selCursor.selectedText().compare(findStr, cs) == 0){
		if(replace){
			selCursor.insertText(ui.lineEditOutputReplace->text());
			selCursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, ui.lineEditOutputReplace->text().length());
			ui.plainTextEditOutput->setTextCursor(selCursor);
			ui.plainTextEditOutput->ensureCursorVisible();
			return;
		}
		if(backwards){
			selCursor.setPosition(qMin(selCursor.position(), selCursor.anchor()));
		}else{
			selCursor.setPosition(qMax(selCursor.position(), selCursor.anchor()));
		}
	}

	QTextCursor findcursor = ui.plainTextEditOutput->document()->find(findStr, selCursor, flags);
	if(findcursor.isNull() || !(findcursor.anchor() >= regionCursor.anchor() && findcursor.position() <= regionCursor.position())){
		if(backwards){
			selCursor.setPosition(regionCursor.position());
		}else{
			selCursor.setPosition(regionCursor.anchor());
		}
		findcursor = ui.plainTextEditOutput->document()->find(findStr, selCursor, flags);
		if(findcursor.isNull() || !(findcursor.anchor() >= regionCursor.anchor() && findcursor.position() <= regionCursor.position())){
			ui.lineEditOutputSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
			return;
		}
	}
	ui.plainTextEditOutput->setTextCursor(findcursor);
	ui.plainTextEditOutput->ensureCursorVisible();
}

void OutputManager::addText(const QString& text, bool insert)
{
	if(insert){
		ui.plainTextEditOutput->textCursor().insertText(text);
	}else{
		QTextCursor cursor = ui.plainTextEditOutput->textCursor();
		if(m_insertMode == InsertMode::Append){
			cursor.movePosition(QTextCursor::End);
		}else if(m_insertMode == InsertMode::Cursor){
			// pass
		}else if(m_insertMode == InsertMode::Replace){
			cursor.movePosition(QTextCursor::Start);
			cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		}
		cursor.insertText(text);
		ui.plainTextEditOutput->setTextCursor(cursor);
	}
	ui.dockWidgetOutput->show();
	ui.actionToggleOutputPane->setChecked(true);
}

bool OutputManager::saveBuffer(const QString& filename)
{
	QString outname = filename;
	if(outname.isEmpty()){
		Source* source = MAIN->getSourceManager()->getSelectedSource();
		QString base = source ? QFileInfo(source->displayname).baseName() : _("output");
		outname = QDir(MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->getValue()).absoluteFilePath(base + ".txt");
		outname = QFileDialog::getSaveFileName(MAIN, _("Save Output..."), outname, QString("%1 (*.txt)").arg(_("Text Files")));
		if(outname.isEmpty()){
			return false;
		}
		MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->setValue(QFileInfo(outname).absolutePath());
	}
	QFile file(outname);
	if(!file.open(QIODevice::WriteOnly)){
		QMessageBox::critical(MAIN, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	file.write(ui.plainTextEditOutput->toPlainText().toLocal8Bit());
	ui.plainTextEditOutput->document()->setModified(false);
	return true;
}

bool OutputManager::clearBuffer()
{
	if(!ui.dockWidgetOutput->isVisible()){
		return true;
	}
	if(ui.plainTextEditOutput->document()->isModified()){
		int response = QMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), QMessageBox::Save, QMessageBox::Discard, QMessageBox::Cancel);
		if(response == QMessageBox::Save){
			if(!saveBuffer()){
				return false;
			}
		}else if(response != QMessageBox::Discard){
			return false;
		}
	}
	ui.plainTextEditOutput->clear();
	m_spell.clearUndoRedo();
	ui.plainTextEditOutput->document()->setModified(false);
	ui.actionToggleOutputPane->setChecked(false);
	return true;
}

bool OutputManager::getBufferModified() const
{
	return ui.plainTextEditOutput->document()->isModified();
}

void OutputManager::setLanguage(const Config::Lang& lang, bool force)
{
	MAIN->hideNotification(m_notifierHandle);
	m_notifierHandle = nullptr;
	QString code = lang.code;
	if(!code.isEmpty() || force){
		if(!m_spell.setLanguage(code)) {
			if(MAIN->getConfig()->getSetting<SwitchSetting>("dictinstall")->getValue()){
				MainWindow::NotificationAction actionDontShowAgain = {_("Don't show again"), MAIN->getConfig(), SLOT(disableDictInstall()), true};
				MainWindow::NotificationAction actionInstall = MainWindow::NotificationAction{_("Install"), this, SLOT(dictionaryAutoinstall()), false};
#ifdef Q_OS_LINUX
				// Wake up the daemon?
				QDBusInterface("org.freedesktop.PackageKit", "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit", QDBusConnection::sessionBus(), this).call(QDBus::BlockWithGui, "VersionMajor");
				delete m_dbusIface;
				m_dbusIface = new QDBusInterface("org.freedesktop.PackageKit", "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify", QDBusConnection::sessionBus(), this);
				if(!m_dbusIface->isValid()){
					actionInstall = {_("Help"), MAIN, SLOT(showHelp()), false}; // TODO #InstallSpelling
					qWarning("Could not find PackageKit on DBus, dictionary autoinstallation will not work");
				}
#endif
				MAIN->addNotification(_("Spelling dictionary missing"), _("The spellcheck dictionary for %1 is not installed").arg(lang.name), {actionInstall, actionDontShowAgain}, &m_notifierHandle);
			}
		}
	}
}

void OutputManager::dictionaryAutoinstall()
{
	const QString& code = MAIN->getRecognizer()->getSelectedLanguage().code;
	MAIN->pushState(MainWindow::State::Busy, _("Installing spelling dictionary for '%1'").arg(code));
#ifdef Q_OS_LINUX
	QStringList files = {"/usr/share/myspell/" + code + ".dic", "/usr/share/hunspell/" + code + ".dic"};
	QList<QVariant> params = {QVariant::fromValue((quint32)MAIN->winId()), QVariant::fromValue(files), QVariant::fromValue(QString("always"))};
	m_dbusIface->setTimeout(3600000);
	m_dbusIface->callWithCallback("InstallProvideFiles", params, this, SLOT(dictionaryAutoinstallDone()), SLOT(dictionaryAutoinstallError(QDBusError)));
#else
	QString url = "http://cgit.freedesktop.org/libreoffice/dictionaries/tree/";
	QString plainurl = "http://cgit.freedesktop.org/libreoffice/dictionaries/plain/";
	QString urlcode = code;
	QByteArray html = Utils::download(url);
	if(html.isNull()){
		MAIN->popState();
		if(QMessageBox::Help == QMessageBox::critical(MAIN, _("Error"), _("Could not read %1.").arg(url), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)){
			MAIN->showHelp("#InstallSpelling");
		}
		return;
	}else if(html.indexOf(QString(">%1<").arg(code)) != -1){
		// Ok
	}else if(html.indexOf(QString(">%1<").arg(code.left(2))) != -1){
		urlcode = code.left(2);
	}else{
		MAIN->popState();
		if(QMessageBox::Help == QMessageBox::critical(MAIN, _("Error"), _("No spelling dictionaries found for '%1'.").arg(code), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)){
			MAIN->showHelp("#InstallSpelling");
		}
		return;
	}
	html = Utils::download(url + urlcode + "/");
	if(html.isNull()){
		MAIN->popState();
		if(QMessageBox::Help == QMessageBox::critical(MAIN, _("Error"), _("Could not read %1.").arg(url + urlcode + "/"), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)){
			MAIN->showHelp("#InstallSpelling");
		}
		return;
	}
	QRegExp pat(QString(">(%1[^<]*\\.(dic|aff))<").arg(code.left(2)));
	QString htmls = html;

	QString downloaded;
	int pos = 0;
	while((pos = htmls.indexOf(pat, pos)) != -1){
		MAIN->pushState(MainWindow::State::Busy, _("Downloading '%1'...").arg(pat.cap(1)));
		QByteArray data = Utils::download(plainurl + urlcode + "/" + pat.cap(1));
		if(!data.isNull()){
			QFile file(QString("%1/../share/myspell/dicts/%2").arg(QApplication::applicationDirPath()).arg(pat.cap(1)));
			if(file.open(QIODevice::WriteOnly)){
				file.write(data);
				downloaded.append(QString("\n%1").arg(pat.cap(1)));
			}
		}
		MAIN->popState();
		pos += pat.matchedLength();
	}
	if(!downloaded.isEmpty()){
		dictionaryAutoinstallDone();
		QMessageBox::information(MAIN, _("Dictionaries installed"), _("The following dictionaries were installed:%1").arg(downloaded));
	}else{
		MAIN->popState();
		if(QMessageBox::Help == QMessageBox::critical(MAIN, _("Error"), _("No spelling dictionaries found for '%1'.").arg(code), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok)){
			MAIN->showHelp("#InstallSpelling");
		}
	}
#endif
}

void OutputManager::dictionaryAutoinstallDone()
{
	MAIN->getRecognizer()->updateLanguagesMenu();
	MAIN->popState();
}

void OutputManager::dictionaryAutoinstallError(const QDBusError& error)
{
#ifdef Q_OS_LINUX
	if(QMessageBox::Help == QMessageBox::critical(MAIN, _("Error"), _("Failed to install spelling dictionary: %1").arg(error.message()), QMessageBox::Ok|QMessageBox::Help, QMessageBox::Ok))
	{
		MAIN->showHelp("#InstallSpelling");
	}
	MAIN->popState();
#endif
}

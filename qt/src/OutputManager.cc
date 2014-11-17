/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputManager.cc
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

#include <QDBusInterface>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include "OutputManager.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "SubstitutionsManager.hh"
#include "Utils.hh"


OutputManager::OutputManager(const UI_MainWindow& _ui)
	: ui(_ui)
{
	m_substitutionsManager = new SubstitutionsManager(ui.plainTextEditOutput, ui.checkBoxOutputSearchMatchCase);

	m_insertMode = InsertMode::Append;

	ui.actionOutputModeAppend->setData(static_cast<int>(InsertMode::Append));
	ui.actionOutputModeCursor->setData(static_cast<int>(InsertMode::Cursor));
	ui.actionOutputModeReplace->setData(static_cast<int>(InsertMode::Replace));
	ui.frameOutputSearch->setVisible(false);
	ui.dockWidgetOutput->setVisible(false);
	ui.plainTextEditOutput->setUndoRedoEnabled(true);

	m_spell.setDecodeLanguageCodes(true);
	m_spell.setShowCheckSpellingCheckbox(true);
	m_spell.setTextEdit(ui.plainTextEditOutput);

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
	connect(ui.plainTextEditOutput, SIGNAL(undoAvailable(bool)), ui.actionOutputUndo, SLOT(setEnabled(bool)));
	connect(ui.plainTextEditOutput, SIGNAL(redoAvailable(bool)), ui.actionOutputRedo, SLOT(setEnabled(bool)));
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

	MAIN->getConfig()->addSetting(new ActionSetting("keepdot", ui.actionOutputPostprocKeepDot));
	MAIN->getConfig()->addSetting(new ActionSetting("keepquote", ui.actionOutputPostprocKeepQuote));
	MAIN->getConfig()->addSetting(new ActionSetting("joinhyphen", ui.actionOutputPostprocJoinHyphen));
	MAIN->getConfig()->addSetting(new ActionSetting("joinspace", ui.actionOutputPostprocCollapseSpaces));
	MAIN->getConfig()->addSetting(new SwitchSetting("searchmatchcase", ui.checkBoxOutputSearchMatchCase));
}

void OutputManager::clearErrorState()
{
	static_cast<QLineEdit*>(QObject::sender())->setStyleSheet("");
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
	QTextCursor cursor = ui.plainTextEditOutput->textCursor();
	if(cursor.anchor() == cursor.position()){
		cursor.movePosition(QTextCursor::Start);
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	}
	QString txt = cursor.selectedText();

	Utils::busyTask([this,&txt]{
		if(ui.actionOutputPostprocJoinHyphen->isChecked()){
			txt.replace(QRegExp("-\\s*\u2029\\s*"), "");
		}
		QString expr = "\u2029"; // Keep if preceeded by line break
		if(ui.actionOutputPostprocKeepDot->isChecked()){
			expr += "\\.";
		}
		if(ui.actionOutputPostprocKeepQuote->isChecked()){
			expr += "'\"";
		}
		expr = "([^" + expr + "])\u2029";
		if(ui.actionOutputPostprocKeepQuote->isChecked()){
			expr += "(?!['\"])";
		}
		txt.replace(QRegExp(expr), "\\1 ");

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
	QTextCursor cursor =  ui.plainTextEditOutput->textCursor();
	int end = qMax(cursor.anchor(), cursor.position());
	if(cursor.anchor() == cursor.position()){
		cursor.movePosition(QTextCursor::Start);
		QTextCursor tmp(cursor);
		tmp.movePosition(QTextCursor::End);
		end = tmp.position();
	}else{
		cursor.setPosition(qMin(cursor.anchor(), cursor.position()));
	}
	while(true){
		cursor = ui.plainTextEditOutput->document()->find(ui.lineEditOutputSearch->text(), cursor);
		if(cursor.isNull() || cursor.position() >= end){
			break;
		}
		cursor.insertText(ui.lineEditOutputReplace->text());
	}
}

void OutputManager::findReplace(bool backwards, bool replace)
{
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
	QTextCursor cursor = ui.plainTextEditOutput->textCursor();

	if(cursor.selectedText().compare(findStr, cs) == 0){
		if(replace){
			cursor.beginEditBlock();
			cursor.insertText(ui.lineEditOutputReplace->text());
			cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, ui.lineEditOutputReplace->text().length());
			cursor.endEditBlock();
			ui.plainTextEditOutput->setTextCursor(cursor);
			ui.plainTextEditOutput->ensureCursorVisible();
			return;
		}
		if(backwards){
			cursor.setPosition(qMin(cursor.position(), cursor.anchor()));
		}else{
			cursor.setPosition(qMax(cursor.position(), cursor.anchor()));
		}
	}

	QTextCursor findcursor = ui.plainTextEditOutput->document()->find(findStr, cursor, flags);

	if(findcursor.isNull()){
		if(backwards){
			cursor.movePosition(QTextCursor::End);
		}else{
			cursor.movePosition(QTextCursor::Start);
		}
		findcursor = ui.plainTextEditOutput->document()->find(findStr, cursor, flags);
		if(findcursor.isNull()){
			ui.lineEditOutputSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
			return;
		}
	}
	ui.plainTextEditOutput->setTextCursor(findcursor);
	ui.lineEditOutputSearch->setStyleSheet("");
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
	}
	ui.dockWidgetOutput->show();
}

bool OutputManager::saveBuffer(const QString& filename)
{
	QString outname = filename;
	if(outname.isEmpty()){
		Source* source = MAIN->getSourceManager()->getSelectedSource();
		QString base = source ? source->displayname : _("output");
		outname = QDir(Utils::documentsFolder()).absoluteFilePath(base + ".txt");
		outname = QFileDialog::getSaveFileName(MAIN, _("Save Output..."), filename, QString("%1 (*.txt)").arg(_("Text Files")));
		if(outname.isEmpty()){
			return false;
		}
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
		int response = QMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), QMessageBox::Yes, QMessageBox::No);
		if((response == QMessageBox::Yes && !saveBuffer()) || response != QMessageBox::No){
			return false;
		}
	}
	ui.plainTextEditOutput->clear();
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
				MainWindow::NotificationAction actionInstall = {_("Help"), MAIN, SLOT(showHelp()), false}; // TODO #InstallSpelling
#ifdef Q_OS_LINUX
				// Wake up the daemon?
				QDBusInterface("org.freedesktop.PackageKit", "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit", QDBusConnection::sessionBus(), this).call(QDBus::BlockWithGui, "VersionMajor");
				delete m_dbusIface;
				m_dbusIface = new QDBusInterface("org.freedesktop.PackageKit", "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify", QDBusConnection::sessionBus(), this);
				if(m_dbusIface->isValid()){
					actionInstall = MainWindow::NotificationAction{_("Install"), this, SLOT(dictionaryAutoinstall()), true};
				}else{
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
	QStringList files = {"/usr/share/myspell/" + code + ".dic", "/usr/share/hunspell/" + code + ".dic"};
	QList<QVariant> params = {QVariant::fromValue((quint32)MAIN->winId()), QVariant::fromValue(files), QVariant::fromValue(QString("always"))};
	m_dbusIface->setTimeout(3600000);
	m_dbusIface->callWithCallback("InstallProvideFiles", params, this, SLOT(dictionaryAutoinstallDone()), SLOT(dictionaryAutoinstallError(QDBusError)));
}

void OutputManager::dictionaryAutoinstallDone()
{
	MAIN->getRecognizer()->updateLanguagesMenu();
	MAIN->popState();
}

void OutputManager::dictionaryAutoinstallError(const QDBusError& error)
{
	QMessageBox::critical(MAIN, _("Error"), _("Failed to install spelling dictionary: %1").arg(error.message()));
	MAIN->popState();
}

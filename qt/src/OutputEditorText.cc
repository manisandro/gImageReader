/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.cc
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

#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#include <tesseract/baseapi.h>

#include "OutputEditorText.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "SubstitutionsManager.hh"
#include "Utils.hh"


OutputEditorText::OutputEditorText() {
	m_insertMode = InsertMode::Append;

	m_widget = new QWidget;
	ui.setupUi(m_widget);

	m_substitutionsManager = new SubstitutionsManager(ui.plainTextEditOutput, ui.checkBoxOutputSearchMatchCase, m_widget);

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

	connect(ui.menuOutputMode, SIGNAL(triggered(QAction*)), this, SLOT(setInsertMode(QAction*)));
	connect(ui.toolButtonOutputPostproc, SIGNAL(clicked()), this, SLOT(filterBuffer()));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.frameOutputSearch, SLOT(setVisible(bool)));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.lineEditOutputSearch, SLOT(clear()));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.lineEditOutputReplace, SLOT(clear()));
	connect(ui.actionOutputUndo, SIGNAL(triggered()), &m_spell, SLOT(undo()));
	connect(ui.actionOutputRedo, SIGNAL(triggered()), &m_spell, SLOT(redo()));
	connect(ui.actionOutputSave, SIGNAL(triggered()), this, SLOT(save()));
	connect(ui.actionOutputClear, SIGNAL(triggered()), this, SLOT(clear()));
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

	MAIN->getConfig()->addSetting(new ActionSetting("keepdot", ui.actionOutputPostprocKeepEndMark, true));
	MAIN->getConfig()->addSetting(new ActionSetting("keepquote", ui.actionOutputPostprocKeepQuote));
	MAIN->getConfig()->addSetting(new ActionSetting("joinhyphen", ui.actionOutputPostprocJoinHyphen, true));
	MAIN->getConfig()->addSetting(new ActionSetting("joinspace", ui.actionOutputPostprocCollapseSpaces, true));
	MAIN->getConfig()->addSetting(new ActionSetting("keepparagraphs", ui.actionOutputPostprocKeepParagraphs, true));
	MAIN->getConfig()->addSetting(new ActionSetting("drawwhitespace", ui.actionOutputPostprocDrawWhitespace));
	MAIN->getConfig()->addSetting(new SwitchSetting("searchmatchcase", ui.checkBoxOutputSearchMatchCase));

	setFont();
}

OutputEditorText::~OutputEditorText() {
	delete m_widget;
	MAIN->getConfig()->removeSetting("keepdot");
	MAIN->getConfig()->removeSetting("keepquote");
	MAIN->getConfig()->removeSetting("joinhyphen");
	MAIN->getConfig()->removeSetting("joinspace");
	MAIN->getConfig()->removeSetting("keepparagraphs");
	MAIN->getConfig()->removeSetting("drawwhitespace");
	MAIN->getConfig()->removeSetting("searchmatchcase");
}

void OutputEditorText::clearErrorState() {
	ui.lineEditOutputSearch->setStyleSheet("");
}

void OutputEditorText::setFont() {
	if(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont")->getValue()) {
		ui.plainTextEditOutput->setFont(QFont());
	} else {
		ui.plainTextEditOutput->setFont(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont")->getValue());
	}
}

void OutputEditorText::setInsertMode(QAction* action) {
	m_insertMode = static_cast<InsertMode>(action->data().value<int>());
	ui.toolButtonOutputMode->setIcon(action->icon());
}

void OutputEditorText::filterBuffer() {
	QTextCursor cursor = ui.plainTextEditOutput->regionBounds();
	QString txt = cursor.selectedText();

	Utils::busyTask([this,&txt] {
		// Always remove trailing whitespace
		txt.replace(QRegExp("\\s+$"), "");

		if(ui.actionOutputPostprocJoinHyphen->isChecked()) {
			txt.replace(QRegExp("[-\u2014]\\s*\u2029\\s*"), "");
		}
		QString preChars, sucChars;
		if(ui.actionOutputPostprocKeepParagraphs->isChecked()) {
			preChars += "\u2029"; // Keep if preceded by line break
		}
		if(ui.actionOutputPostprocKeepEndMark->isChecked()) {
			preChars += "\\.\\?!"; // Keep if preceded by end mark (.?!)
		}
		if(ui.actionOutputPostprocKeepQuote->isChecked()) {
			preChars += "'\"\u00BB\u00AB"; // Keep if preceded by quote
			sucChars += "'\"\u00AB\u00BB"; // Keep if succeeded by quote
		}
		if(ui.actionOutputPostprocKeepParagraphs->isChecked()) {
			sucChars += "\u2029"; // Keep if succeeded by line break
		}
		if(!preChars.isEmpty()) {
			preChars = "([^" + preChars + "])";
		}
		if(!sucChars.isEmpty()) {
			sucChars = "(?![" + sucChars + "])";
		}
		QString expr = preChars + "\u2029" + sucChars;
		txt.replace(QRegExp(expr), preChars.isEmpty() ? " " : "\\1 ");

		if(ui.actionOutputPostprocCollapseSpaces->isChecked()) {
			txt.replace(QRegExp("[ \t]+"), " ");
		}
		return true;
	}, _("Stripping line breaks..."));
	ui.plainTextEditOutput->setTextCursor(cursor);
	cursor.insertText(txt);
	cursor.setPosition(cursor.position() - txt.length(), QTextCursor::KeepAnchor);
	ui.plainTextEditOutput->setTextCursor(cursor);
}

void OutputEditorText::findNext() {
	findReplace(false, false);
}

void OutputEditorText::findPrev() {
	findReplace(true, false);
}

void OutputEditorText::replaceNext() {
	findReplace(false, true);
}

void OutputEditorText::replaceAll() {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	QString searchstr = ui.lineEditOutputSearch->text();
	QString replacestr = ui.lineEditOutputReplace->text();
	if(!ui.plainTextEditOutput->replaceAll(searchstr, replacestr, ui.checkBoxOutputSearchMatchCase->isChecked())) {
		ui.lineEditOutputSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
	}
	MAIN->popState();
}

void OutputEditorText::findReplace(bool backwards, bool replace) {
	clearErrorState();
	QString searchstr = ui.lineEditOutputSearch->text();
	QString replacestr = ui.lineEditOutputReplace->text();
	if(!ui.plainTextEditOutput->findReplace(backwards, replace, ui.checkBoxOutputSearchMatchCase->isChecked(), searchstr, replacestr)) {
		ui.lineEditOutputSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
	}
}

void OutputEditorText::read(tesseract::TessBaseAPI &tess, ReadSessionData *data) {
	char* text = tess.GetUTF8Text();
	bool& insertText = static_cast<TextReadSessionData*>(data)->insertText;
	QMetaObject::invokeMethod(this, "addText", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(text)), Q_ARG(bool, insertText));
	delete[] text;
	insertText = true;
}

void OutputEditorText::readError(const QString &errorMsg, ReadSessionData *data) {
	bool& insertText = static_cast<TextReadSessionData*>(data)->insertText;
	QMetaObject::invokeMethod(this, "addText", Qt::QueuedConnection, Q_ARG(QString, errorMsg), Q_ARG(bool, insertText));
	insertText = true;
}

void OutputEditorText::addText(const QString& text, bool insert) {
	if(insert) {
		ui.plainTextEditOutput->textCursor().insertText(text);
	} else {
		QTextCursor cursor = ui.plainTextEditOutput->textCursor();
		if(m_insertMode == InsertMode::Append) {
			cursor.movePosition(QTextCursor::End);
		} else if(m_insertMode == InsertMode::Cursor) {
			// pass
		} else if(m_insertMode == InsertMode::Replace) {
			cursor.movePosition(QTextCursor::Start);
			cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		}
		cursor.insertText(text);
		ui.plainTextEditOutput->setTextCursor(cursor);
	}
	MAIN->setOutputPaneVisible(true);
}

bool OutputEditorText::save(const QString& filename) {
	QString outname = filename;
	if(outname.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		QString base = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
		outname = QDir(MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->getValue()).absoluteFilePath(base + ".txt");
		outname = QFileDialog::getSaveFileName(MAIN, _("Save Output..."), outname, QString("%1 (*.txt)").arg(_("Text Files")));
		if(outname.isEmpty()) {
			return false;
		}
		MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->setValue(QFileInfo(outname).absolutePath());
	}
	QFile file(outname);
	if(!file.open(QIODevice::WriteOnly)) {
		QMessageBox::critical(MAIN, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	file.write(MAIN->getConfig()->useUtf8() ? ui.plainTextEditOutput->toPlainText().toUtf8() : ui.plainTextEditOutput->toPlainText().toLocal8Bit());
	ui.plainTextEditOutput->document()->setModified(false);
	return true;
}

bool OutputEditorText::clear(bool hide) {
	if(!m_widget->isVisible()) {
		return true;
	}
	if(getModified()) {
		int response = QMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), QMessageBox::Save, QMessageBox::Discard, QMessageBox::Cancel);
		if(response == QMessageBox::Save) {
			if(!save()) {
				return false;
			}
		} else if(response != QMessageBox::Discard) {
			return false;
		}
	}
	ui.plainTextEditOutput->clear();
	m_spell.clearUndoRedo();
	ui.plainTextEditOutput->document()->setModified(false);
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

bool OutputEditorText::getModified() const {
	return ui.plainTextEditOutput->document()->isModified();
}

void OutputEditorText::onVisibilityChanged(bool /*visibile*/) {
	m_substitutionsManager->hide();
}

void OutputEditorText::setLanguage(const Config::Lang& lang) {
	m_spell.setLanguage(lang.code.isEmpty() ? Utils::getSpellingLanguage() : lang.code);
}

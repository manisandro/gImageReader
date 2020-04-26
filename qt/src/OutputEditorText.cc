/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.cc
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

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "OutputEditorText.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"


void OutputEditorText::TextBatchProcessor::appendOutput(QIODevice* dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo, bool firstArea) const {
	char* text = tess->GetUTF8Text();
	if(firstArea && m_prependPage) {
		dev->write(_("Page: %1\n").arg(pageInfo.page).toUtf8());
	}
	dev->write(text);
	dev->write("\n");
	delete[] text;
}


OutputEditorText::OutputEditorText() {
	m_insertMode = InsertMode::Append;

	m_widget = new QWidget;
	ui.setupUi(m_widget);

	ui.actionOutputModeAppend->setData(static_cast<int>(InsertMode::Append));
	ui.actionOutputModeCursor->setData(static_cast<int>(InsertMode::Cursor));
	ui.actionOutputModeReplace->setData(static_cast<int>(InsertMode::Replace));

	ui.actionOutputReplace->setShortcut(Qt::CTRL + Qt::Key_F);
	ui.actionOutputSave->setShortcut(Qt::CTRL + Qt::Key_S);

	m_spell.setDecodeLanguageCodes(true);
	m_spell.setShowCheckSpellingCheckbox(true);
	m_spell.setTextEdit(ui.plainTextEditOutput);
	m_spell.setUndoRedoEnabled(true);

	connect(ui.menuOutputMode, &QMenu::triggered, this, &OutputEditorText::setInsertMode);
	connect(ui.toolButtonOutputPostproc, &QToolButton::clicked, this, &OutputEditorText::filterBuffer);
	connect(ui.actionOutputReplace, &QAction::toggled, ui.searchFrame, &SearchReplaceFrame::setVisible);
	connect(ui.actionOutputReplace, &QAction::toggled, ui.searchFrame, &SearchReplaceFrame::clear);
	connect(ui.actionOutputUndo, &QAction::triggered, &m_spell, &QtSpell::TextEditChecker::undo);
	connect(ui.actionOutputRedo, &QAction::triggered, &m_spell, &QtSpell::TextEditChecker::redo);
	connect(ui.actionOutputSave, &QAction::triggered, this, [this] { save(); });
	connect(ui.actionOutputClear, &QAction::triggered, this, &OutputEditorText::clear);
	connect(ui.searchFrame, &SearchReplaceFrame::findReplace, this, &OutputEditorText::findReplace);
	connect(ui.searchFrame, &SearchReplaceFrame::replaceAll, this, &OutputEditorText::replaceAll);
	connect(ui.searchFrame, &SearchReplaceFrame::applySubstitutions, this, &OutputEditorText::applySubstitutions);
	connect(&m_spell, &QtSpell::TextEditChecker::undoAvailable, ui.actionOutputUndo, &QAction::setEnabled);
	connect(&m_spell, &QtSpell::TextEditChecker::redoAvailable, ui.actionOutputRedo, &QAction::setEnabled);
	connect(ConfigSettings::get<FontSetting>("customoutputfont"), &FontSetting::changed, this, &OutputEditorText::setFont);
	connect(ConfigSettings::get<SwitchSetting>("systemoutputfont"), &SwitchSetting::changed, this, &OutputEditorText::setFont);
	connect(ui.actionOutputPostprocDrawWhitespace, &QAction::toggled, ui.plainTextEditOutput, &OutputTextEdit::setDrawWhitespace);

	ADD_SETTING(ActionSetting("keepdot", ui.actionOutputPostprocKeepEndMark, true));
	ADD_SETTING(ActionSetting("keepquote", ui.actionOutputPostprocKeepQuote));
	ADD_SETTING(ActionSetting("joinhyphen", ui.actionOutputPostprocJoinHyphen, true));
	ADD_SETTING(ActionSetting("joinspace", ui.actionOutputPostprocCollapseSpaces, true));
	ADD_SETTING(ActionSetting("keepparagraphs", ui.actionOutputPostprocKeepParagraphs, true));
	ADD_SETTING(ActionSetting("drawwhitespace", ui.actionOutputPostprocDrawWhitespace));

	setFont();
}

OutputEditorText::~OutputEditorText() {
	delete m_widget;
}

void OutputEditorText::setFont() {
	if(ConfigSettings::get<SwitchSetting>("systemoutputfont")->getValue()) {
		ui.plainTextEditOutput->setFont(QFont());
	} else {
		ui.plainTextEditOutput->setFont(ConfigSettings::get<FontSetting>("customoutputfont")->getValue());
	}
}

void OutputEditorText::setInsertMode(QAction* action) {
	m_insertMode = static_cast<InsertMode>(action->data().value<int>());
	ui.toolButtonOutputMode->setIcon(action->icon());
}

void OutputEditorText::filterBuffer() {
	QTextCursor cursor = ui.plainTextEditOutput->regionBounds();
	QString txt = cursor.selectedText();

	Utils::busyTask([this, &txt] {
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

void OutputEditorText::findReplace(const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace) {
	ui.searchFrame->clearErrorState();
	if(!ui.plainTextEditOutput->findReplace(backwards, replace, matchCase, searchstr, replacestr)) {
		ui.searchFrame->setErrorState();
	}
}

void OutputEditorText::replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	if(!ui.plainTextEditOutput->replaceAll(searchstr, replacestr, matchCase)) {
		ui.searchFrame->setErrorState();
	}
	MAIN->popState();
}

void OutputEditorText::applySubstitutions(const QMap<QString, QString>& substitutions, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	QTextCursor cursor = ui.plainTextEditOutput->regionBounds();
	int end = cursor.position();
	cursor.setPosition(cursor.anchor());
	QTextDocument::FindFlags flags = 0;
	if(matchCase) {
		flags = QTextDocument::FindCaseSensitively;
	}
	int start = cursor.position();
	for(auto it = substitutions.begin(), itEnd = substitutions.end(); it != itEnd; ++it) {
		QString search = it.key();
		QString replace = it.value();
		int diff = replace.length() - search.length();
		cursor.setPosition(start);
		while(true) {
			cursor = ui.plainTextEditOutput->document()->find(search, cursor, flags);
			if(cursor.isNull() || std::max(cursor.anchor(), cursor.position()) > end) {
				break;
			}
			cursor.insertText(replace);
			end += diff;
		}
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}
	MAIN->popState();
}

void OutputEditorText::read(tesseract::TessBaseAPI& tess, ReadSessionData* data) {
	char* textbuf = tess.GetUTF8Text();
	QString text = QString::fromUtf8(textbuf);
	if(!text.endsWith('\n')) {
		text.append('\n');
	}
	if(data->prependFile || data->prependPage) {
		QStringList prepend;
		if(data->prependFile) {
			prepend.append(_("File: %1").arg(data->pageInfo.filename));
		}
		if(data->prependPage) {
			prepend.append(_("Page: %1").arg(data->pageInfo.page));
		}
		text.prepend(QString("[%1]\n").arg(prepend.join("; ")));
	}
	bool& insertText = static_cast<TextReadSessionData*>(data)->insertText;
	QMetaObject::invokeMethod(this, "addText", Qt::QueuedConnection, Q_ARG(QString, text), Q_ARG(bool, insertText));
	delete[] textbuf;
	insertText = true;
}

void OutputEditorText::readError(const QString& errorMsg, ReadSessionData* data) {
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

bool OutputEditorText::open(const QString& filename) {
	if(!clear(false)) {
		return false;
	}
	QFile file(filename);
	if(file.open(QIODevice::ReadOnly)) {
		ui.plainTextEditOutput->setPlainText(file.readAll());
		MAIN->setOutputPaneVisible(true);
		return true;
	} else {
		QString errorMsg(_("The following files could not be opened:\n%1").arg(filename));
		QMessageBox::critical(MAIN, _("Unable to open files"), errorMsg);
		return false;
	}
}

bool OutputEditorText::save(const QString& filename) {
	QString outname = filename;
	if(outname.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		QString suggestion;
		if(!sources.isEmpty()) {
			QFileInfo finfo(sources.first()->path);
			suggestion = finfo.absoluteDir().absoluteFilePath(finfo.completeBaseName());
		} else {
			suggestion = _("output");
		}
		outname = FileDialogs::saveDialog(_("Save Output..."), suggestion + ".txt", "outputdir", QString("%1 (*.txt)").arg(_("Text Files")));
		if(outname.isEmpty()) {
			return false;
		}
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
	if(hide) {
		MAIN->setOutputPaneVisible(false);
	}
	return true;
}

bool OutputEditorText::getModified() const {
	return ui.plainTextEditOutput->document()->isModified();
}

void OutputEditorText::onVisibilityChanged(bool /*visible*/) {
	ui.searchFrame->hideSubstitutionsManager();
}

void OutputEditorText::setLanguage(const Config::Lang& lang) {
	m_spell.setLanguage(lang.code.isEmpty() ? Utils::getSpellingLanguage(lang.prefix) : lang.code);
}

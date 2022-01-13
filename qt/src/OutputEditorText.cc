/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.cc
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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

	m_recentMenu = new QMenu();
	ui.toolButtonOpen->setMenu(m_recentMenu);

	ui.actionOutputModeAppend->setData(static_cast<int>(InsertMode::Append));
	ui.actionOutputModeCursor->setData(static_cast<int>(InsertMode::Cursor));
	ui.actionOutputModeReplace->setData(static_cast<int>(InsertMode::Replace));

	ui.actionOutputReplace->setShortcut(Qt::CTRL + Qt::Key_F);
	ui.actionOutputSave->setShortcut(Qt::CTRL + Qt::Key_S);

	m_spell.setDecodeLanguageCodes(true);
	m_spell.setShowCheckSpellingCheckbox(true);
	m_spell.setUndoRedoEnabled(true);

	connect(ui.toolButtonOpen, &QToolButton::clicked, [this] { open(); });
	connect(m_recentMenu, &QMenu::aboutToShow, this, &OutputEditorText::prepareRecentMenu);
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
	connect(ui.actionOutputPostprocDrawWhitespace, &QAction::toggled, [this] (bool active) { textEdit()->setDrawWhitespace(active); });
	connect(ui.tabWidget, &QTabWidget::currentChanged, this, &OutputEditorText::tabChanged);
	connect(ui.tabWidget, &QTabWidget::tabCloseRequested, this, &OutputEditorText::closeTab);
	connect(ui.toolButtonAddTab, &QToolButton::clicked, this, [this] { addTab(); });

	addTab();

	ADD_SETTING(ActionSetting("keepdot", ui.actionOutputPostprocKeepEndMark, true));
	ADD_SETTING(ActionSetting("keepquote", ui.actionOutputPostprocKeepQuote));
	ADD_SETTING(ActionSetting("joinhyphen", ui.actionOutputPostprocJoinHyphen, true));
	ADD_SETTING(ActionSetting("joinspace", ui.actionOutputPostprocCollapseSpaces, true));
	ADD_SETTING(ActionSetting("keepparagraphs", ui.actionOutputPostprocKeepParagraphs, true));
	ADD_SETTING(ActionSetting("drawwhitespace", ui.actionOutputPostprocDrawWhitespace));
	ADD_SETTING(VarSetting<QStringList>("recenttxtitems"));

	setFont();
}

OutputEditorText::~OutputEditorText() {
	delete m_widget;
}

int OutputEditorText::addTab(const QString& title) {
	OutputTextEdit* textEdit = new OutputTextEdit;

	if(ConfigSettings::get<SwitchSetting>("systemoutputfont")->getValue()) {
		textEdit->setFont(QFont());
	} else {
		textEdit->setFont(ConfigSettings::get<FontSetting>("customoutputfont")->getValue());
	}
	textEdit->setWordWrapMode(QTextOption::WordWrap);

	QString tabLabel = title.isEmpty() ? QString("Untitled %1").arg(++m_tabCounter) :  title;
	int page = ui.tabWidget->addTab(textEdit, tabLabel);

	auto documentChanged = [this, textEdit, page] {
		QString text = ui.tabWidget->tabText(page);
		if (textEdit->document()->isModified()) {
			ui.tabWidget->setTabText(page, text.endsWith('*') ? text : text + "*");
		} else {
			ui.tabWidget->setTabText(page, text.endsWith('*') ? text.left(text.length() - 1) : text);
		}
	};
	connect(textEdit->document(), &QTextDocument::modificationChanged, documentChanged);
	connect(textEdit->document(), &QTextDocument::contentsChanged, documentChanged);
	ui.tabWidget->setCurrentIndex(page);
	return page;
}

void OutputEditorText::tabChanged(int) {
	if (ui.tabWidget->currentIndex() >= 0) {
		OutputTextEdit*  edit = textEdit();
		bool isModified = edit->document()->isModified();
		m_spell.setTextEdit(edit);
		edit->document()->setModified(isModified);
		// FIXME: setModified above seems to not trigger the modificationChanged signal, manually force the correct tab title text
		QString text = ui.tabWidget->tabText(ui.tabWidget->currentIndex());
		if (isModified && !text.endsWith("*")) {
			ui.tabWidget->setTabText(ui.tabWidget->currentIndex(), text + "*");
		} else if (!isModified && text.endsWith("*")) {
			ui.tabWidget->setTabText(ui.tabWidget->currentIndex(), text.left(text.length() - 1));
		}
		edit->setDrawWhitespace(ui.actionOutputPostprocDrawWhitespace->isChecked());
		edit->setFocus();
	}
}

void OutputEditorText::closeTab(int page) {
	if(textEdit(page)->document()->isModified()) {
		int response = QMessageBox::question(MAIN, _("Document not saved"), _("Save document before proceeding?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		if (response == QMessageBox::Cancel) {
			return;
		}
		if (response == QMessageBox::Save && !save(page, textEdit(page)->filename())) {
			return;
		}
	}
	ui.tabWidget->removeTab(page);
	if (ui.tabWidget->count() == 0) {
		m_tabCounter = 0;
		addTab();
	}
}

QString OutputEditorText::tabName(int page) const {
	QString title = ui.tabWidget->tabText(page);
	return title.endsWith("*") ? title.left(title.length() - 1) : title;
}

void OutputEditorText::setTabName(int page, const QString& title) {
	ui.tabWidget->setTabText(page, title);
}

OutputTextEdit* OutputEditorText::textEdit(int page) const {
	page = page == -1 ? ui.tabWidget->currentIndex() : page;
	return qobject_cast<OutputTextEdit*>(ui.tabWidget->widget(page));
}

void OutputEditorText::setFont() {
	QFont font;
	if(!ConfigSettings::get<SwitchSetting>("systemoutputfont")->getValue()) {
		QFont font = ConfigSettings::get<FontSetting>("customoutputfont")->getValue();
	}
	for(int i = 0, n = ui.tabWidget->count(); i < n; ++i) {
		textEdit(i)->setFont(font);
	}
}

void OutputEditorText::setInsertMode(QAction* action) {
	m_insertMode = static_cast<InsertMode>(action->data().value<int>());
	ui.toolButtonOutputMode->setIcon(action->icon());
}

void OutputEditorText::filterBuffer() {
	QTextCursor cursor = textEdit()->regionBounds();
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
	textEdit()->setTextCursor(cursor);
	cursor.insertText(txt);
	cursor.setPosition(cursor.position() - txt.length(), QTextCursor::KeepAnchor);
	textEdit()->setTextCursor(cursor);
}

void OutputEditorText::findReplace(const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace) {
	ui.searchFrame->clearErrorState();
	if(!textEdit()->findReplace(backwards, replace, matchCase, searchstr, replacestr)) {
		ui.searchFrame->setErrorState();
	}
}

void OutputEditorText::replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	if(!textEdit()->replaceAll(searchstr, replacestr, matchCase)) {
		ui.searchFrame->setErrorState();
	}
	MAIN->popState();
}

void OutputEditorText::applySubstitutions(const QMap<QString, QString>& substitutions, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	QTextCursor cursor = textEdit()->regionBounds();
	int end = cursor.position();
	cursor.setPosition(cursor.anchor());
	QTextDocument::FindFlags flags;
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
			cursor = textEdit()->document()->find(search, cursor, flags);
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
		textEdit()->textCursor().insertText(text);
	} else {
		QTextCursor cursor = textEdit()->textCursor();
		if(m_insertMode == InsertMode::Append) {
			cursor.movePosition(QTextCursor::End);
		} else if(m_insertMode == InsertMode::Cursor) {
			// pass
		} else if(m_insertMode == InsertMode::Replace) {
			cursor.movePosition(QTextCursor::Start);
			cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		}
		cursor.insertText(text);
		textEdit()->setTextCursor(cursor);
	}
	MAIN->setOutputPaneVisible(true);
}

bool OutputEditorText::open(const QString& filename) {
	QStringList files;
	if (filename.isEmpty()) {
		files = FileDialogs::openDialog(_("Select Files"), "", "outputdir", QString("%1 (*.txt)").arg(_("Text Files")), true, MAIN);
		if (files.isEmpty()) {
			return false;
		}
	} else {
		files.append(filename);
	}

	int currentPage = -1;
	QStringList failed;
	QStringList recentItems = ConfigSettings::get<VarSetting<QStringList>>("recenttxtitems")->getValue();
	for(const QString& file : files) {
		QFile fh(file);
		if(!fh.open(QIODevice::ReadOnly)) {
			failed.append(file);
			continue;
		}
		recentItems.removeAll(file);
		recentItems.prepend(file);
		// Look if document already opened, if so, switch to that tab
		bool alreadyOpen = false;
		for(int i = 0, n = ui.tabWidget->count(); i < n; ++i) {
			if (textEdit(i)->filename() == file) {
				currentPage = i;
				alreadyOpen = true;
				break;
			}
		}
		if (!alreadyOpen) {
			OutputTextEdit* edit = textEdit();
			int page = ui.tabWidget->currentIndex();
			// Only add new tab if current tab is not empty
			if (edit->document()->isModified() == true || !edit->filename().isEmpty()) {
				page = addTab(QFileInfo(file).fileName());
				edit = textEdit(page);
			} else {
				setTabName(page, QFileInfo(file).fileName());
			}
			edit->setPlainText(fh.readAll());
			edit->document()->setModified(false);
			edit->document()->clearUndoRedoStacks();
			edit->setFilename(file);
			currentPage = page;
		}
	}

	if (currentPage >= 0) {
		ui.tabWidget->setCurrentIndex(currentPage);
		m_spell.clearUndoRedo();
		m_spell.checkSpelling();
	}
	if (!failed.empty()) {
		QMessageBox::critical(MAIN, _("Unable to open files"), _("The following files could not be opened:\n%1").arg(failed.join("\n")));
	}
	ConfigSettings::get<VarSetting<QStringList>>("recenttxtitems")->setValue(recentItems.mid(0, sMaxNumRecent));

	MAIN->setOutputPaneVisible(true);
	return failed.empty();
}

bool OutputEditorText::save(int page, const QString& filename) {
	QString outname = filename;
	page = page == -1 ? ui.tabWidget->currentIndex() : page;
	OutputTextEdit* edit = textEdit(page);

	if(outname.isEmpty()) {
		QString suggestion = edit->filename();
		suggestion = suggestion.isEmpty() ? tabName(page) + ".txt" : suggestion;
		outname = FileDialogs::saveDialog(_("Save Output..."), suggestion, "outputdir", QString("%1 (*.txt)").arg(_("Text Files")));
		if(outname.isEmpty()) {
			return false;
		}
	}
	QFile file(outname);
	if(!file.open(QIODevice::WriteOnly)) {
		QMessageBox::critical(MAIN, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	file.write(MAIN->getConfig()->useUtf8() ? edit->toPlainText().toUtf8() : edit->toPlainText().toLocal8Bit());
	edit->document()->setModified(false);
	edit->setFilename(outname);
	setTabName(page, QFileInfo(outname).fileName());
	QStringList recentItems = ConfigSettings::get<VarSetting<QStringList>>("recenttxtitems")->getValue();
	recentItems.removeAll(outname);
	recentItems.prepend(outname);
	ConfigSettings::get<VarSetting<QStringList>>("recenttxtitems")->setValue(recentItems.mid(0, sMaxNumRecent));
	return true;
}

bool OutputEditorText::crashSave(const QString& filename) const {
	QFile file(filename);
	if(file.open(QIODevice::WriteOnly)) {
		file.write(MAIN->getConfig()->useUtf8() ? textEdit()->toPlainText().toUtf8() : textEdit()->toPlainText().toLocal8Bit());
		return true;
	}
	return false;
}

bool OutputEditorText::clear(bool hide) {
	if(!m_widget->isVisible()) {
		return true;
	}
	QMap<int, bool> changed;
	for(int i = 0, n = ui.tabWidget->count(); i < n; ++i) {
		if (textEdit(i)->document()->isModified()) {
			changed.insert(i, true);
		}
	}

	if(!changed.isEmpty()) {
		QListWidget* list = new QListWidget();;
		for(auto it = changed.begin(), itEnd = changed.end(); it != itEnd; ++it) {
			QListWidgetItem* item = new QListWidgetItem(tabName(it.key()));
			item->setCheckState(Qt::Checked);
			item->setData(Qt::UserRole, it.key());
			list->addItem(item);
		}
		QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel);
		QDialog dialog;
		dialog.setLayout(new QVBoxLayout);
		dialog.layout()->addWidget(new QLabel(_("The following documents have unsaved changes. Which documents do you want to save before proceeding?")));
		dialog.layout()->addWidget(list);
		dialog.layout()->addWidget(bbox);

		connect(list, &QListWidget::itemChanged, [&](QListWidgetItem * item) { changed[item->data(Qt::UserRole).toInt()] = item->checkState() == Qt::Checked; });
		connect(bbox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(bbox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

		int response = QDialogButtonBox::Cancel;
		connect(bbox, &QDialogButtonBox::clicked, [&](QAbstractButton * button) { response = bbox->standardButton(button); dialog.accept(); });

		dialog.exec();
		if (response == QDialogButtonBox::Cancel) {
			return false;
		}
		if (response == QDialogButtonBox::Save) {
			for(auto it = changed.begin(), itEnd = changed.end(); it != itEnd; ++it) {
				if(it.value() && !save(it.key(), textEdit(it.key())->filename())) {
					return false;
				}
			}
		}
	}
	for(int i = ui.tabWidget->count(); i >= 0; --i) {
		ui.tabWidget->removeTab(i);
	}
	m_tabCounter = 0;
	addTab(); // Add one empty tab
	if(hide) {
		MAIN->setOutputPaneVisible(false);
	}
	return true;
}

void OutputEditorText::onVisibilityChanged(bool /*visible*/) {
	ui.searchFrame->hideSubstitutionsManager();
}

void OutputEditorText::setLanguage(const Config::Lang& lang) {
	m_spell.setLanguage(lang.code.isEmpty() ? Utils::getSpellingLanguage(lang.prefix) : lang.code);
}

void OutputEditorText::prepareRecentMenu() {
	// Build recent menu
	m_recentMenu->clear();
	int count = 0;
	for(const QString& filename : ConfigSettings::get<VarSetting<QStringList>>("recenttxtitems")->getValue()) {
		if(QFile(filename).exists()) {
			QAction* action = new QAction(QFileInfo(filename).fileName(), m_recentMenu);
			action->setToolTip(filename);
			connect(action, &QAction::triggered, this, [this, action] { open(action->toolTip()); });
			m_recentMenu->addAction(action);
			if(++count >= sMaxNumRecent) {
				break;
			}
		}
	}
	if(m_recentMenu->isEmpty()) {
		m_recentMenu->addAction(_("No recent files"))->setEnabled(false);
	}
}

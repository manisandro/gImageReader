/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
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

#include <QDir>
#include <QMessageBox>
#include <QFileInfo>
#include <QFileDialog>
#include <tesseract/baseapi.h>

#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"


OutputEditorHOCR::OutputEditorHOCR()
{
	m_widget = new QWidget;
	ui.setupUi(m_widget);
	ui.frameOutputSearch->setVisible(false);

	ui.actionOutputReplace->setShortcut(Qt::CTRL + Qt::Key_F);
	ui.actionOutputSave->setShortcut(Qt::CTRL + Qt::Key_S);

	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.frameOutputSearch, SLOT(setVisible(bool)));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.lineEditOutputSearch, SLOT(clear()));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.lineEditOutputReplace, SLOT(clear()));
	connect(ui.actionOutputSave, SIGNAL(triggered()), this, SLOT(save()));
	connect(ui.actionOutputClear, SIGNAL(triggered()), this, SLOT(clear()));
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

	MAIN->getConfig()->addSetting(new SwitchSetting("searchmatchcase", ui.checkBoxOutputSearchMatchCase));
	MAIN->getConfig()->addSetting(new VarSetting<QString>("outputdir", Utils::documentsFolder()));

	setFont();
}

OutputEditorHOCR::~OutputEditorHOCR()
{
	delete m_widget;
}

void OutputEditorHOCR::clearErrorState()
{
	ui.lineEditOutputSearch->setStyleSheet("");
}

void OutputEditorHOCR::setFont()
{
	if(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont")->getValue()){
		ui.plainTextEditOutput->setFont(QFont());
	}else{
		ui.plainTextEditOutput->setFont(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont")->getValue());
	}
}

void OutputEditorHOCR::findNext()
{
	findReplace(false, false);
}

void OutputEditorHOCR::findPrev()
{
	findReplace(true, false);
}

void OutputEditorHOCR::replaceNext()
{
	findReplace(false, true);
}

void OutputEditorHOCR::replaceAll()
{
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	QString searchstr = ui.lineEditOutputSearch->text();
	QString replacestr = ui.lineEditOutputReplace->text();
	if(!ui.plainTextEditOutput->replaceAll(searchstr, replacestr, ui.checkBoxOutputSearchMatchCase->isChecked())){
		ui.lineEditOutputSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
	}
	MAIN->popState();
}

void OutputEditorHOCR::findReplace(bool backwards, bool replace)
{
	clearErrorState();
	QString searchstr = ui.lineEditOutputSearch->text();
	QString replacestr = ui.lineEditOutputReplace->text();
	if(!ui.plainTextEditOutput->findReplace(backwards, replace, ui.checkBoxOutputSearchMatchCase->isChecked(), searchstr, replacestr)){
		ui.lineEditOutputSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
	}
}

void OutputEditorHOCR::read(tesseract::TessBaseAPI &tess, ReadSessionData *data)
{
	char* text = tess.GetHOCRText(data->currentPage);
	QMetaObject::invokeMethod(this, "addText", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(text)));
	delete[] text;
}

void OutputEditorHOCR::readError(const QString &errorMsg, ReadSessionData */*data*/)
{
	QMetaObject::invokeMethod(this, "addText", Qt::QueuedConnection, Q_ARG(QString, errorMsg));
}

void OutputEditorHOCR::addText(const QString& text)
{
	QTextCursor cursor = ui.plainTextEditOutput->textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.insertText(text);
	ui.plainTextEditOutput->setTextCursor(cursor);
	MAIN->setOutputPaneVisible(true);
}

bool OutputEditorHOCR::save(const QString& filename)
{
	QString outname = filename;
	if(outname.isEmpty()){
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		QString base = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
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

bool OutputEditorHOCR::clear()
{
	if(!m_widget->isVisible()){
		return true;
	}
	if(ui.plainTextEditOutput->document()->isModified()){
		int response = QMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), QMessageBox::Save, QMessageBox::Discard, QMessageBox::Cancel);
		if(response == QMessageBox::Save){
			if(!save()){
				return false;
			}
		}else if(response != QMessageBox::Discard){
			return false;
		}
	}
	ui.plainTextEditOutput->clear();
	ui.plainTextEditOutput->document()->setModified(false);
	MAIN->setOutputPaneVisible(false);
	return true;
}

bool OutputEditorHOCR::getModified() const
{
	return ui.plainTextEditOutput->document()->isModified();
}

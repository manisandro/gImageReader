/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SubstitutionsManager.cc
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
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QList>
#include <QMessageBox>

#include "SubstitutionsManager.hh"
#include "MainWindow.hh"
#include "Config.hh"
#include "ConfigSettings.hh"
#include "Utils.hh"

SubstitutionsManager::SubstitutionsManager(QPlainTextEdit* textEdit, QCheckBox* csCheckBox)
{
	setWindowTitle(_("Substitutions"));
	setWindowFlags(Qt::WindowStaysOnTopHint);

	m_textEdit = textEdit;
	m_csCheckBox = csCheckBox;

	QAction* openAction = new QAction(QIcon::fromTheme("document-open"), _("Open"), this);
	openAction->setToolTip(_("Open"));

	QAction* saveAction = new QAction(QIcon::fromTheme("document-save"), _("Save"), this);
	saveAction->setToolTip(_("Save"));

	QAction* clearAction = new QAction(QIcon::fromTheme("edit-clear"), _("Clear"), this);
	clearAction->setToolTip(_("Clear"));

	QAction* addAction = new QAction(QIcon::fromTheme("list-add"), _("Add"), this);
	addAction->setToolTip(_("Add"));

	m_removeAction = new QAction(QIcon::fromTheme("list-remove"), _("Remove"), this);
	m_removeAction->setToolTip(_("Remove"));
	m_removeAction->setEnabled(false);

	QToolBar* toolbar = new QToolBar(this);
	toolbar->setIconSize(QSize(1, 1) * toolbar->style()->pixelMetric(QStyle::PM_SmallIconSize));
	toolbar->addAction(openAction);
	toolbar->addAction(saveAction);
	toolbar->addAction(clearAction);
	toolbar->addSeparator();
	toolbar->addAction(addAction);
	toolbar->addAction(m_removeAction);

	m_tableWidget = new QTableWidget(0, 2, this);
	m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_tableWidget->setEditTriggers(QAbstractItemView::CurrentChanged);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	m_tableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#else
	m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#endif
	m_tableWidget->horizontalHeader()->setVisible(true);
	m_tableWidget->verticalHeader()->setVisible(false);
	m_tableWidget->setHorizontalHeaderLabels(QStringList() << _("Search for") << _("Replace with"));

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Close);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setMargin(4);
	layout->addWidget(toolbar);
	layout->addWidget(m_tableWidget);
	layout->addWidget(buttonBox);

	setLayout(layout);

	connect(openAction, SIGNAL(triggered()), this, SLOT(openList()));
	connect(saveAction, SIGNAL(triggered()), this, SLOT(saveList()));
	connect(clearAction, SIGNAL(triggered()), this, SLOT(clearList()));
	connect(addAction, SIGNAL(triggered()), this, SLOT(addRow()));
	connect(buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(applySubstitutions()));
	connect(buttonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()), this, SLOT(hide()));
	connect(m_removeAction, SIGNAL(triggered()), this, SLOT(removeRows()));
	connect(m_tableWidget->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(onTableSelectionChanged(QItemSelection,QItemSelection)));

	MAIN->getConfig()->addSetting(new TableSetting("substitutionslist", m_tableWidget));
	MAIN->getConfig()->addSetting(new VarSetting<QString>("substitutionslistfile", QDir(Utils::documentsFolder()).absoluteFilePath(_("substitution_list.txt"))));

	m_currentFile = MAIN->getConfig()->getSetting<VarSetting<QString>>("substitutionslistfile")->getValue();
}

void SubstitutionsManager::openList()
{
	if(!clearList()) {
		return;
	}
	QString dir = !m_currentFile.isEmpty() ? QFileInfo(m_currentFile).absolutePath() : Utils::documentsFolder();
	QString filename = QFileDialog::getOpenFileName(this, _("Open Substitutions List"), dir, QString("%1 (*.txt)").arg(_("Substitutions List")));

	if(!filename.isEmpty()) {
		QFile file(filename);
		if(!file.open(QIODevice::ReadOnly)){
			QMessageBox::critical(this, _("Error Reading File"), _("Unable to read '%1'.").arg(filename));
			return;
		}
		m_currentFile = filename;
		MAIN->getConfig()->getSetting<VarSetting<QString>>("substitutionslistfile")->setValue(m_currentFile);

		bool errors = false;
		m_tableWidget->blockSignals(true);
		while(!file.atEnd()){
			QList<QByteArray> fields = file.readLine().split('\t');
			if(fields.size() < 2) {
				errors = true;
				continue;
			}
			int row = m_tableWidget->rowCount();
			m_tableWidget->insertRow(row);
			m_tableWidget->setItem(row, 0, new QTableWidgetItem(QString::fromLocal8Bit(fields[0])));
			m_tableWidget->setItem(row, 1, new QTableWidgetItem(QString::fromLocal8Bit(fields[1])));
		}
		m_tableWidget->blockSignals(false);
		MAIN->getConfig()->getSetting<TableSetting>("substitutionslist")->serialize();
		if(errors){
			QMessageBox::warning(this, _("Errors Occurred Reading File"), _("Some entries of the substitutions list could not be read."));
		}
	}
}

bool SubstitutionsManager::saveList()
{
	QString filename = QFileDialog::getSaveFileName(this, _("Save Substitutions List"), m_currentFile, QString("%1 (*.txt)").arg(_("Substitutions List")));
	if(filename.isEmpty()){
		return false;
	}
	QFile file(filename);
	if(!file.open(QIODevice::WriteOnly)){
		QMessageBox::critical(this, _("Error Saving File"), _("Unable to write to '%1'.").arg(filename));
		return false;
	}
	m_currentFile = filename;
	MAIN->getConfig()->getSetting<VarSetting<QString>>("substitutionslistfile")->setValue(m_currentFile);
	for(int row = 0, nRows = m_tableWidget->rowCount(); row < nRows; ++row){
		QByteArray line = QString("%1\t%2\n").arg(m_tableWidget->item(row, 0)->text()).arg(m_tableWidget->item(row, 1)->text()).toLocal8Bit();
		file.write(line);
	}
	return true;
}

bool SubstitutionsManager::clearList()
{
	if(m_tableWidget->rowCount() > 0) {
		int response = QMessageBox::question(this, _("Save List?"), _("Do you want to save the current list?"), QMessageBox::Yes, QMessageBox::No);
		if((response == QMessageBox::Yes && !saveList()) || response != QMessageBox::No){
			return false;
		}
		m_tableWidget->setRowCount(0);
		MAIN->getConfig()->getSetting<TableSetting>("substitutionslist")->serialize();
	}
	return true;
}

void SubstitutionsManager::addRow()
{
	int row = m_tableWidget->rowCount();
	m_tableWidget->insertRow(row);
	m_tableWidget->setItem(row, 0, new QTableWidgetItem());
	m_tableWidget->setItem(row, 1, new QTableWidgetItem());
	m_tableWidget->editItem(m_tableWidget->item(row, 0));
}

void SubstitutionsManager::removeRows()
{
	m_tableWidget->blockSignals(true);
	for(const QModelIndex& index : m_tableWidget->selectionModel()->selectedRows())
	{
		m_tableWidget->removeRow(index.row());
	}
	m_tableWidget->blockSignals(false);
	MAIN->getConfig()->getSetting<TableSetting>("substitutionslist")->serialize();
}

void SubstitutionsManager::onTableSelectionChanged(const QItemSelection& selected, const QItemSelection& /*deselected*/)
{
	m_removeAction->setEnabled(!selected.isEmpty());
}

void SubstitutionsManager::applySubstitutions()
{
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	QTextCursor cursor =  m_textEdit->textCursor();
	int end = qMax(cursor.anchor(), cursor.position());
	if(cursor.anchor() == cursor.position()){
		cursor.movePosition(QTextCursor::Start);
		QTextCursor tmp(cursor);
		tmp.movePosition(QTextCursor::End);
		end = tmp.position();
	}else{
		cursor.setPosition(qMin(cursor.anchor(), cursor.position()));
	}
	QTextDocument::FindFlags flags = 0;
	if(m_csCheckBox->isChecked()){
		flags = QTextDocument::FindCaseSensitively;
	}
	int start = cursor.position();
	for(int row = 0, nRows = m_tableWidget->rowCount(); row < nRows; ++row){
		QString search = m_tableWidget->item(row, 0)->text();
		QString replace = m_tableWidget->item(row, 1)->text();
		int diff = replace.length() - search.length();
		cursor.setPosition(start);
		while(true){
			cursor = m_textEdit->document()->find(search, cursor, flags);
			if(cursor.isNull() || qMax(cursor.anchor(), cursor.position()) > end){
				break;
			}
			cursor.insertText(replace);
			end += diff;
		}
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}
	MAIN->popState();
}

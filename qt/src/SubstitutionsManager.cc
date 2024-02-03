/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SubstitutionsManager.cc
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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
#include <QList>
#include <QMessageBox>

#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "OutputTextEdit.hh"
#include "SubstitutionsManager.hh"
#include "Utils.hh"

SubstitutionsManager::SubstitutionsManager(QWidget* parent)
	: QDialog(parent) {
	setWindowTitle(_("Substitutions"));

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
	m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_tableWidget->horizontalHeader()->setVisible(true);
	m_tableWidget->verticalHeader()->setVisible(false);
	m_tableWidget->setHorizontalHeaderLabels(QStringList() << _("Search for") << _("Replace with"));

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->addWidget(toolbar);
	layout->addWidget(m_tableWidget);
	layout->addWidget(buttonBox);

	setLayout(layout);

	connect(openAction, &QAction::triggered, this, &SubstitutionsManager::openList);
	connect(saveAction, &QAction::triggered, this, &SubstitutionsManager::saveList);
	connect(clearAction, &QAction::triggered, this, &SubstitutionsManager::clearList);
	connect(addAction, &QAction::triggered, this, &SubstitutionsManager::addRow);
	connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SubstitutionsManager::emitApplySubstitutions);
	connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &SubstitutionsManager::hide);
	connect(m_removeAction, &QAction::triggered, this, &SubstitutionsManager::removeRows);
	connect(m_tableWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SubstitutionsManager::onTableSelectionChanged);

	ADD_SETTING(TableSetting("substitutionslist", m_tableWidget));
}

SubstitutionsManager::~SubstitutionsManager() {
	ConfigSettings::get<TableSetting>("substitutionslist")->serialize();
}

void SubstitutionsManager::openList() {
	if(!clearList()) {
		return;
	}
	QString dir = !m_currentFile.isEmpty() ? QFileInfo(m_currentFile).absolutePath() : "";
	QStringList files = FileDialogs::openDialog(_("Open Substitutions List"), dir, "auxdir", QString("%1 (*.txt)").arg(_("Substitutions List")), false, this);

	if(!files.isEmpty()) {
		QString filename = files.front();
		QFile file(filename);
		if(!file.open(QIODevice::ReadOnly)) {
			QMessageBox::critical(this, _("Error Reading File"), _("Unable to read '%1'.").arg(filename));
			return;
		}
		m_currentFile = filename;

		bool errors = false;
		m_tableWidget->blockSignals(true);
		while(!file.atEnd()) {
			QString line = MAIN->getConfig()->useUtf8() ? QString::fromUtf8(file.readLine()) : QString::fromLocal8Bit(file.readLine());
			line.chop(1);
			if(line.isEmpty()) {
				continue;
			}
			QList<QString> fields = line.split('\t');
			if(fields.size() < 2) {
				errors = true;
				continue;
			}
			int row = m_tableWidget->rowCount();
			m_tableWidget->insertRow(row);
			m_tableWidget->setItem(row, 0, new QTableWidgetItem(fields[0]));
			m_tableWidget->setItem(row, 1, new QTableWidgetItem(fields[1]));
		}
		m_tableWidget->blockSignals(false);
		if(errors) {
			QMessageBox::warning(this, _("Errors Occurred Reading File"), _("Some entries of the substitutions list could not be read."));
		}
	}
}

bool SubstitutionsManager::saveList() {
	QString filename = FileDialogs::saveDialog(_("Save Substitutions List"), m_currentFile, "auxdir", QString("%1 (*.txt)").arg(_("Substitutions List")), false, this);
	if(filename.isEmpty()) {
		return false;
	}
	QFile file(filename);
	if(!file.open(QIODevice::WriteOnly)) {
		QMessageBox::critical(this, _("Error Saving File"), _("Unable to write to '%1'.").arg(filename));
		return false;
	}
	m_currentFile = filename;
	for(int row = 0, nRows = m_tableWidget->rowCount(); row < nRows; ++row) {
		QString line = QString("%1\t%2\n").arg(m_tableWidget->item(row, 0)->text()).arg(m_tableWidget->item(row, 1)->text());
		file.write(MAIN->getConfig()->useUtf8() ? line.toUtf8() : line.toLocal8Bit());
	}
	return true;
}

bool SubstitutionsManager::clearList() {
	if(m_tableWidget->rowCount() > 0) {
		int response = QMessageBox::question(this, _("Save List?"), _("Do you want to save the current list?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		if(response == QMessageBox::Save) {
			if(!saveList()) {
				return false;
			}
		} else if(response != QMessageBox::Discard) {
			return false;
		}
		m_tableWidget->setRowCount(0);
	}
	return true;
}

void SubstitutionsManager::addRow() {
	int row = m_tableWidget->rowCount();
	m_tableWidget->insertRow(row);
	m_tableWidget->setItem(row, 0, new QTableWidgetItem());
	m_tableWidget->setItem(row, 1, new QTableWidgetItem());
	m_tableWidget->editItem(m_tableWidget->item(row, 0));
}

void SubstitutionsManager::removeRows() {
	m_tableWidget->blockSignals(true);
	for(const QModelIndex& index : m_tableWidget->selectionModel()->selectedRows()) {
		m_tableWidget->removeRow(index.row());
	}
	m_tableWidget->blockSignals(false);
}

void SubstitutionsManager::onTableSelectionChanged(const QItemSelection& selected, const QItemSelection& /*deselected*/) {
	m_removeAction->setEnabled(!selected.isEmpty());
}

void SubstitutionsManager::emitApplySubstitutions() {
	QMap<QString, QString> substitutions;
	for(int row = 0, nRows = m_tableWidget->rowCount(); row < nRows; ++row) {
		substitutions.insert(m_tableWidget->item(row, 0)->text(), m_tableWidget->item(row, 1)->text());
	}
	emit applySubstitutions(substitutions);
}

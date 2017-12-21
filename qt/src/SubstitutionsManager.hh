/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SubstitutionsManager.hh
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

#ifndef SUBSTITUTIONS_MANAGER_HH
#define SUBSTITUTIONS_MANAGER_HH

#include <QDialog>

class QCheckBox;
class QItemSelection;
class OutputTextEdit;
class QTableWidget;

class SubstitutionsManager : public QDialog {
	Q_OBJECT
public:
	SubstitutionsManager(QWidget* parent = nullptr);
	~SubstitutionsManager();

signals:
	void applySubstitutions(const QMap<QString, QString>& substitutions);

private:
	QAction* m_removeAction;
	QString m_currentFile;
	QTableWidget* m_tableWidget;

private slots:
	void addRow();
	void emitApplySubstitutions();
	bool clearList();
	void onTableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
	void openList();
	void removeRows();
	bool saveList();
};

#endif // SUBSTITUTIONS_MANAGER_HH

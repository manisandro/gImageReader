/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRProofReadWidget.hh
 * Copyright (C) 2020 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRPROOFREADWIDGET_HH
#define HOCRPROOFREADWIDGET_HH

#include <QMap>
#include <QFrame>

class QTreeView;
class QVBoxLayout;
class HOCRItem;

class HOCRProofReadWidget : public QFrame {
	Q_OBJECT
public:
	HOCRProofReadWidget(QTreeView* treeView, QWidget* parent = nullptr);
	void clear();

private:
	class LineEdit;

	QTreeView* m_treeView = nullptr;
	QVBoxLayout* m_linesLayout = nullptr;
	QWidget* m_controlsWidget = nullptr;
	QMap<const HOCRItem*, QWidget*> m_currentLines;

	// Disable auto tab handling
	bool focusNextPrevChild(bool) override { return false; }

	void repositionWidget();


private slots:
	void updateRows();
	void setCurrentRow(const QModelIndex& current);
	void showShortcutsDialog();
};


#endif // HOCRPROOFREADWIDGET_HH

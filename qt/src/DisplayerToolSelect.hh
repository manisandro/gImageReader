/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolSelect.hh
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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

#ifndef DISPLAYERTOOLSELECT_HH
#define DISPLAYERTOOLSELECT_HH

#include <QGraphicsRectItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QSpinBox>
#include <QWidgetAction>
#include "Displayer.hh"

class NumberedDisplayerSelection;

class DisplayerToolSelect : public DisplayerTool {
	Q_OBJECT
public:
	DisplayerToolSelect(Displayer* displayer, QObject* parent = 0);
	~DisplayerToolSelect();
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void resolutionChanged(double factor) override;
	void rotationChanged(double delta) override;

	QList<QImage> getOCRAreas() override;
	bool hasMultipleOCRAreas() const override {
		return !m_selections.isEmpty();
	}
	bool allowAutodetectOCRAreas() const override {
		return true;
	}
	void autodetectOCRAreas() override {
		autodetectLayout();
	}
	void reset() override {
		clearSelections();
	}

private:
	friend class NumberedDisplayerSelection;
	NumberedDisplayerSelection* m_curSel = nullptr;
	QList<NumberedDisplayerSelection*> m_selections;

	void clearSelections();
	void removeSelection(int num);
	void reorderSelection(int oldNum, int newNum);
	void saveSelection(NumberedDisplayerSelection* selection);
	void updateRecognitionModeLabel();
	void autodetectLayout(bool noDeskew = false);
};

class NumberedDisplayerSelection : public DisplayerSelection {
	Q_OBJECT
public:
	NumberedDisplayerSelection(DisplayerToolSelect* selectTool, int number, const QPointF& anchor)
		: DisplayerSelection(selectTool, anchor), m_number(number) {
	}
	void setNumber(int number) {
		m_number = number;
	}

private slots:
	void reorderSelection(int newNumber);

private:
	int m_number;
	void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

#endif // DISPLAYERTOOLSELECT_HH

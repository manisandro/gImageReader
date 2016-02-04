/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolSelect.hh
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

#ifndef DISPLAYERTOOLSELECT_HH
#define DISPLAYERTOOLSELECT_HH

#include <QGraphicsRectItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QSpinBox>
#include <QWidgetAction>
#include "Displayer.hh"

class DisplaySelection;

class DisplayerToolSelect : public DisplayerTool {
	Q_OBJECT
public:
	DisplayerToolSelect(QAction* actionAutodetectLayout, Displayer* displayer, QObject* parent = 0);
	~DisplayerToolSelect();
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void pageChanged() override;
	void resolutionChanged(double factor) override;
	void rotationChanged(double delta) override;

	QList<QImage> getOCRAreas();
	bool hasMultipleOCRAreas() const{ return !m_selections.isEmpty(); }
	void autodetectOCRAreas(){ autodetectLayout(); }

private:
	friend class DisplaySelection;
	QAction* mActionAutodetectLayout = nullptr;
	DisplaySelection* m_curSel = nullptr;
	QList<DisplaySelection*> m_selections;

	void clearSelections();
	void removeSelection(int num);
	void reorderSelection(int oldNum, int newNum);
	void saveSelection(DisplaySelection* selection);
	void updateRecognitionModeLabel();

private slots:
	void autodetectLayout(bool noDeskew = false);
};

class DisplaySelection : public QObject, public QGraphicsRectItem
{
	Q_OBJECT
public:
	DisplaySelection(DisplayerToolSelect* selectTool, int number, const QPointF& anchor)
		: QGraphicsRectItem(QRectF(anchor, anchor)), m_selectTool(selectTool), m_number(number), m_anchor(anchor), m_point(anchor)
	{
		setAcceptHoverEvents(true);
	}
	void setPoint(const QPointF& point){
		m_point = point;
		setRect(QRectF(m_anchor, m_point).normalized());
	}
	void rotate(const QTransform& transform){
		m_anchor = transform.map(m_anchor);
		m_point = transform.map(m_point);
		setRect(QRectF(m_anchor, m_point).normalized());
	}
	void scale(double factor){
		m_anchor *= factor;
		m_point *= factor;
	}

	void setNumber(int number){
		m_number = number;
	}

private slots:
	void reorderSelection(int newNumber);

private:
	typedef void(*ResizeHandler)(const QPointF&, QPointF&, QPointF&);

	DisplayerToolSelect* m_selectTool;
	int m_number;
	QPointF m_anchor;
	QPointF m_point;
	QVector<ResizeHandler> m_resizeHandlers;
	QPointF m_resizeOffset;

	void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
	void mousePressEvent(QGraphicsSceneMouseEvent *event);
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
	void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

	static void resizeAnchorX(const QPointF& pos, QPointF& anchor, QPointF& /*point*/){ anchor.rx() = pos.x(); }
	static void resizeAnchorY(const QPointF& pos, QPointF& anchor, QPointF& /*point*/){ anchor.ry() = pos.y(); }
	static void resizePointX(const QPointF& pos, QPointF& /*anchor*/, QPointF& point){ point.rx() = pos.x(); }
	static void resizePointY(const QPointF& pos, QPointF& /*anchor*/, QPointF& point){ point.ry() = pos.y(); }
};

#endif // DISPLAYERTOOLSELECT_HH

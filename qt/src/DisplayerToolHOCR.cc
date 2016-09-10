/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolHOCR.cc
 * Copyright (C) 2016 Sandro Mani <manisandro@gmail.com>
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

#include "DisplayerToolHOCR.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"

#include <QGraphicsRectItem>
#include <QMouseEvent>


DisplayerToolHOCR::DisplayerToolHOCR(Displayer *displayer, QObject *parent)
	: DisplayerTool(displayer, parent)
{
	MAIN->getRecognizer()->setRecognizeMode(_("Recognize"));
}

DisplayerToolHOCR::~DisplayerToolHOCR()
{
	clearSelection();
}

QList<QImage> DisplayerToolHOCR::getOCRAreas()
{
	return QList<QImage>() << m_displayer->getImage(m_displayer->getSceneBoundingRect());
}

void DisplayerToolHOCR::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::LeftButton && m_drawingSelection){
		clearSelection();
		m_selection = new DisplayerSelection(this,  m_displayer->mapToSceneClamped(event->pos()));
		m_displayer->scene()->addItem(m_selection);
		event->accept();
	}
}

void DisplayerToolHOCR::mouseMoveEvent(QMouseEvent *event)
{
	if(m_selection && m_drawingSelection){
		QPointF p = m_displayer->mapToSceneClamped(event->pos());
		m_selection->setPoint(p);
		m_displayer->ensureVisible(QRectF(p, p));
		event->accept();
	}
}

void DisplayerToolHOCR::mouseReleaseEvent(QMouseEvent *event)
{
	if(m_selection && m_drawingSelection){
		if(m_selection->rect().width() < 5. || m_selection->rect().height() < 5.){
			clearSelection();
		} else {
			QRect r = m_selection->rect().translated(-m_displayer->getSceneBoundingRect().toRect().topLeft()).toRect();
			emit selectionDrawn(r);
		}
		event->accept();
	}
	m_drawingSelection = false;
}

void DisplayerToolHOCR::setSelection(const QRect& rect)
{
	m_drawingSelection = false;
	QRect r = rect.translated(m_displayer->getSceneBoundingRect().toRect().topLeft());
	if(!m_selection) {
		m_selection = new DisplayerSelection(this, r.topLeft());
		connect(m_selection, SIGNAL(geometryChanged(QRectF)), this, SLOT(selectionChanged(QRectF)));
		m_displayer->scene()->addItem(m_selection);
	}
	m_selection->setAnchorAndPoint(r.topLeft(), r.bottomRight());
}

QImage DisplayerToolHOCR::getSelection(const QRect& rect)
{
	return m_displayer->getImage(rect.translated(m_displayer->getSceneBoundingRect().toRect().topLeft()));
}

void DisplayerToolHOCR::clearSelection()
{
	delete m_selection;
	m_selection = nullptr;
}

void DisplayerToolHOCR::selectionChanged(QRectF rect)
{
	QRect r = rect.translated(-m_displayer->getSceneBoundingRect().toRect().topLeft()).toRect();
	emit selectionGeometryChanged(r);
}

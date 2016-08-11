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

class DisplayerToolHOCR::GraphicsRectItem : public QGraphicsRectItem {
public:
	GraphicsRectItem(DisplayerToolHOCR* tool) : m_tool(tool) {}

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) override {
		QPen curPen = pen();
		curPen.setWidth(1 / m_tool->m_displayer->getCurrentScale());
		setPen(curPen);
		painter->setRenderHint(QPainter::Antialiasing, false);
		QGraphicsRectItem::paint(painter, option, widget);
		painter->setRenderHint(QPainter::Antialiasing, true);
	}
private:
	DisplayerToolHOCR* m_tool;
};

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

void DisplayerToolHOCR::setSelection(const QRect& rect)
{
	if(!m_selection) {
		m_selection = new GraphicsRectItem(this);

		QColor c = QPalette().highlight().color();
		m_selection->setBrush(QColor(c.red(), c.green(), c.blue(), 63));
		QPen pen;
		pen.setColor(c);
		m_selection->setPen(pen);
		m_displayer->scene()->addItem(m_selection);
	}
	m_selection->setRect(rect.translated(m_displayer->getSceneBoundingRect().toRect().topLeft()));
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

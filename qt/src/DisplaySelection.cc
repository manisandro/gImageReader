/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplaySelection.cc
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

#include "DisplaySelection.hh"
#include "Displayer.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStyle>

void DisplaySelection::contextMenuEvent(QGraphicsSceneContextMenuEvent *event){
	QMenu menu;

	QWidget* orderWidget = new QWidget(&menu);
	QHBoxLayout* layout = new QHBoxLayout(orderWidget);

	QLabel* orderIcon = new QLabel(&menu);
	int iconSize = orderIcon->style()->pixelMetric(QStyle::PM_SmallIconSize);
	orderIcon->setPixmap(QIcon::fromTheme("object-order-front").pixmap(iconSize, iconSize));
	layout->addWidget(orderIcon);
	layout->setContentsMargins(4, 0, 4, 0);

	QLabel* orderLabel = new QLabel(_("Order:"), &menu);
	orderLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	layout->addWidget(orderLabel);

	QSpinBox* orderSpin = new QSpinBox();
	orderSpin->setRange(1, m_displayer->m_selections.size());
	orderSpin->setValue(m_number);
	orderSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	connect(orderSpin, SIGNAL(valueChanged(int)), this, SLOT(reorderSelection(int)));
	layout->addWidget(orderSpin);

	QWidgetAction* spinAction = new QWidgetAction(&menu);
	spinAction->setDefaultWidget(orderWidget);

	QAction* deleteAction = new QAction(QIcon::fromTheme("edit-delete"), _("Delete"), &menu);
	QAction* ocrAction = new QAction(QIcon::fromTheme("insert-text"), _("Recognize"), &menu);
	QAction* ocrClipboardAction = new QAction(QIcon::fromTheme("edit-copy"), _("Recognize to clipboard"), &menu);
	QAction* saveAction = new QAction(QIcon::fromTheme("document-save-as"), _("Save as image"), &menu);
	menu.addActions(QList<QAction*>() << spinAction << deleteAction << ocrAction << ocrClipboardAction << saveAction);
	QAction* selected = menu.exec(event->screenPos());
	if(selected == deleteAction){
		m_displayer->removeSelection(m_number);
		}else if(selected == ocrAction){
			MAIN->getRecognizer()->recognizeImage(m_displayer->getImage(rect()), Recognizer::OutputDestination::Buffer);
		}else if(selected == ocrClipboardAction){
			MAIN->getRecognizer()->recognizeImage(m_displayer->getImage(rect()), Recognizer::OutputDestination::Clipboard);
		}else if(selected == saveAction){
			m_displayer->saveSelection(this);
		}
}

void DisplaySelection::reorderSelection(int newNumber)
{
	m_displayer->reorderSelection(m_number, newNumber);
}

void DisplaySelection::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	QColor c = QPalette().highlight().color();
	setBrush(QColor(c.red(), c.green(), c.blue(), 63));
	QPen pen;
	pen.setColor(c);
	pen.setWidth(1 / m_displayer->m_scale);
	setPen(pen);

	painter->setRenderHint(QPainter::Antialiasing, false);
	QGraphicsRectItem::paint(painter, option, widget);

	QRectF r = rect();
	qreal w = 20. / m_displayer->m_scale;
	w = qMin(w, qMin(r.width(), r.height()));
	QRectF box(r.x(), r.y(), w, w);

	painter->setBrush(QPalette().highlight());
	painter->drawRect(box);

	painter->setRenderHint(QPainter::Antialiasing, true);

	if(w > 1.25){
		QFont font;
		font.setPixelSize(0.8 * w);
		font.setBold(true);
		painter->setFont(font);
		painter->setPen(QPalette().highlightedText().color());
		painter->drawText(box, Qt::AlignCenter, QString::number(m_number));
	}
}

void DisplaySelection::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
	QPointF p = event->pos();
	QRectF r = rect();
	double tol = 10.0 / m_displayer->m_scale;

	bool left = qAbs(r.x() - p.x()) < tol;
	bool right = qAbs(r.x() + r.width() - p.x()) < tol;
	bool top = qAbs(r.y() - p.y()) < tol;
	bool bottom = qAbs(r.y() + r.height() - p.y()) < tol;

	if((top && left) || (bottom && right)){
		setCursor(Qt::SizeFDiagCursor);
	}else if((top && right) || (bottom && left)){
		setCursor(Qt::SizeBDiagCursor);
	}else if(top || bottom){
		setCursor(Qt::SizeVerCursor);
	}else if(left || right){
		setCursor(Qt::SizeHorCursor);
	}else{
		unsetCursor();
	}
}

void DisplaySelection::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	QPointF p = event->pos();
	double tol = 10.0 / m_displayer->m_scale;
	m_resizeHandlers.clear();
	m_resizeOffset = QPointF(0., 0.);
	if(qAbs(m_point.x() - p.x()) < tol){ // pointx
		m_resizeHandlers.append(resizePointX);
		m_resizeOffset.setX(event->pos().x() - m_point.x());
	}else if(qAbs(m_anchor.x() - p.x()) < tol){ // anchorx
		m_resizeHandlers.append(resizeAnchorX);
		m_resizeOffset.setX(event->pos().x() - m_anchor.x());
	}
	if(qAbs(m_point.y() - p.y()) < tol){ // pointy
		m_resizeHandlers.append(resizePointY);
		m_resizeOffset.setY(event->pos().y() - m_point.y());
	}else if(qAbs(m_anchor.y() - p.y()) < tol){ // anchory
		m_resizeHandlers.append(resizeAnchorY);
		m_resizeOffset.setY(event->pos().y() - m_anchor.y());
	}
	return m_resizeHandlers.empty() ? event->ignore() : event->accept();
}

void DisplaySelection::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	QPointF p = event->pos() - m_resizeOffset;
	QRectF bb = m_displayer->m_imageItem->sceneBoundingRect();
	p.rx() = qMin(qMax(bb.x(), p.x()), bb.x() + bb.width());
	p.ry() = qMin(qMax(bb.y(), p.y()), bb.y() + bb.height());
	if(!m_resizeHandlers.isEmpty()){
		for(const ResizeHandler& handler : m_resizeHandlers){
			handler(p, m_anchor, m_point);
		}
		setRect(QRectF(m_anchor, m_point).normalized());
		event->accept();
	}else{
		event->ignore();
	}
}

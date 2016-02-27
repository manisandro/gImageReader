/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolSelect.cc
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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

#include "DisplayerToolSelect.hh"
#include "Displayer.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"
#include "Utils.hh"

#include <cmath>
#include <tesseract/baseapi.h>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>


DisplayerToolSelect::DisplayerToolSelect(QAction* actionAutodetectLayout, Displayer *displayer, QObject *parent)
	: DisplayerTool(displayer, parent), mActionAutodetectLayout(actionAutodetectLayout)

{
	connect(mActionAutodetectLayout, SIGNAL(triggered()), this, SLOT(autodetectLayout()));

	MAIN->getConfig()->addSetting(new VarSetting<QString>("selectionsavefile", QDir(Utils::documentsFolder()).absoluteFilePath(_("selection.png"))));

	mActionAutodetectLayout->setVisible(true);
	updateRecognitionModeLabel();
}

DisplayerToolSelect::~DisplayerToolSelect()
{
	clearSelections();
	mActionAutodetectLayout->setVisible(false);
}

void DisplayerToolSelect::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::LeftButton &&  m_curSel == nullptr){
		if((event->modifiers() & Qt::ControlModifier) == 0){
			clearSelections();
		}
		m_curSel = new DisplaySelection(this, 1 + m_selections.size(), mapToSceneClamped(event->pos()));
		m_curSel->setZValue(1 + m_selections.size());
		addItemToScene(m_curSel);
		event->accept();
	}
}

void DisplayerToolSelect::mouseMoveEvent(QMouseEvent *event)
{
	if(m_curSel){
		QPointF p = mapToSceneClamped(event->pos());
		m_curSel->setPoint(p);
		m_displayer->ensureVisible(QRectF(p, p));
		event->accept();
	}
}

void DisplayerToolSelect::mouseReleaseEvent(QMouseEvent *event)
{
	if(m_curSel){
		if(m_curSel->rect().width() < 5. || m_curSel->rect().height() < 5.){
			delete m_curSel;
		}else{
			m_selections.append(m_curSel);
			updateRecognitionModeLabel();
		}
		m_curSel = nullptr;
		event->accept();
	}
}

void DisplayerToolSelect::pageChanged()
{
	clearSelections();
}

void DisplayerToolSelect::resolutionChanged(double factor)
{
	for(DisplaySelection* sel : m_selections){
		sel->scale(factor);
	}
}

void DisplayerToolSelect::rotationChanged(double delta)
{
	QTransform t;
	t.rotate(delta);
	for(DisplaySelection* sel : m_selections){
		sel->rotate(t);
	}
}

QList<QImage> DisplayerToolSelect::getOCRAreas()
{
	QList<QImage> images;
	if(m_selections.empty()){
		images.append(getImage(getSceneBoundingRect()));
	}else{
		for(const DisplaySelection* sel : m_selections){
			images.append(getImage(sel->rect()));
		}
	}
	return images;
}

void DisplayerToolSelect::clearSelections()
{
	qDeleteAll(m_selections);
	m_selections.clear();
	updateRecognitionModeLabel();
}

void DisplayerToolSelect::removeSelection(int num)
{
	delete m_selections[num - 1];
	m_selections.removeAt(num - 1);
	for(int i = 0, n = m_selections.size(); i < n; ++i){
		m_selections[i]->setNumber(1 + i);
		m_selections[i]->setZValue(1 + i);
		invalidateRect(m_selections[i]->rect());
	}
}

void DisplayerToolSelect::reorderSelection(int oldNum, int newNum)
{
	DisplaySelection* sel = m_selections[oldNum - 1];
	m_selections.removeAt(oldNum - 1);
	m_selections.insert(newNum - 1, sel);
	for(int i = 0, n = m_selections.size(); i < n; ++i){
		m_selections[i]->setNumber(1 + i);
		m_selections[i]->setZValue(1 + i);
		invalidateRect(m_selections[i]->rect());
	}
}

void DisplayerToolSelect::saveSelection(DisplaySelection* selection)
{
	QImage img = getImage(selection->rect());
	QString filename = Utils::makeOutputFilename(MAIN->getConfig()->getSetting<VarSetting<QString>>("selectionsavefile")->getValue());
	filename = QFileDialog::getSaveFileName(MAIN, _("Save Selection Image"), filename, QString("%1 (*.png)").arg(_("PNG Images")));
	if(!filename.isEmpty()){
		MAIN->getConfig()->getSetting<VarSetting<QString>>("selectionsavefile")->setValue(filename);
		img.save(filename);
	}
}

void DisplayerToolSelect::updateRecognitionModeLabel()
{
	MAIN->getRecognizer()->setRecognizeMode(m_selections.isEmpty() ? _("Recognize all") : _("Recognize selection"));
}

void DisplayerToolSelect::autodetectLayout(bool noDeskew)
{
	clearSelections();

	double avgDeskew = 0.;
	int nDeskew = 0;
	QList<QRectF> rects;
	QImage img = getImage(getSceneBoundingRect());

	// Perform layout analysis
	Utils::busyTask([this,&nDeskew,&avgDeskew,&rects,&img]{
		tesseract::TessBaseAPI tess;
		tess.InitForAnalysePage();
		tess.SetPageSegMode(tesseract::PSM_AUTO_ONLY);
		tess.SetImage(img.bits(), img.width(), img.height(), 4, img.bytesPerLine());
		tesseract::PageIterator* it = tess.AnalyseLayout();
		if(it && !it->Empty(tesseract::RIL_BLOCK)){
			do{
				int x1, y1, x2, y2;
				tesseract::Orientation orient;
				tesseract::WritingDirection wdir;
				tesseract::TextlineOrder tlo;
				float deskew;
				it->BoundingBox(tesseract::RIL_BLOCK, &x1, &y1, &x2, &y2);
				it->Orientation(&orient, &wdir, &tlo, &deskew);
				avgDeskew += deskew;
				++nDeskew;
				float width = x2 - x1, height = y2 - y1;
				if(width > 10 && height > 10){
					rects.append(QRectF(x1 - 0.5 * img.width(), y1 - 0.5 * img.height(), width, height));
				}
			}while(it->Next(tesseract::RIL_BLOCK));
		}
		delete it;
		return true;
	}, _("Performing layout analysis"));

	// If a somewhat large deskew angle is detected, automatically rotate image and redetect layout,
	// unless we already attempted to rotate (to prevent endless loops)
	avgDeskew = qRound(((avgDeskew/nDeskew)/M_PI * 180.) * 10.) / 10.;
	if(qAbs(avgDeskew > .1) && !noDeskew){
		m_displayer->setRotation(m_displayer->getCurrentAngle() - avgDeskew);
		autodetectLayout(true);
	}else{
		// Merge overlapping rectangles
		for(int i = rects.size(); i-- > 1;) {
			for(int j = i; j-- > 0;) {
				if(rects[j].intersects(rects[i])) {
					rects[j] = rects[j].united(rects[i]);
					rects.removeAt(i);
					break;
				}
			}
		}
		for(int i = 0, n = rects.size(); i < n; ++i){
			m_selections.append(new DisplaySelection(this, 1 + i, rects[i].topLeft()));
			m_selections.back()->setPoint(rects[i].bottomRight());
			addItemToScene(m_selections.back());
		}
		updateRecognitionModeLabel();
	}
}

///////////////////////////////////////////////////////////////////////////////

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
	orderSpin->setRange(1, m_selectTool->m_selections.size());
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
		m_selectTool->removeSelection(m_number);
	}else if(selected == ocrAction){
		MAIN->getRecognizer()->recognizeImage(m_selectTool->getImage(rect()), Recognizer::OutputDestination::Buffer);
	}else if(selected == ocrClipboardAction){
		MAIN->getRecognizer()->recognizeImage(m_selectTool->getImage(rect()), Recognizer::OutputDestination::Clipboard);
	}else if(selected == saveAction){
		m_selectTool->saveSelection(this);
	}
}

void DisplaySelection::reorderSelection(int newNumber)
{
	m_selectTool->reorderSelection(m_number, newNumber);
}

void DisplaySelection::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	QColor c = QPalette().highlight().color();
	setBrush(QColor(c.red(), c.green(), c.blue(), 63));
	QPen pen;
	pen.setColor(c);
	pen.setWidth(1 / m_selectTool->getDisplayScale());
	setPen(pen);

	painter->setRenderHint(QPainter::Antialiasing, false);
	QGraphicsRectItem::paint(painter, option, widget);

	QRectF r = rect();
	qreal w = 20. / m_selectTool->getDisplayScale();
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
	double tol = 10.0 / m_selectTool->getDisplayScale();

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
	double tol = 10.0 / m_selectTool->getDisplayScale();
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
	QRectF bb = m_selectTool->getSceneBoundingRect();
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

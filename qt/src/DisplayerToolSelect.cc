/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolSelect.cc
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

#include "DisplayerToolSelect.hh"
#include "Displayer.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"
#include "Utils.hh"

#include <cmath>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>


DisplayerToolSelect::DisplayerToolSelect(QAction* actionAutodetectLayout, Displayer *displayer, QObject *parent)
	: DisplayerTool(displayer, parent), mActionAutodetectLayout(actionAutodetectLayout) {
	connect(mActionAutodetectLayout, SIGNAL(triggered()), this, SLOT(autodetectLayout()));

	MAIN->getConfig()->addSetting(new VarSetting<QString>("selectionsavefile", QDir(Utils::documentsFolder()).absoluteFilePath(_("selection.png"))));

	mActionAutodetectLayout->setVisible(true);
	updateRecognitionModeLabel();
}

DisplayerToolSelect::~DisplayerToolSelect() {
	clearSelections();
	mActionAutodetectLayout->setVisible(false);
}

void DisplayerToolSelect::mousePressEvent(QMouseEvent *event) {
	if(event->button() == Qt::LeftButton &&  m_curSel == nullptr) {
		if((event->modifiers() & Qt::ControlModifier) == 0) {
			clearSelections();
		}
		m_curSel = new NumberedDisplayerSelection(this, 1 + m_selections.size(), m_displayer->mapToSceneClamped(event->pos()));
		m_curSel->setZValue(1 + m_selections.size());
		m_displayer->scene()->addItem(m_curSel);
		event->accept();
	}
}

void DisplayerToolSelect::mouseMoveEvent(QMouseEvent *event) {
	if(m_curSel) {
		QPointF p = m_displayer->mapToSceneClamped(event->pos());
		m_curSel->setPoint(p);
		m_displayer->ensureVisible(QRectF(p, p));
		event->accept();
	}
}

void DisplayerToolSelect::mouseReleaseEvent(QMouseEvent *event) {
	if(m_curSel) {
		if(m_curSel->rect().width() < 5.0 || m_curSel->rect().height() < 5.0) {
			delete m_curSel;
		} else {
			m_selections.append(m_curSel);
			updateRecognitionModeLabel();
		}
		m_curSel = nullptr;
		event->accept();
	}
}

void DisplayerToolSelect::resolutionChanged(double factor) {
	for(NumberedDisplayerSelection* sel : m_selections) {
		sel->scale(factor);
	}
}

void DisplayerToolSelect::rotationChanged(double delta) {
	QTransform t;
	t.rotate(delta);
	for(NumberedDisplayerSelection* sel : m_selections) {
		sel->rotate(t);
	}
}

QList<QImage> DisplayerToolSelect::getOCRAreas() {
	QList<QImage> images;
	if(m_selections.empty()) {
		images.append(m_displayer->getImage(m_displayer->getSceneBoundingRect()));
	} else {
		for(const NumberedDisplayerSelection* sel : m_selections) {
			images.append(m_displayer->getImage(sel->rect()));
		}
	}
	return images;
}

void DisplayerToolSelect::clearSelections() {
	qDeleteAll(m_selections);
	m_selections.clear();
	updateRecognitionModeLabel();
}

void DisplayerToolSelect::removeSelection(int num) {
	delete m_selections[num - 1];
	m_selections.removeAt(num - 1);
	for(int i = 0, n = m_selections.size(); i < n; ++i) {
		m_selections[i]->setNumber(1 + i);
		m_selections[i]->setZValue(1 + i);
	}
}

void DisplayerToolSelect::reorderSelection(int oldNum, int newNum) {
	NumberedDisplayerSelection* sel = m_selections[oldNum - 1];
	m_selections.removeAt(oldNum - 1);
	m_selections.insert(newNum - 1, sel);
	for(int i = 0, n = m_selections.size(); i < n; ++i) {
		m_selections[i]->setNumber(1 + i);
		m_selections[i]->setZValue(1 + i);
	}
}

void DisplayerToolSelect::saveSelection(NumberedDisplayerSelection* selection) {
	QImage img = m_displayer->getImage(selection->rect());
	QString filename = Utils::makeOutputFilename(MAIN->getConfig()->getSetting<VarSetting<QString>>("selectionsavefile")->getValue());
	filename = QFileDialog::getSaveFileName(MAIN, _("Save Selection Image"), filename, QString("%1 (*.png)").arg(_("PNG Images")));
	if(!filename.isEmpty()) {
		MAIN->getConfig()->getSetting<VarSetting<QString>>("selectionsavefile")->setValue(filename);
		img.save(filename);
	}
}

void DisplayerToolSelect::updateRecognitionModeLabel() {
	MAIN->getRecognizer()->setRecognizeMode(m_selections.isEmpty() ? _("Recognize all") : _("Recognize selection"));
}

void DisplayerToolSelect::autodetectLayout(bool noDeskew) {
	clearSelections();

	double avgDeskew = 0.0;
	int nDeskew = 0;
	QList<QRectF> rects;
	QImage img = m_displayer->getImage(m_displayer->getSceneBoundingRect());

	// Perform layout analysis
	Utils::busyTask([this,&nDeskew,&avgDeskew,&rects,&img] {
		tesseract::TessBaseAPI tess;
		tess.InitForAnalysePage();
		tess.SetPageSegMode(tesseract::PSM_AUTO_ONLY);
		tess.SetImage(img.bits(), img.width(), img.height(), 4, img.bytesPerLine());
		tesseract::PageIterator* it = tess.AnalyseLayout();
		if(it && !it->Empty(tesseract::RIL_BLOCK)) {
			do {
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
				if(width > 10 && height > 10) {
					rects.append(QRectF(x1 - 0.5 * img.width(), y1 - 0.5 * img.height(), width, height));
				}
			} while(it->Next(tesseract::RIL_BLOCK));
		}
		delete it;
		return true;
	}, _("Performing layout analysis"));

	// If a somewhat large deskew angle is detected, automatically rotate image and redetect layout,
	// unless we already attempted to rotate (to prevent endless loops)
	avgDeskew = qRound(((avgDeskew/nDeskew)/M_PI * 180.0) * 10.0) / 10.0;
	if(std::abs(avgDeskew) > 0.1 && !noDeskew) {
		m_displayer->setAngle(m_displayer->getCurrentAngle() - avgDeskew);
		autodetectLayout(true);
	} else {
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
		for(int i = 0, n = rects.size(); i < n; ++i) {
			m_selections.append(new NumberedDisplayerSelection(this, 1 + i, rects[i].topLeft()));
			m_selections.back()->setPoint(rects[i].bottomRight());
			m_displayer->scene()->addItem(m_selections.back());
		}
		updateRecognitionModeLabel();
	}
}

///////////////////////////////////////////////////////////////////////////////

void NumberedDisplayerSelection::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
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
	orderSpin->setRange(1, static_cast<DisplayerToolSelect*>(m_tool)->m_selections.size());
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
	if(selected == deleteAction) {
		static_cast<DisplayerToolSelect*>(m_tool)->removeSelection(m_number);
	} else if(selected == ocrAction) {
		MAIN->getRecognizer()->recognizeImage(m_tool->getDisplayer()->getImage(rect()), Recognizer::OutputDestination::Buffer);
	} else if(selected == ocrClipboardAction) {
		MAIN->getRecognizer()->recognizeImage(m_tool->getDisplayer()->getImage(rect()), Recognizer::OutputDestination::Clipboard);
	} else if(selected == saveAction) {
		static_cast<DisplayerToolSelect*>(m_tool)->saveSelection(this);
	}
}

void NumberedDisplayerSelection::reorderSelection(int newNumber) {
	static_cast<DisplayerToolSelect*>(m_tool)->reorderSelection(m_number, newNumber);
}

void NumberedDisplayerSelection::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
	DisplayerSelection::paint(painter, option, widget);

	painter->setRenderHint(QPainter::Antialiasing, false);
	QRectF r = rect();
	qreal w = 20.0 / m_tool->getDisplayer()->getCurrentScale();
	w = std::min(w, std::min(r.width(), r.height()));
	QRectF box(r.x(), r.y(), w, w);
	painter->setBrush(QPalette().highlight());
	painter->drawRect(box);
	painter->setRenderHint(QPainter::Antialiasing, true);

	if(w > 1.25) {
		QFont font;
		font.setPixelSize(0.8 * w);
		font.setBold(true);
		painter->setFont(font);
		painter->setPen(QPalette().highlightedText().color());
		painter->drawText(box, Qt::AlignCenter, QString::number(m_number));
	}
}

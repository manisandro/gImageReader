/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.cc
 * Copyright (C) 2013-2018 Sandro Mani <manisandro@gmail.com>
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

#include "MainWindow.hh"
#include "Displayer.hh"
#include "DisplayRenderer.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <cmath>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneDragDropEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>


class GraphicsScene : public QGraphicsScene {
public:
	using QGraphicsScene::QGraphicsScene;

protected:
	void dragEnterEvent(QGraphicsSceneDragDropEvent *event) {
		if(Utils::handleSourceDragEvent(event->mimeData())) {
			event->acceptProposedAction();
		}
	}
	void dragMoveEvent(QGraphicsSceneDragDropEvent *event) {}
	void dropEvent(QGraphicsSceneDragDropEvent *event) {
		Utils::handleSourceDropEvent(event->mimeData());
	}
};

Displayer::Displayer(const UI_MainWindow& _ui, QWidget* parent)
	: QGraphicsView(parent), ui(_ui), m_scaleThread(std::bind(&Displayer::scaleThread, this)) {
	m_scene = new GraphicsScene();
	setScene(m_scene);
	setBackgroundBrush(Qt::gray);
	setRenderHint(QPainter::Antialiasing);

	m_rotateMode = RotateMode::AllPages;
	ui.actionRotateCurrentPage->setData(static_cast<int>(RotateMode::CurrentPage));
	ui.actionRotateAllPages->setData(static_cast<int>(RotateMode::AllPages));

	m_renderTimer.setSingleShot(true);
	m_scaleTimer.setSingleShot(true);

	ui.actionRotateLeft->setData(270.0);
	ui.actionRotateRight->setData(90.0);

	connect(ui.menuRotation, SIGNAL(triggered(QAction*)), this, SLOT(setRotateMode(QAction*)));
	connect(ui.actionRotateLeft, SIGNAL(triggered()), this, SLOT(rotate90()));
	connect(ui.actionRotateRight, SIGNAL(triggered()), this, SLOT(rotate90()));
	connect(ui.spinBoxRotation, SIGNAL(valueChanged(double)), this, SLOT(setAngle(double)));
	connect(ui.spinBoxPage, SIGNAL(valueChanged(int)), this, SLOT(queueRenderImage()));
	connect(ui.spinBoxBrightness, SIGNAL(valueChanged(int)), this, SLOT(queueRenderImage()));
	connect(ui.spinBoxContrast, SIGNAL(valueChanged(int)), this, SLOT(queueRenderImage()));
	connect(ui.spinBoxResolution, SIGNAL(valueChanged(int)), this, SLOT(queueRenderImage()));
	connect(ui.checkBoxInvertColors, SIGNAL(toggled(bool)), this, SLOT(queueRenderImage()));
	connect(ui.actionZoomIn, SIGNAL(triggered()), this, SLOT(zoomIn()));
	connect(ui.actionZoomOut, SIGNAL(triggered()), this, SLOT(zoomOut()));
	connect(ui.actionBestFit, SIGNAL(triggered()), this, SLOT(zoomFit()));
	connect(ui.actionOriginalSize, SIGNAL(triggered()), this, SLOT(zoomOriginal()));
	connect(&m_renderTimer, SIGNAL(timeout()), this, SLOT(renderImage()));
	connect(&m_scaleTimer, SIGNAL(timeout()), this, SLOT(scaleTimerElapsed()));
}

Displayer::~Displayer() {
	setSources(QList<Source*>());
	delete m_scene;
}

bool Displayer::setSources(QList<Source*> sources) {
	if(sources == m_sources) {
		return true;
	}

	m_scaleTimer.stop();
	if(m_scaleThread.isRunning()) {
		sendScaleRequest({ScaleRequest::Abort});
		sendScaleRequest({ScaleRequest::Quit});
		while(m_scaleThread.isRunning()) {
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		m_scaleRequests.clear();
	}
	if(m_tool) {
		m_tool->reset();
	}
	m_renderTimer.stop();
	m_scene->clear();
	delete m_renderer;
	m_renderer = nullptr;
	m_currentSource = nullptr;
	m_sources.clear();
	m_pageMap.clear();
	m_pixmap = QPixmap();
	m_imageItem = nullptr;
	ui.actionBestFit->setChecked(true);
	ui.actionPage->setVisible(false);
	ui.spinBoxPage->blockSignals(true);
	ui.spinBoxPage->setRange(1, 1);
	ui.spinBoxPage->blockSignals(false);
	Utils::setSpinBlocked(ui.spinBoxRotation, 0);
	Utils::setSpinBlocked(ui.spinBoxBrightness, 0);
	Utils::setSpinBlocked(ui.spinBoxContrast, 0);
	Utils::setSpinBlocked(ui.spinBoxResolution, 100);
	ui.checkBoxInvertColors->blockSignals(true);
	ui.checkBoxInvertColors->setChecked(false);
	ui.checkBoxInvertColors->blockSignals(false);
	ui.actionBestFit->setChecked(true);
	ui.actionOriginalSize->setChecked(false);
	ui.actionZoomIn->setEnabled(true);
	ui.actionZoomOut->setEnabled(true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	unsetCursor();

	m_sources = sources;

	if(m_sources.isEmpty()) {
		return false;
	}

	int page = 0;
	for(Source* source : m_sources) {
		DisplayRenderer* renderer;
		if(source->path.endsWith(".pdf", Qt::CaseInsensitive)) {
			renderer = new PDFRenderer(source->path, source->password);
		} else if(source->path.endsWith(".djvu", Qt::CaseInsensitive)) {
			renderer = new DJVURenderer(source->path);
		} else {
			renderer = new ImageRenderer(source->path);
		}
		source->angle.resize(renderer->getNPages());
		for(int iPage = 1, nPages = renderer->getNPages(); iPage <= nPages; ++iPage) {
			m_pageMap.insert(++page, qMakePair(source, iPage));
		}
		delete renderer;
	}
	if(page == 0) {
		m_pageMap.clear();
		m_sources.clear();
		return false;
	}

	ui.spinBoxPage->blockSignals(true);
	ui.spinBoxPage->setMaximum(page);
	ui.spinBoxPage->blockSignals(false);
	ui.actionPage->setVisible(page > 1);
	m_imageItem = new QGraphicsPixmapItem();
	m_imageItem->setTransformationMode(Qt::SmoothTransformation);
	m_scene->addItem(m_imageItem);
	if(!renderImage()) {
		Q_ASSERT(m_currentSource);
		QMessageBox::critical(this, _("Failed to load image"), _("The file might not be an image or be corrupt:\n%1").arg(m_currentSource->displayname));
		setSources(QList<Source*>());
		return false;
	}
	return true;
}

bool Displayer::setup(const int* page, const int* resolution, const double* angle) {
	bool changed = false;
	if(page) {
		changed |= *page != ui.spinBoxPage->value();
		Utils::setSpinBlocked(ui.spinBoxPage, *page);
	}
	if(resolution) {
		changed |= *resolution != ui.spinBoxResolution->value();
		Utils::setSpinBlocked(ui.spinBoxResolution, *resolution);
	}
	if(changed && !renderImage()) {
		return false;
	}
	if(angle) {
		setAngle(*angle);
	}
	return true;
}

void Displayer::queueRenderImage() {
	m_renderTimer.start(500);
}

bool Displayer::renderImage() {
	if(m_sources.isEmpty()) {
		return false;
	}
	int page = ui.spinBoxPage->value();

	// Set current source according to selected page
	Source* source = m_pageMap[page].first;
	if(!source) {
		return false;
	}

	int oldResolution = m_currentSource ? m_currentSource->resolution : -1;
	int oldPage = m_currentSource ? m_currentSource->page : -1;
	Source* oldSource = m_currentSource;

	if(source != m_currentSource) {
		sendScaleRequest({ScaleRequest::Abort});
		sendScaleRequest({ScaleRequest::Quit});
		while(m_scaleThread.isRunning()) {
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		m_scaleRequests.clear();
		delete m_renderer;
		if(source->path.endsWith(".pdf", Qt::CaseInsensitive)) {
			m_renderer = new PDFRenderer(source->path, source->password);
			if(source->resolution == -1) source->resolution = 300;
		} else if(source->path.endsWith(".djvu", Qt::CaseInsensitive)) {
			m_renderer = new DJVURenderer(source->path);
			if(source->resolution == -1) source->resolution = 300;
		} else {
			m_renderer = new ImageRenderer(source->path);
			if(source->resolution == -1) source->resolution = 100;
		}

		Utils::setSpinBlocked(ui.spinBoxResolution, source->resolution);
		Utils::setSpinBlocked(ui.spinBoxBrightness, source->brightness);
		Utils::setSpinBlocked(ui.spinBoxContrast, source->contrast);
		ui.checkBoxInvertColors->blockSignals(true);
		ui.checkBoxInvertColors->setChecked(source->invert);
		ui.checkBoxInvertColors->blockSignals(false);
		m_currentSource = source;
		m_scaleThread.start();
	}

	// Update source struct
	m_currentSource->page = m_pageMap[ui.spinBoxPage->value()].second;
	m_currentSource->brightness = ui.spinBoxBrightness->value();
	m_currentSource->contrast = ui.spinBoxContrast->value();
	m_currentSource->resolution = ui.spinBoxResolution->value();
	m_currentSource->invert = ui.checkBoxInvertColors->isChecked();

	// Notify tools about changes
	if(m_tool) {
		if(m_currentSource != oldSource || m_currentSource->page != oldPage) {
			m_tool->pageChanged();
		}
		if(oldResolution != m_currentSource->resolution) {
			double factor = double(m_currentSource->resolution) / double(oldResolution);
			m_tool->resolutionChanged(factor);
		}
	}

	Utils::setSpinBlocked(ui.spinBoxRotation, m_currentSource->angle[m_currentSource->page - 1]);

	// Render new image
	sendScaleRequest({ScaleRequest::Abort});
	QImage image = m_renderer->render(m_currentSource->page, m_currentSource->resolution);
	if(image.isNull()) {
		return false;
	}
	m_renderer->adjustImage(image, m_currentSource->brightness, m_currentSource->contrast, m_currentSource->invert);
	m_pixmap = QPixmap::fromImage(image);
	m_imageItem->setPixmap(m_pixmap);
	m_imageItem->setScale(1.);
	m_imageItem->setTransformOriginPoint(m_imageItem->boundingRect().center());
	m_imageItem->setPos(m_imageItem->pos() - m_imageItem->sceneBoundingRect().center());
	m_scene->setSceneRect(m_imageItem->sceneBoundingRect());
	centerOn(sceneRect().center());
	setAngle(ui.spinBoxRotation->value());
	if(m_scale < 1.0) {
		m_pendingScaleRequest = {ScaleRequest::Scale, m_scale, m_currentSource->resolution, m_currentSource->page, m_currentSource->brightness, m_currentSource->contrast, m_currentSource->invert};
		m_scaleTimer.start(100);
	}
	return true;
}

int Displayer::getCurrentPage() const {
	return ui.spinBoxPage->value();
}

int Displayer::getNPages() const {
	return ui.spinBoxPage->maximum();
}

double Displayer::getCurrentAngle() const {
	return ui.spinBoxRotation->value();
}

int Displayer::getCurrentResolution() const {
	return ui.spinBoxResolution->value();
}

QString Displayer::getCurrentImage(int& page) const {
	page = m_pageMap[ui.spinBoxPage->value()].second;
	return m_pageMap[ui.spinBoxPage->value()].first ? m_pageMap[ui.spinBoxPage->value()].first->path : "";
}

bool Displayer::hasMultipleOCRAreas() {
	return m_tool->hasMultipleOCRAreas();
}

QList<QImage> Displayer::getOCRAreas() {
	return m_tool->getOCRAreas();
}

bool Displayer::allowAutodetectOCRAreas() const {
	return m_tool->allowAutodetectOCRAreas();
}

void Displayer::autodetectOCRAreas() {
	m_tool->autodetectOCRAreas();
}

void Displayer::setZoom(Zoom action, ViewportAnchor anchor) {
	if(!m_imageItem) {
		return;
	}
	sendScaleRequest({ScaleRequest::Abort});
	setUpdatesEnabled(false);

	QRectF bb = m_imageItem->sceneBoundingRect();
	double fit = std::min(viewport()->width() / bb.width(), viewport()->height() / bb.height());

	if(action == Zoom::Original) {
		m_scale = 1.0;
	} else if(action == Zoom::In) {
		m_scale = std::min(10., m_scale * 1.25);
	} else if(action == Zoom::Out) {
		m_scale = std::max(0.05, m_scale * 0.8);
	}
	ui.actionBestFit->setChecked(false);
	if(action == Zoom::Fit || (m_scale / fit >= 0.9 && m_scale / fit <= 1.09)) {
		m_scale = fit;
		ui.actionBestFit->setChecked(true);
	}
	Qt::ScrollBarPolicy scrollPolicy = m_scale <= fit ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded;
	setHorizontalScrollBarPolicy(scrollPolicy);
	setVerticalScrollBarPolicy(scrollPolicy);
	ui.actionOriginalSize->setChecked(m_scale == 1.0);
	ui.actionZoomIn->setEnabled(m_scale < 10);
	ui.actionZoomOut->setEnabled(m_scale > 0.01);
	setTransformationAnchor(anchor);
	QTransform t;
	t.scale(m_scale, m_scale);
	setTransform(t);
	if(m_scale < 1.0) {
		m_pendingScaleRequest = {ScaleRequest::Scale, m_scale, m_currentSource->resolution, m_currentSource->page, m_currentSource->brightness, m_currentSource->contrast, m_currentSource->invert};
		m_scaleTimer.start(100);
	} else {
		m_imageItem->setPixmap(m_pixmap);
		m_imageItem->setScale(1.);
		m_imageItem->setTransformOriginPoint(m_imageItem->boundingRect().center());
		m_imageItem->setPos(m_imageItem->pos() - m_imageItem->sceneBoundingRect().center());
	}
	setUpdatesEnabled(true);
	update();
}

void Displayer::setAngle(double angle) {
	if(m_imageItem) {
		angle = angle < 0.0 ? angle + 360.0 : angle >= 360.0 ? angle - 360.0 : angle,
		Utils::setSpinBlocked(ui.spinBoxRotation, angle);
		int sourcePage = m_pageMap[getCurrentPage()].second;
		double delta = angle - m_currentSource->angle[sourcePage - 1];
		if(m_rotateMode == RotateMode::CurrentPage) {
			m_currentSource->angle[sourcePage - 1] = angle;
		} else if(delta != 0) {
			for(int page : m_pageMap.keys()) {
				auto pair = m_pageMap[page];
				double newangle = pair.first->angle[pair.second - 1] + delta;
				newangle = newangle < 0.0 ? newangle + 360.0 : newangle >= 360.0 ? newangle - 360.0 : newangle,
				pair.first->angle[pair.second - 1] = newangle;
			}
		}
		m_imageItem->setRotation(angle);
		if(m_tool && delta != 0) {
			m_tool->rotationChanged(delta);
		}
		m_scene->setSceneRect(m_imageItem->sceneBoundingRect());
		if(ui.actionBestFit->isChecked()) {
			setZoom(Zoom::Fit);
		}
	}
}

void Displayer::rotate90() {
	setAngle(ui.spinBoxRotation->value() + qobject_cast<QAction*>(QObject::sender())->data().toDouble());
}

void Displayer::resizeEvent(QResizeEvent *event) {
	QGraphicsView::resizeEvent(event);
	if(ui.actionBestFit->isChecked()) {
		setZoom(Zoom::Fit);
	}
}

void Displayer::keyPressEvent(QKeyEvent *event) {
	if(event->key() == Qt::Key_PageUp) {
		ui.spinBoxPage->setValue(ui.spinBoxPage->value() - 1);
		event->accept();
	} else if(event->key() == Qt::Key_PageDown) {
		ui.spinBoxPage->setValue(ui.spinBoxPage->value() + 1);
		event->accept();
	} else {
		QGraphicsView::keyPressEvent(event);
	}
}

void Displayer::mousePressEvent(QMouseEvent *event) {
	if(event->button() == Qt::MiddleButton) {
		m_panPos = event->pos();
	} else {
		event->ignore();
		QGraphicsView::mousePressEvent(event);
		if(!event->isAccepted() && m_tool && m_currentSource) {
			m_tool->mousePressEvent(event);
		}
	}
}

void Displayer::mouseMoveEvent(QMouseEvent *event) {
	if((event->buttons() & Qt::MiddleButton) == Qt::MiddleButton) {
		QPoint delta = event->pos() - m_panPos;
		horizontalScrollBar()->setValue( horizontalScrollBar()->value() - delta.x() );
		verticalScrollBar()->setValue( verticalScrollBar()->value() - delta.y() );
		m_panPos = event->pos();
	} else {
		event->ignore();
		QGraphicsView::mouseMoveEvent(event);
		if(!event->isAccepted() && m_tool && m_currentSource) {
			m_tool->mouseMoveEvent(event);
		}
	}
}

void Displayer::mouseReleaseEvent(QMouseEvent *event) {
	event->ignore();
	QGraphicsView::mouseReleaseEvent(event);
	if(!event->isAccepted() && m_tool && m_currentSource) {
		m_tool->mouseReleaseEvent(event);
	}
}

void Displayer::wheelEvent(QWheelEvent *event) {
	if(event->modifiers() & Qt::ControlModifier) {
		setZoom(event->delta() > 0 ? Zoom::In : Zoom::Out, QGraphicsView::AnchorUnderMouse);
		event->accept();
	} else if(event->modifiers() & Qt::ShiftModifier) {
		QScrollBar* hscroll = horizontalScrollBar();
		if(event->delta() < 0) {
			hscroll->setValue(hscroll->value() + hscroll->singleStep());
		} else {
			hscroll->setValue(hscroll->value() - hscroll->singleStep());
		}
		event->accept();
	} else {
		QGraphicsView::wheelEvent(event);
	}
}

QPointF Displayer::mapToSceneClamped(const QPoint &p) const {
	QPointF q = mapToScene(p);
	QRectF bb = m_imageItem->sceneBoundingRect();
	q.rx() = std::min(std::max(bb.x(), q.x()), bb.x() + bb.width());
	q.ry() = std::min(std::max(bb.y(), q.y()), bb.y() + bb.height());
	return q;
}

void Displayer::setRotateMode(QAction *action) {
	m_rotateMode = static_cast<RotateMode>(action->data().value<int>());
	ui.toolButtonRotation->setIcon(action->icon());
}

QImage Displayer::getImage(const QRectF& rect) {
	QImage image(rect.width(), rect.height(), QImage::Format_RGB32);
	image.fill(Qt::black);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	QTransform t;
	t.translate(-rect.x(), -rect.y());
	t.rotate(ui.spinBoxRotation->value());
	t.translate(-0.5 * m_pixmap.width(), -0.5 * m_pixmap.height());
	painter.setTransform(t);
	painter.drawPixmap(0, 0, m_pixmap);
	return image;
}

QRectF Displayer::getSceneBoundingRect() const {
	// We cannot use m_imageItem->sceneBoundingRect() since its pixmap
	// can currently be downscaled and therefore have slightly different
	// proportions.
	int width = m_pixmap.width();
	int height = m_pixmap.height();
	QRectF rect(width * -0.5, height * -0.5, width, height);
	QTransform transform;
	transform.rotate(ui.spinBoxRotation->value());
	return transform.mapRect(rect);
}

void Displayer::scaleTimerElapsed() {
	sendScaleRequest(m_pendingScaleRequest);
}

void Displayer::sendScaleRequest(const ScaleRequest& request) {
	m_scaleTimer.stop();
	m_scaleMutex.lock();
	m_scaleRequests.append(request);
	m_scaleCond.wakeOne();
	m_scaleMutex.unlock();
}

void Displayer::scaleThread() {
	m_scaleMutex.lock();
	while(true) {
		while(m_scaleRequests.isEmpty()) {
			m_scaleCond.wait(&m_scaleMutex);
		}
		ScaleRequest req = m_scaleRequests.takeFirst();
		if(req.type == ScaleRequest::Quit) {
			break;
		} else if(req.type == ScaleRequest::Scale) {
			m_scaleMutex.unlock();
			QImage image = m_renderer->render(req.page, req.scale * req.resolution);
			if(image.isNull()) {
				continue;
			}

			m_scaleMutex.lock();
			if(!m_scaleRequests.isEmpty() && m_scaleRequests.first().type == ScaleRequest::Abort) {
				m_scaleRequests.removeFirst();
				continue;
			}
			m_scaleMutex.unlock();

			m_renderer->adjustImage(image, req.brightness, req.contrast, req.invert);

			m_scaleMutex.lock();
			if(!m_scaleRequests.isEmpty() && m_scaleRequests.first().type == ScaleRequest::Abort) {
				m_scaleRequests.removeFirst();
				continue;
			}
			m_scaleMutex.unlock();

			QMetaObject::invokeMethod(this, "setScaledImage", Qt::BlockingQueuedConnection, Q_ARG(QImage, image), Q_ARG(double, m_scale));
			m_scaleMutex.lock();
		}
	}
	m_scaleMutex.unlock();
}

void Displayer::setScaledImage(const QImage &image, double scale) {
	m_scaleMutex.lock();
	if(!m_scaleRequests.isEmpty() && m_scaleRequests.first().type == ScaleRequest::Abort) {
		m_scaleRequests.removeFirst();
	} else {
		m_imageItem->setPixmap(QPixmap::fromImage(image));
		m_imageItem->setScale(1.0 / scale);
		m_imageItem->setTransformOriginPoint(m_imageItem->boundingRect().center());
		m_imageItem->setPos(m_imageItem->pos() - m_imageItem->sceneBoundingRect().center());
	}
	m_scaleMutex.unlock();
}

///////////////////////////////////////////////////////////////////////////////

void DisplayerSelection::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
	QColor c = QPalette().highlight().color();
	setBrush(QColor(c.red(), c.green(), c.blue(), 63));
	QPen pen;
	pen.setColor(c);
	pen.setWidth(1 / m_tool->getDisplayer()->getCurrentScale());
	setPen(pen);

	painter->setRenderHint(QPainter::Antialiasing, false);
	QGraphicsRectItem::paint(painter, option, widget);
	painter->setRenderHint(QPainter::Antialiasing, true);
}

void DisplayerSelection::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
	QPointF p = event->pos();
	QRectF r = rect();
	double tol = 10.0 / m_tool->getDisplayer()->getCurrentScale();

	bool left = std::abs(r.x() - p.x()) < tol;
	bool right = std::abs(r.x() + r.width() - p.x()) < tol;
	bool top = std::abs(r.y() - p.y()) < tol;
	bool bottom = std::abs(r.y() + r.height() - p.y()) < tol;

	if((top && left) || (bottom && right)) {
		setCursor(Qt::SizeFDiagCursor);
	} else if((top && right) || (bottom && left)) {
		setCursor(Qt::SizeBDiagCursor);
	} else if(top || bottom) {
		setCursor(Qt::SizeVerCursor);
	} else if(left || right) {
		setCursor(Qt::SizeHorCursor);
	} else {
		unsetCursor();
	}
}

void DisplayerSelection::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	QPointF p = event->pos();
	double tol = 10.0 / m_tool->getDisplayer()->getCurrentScale();
	m_resizeHandlers.clear();
	m_resizeOffset = QPointF(0.0, 0.0);
	if(std::abs(m_point.x() - p.x()) < tol) { // pointx
		m_resizeHandlers.append(resizePointX);
		m_resizeOffset.setX(event->pos().x() - m_point.x());
	} else if(std::abs(m_anchor.x() - p.x()) < tol) { // anchorx
		m_resizeHandlers.append(resizeAnchorX);
		m_resizeOffset.setX(event->pos().x() - m_anchor.x());
	}
	if(std::abs(m_point.y() - p.y()) < tol) { // pointy
		m_resizeHandlers.append(resizePointY);
		m_resizeOffset.setY(event->pos().y() - m_point.y());
	} else if(std::abs(m_anchor.y() - p.y()) < tol) { // anchory
		m_resizeHandlers.append(resizeAnchorY);
		m_resizeOffset.setY(event->pos().y() - m_anchor.y());
	}
	if(!m_resizeHandlers.empty()) {
		event->accept();
	} else {
		event->ignore();
	}
}

void DisplayerSelection::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	QPointF p = event->pos() - m_resizeOffset;
	QRectF bb = m_tool->getDisplayer()->getSceneBoundingRect();
	p.rx() = std::min(std::max(bb.x(), p.x()), bb.x() + bb.width());
	p.ry() = std::min(std::max(bb.y(), p.y()), bb.y() + bb.height());
	if(!m_resizeHandlers.isEmpty()) {
		for(const ResizeHandler& handler : m_resizeHandlers) {
			handler(p, m_anchor, m_point);
		}
		QRectF newRect = QRectF(m_anchor, m_point).normalized().united(m_minRect);
		setRect(newRect);
		emit geometryChanged(rect());
		event->accept();
	} else {
		event->ignore();
	}
}

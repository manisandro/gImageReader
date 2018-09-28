/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.hh
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

#ifndef DISPLAYER_HH
#define DISPLAYER_HH

#include <functional>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QImage>
#include <QMap>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QQueue>
#include <QWaitCondition>

class DisplayerTool;
class DisplayRenderer;
class Source;
class UI_MainWindow;
class GraphicsScene;

class Displayer : public QGraphicsView {
	Q_OBJECT
public:
	Displayer(const UI_MainWindow& _ui, QWidget* parent = nullptr);
	~Displayer();
	void setTool(DisplayerTool* tool) {
		m_tool = tool;
	}
	bool setSources(QList<Source*> sources);
	bool setup(const int* page = nullptr, const int* resolution = nullptr, const double* angle = nullptr);
	int getCurrentPage() const;
	int getNPages() const;
	int getCurrentResolution() const;
	double getCurrentAngle() const;
	double getCurrentScale() const {
		return m_scale;
	}
	QString getCurrentImage(int& page) const;
	QImage getImage(const QRectF& rect);
	QRectF getSceneBoundingRect() const;
	QPointF mapToSceneClamped(const QPoint& p) const;
	bool hasMultipleOCRAreas();
	QList<QImage> getOCRAreas();
	bool allowAutodetectOCRAreas() const;
	void setCursor(const QCursor& cursor) {
		viewport()->setCursor(cursor);
	}
	void unsetCursor() {
		viewport()->unsetCursor();
	}

public slots:
	void autodetectOCRAreas();

private:
	enum class RotateMode { CurrentPage, AllPages } m_rotateMode;
	enum class Zoom { In, Out, Fit, Original };
	const UI_MainWindow& ui;
	GraphicsScene* m_scene;
	QList<Source*> m_sources;
	QMap<int, QPair<Source*, int>> m_pageMap;
	Source* m_currentSource = nullptr;
	DisplayRenderer* m_renderer = nullptr;
	QPixmap m_pixmap;
	QGraphicsPixmapItem* m_imageItem = nullptr;
	double m_scale = 1.0;
	DisplayerTool* m_tool = nullptr;
	QPoint m_panPos;
	QTimer m_renderTimer;

	void keyPressEvent(QKeyEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

	void setZoom(Zoom action, QGraphicsView::ViewportAnchor anchor = QGraphicsView::AnchorViewCenter);

	struct ScaleRequest {
		enum Request { Scale, Abort, Quit } type;
		double scale;
		int resolution;
		int page;
		int brightness;
		int contrast;
		bool invert;
	};
	class ScaleThread : public QThread {
	public:
		ScaleThread(const std::function<void()>& f) : m_f(f) {}
	private:
		std::function<void()> m_f;
		void run() {
			m_f();
		}
	};
	QMutex m_scaleMutex;
	QWaitCondition m_scaleCond;
	QQueue<ScaleRequest> m_scaleRequests;
	QTimer m_scaleTimer;
	ScaleRequest m_pendingScaleRequest;
	ScaleThread m_scaleThread;

	void scaleThread();

private slots:
	void queueRenderImage();
	void scaleTimerElapsed();
	void sendScaleRequest(const ScaleRequest& request);
	bool renderImage();
	void rotate90();
	void setAngle(double angle);
	void setRotateMode(QAction* action);
	void setScaledImage(const QImage& image, double scale);
	void zoomIn() {
		setZoom(Zoom::In);
	}
	void zoomOut() {
		setZoom(Zoom::Out);
	}
	void zoomFit() {
		setZoom(Zoom::Fit);
	}
	void zoomOriginal() {
		setZoom(Zoom::Original);
	}
};


class DisplayerTool : public QObject {
public:
	DisplayerTool(Displayer* displayer, QObject* parent = 0) : QObject(parent), m_displayer(displayer) {}
	virtual ~DisplayerTool() {}
	virtual void mousePressEvent(QMouseEvent* /*event*/) {}
	virtual void mouseMoveEvent(QMouseEvent* /*event*/) {}
	virtual void mouseReleaseEvent(QMouseEvent* /*event*/) {}
	virtual void pageChanged() {}
	virtual void resolutionChanged(double /*factor*/) {}
	virtual void rotationChanged(double /*delta*/) {}
	virtual QList<QImage> getOCRAreas() = 0;
	virtual bool hasMultipleOCRAreas() const {
		return false;
	}
	virtual bool allowAutodetectOCRAreas() const {
		return false;
	}
	virtual void autodetectOCRAreas() {}
	virtual void reset() {}

	Displayer* getDisplayer() const {
		return m_displayer;
	}

protected:
	Displayer* m_displayer;
};

class DisplayerSelection : public QObject, public QGraphicsRectItem {
	Q_OBJECT
public:
	DisplayerSelection(DisplayerTool* tool, const QPointF& anchor)
		: QGraphicsRectItem(QRectF(anchor, anchor)), m_tool(tool), m_anchor(anchor), m_point(anchor) {
		setAcceptHoverEvents(true);
		setZValue(10);
	}
	void setAnchorAndPoint(const QPointF& anchor, const QPointF& point) {
		m_anchor = anchor;
		m_point = point;
		setRect(QRectF(m_anchor, m_point).normalized());
	}
	void setMinimumRect(const QRectF& rect) {
		m_minRect = rect;
	}
	void setPoint(const QPointF& point) {
		m_point = point;
		setRect(QRectF(m_anchor, m_point).normalized());
	}
	void rotate(const QTransform& transform) {
		m_anchor = transform.map(m_anchor);
		m_point = transform.map(m_point);
		setRect(QRectF(m_anchor, m_point).normalized());
	}
	void scale(double factor) {
		m_anchor *= factor;
		m_point *= factor;
	}

protected:
	DisplayerTool* m_tool;

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
	void geometryChanged(QRectF rect);

private:
	typedef void(*ResizeHandler)(const QPointF&, QPointF&, QPointF&);

	QPointF m_anchor;
	QPointF m_point;
	QRectF m_minRect;
	QVector<ResizeHandler> m_resizeHandlers;
	QPointF m_resizeOffset;

	void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

	static void resizeAnchorX(const QPointF& pos, QPointF& anchor, QPointF& /*point*/) {
		anchor.rx() = pos.x();
	}
	static void resizeAnchorY(const QPointF& pos, QPointF& anchor, QPointF& /*point*/) {
		anchor.ry() = pos.y();
	}
	static void resizePointX(const QPointF& pos, QPointF& /*anchor*/, QPointF& point) {
		point.rx() = pos.x();
	}
	static void resizePointY(const QPointF& pos, QPointF& /*anchor*/, QPointF& point) {
		point.ry() = pos.y();
	}
};

#endif // IMAGEDISPLAYER_HH

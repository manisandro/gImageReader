/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.hh
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

#ifndef DISPLAYER_HH
#define DISPLAYER_HH

#include <functional>
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
	void setTool(DisplayerTool* tool) { m_tool = tool; }
	bool setSources(QList<Source*> sources);
	int getCurrentPage() const;
	double getCurrentAngle() const;
	int getCurrentResolution() const;
	const QString& getCurrentImage(int& page) const;
	int getNPages() const;
	bool hasMultipleOCRAreas();
	QList<QImage> getOCRAreas();
	void autodetectOCRAreas();

public slots:
	bool setCurrentPage(int page);
	void setAngle(double angle);
	void setResolution(int resolution);

private:
	friend class DisplayerTool;

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

	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void resizeEvent(QResizeEvent *event);
	void wheelEvent(QWheelEvent *event);

	QImage getImage(const QRectF& rect);
	QPointF mapToSceneClamped(const QPoint& p) const;
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
		ScaleThread(const std::function<void()> &f) : m_f(f) {}
	private:
		std::function<void()> m_f;
		void run(){ m_f(); }
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
	void scaleTimerElapsed(){ sendScaleRequest(m_pendingScaleRequest); }
	void sendScaleRequest(const ScaleRequest& request);
	bool renderImage();
	void rotate90();
	void setScaledImage(const QImage& image, double scale);
	void zoomIn(){ setZoom(Zoom::In); }
	void zoomOut(){ setZoom(Zoom::Out); }
	void zoomFit(){ setZoom(Zoom::Fit); }
	void zoomOriginal(){ setZoom(Zoom::Original); }
};


class DisplayerTool : public QObject {
public:
	DisplayerTool(Displayer* displayer, QObject* parent = 0) : QObject(parent), m_displayer(displayer) {}
	virtual ~DisplayerTool(){}
	virtual void mousePressEvent(QMouseEvent */*event*/){}
	virtual void mouseMoveEvent(QMouseEvent */*event*/){}
	virtual void mouseReleaseEvent(QMouseEvent */*event*/){}
	virtual void pageChanged(){}
	virtual void resolutionChanged(double /*factor*/){}
	virtual void rotationChanged(double /*delta*/){}
	virtual QList<QImage> getOCRAreas() = 0;
	virtual bool hasMultipleOCRAreas() const{ return false; }
	virtual void autodetectOCRAreas(){}

protected:
	Displayer* m_displayer;

	void addItemToScene(QGraphicsItem* item);
	QRectF getSceneBoundingRect() const;
	void invalidateRect(const QRectF& rect);
	QPointF mapToSceneClamped(const QPoint& point);
	QImage getImage(const QRectF& rect);
	double getDisplayScale() const;
};

#endif // IMAGEDISPLAYER_HH

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.hh
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

#ifndef DISPLAYER_HH
#define DISPLAYER_HH

#include "Recognizer.hh"

#include <functional>
#include <QGraphicsView>
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QQueue>
#include <QWaitCondition>

class DisplaySelection;
class DisplayRenderer;
class Source;
class UI_MainWindow;
class QGraphicsScene;

class Displayer : public QGraphicsView {
	Q_OBJECT
public:
	Displayer(const UI_MainWindow& _ui, QWidget* parent = nullptr);
	~Displayer(){ setSource(nullptr); }
	bool setCurrentPage(int page);
	int getCurrentPage() const;
	int getNPages() const;
	QList<QImage> getSelections();
	bool getHasSelections() const{ return !m_selections.isEmpty(); }
	bool setSource(Source* source);

public slots:
	void autodetectLayout(bool rotated = false);

signals:
	void selectionChanged(bool haveSelection);

private:
	friend class DisplaySelection;

	enum class Zoom { In, Out, Fit, Original };
	const UI_MainWindow& ui;
	QGraphicsScene m_scene;
	Source* m_source = nullptr;
	DisplayRenderer* m_renderer = nullptr;
	QPixmap m_pixmap;
	QGraphicsPixmapItem* m_imageItem = nullptr;
	double m_scale = 1.0;
	QTimer m_renderTimer;

	DisplaySelection* m_curSel = nullptr;
	QList<DisplaySelection*> m_selections;

	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void resizeEvent(QResizeEvent *event);
	void wheelEvent(QWheelEvent *event);

	void clearSelections();
	void removeSelection(int num);
	void reorderSelection(int oldNum, int newNum);
	void saveSelection(DisplaySelection* selection);

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
	void setRotation(double angle);
	void setScaledImage(const QImage& image, double scale);
	void zoomIn(){ setZoom(Zoom::In); }
	void zoomOut(){ setZoom(Zoom::Out); }
	void zoomFit(){ setZoom(Zoom::Fit); }
	void zoomOriginal(){ setZoom(Zoom::Original); }
};

#endif // IMAGEDISPLAYER_HH

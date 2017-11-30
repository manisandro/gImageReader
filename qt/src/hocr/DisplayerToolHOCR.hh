/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolHOCR.hh
 * Copyright (C) 2016-2017 Sandro Mani <manisandro@gmail.com>
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

#ifndef DISPLAYERTOOLHOCR_HH
#define DISPLAYERTOOLHOCR_HH

#include "Displayer.hh"

class DisplayerToolHOCR : public DisplayerTool {
	Q_OBJECT
public:
	DisplayerToolHOCR(Displayer* displayer, QObject* parent = 0);
	~DisplayerToolHOCR();

	QList<QImage> getOCRAreas() override;
	void pageChanged() override {
		emit displayedSourceChanged();
		clearSelection();
	}
	void resolutionChanged(double /*factor*/) override {
		clearSelection();
	}
	void rotationChanged(double /*delta*/) override {
		clearSelection();
	}
	void reset() override {
		clearSelection();
	}

	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

	void activateDrawSelection() {
		m_drawingSelection = true;
	}
	void setSelection(const QRect& rect);
	QImage getSelection(const QRect& rect);
	void clearSelection();

signals:
	void displayedSourceChanged();
	void selectionDrawn(QRect rect);
	void selectionGeometryChanged(QRect rect);

private:
	DisplayerSelection* m_selection = nullptr;
	bool m_drawingSelection = false;

private slots:
	void selectionChanged(QRectF rect);
};

#endif // DISPLAYERTOOLHOCR_HH

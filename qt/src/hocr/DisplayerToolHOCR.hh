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
	enum Action {ACTION_NONE, ACTION_DRAW_RECT};

	DisplayerToolHOCR(Displayer* displayer, QObject* parent = 0);
	~DisplayerToolHOCR();

	QList<QImage> getOCRAreas() override;
	void pageChanged() override {
		emit displayedSourceChanged();
		reset();
	}
	void resolutionChanged(double /*factor*/) override {
		reset();
	}
	void rotationChanged(double /*delta*/) override {
		reset();
	}
	void reset() override {
		setAction(ACTION_NONE, true);
	}

	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

	void setAction(Action action, bool clearSel = true);
	void setSelection(const QRect& rect);
	QImage getSelection(const QRect& rect) const;
	void clearSelection();

signals:
	void displayedSourceChanged();
	void bboxDrawn(QRect rect);
	void bboxChanged(QRect rect);
	void positionPicked(QPoint pos);
	void actionChanged(int action);

private:
	DisplayerSelection* m_selection = nullptr;
	Action m_currentAction = ACTION_NONE;

private slots:
	void selectionChanged(QRectF rect);
};

#endif // DISPLAYERTOOLHOCR_HH

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolHOCR.hh
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

#ifndef DISPLAYERTOOLHOCR_HH
#define DISPLAYERTOOLHOCR_HH

#include "Displayer.hh"

class DisplayerToolHOCR : public DisplayerTool
{
public:
	DisplayerToolHOCR(Displayer* displayer, QObject* parent = 0);
	~DisplayerToolHOCR();

	QList<QImage> getOCRAreas();
	virtual void pageChanged(){ clearSelection(); }
	virtual void resolutionChanged(double /*factor*/){ clearSelection(); }
	virtual void rotationChanged(double /*delta*/){ clearSelection(); }

	void setSelection(const QRect& rect);
	QImage getSelection(const QRect& rect);
	void clearSelection();

private:
	class GraphicsRectItem;
	GraphicsRectItem* m_selection = nullptr;
};

#endif // DISPLAYERTOOLHOCR_HH

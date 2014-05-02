/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplaySelection.hh
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

#ifndef DISPLAYSELECTION_HH
#define DISPLAYSELECTION_HH

#include <vector>
#include "Geometry.hh"

class DisplaySelectionHandle;

class DisplaySelection {
public:
	DisplaySelection(const Geometry::Point& anchor);
	DisplaySelection(double x, double y, double width, double height);
	Geometry::Rectangle setPoint(const Geometry::Point& p, const std::vector<int> idx);
	void rotate(const Geometry::Rotation& R);
	DisplaySelectionHandle* getResizeHandle(const Geometry::Point& p, double scale);
	void draw(Cairo::RefPtr<Cairo::Context> ctx, double scale, const Glib::ustring& idx, Gdk::RGBA* colors) const;

	const Geometry::Point* getPoints() const{ return m_points; }
	const Geometry::Rectangle& getRect() const{ return m_rect; }
	bool isEmpty() const{ return m_rect.width < 5 || m_rect.height < 5; }


private:
	Geometry::Point m_points[2];
	Geometry::Rectangle m_rect;
};

class DisplaySelectionHandle {
public:
	DisplaySelectionHandle(DisplaySelection* sel, const std::vector<int>& idx = {1, 0, 0, 1, 1, 1})
		: m_sel(sel), m_idx(idx) {}

	Geometry::Rectangle setPoint(const Geometry::Point& point){ return m_sel->setPoint(point, m_idx); }
	DisplaySelection* getSelection(){ return m_sel; }
	Gdk::CursorType getResizeCursor() const;

private:
	DisplaySelection* m_sel;
	std::vector<int> m_idx;
};

#endif // DISPLAYSELECTION_HH

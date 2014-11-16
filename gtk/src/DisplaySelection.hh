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

class DisplaySelection {
public:
	typedef void(*ResizeHandle)(const Geometry::Point&, Geometry::Point*);

	struct Handle {
		DisplaySelection* sel;
		std::vector<ResizeHandle> resizeHandles;
		Geometry::Point resizeOffset;

		void setPoint(const Geometry::Point& p){ sel->setPoint(p, this); }
	};

	DisplaySelection(double x, double y, double width = 0., double height = 0.);
	void setPoint(const Geometry::Point& p, const Handle* handle);
	void rotate(const Geometry::Rotation& R);
	Handle* getResizeHandle(const Geometry::Point& p, double scale);
	Gdk::CursorType getResizeCursor(const Geometry::Point& p, double scale) const;
	void draw(Cairo::RefPtr<Cairo::Context> ctx, double scale, const Glib::ustring& idx) const;

	const Geometry::Rectangle& getRect() const{ return m_rect; }
	bool isEmpty() const{ return m_rect.width < 5 || m_rect.height < 5; }

private:
	Geometry::Point m_points[2];
	Geometry::Rectangle m_rect;

	static void resizeAnchorX(const Geometry::Point& pos, Geometry::Point* points){ points[0].x = pos.x; }
	static void resizeAnchorY(const Geometry::Point& pos, Geometry::Point* points){ points[0].y = pos.y; }
	static void resizePointX(const Geometry::Point& pos, Geometry::Point* points){ points[1].x = pos.x; }
	static void resizePointY(const Geometry::Point& pos, Geometry::Point* points){ points[1].y = pos.y; }
};

#endif // DISPLAYSELECTION_HH

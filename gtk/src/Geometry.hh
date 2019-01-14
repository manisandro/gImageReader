/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Geometry.hh
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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

#ifndef GEOMETRY_HH
#define GEOMETRY_HH

#include "common.hh"

namespace Geometry {

class Point {
public:
	union {
		double data[2];
		struct {
			double x, y;
		};
	};
	Point(double _x = 0., double _y = 0.) : x(_x), y(_y) {}
	double operator[](int i) const {
		return data[i];
	}
	double& operator[](int i) {
		return data[i];
	}
};

class Rotation {
public:
	Rotation(double angle = 0.) {
		double cosa = std::cos(angle), sina = std::sin(angle);
		m_data[0] = cosa;
		m_data[1] = -sina;
		m_data[2] = sina;
		m_data[3] = cosa;
	}
	Point rotate(const Point& p) const {
		return Point(m_data[0] * p.x + m_data[1] * p.y, m_data[2] * p.x + m_data[3] * p.y);
	}
	double operator()(int i, int j) const {
		return m_data[2 * i + j];
	}
private:
	double m_data[4];
};

class Rectangle {
public:
	double x, y;
	double width, height;

	Rectangle(double _x = 0., double _y = 0., double _width = -1., double _height = -1.)
		: x(_x), y(_y), width(_width), height(_height) {}
	Rectangle(const Point& p1, const Point& p2) {
		setCoords(p1.x, p1.y, p2.x, p2.y);
	}
	void setCoords(double x1, double y1, double x2, double y2) {
		x = std::min(x1, x2);
		y = std::min(y1, y2);
		width = std::abs(x2 - x1);
		height = std::abs(y2 - y1);
	}
	bool contains(const Point& p) const {
		if(isEmpty()) {
			return false;
		}
		return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
	}
	bool overlaps(const Rectangle& r) const {
		if(isEmpty() || r.isEmpty()) {
			return false;
		}
		return x < r.x + r.width && x + width > r.x && y < r.y + r.height && y + height > r.y;
	}
	Rectangle unite(const Rectangle& r) const {
		if(isEmpty() && r.isEmpty()) {
			return Rectangle();
		} else if(isEmpty()) {
			return r;
		} else if(r.isEmpty()) {
			return *this;
		}
		double _x = std::min(x, r.x);
		double _y = std::min(y, r.y);
		double _w = std::max(x + width, r.x + r.width) - _x;
		double _h = std::max(y + height, r.y + r.height) - _y;
		return Rectangle(_x, _y, _w, _h);
	}
	Rectangle translate(double dx, double dy) const {
		return Rectangle(x + dx, y + dy, width, height);
	}
	bool isEmpty() const {
		return width < 0. || height < 0.;
	}
};

}
#endif // GEOMETRY_HH

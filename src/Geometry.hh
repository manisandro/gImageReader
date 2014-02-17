/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Geometry.hh
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

#ifndef GEOMETRY_HH
#define GEOMETRY_HH

#include "common.hh"

namespace Geometry {

class Point {
public:
	union {
		double data[2];
		struct { double x, y; };
	};
	Point(double _x = 0., double _y = 0.) : x(_x), y(_y) {}
	double operator[](int i) const{ return data[i]; }
	double& operator[](int i){ return data[i]; }
	double sqrDist(const Point& p) const{
		double dx = this->x - p.x, dy = this->y - p.y;
		return dx*dx + dy*dy;
	}
};

class Rotation {
public:
	Rotation(double angle = 0.) {
		double cosa = std::cos(angle), sina = std::sin(angle);
		m_data[0] = cosa; m_data[1] = -sina;
		m_data[2] = sina; m_data[3] = cosa;
	}
	Point rotate(const Point& p) const{
		return Point(m_data[0] * p.x + m_data[1] * p.y, m_data[2] * p.x + m_data[3] * p.y);
	}
	double operator()(int i, int j) const{ return m_data[2*i + j]; }
	double& operator()(int i, int j){ return m_data[2*i + j]; }
private:
	double m_data[4];
};

class Rectangle {
public:
	double x, y;
	double width, height;

	Rectangle(double _x = 0., double _y = 0., double _width = 0., double _height = 0.)
		: x(_x), y(_y), width(_width), height(_height) {}
	Rectangle(const Rectangle& r)
		: x(r.x), y(r.y), width(r.width), height(r.height) {}
	Rectangle(const Point& p1, const Point& p2)
		: x(std::min(p1.x, p2.x)), y(std::min(p1.y, p2.y)),
		  width(std::abs(p2.x - p1.x)), height(std::abs(p2.y - p1.y)) {}
	Point center() const{
		return Point(x + 0.5 * width, y + 0.5 * height);
	}
	bool contains(const Point& p) const{
		return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
	}
	bool overlaps(const Rectangle& r) {
		return x < r.x + r.width && x + width > r.x && y < r.y + r.height && y + height > r.y;
	}

	Rectangle unite(const Rectangle& r) const{
		double _x = std::min(x, r.x);
		double _y = std::min(y, r.y);
		double _w = std::max(x + width, r.x + r.width) - _x;
		double _h = std::max(y + height, r.y + r.height) - _y;
		return Rectangle(_x, _y, _w, _h);
	}
};

}
#endif // GEOMETRY_HH

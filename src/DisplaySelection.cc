/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplaySelection.cc
 * Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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

#include "DisplaySelection.hh"
#include "Utils.hh"

DisplaySelection::DisplaySelection(const Geometry::Point &anchor)
{
	m_points[0] = anchor;
	m_points[1] = anchor;
	m_rect = Geometry::Rectangle(m_points[0], m_points[1]);
}

DisplaySelection::DisplaySelection(double x, double y, double width, double height)
{
	m_points[0] = Geometry::Point(x, y);
	m_points[1] = Geometry::Point(x + width, y + height);
	m_rect = Geometry::Rectangle(x, y, width, height);
}

Geometry::Rectangle DisplaySelection::setPoint(const Geometry::Point &p, const std::vector<int> idx)
{
	m_points[idx[0]][idx[1]] = p[idx[2]];
	if(idx.size() > 3){
		m_points[idx[3]][idx[4]] = p[idx[5]];
	}
	Geometry::Rectangle oldrect = m_rect;
	m_rect = Geometry::Rectangle(m_points[0], m_points[1]);
	return oldrect.unite(m_rect);
}

void DisplaySelection::rotate(const Geometry::Rotation &R)
{
	m_points[0] = R.rotate(m_points[0]);
	m_points[1] = R.rotate(m_points[1]);
	m_rect = Geometry::Rectangle(m_points[0], m_points[1]);
}

DisplaySelectionHandle* DisplaySelection::getResizeHandle(const Geometry::Point &p, double scale)
{
	double r = 5. / scale, rsq = r * r;
	if(p.sqrDist(m_points[0]) < rsq){
		return new DisplaySelectionHandle(this, {0, 0, 0, 0, 1, 1}); // Anchor.x, Anchor.y
	}else if(p.sqrDist(m_points[1]) < rsq){
		return new DisplaySelectionHandle(this, {1, 0, 0, 1, 1, 1}); // Point.x, Point.y
	}else if(p.sqrDist(Geometry::Point(m_points[0].x, m_points[1].y)) < rsq){
		return new DisplaySelectionHandle(this, {0, 0, 0, 1, 1, 1}); // Anchor.x, Point.y
	}else if(p.sqrDist(Geometry::Point(m_points[1].x, m_points[0].y)) < rsq){
		return new DisplaySelectionHandle(this, {1, 0, 0, 0, 1, 1}); // Point.x, Anchor.y
	}else if(p.x > m_rect.x && p.x < m_rect.x + m_rect.width){
		if(std::abs(p.y - m_points[0].y) < r){
			return new DisplaySelectionHandle(this, {0, 1, 1}); // Anchor.y
		}else if(std::abs(p.y - m_points[1].y) < r){
			return new DisplaySelectionHandle(this, {1, 1, 1}); // Point.y
		}
	}else if(p.y > m_rect.y && p.y < m_rect.y + m_rect.height){
		if(std::abs(p.x - m_points[0].x) < r){
			return new DisplaySelectionHandle(this, {0, 0, 0}); // Anchor.x
		}else if(std::abs(p.x - m_points[1].x) < r){
			return new DisplaySelectionHandle(this, {1, 0, 0}); // Point.x
		}
	}
	return nullptr;
}

void DisplaySelection::draw(Cairo::RefPtr<Cairo::Context> ctx, double scale, const Glib::ustring &idx, Gdk::RGBA *colors) const
{
	double d = 0.5 / scale;
	double x1 = Utils::round(m_rect.x * scale) / scale + d;
	double y1 = Utils::round(m_rect.y * scale) / scale + d;
	double x2 = Utils::round((m_rect.x + m_rect.width) * scale) / scale - d;
	double y2 = Utils::round((m_rect.y + m_rect.height) * scale) / scale - d;
	Geometry::Rectangle rect(x1, y1, x2 - x1, y2 - y1);
	ctx->save();
	// Semitransparent rectangle with frame
	ctx->set_line_width(2. * d);
	ctx->rectangle(rect.x, rect.y, rect.width, rect.height);
	ctx->set_source_rgba(colors[1].get_red(), colors[1].get_green(), colors[1].get_blue(), 0.25);
	ctx->fill_preserve();
	ctx->set_source_rgba(colors[1].get_red(), colors[1].get_green(), colors[1].get_blue(), 1.0);
	ctx->stroke();
	// Text box
	double w = std::min(std::min(40. * d, rect.width), rect.height);
	ctx->rectangle(rect.x, rect.y, w - d, w - d);
	ctx->set_source_rgba(colors[1].get_red(), colors[1].get_green(), colors[1].get_blue(), 1.0);
	ctx->fill();
	// Text
	ctx->select_font_face("sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
	ctx->set_font_size(0.7 * w);
	Cairo::TextExtents ext; ctx->get_text_extents(idx, ext);
	ctx->translate(rect.x + .5 * w, rect.y + .5 * w);
	ctx->translate(-ext.x_bearing - .5 * ext.width, -ext.y_bearing - .5 * ext.height);
	ctx->set_source_rgba(colors[0].get_red(), colors[0].get_green(), colors[1].get_blue(), 1.0);
	ctx->show_text(idx);
	ctx->restore();
}

Gdk::CursorType DisplaySelectionHandle::getResizeCursor() const
{
	const Geometry::Point* points = m_sel->getPoints();
	if(m_idx.size() == 3){
		if(m_idx[1] == 0){
			if(points[m_idx[0]][0] > points[1 - m_idx[0]][0]){
				return Gdk::RIGHT_SIDE;
			}else{
				return Gdk::LEFT_SIDE;
			}
		}else{
			if(points[m_idx[0]][1] > points[1 - m_idx[0]][1]){
				return Gdk::BOTTOM_SIDE;
			}else{
				return Gdk::TOP_SIDE;
			}
		}
	}else if(m_idx.size() == 6){
		if(points[m_idx[0]][0] > points[1 - m_idx[0]][0]){
			if(points[m_idx[3]][1] > points[1 - m_idx[3]][1]){
				return Gdk::BOTTOM_RIGHT_CORNER;
			}else{
				return Gdk::TOP_RIGHT_CORNER;
			}
		}else{
			if(points[m_idx[3]][1] > points[1 - m_idx[3]][1]){
				return Gdk::BOTTOM_LEFT_CORNER;
			}else{
				return Gdk::TOP_LEFT_CORNER;
			}
		}
	}
	return Gdk::CROSSHAIR;
}

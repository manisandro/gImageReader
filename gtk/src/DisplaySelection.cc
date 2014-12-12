/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplaySelection.cc
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

#include "DisplaySelection.hh"
#include "Utils.hh"

DisplaySelection::DisplaySelection(double x, double y, double width, double height)
{
	m_points[0] = Geometry::Point(x, y);
	m_points[1] = Geometry::Point(x + width, y + height);
	m_rect = Geometry::Rectangle(x, y, width, height);
}

void DisplaySelection::setPoint(const Geometry::Point &p, const Handle* handle)
{
	Geometry::Point pos(p.x - handle->resizeOffset.x, p.y - handle->resizeOffset.y);
	for(const ResizeHandle& h : handle->resizeHandles){
		h(pos, m_points);
	}
	m_rect = Geometry::Rectangle(m_points[0], m_points[1]);
}

void DisplaySelection::rotate(const Geometry::Rotation &R)
{
	m_points[0] = R.rotate(m_points[0]);
	m_points[1] = R.rotate(m_points[1]);
	m_rect = Geometry::Rectangle(m_points[0], m_points[1]);
}

void DisplaySelection::scale(double factor)
{
	m_points[0].x *= factor;
	m_points[0].y *= factor;
	m_points[1].x *= factor;
	m_points[1].y *= factor;
}

DisplaySelection::Handle* DisplaySelection::getResizeHandle(const Geometry::Point& p, double scale)
{
	double tol = 10.0 / scale;
	std::vector<ResizeHandle> resizeHandles;
	Geometry::Point resizeOffset;
	if(p.x > m_rect.x - tol && p.x < m_rect.x + m_rect.width + tol && p.y > m_rect.y - tol && p.y < m_rect.y + m_rect.height + tol){
		if(std::abs(m_points[1].x - p.x) < tol){ // pointx
			resizeHandles.push_back(resizePointX);
			resizeOffset.x = p.x - m_points[1].x;
		}else if(std::abs(m_points[0].x - p.x) < tol){ // anchorx
			resizeHandles.push_back(resizeAnchorX);
			resizeOffset.x = p.x - m_points[0].x;
		}
		if(std::abs(m_points[1].y - p.y) < tol){ // pointy
			resizeHandles.push_back(resizePointY);
			resizeOffset.y = p.y - m_points[1].y;
		}else if(std::abs(m_points[0].y - p.y) < tol){ // anchory
			resizeHandles.push_back(resizeAnchorY);
			resizeOffset.y = p.y - m_points[0].y;
		}
	}
	return !resizeHandles.empty() ? new DisplaySelection::Handle{this, resizeHandles, resizeOffset} : nullptr;
}

Gdk::CursorType DisplaySelection::getResizeCursor(const Geometry::Point& p, double scale) const
{
	double tol = 10.0 / scale;

	if(p.x > m_rect.x - tol && p.x < m_rect.x + m_rect.width + tol && p.y > m_rect.y - tol && p.y < m_rect.y + m_rect.height + tol){
		bool left = std::abs(m_rect.x - p.x) < tol;
		bool right = std::abs(m_rect.x + m_rect.width - p.x) < tol;
		bool top = std::abs(m_rect.y - p.y) < tol;
		bool bottom = std::abs(m_rect.y + m_rect.height - p.y) < tol;

		if(top){
			return left ? Gdk::TOP_LEFT_CORNER : right ? Gdk::TOP_RIGHT_CORNER : Gdk::TOP_SIDE;
		}else if(bottom){
			return left ? Gdk::BOTTOM_LEFT_CORNER : right ? Gdk::BOTTOM_RIGHT_CORNER : Gdk::BOTTOM_SIDE;
		}else{
			return left ? Gdk::LEFT_SIDE : right ? Gdk::RIGHT_SIDE : Gdk::BLANK_CURSOR;
		}
	}
	return Gdk::BLANK_CURSOR;
}

void DisplaySelection::draw(Cairo::RefPtr<Cairo::Context> ctx, double scale, const Glib::ustring &idx) const
{
	Gdk::RGBA fgcolor = Gtk::Entry().get_style_context()->get_color(Gtk::STATE_FLAG_SELECTED);
	Gdk::RGBA bgcolor = Gtk::Entry().get_style_context()->get_background_color(Gtk::STATE_FLAG_SELECTED);

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
	ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 0.25);
	ctx->fill_preserve();
	ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 1.0);
	ctx->stroke();
	// Text box
	double w = std::min(std::min(40. * d, rect.width), rect.height);
	ctx->rectangle(rect.x, rect.y, w - d, w - d);
	ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 1.0);
	ctx->fill();
	// Text
	ctx->select_font_face("sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
	ctx->set_font_size(0.7 * w);
	Cairo::TextExtents ext; ctx->get_text_extents(idx, ext);
	ctx->translate(rect.x + .5 * w, rect.y + .5 * w);
	ctx->translate(-ext.x_bearing - .5 * ext.width, -ext.y_bearing - .5 * ext.height);
	ctx->set_source_rgba(fgcolor.get_red(), fgcolor.get_green(), bgcolor.get_blue(), 1.0);
	ctx->show_text(idx);
	ctx->restore();
}

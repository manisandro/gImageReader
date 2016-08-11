/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolHOCR.cc
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

#include "DisplayerToolHOCR.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"
#include "Utils.hh"

class DisplayerToolHOCR::SelectionRect : public DisplayerItem {
public:
	using DisplayerItem::DisplayerItem;

	void draw(Cairo::RefPtr<Cairo::Context> ctx) const override {
		Gdk::RGBA bgcolor("#4A90D9");

		double scale = displayer()->getCurrentScale();

		double d = 0.5 / scale;
		double x1 = Utils::round(rect().x * scale) / scale + d;
		double y1 = Utils::round(rect().y * scale) / scale + d;
		double x2 = Utils::round((rect().x + rect().width) * scale) / scale - d;
		double y2 = Utils::round((rect().y + rect().height) * scale) / scale - d;
		Geometry::Rectangle rect(x1, y1, x2 - x1, y2 - y1);
		ctx->save();
		// Semitransparent rectangle with frame
		ctx->set_line_width(2. * d);
		ctx->rectangle(rect.x, rect.y, rect.width, rect.height);
		ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 0.25);
		ctx->fill_preserve();
		ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 1.0);
		ctx->stroke();
		ctx->restore();
	}
};

DisplayerToolHOCR::DisplayerToolHOCR(Displayer *displayer)
	: DisplayerTool(displayer)
{
	MAIN->getRecognizer()->setRecognizeMode(_("Recognize"));
}

DisplayerToolHOCR::~DisplayerToolHOCR()
{
	clearSelection();
}

std::vector<Cairo::RefPtr<Cairo::ImageSurface>> DisplayerToolHOCR::getOCRAreas()
{
	std::vector<Cairo::RefPtr<Cairo::ImageSurface>> surfaces;
	surfaces.push_back(m_displayer->getImage(m_displayer->getSceneBoundingRect()));
	return surfaces;
}

void DisplayerToolHOCR::setSelection(const Geometry::Rectangle& rect)
{
	if(!m_selection) {
		m_selection = new SelectionRect();
		m_displayer->addItem(m_selection);
	}
	Geometry::Rectangle sceneRect = m_displayer->getSceneBoundingRect();
	m_selection->setRect(rect.translate(sceneRect.x, sceneRect.y));
}

Cairo::RefPtr<Cairo::ImageSurface> DisplayerToolHOCR::getSelection(const Geometry::Rectangle& rect)
{
	Geometry::Rectangle sceneRect = m_displayer->getSceneBoundingRect();
	return m_displayer->getImage(rect.translate(sceneRect.x, sceneRect.y));
}

void DisplayerToolHOCR::clearSelection()
{
	if(m_selection) {
		m_displayer->removeItem(m_selection);
		delete m_selection;
		m_selection = nullptr;
	}
}

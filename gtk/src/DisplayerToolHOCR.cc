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
	Geometry::Rectangle sceneRect = m_displayer->getSceneBoundingRect();
	Geometry::Rectangle r = rect.translate(sceneRect.x, sceneRect.y);
	if(!m_selection) {
		m_selection = new DisplayerSelection(this, Geometry::Point(r.x, r.y));
		CONNECT(m_selection, geometry_changed, [this](const Geometry::Rectangle& rect){ selectionChanged(rect); });
		m_displayer->addItem(m_selection);
	}
	m_selection->setAnchorAndPoint(Geometry::Point(r.x, r.y), Geometry::Point(r.x + r.width, r.y + r.height));
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

void DisplayerToolHOCR::selectionChanged(const Geometry::Rectangle& rect)
{
	Geometry::Rectangle sceneRect = m_displayer->getSceneBoundingRect();
	Geometry::Rectangle r = rect.translate(-sceneRect.x, -sceneRect.y);
	m_signalSelectionGeometryChanged.emit(Geometry::Rectangle(int(r.x), int(r.y), int(r.width), int(r.height)));
}

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolHOCR.cc
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

#include "DisplayerToolHOCR.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"
#include "Utils.hh"


DisplayerToolHOCR::DisplayerToolHOCR(Displayer *displayer)
	: DisplayerTool(displayer) {
	MAIN->getRecognizer()->setRecognizeMode(_("Recognize"));
}

DisplayerToolHOCR::~DisplayerToolHOCR() {
	clearSelection();
}

std::vector<Cairo::RefPtr<Cairo::ImageSurface>> DisplayerToolHOCR::getOCRAreas() {
	std::vector<Cairo::RefPtr<Cairo::ImageSurface>> surfaces;
	surfaces.push_back(m_displayer->getImage(m_displayer->getSceneBoundingRect()));
	return surfaces;
}

bool DisplayerToolHOCR::mousePressEvent(GdkEventButton* event) {
	m_pressed = true;
	if(event->button == 1 && m_currentAction == ACTION_DRAW_RECT) {
		clearSelection();
		m_selection = new DisplayerSelection(this, m_displayer->mapToSceneClamped(Geometry::Point(event->x, event->y)));
		m_displayer->addItem(m_selection);
		return true;
	}
	return false;
}

bool DisplayerToolHOCR::mouseMoveEvent(GdkEventMotion* event) {
	if(m_selection && m_currentAction == ACTION_DRAW_RECT) {
		Geometry::Point p = m_displayer->mapToSceneClamped(Geometry::Point(event->x, event->y));
		m_selection->setPoint(p);
		m_displayer->ensureVisible(event->x, event->y);
		return true;
	}
	return false;
}

bool DisplayerToolHOCR::mouseReleaseEvent(GdkEventButton* event) {
	// Don't do anything if the release event does not follow a press event...
	if(!m_pressed) {
		return false;
	}
	m_pressed = false;
	if(m_selection && m_currentAction == ACTION_DRAW_RECT) {
		if(m_selection->rect().width < 5. || m_selection->rect().height < 5.) {
			clearSelection();
		} else {
			Geometry::Rectangle sceneRect = m_displayer->getSceneBoundingRect();
			Geometry::Rectangle r = m_selection->rect().translate(-sceneRect.x, -sceneRect.y);
			m_signal_bbox_drawn.emit(Geometry::Rectangle(int(r.x), int(r.y), int(r.width), int(r.height)));
		}
	} else {
		Geometry::Point p = m_displayer->mapToSceneClamped(Geometry::Point(event->x, event->y));
		m_signal_position_picked.emit(p);
	}
	setAction(ACTION_NONE, false);
	return true;
}

void DisplayerToolHOCR::setAction(Action action, bool clearSel) {
	if(action != m_currentAction) {
		m_signal_action_changed.emit(action);
	}
	if(clearSel) {
		clearSelection();
	}
	m_currentAction = action;
}

void DisplayerToolHOCR::setSelection(const Geometry::Rectangle& rect) {
	setAction(ACTION_NONE, false);
	Geometry::Rectangle sceneRect = m_displayer->getSceneBoundingRect();
	Geometry::Rectangle r = rect.translate(sceneRect.x, sceneRect.y);
	if(!m_selection) {
		m_selection = new DisplayerSelection(this, Geometry::Point(r.x, r.y));
		CONNECT(m_selection, geometry_changed, sigc::mem_fun(this, &DisplayerToolHOCR::selectionChanged));
		m_displayer->addItem(m_selection);
	}
	m_selection->setAnchorAndPoint(Geometry::Point(r.x, r.y), Geometry::Point(r.x + r.width, r.y + r.height));
	m_displayer->ensureVisible(m_selection->rect());
}

Cairo::RefPtr<Cairo::ImageSurface> DisplayerToolHOCR::getSelection(const Geometry::Rectangle& rect) const {
	Geometry::Rectangle sceneRect = m_displayer->getSceneBoundingRect();
	return m_displayer->getImage(rect.translate(sceneRect.x, sceneRect.y));
}

void DisplayerToolHOCR::clearSelection() {
	if(m_selection) {
		m_displayer->removeItem(m_selection);
		delete m_selection;
		m_selection = nullptr;
	}
}

void DisplayerToolHOCR::selectionChanged(const Geometry::Rectangle& rect) {
	Geometry::Rectangle sceneRect = m_displayer->getSceneBoundingRect();
	Geometry::Rectangle r = rect.translate(-sceneRect.x, -sceneRect.y);
	m_signal_bbox_changed.emit(Geometry::Rectangle(int(r.x), int(r.y), int(r.width), int(r.height)));
}

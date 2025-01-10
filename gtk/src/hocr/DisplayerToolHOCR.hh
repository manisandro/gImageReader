/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolHOCR.hh
 * Copyright (C) 2016-2025 Sandro Mani <manisandro@gmail.com>
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

namespace Geometry {
class Rectangle;
}

class DisplayerToolHOCR : public DisplayerTool {
public:
	enum Action {ACTION_NONE, ACTION_DRAW_GRAPHIC_RECT, ACTION_DRAW_CAREA_RECT, ACTION_DRAW_PAR_RECT, ACTION_DRAW_LINE_RECT, ACTION_DRAW_WORD_RECT};

	DisplayerToolHOCR(Displayer* displayer);
	~DisplayerToolHOCR();

	std::vector<Cairo::RefPtr<Cairo::ImageSurface >> getOCRAreas() override;
	void pageChanged() override {
		m_signal_displayed_source_changed.emit();
		reset();
	}
	void resolutionChanged(double /*factor*/) override {
		reset();
	}
	void rotationChanged(double /*delta*/) override {
		reset();
	}
	void reset() override {
		setAction(ACTION_NONE, true);
	}

	bool mousePressEvent(GdkEventButton* event) override;
	bool mouseMoveEvent(GdkEventMotion* event) override;
	bool mouseReleaseEvent(GdkEventButton* event) override;

	void setAction(Action action, bool clearSel = true);
	void setSelection(const Geometry::Rectangle& rect, const Geometry::Rectangle& minRect);
	Cairo::RefPtr<Cairo::ImageSurface> getSelection(const Geometry::Rectangle& rect) const;
	void clearSelection();

	sigc::signal<void> signal_displayed_source_changed() {
		return m_signal_displayed_source_changed;
	}
	sigc::signal<void, Geometry::Rectangle, Action> signal_bbox_drawn() {
		return m_signal_bbox_drawn;
	}
	sigc::signal<void, Geometry::Rectangle> signal_bbox_changed() {
		return m_signal_bbox_changed;
	}
	sigc::signal<void, Geometry::Point> signal_position_picked() {
		return m_signal_position_picked;
	}
	sigc::signal<void, Action> signal_action_changed() {
		return m_signal_action_changed;
	}

private:
	ClassData m_classdata;

	DisplayerSelection* m_selection = nullptr;
	Action m_currentAction = ACTION_NONE;
	bool m_pressed = false;
	sigc::signal<void> m_signal_displayed_source_changed;
	sigc::signal<void, Geometry::Rectangle, Action> m_signal_bbox_drawn;
	sigc::signal<void, Geometry::Rectangle> m_signal_bbox_changed;
	sigc::signal<void, Geometry::Point> m_signal_position_picked;
	sigc::signal<void, Action> m_signal_action_changed;

	void selectionChanged(const Geometry::Rectangle& rect);
};

#endif // DISPLAYERTOOLHOCR_HH

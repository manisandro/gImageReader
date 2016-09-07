/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolSelect.hh
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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

#ifndef DISPLAYERTOOLSELECT_HH
#define DISPLAYERTOOLSELECT_HH

#include "Displayer.hh"

class DisplaySelection;

class DisplayerToolSelect : public DisplayerTool {
public:
	DisplayerToolSelect(Displayer* displayer);
	~DisplayerToolSelect();
	bool mousePressEvent(GdkEventButton *event) override;
	bool mouseMoveEvent(GdkEventMotion *event) override;
	bool mouseReleaseEvent(GdkEventButton *event) override;
	void resolutionChanged(double factor) override;
	void rotationChanged(double delta) override;

	std::vector<Cairo::RefPtr<Cairo::ImageSurface>> getOCRAreas() override;
	bool hasMultipleOCRAreas() const override{ return !m_selections.empty(); }
	bool allowAutodetectOCRAreas() const override{ return true; }
	void autodetectOCRAreas() override{ autodetectLayout(); }
	void reset() override{ clearSelections(); }

private:
	friend class DisplaySelection;
	DisplaySelection* m_curSel = nullptr;
	std::vector<DisplaySelection*> m_selections;
	sigc::connection m_connectionAutolayout;

	void clearSelections();
	void removeSelection(int num);
	void reorderSelection(int oldNum, int newNum);
	void saveSelection(DisplaySelection* selection);
	void updateRecognitionModeLabel();
	void autodetectLayout(bool noDeskew = false);
};

class DisplaySelection : public DisplayerItem
{
public:
	DisplaySelection(DisplayerToolSelect* selectTool, int number, const Geometry::Point& anchor)
		: m_selectTool(selectTool), m_number(number), m_anchor(anchor), m_point(anchor)
	{
		setRect(Geometry::Rectangle(anchor, anchor));
	}
	void setPoint(const Geometry::Point& point){
		m_point = point;
		setRect(Geometry::Rectangle(m_anchor, m_point));
	}
	void rotate(const Geometry::Rotation &R){
		m_anchor = R.rotate(m_anchor);
		m_point = R.rotate(m_point);
		setRect(Geometry::Rectangle(m_anchor, m_point));
	}
	void scale(double factor){
		m_anchor = Geometry::Point(m_anchor.x * factor, m_anchor.y * factor);
		m_point = Geometry::Point(m_point.x * factor, m_point.y * factor);
	}
	void setNumber(int number){
		m_number = number;
	}

	void reorderSelection(int newNumber);

	void draw(Cairo::RefPtr<Cairo::Context> ctx) const override;
	bool mousePressEvent(GdkEventButton *event) override;
	bool mouseReleaseEvent(GdkEventButton *event) override;
	bool mouseMoveEvent(GdkEventMotion *event) override;

private:
	typedef void(*ResizeHandler)(const Geometry::Point&, Geometry::Point&, Geometry::Point&);

	DisplayerToolSelect* m_selectTool;
	int m_number;
	Geometry::Point m_anchor;
	Geometry::Point m_point;
	std::vector<ResizeHandler> m_resizeHandlers;
	Geometry::Point m_resizeOffset;

	void showContextMenu(GdkEventButton *event);

	static void resizeAnchorX(const Geometry::Point& pos, Geometry::Point& anchor, Geometry::Point& /*point*/){ anchor.x = pos.x; }
	static void resizeAnchorY(const Geometry::Point& pos, Geometry::Point& anchor, Geometry::Point& /*point*/){ anchor.y = pos.y; }
	static void resizePointX(const Geometry::Point& pos, Geometry::Point& /*anchor*/, Geometry::Point& point){ point.x = pos.x; }
	static void resizePointY(const Geometry::Point& pos, Geometry::Point& /*anchor*/, Geometry::Point& point){ point.y = pos.y; }
};

#endif // DISPLAYERTOOLSELECT_HH

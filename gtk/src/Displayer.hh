/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.hh
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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

#ifndef DISPLAYER_HH
#define DISPLAYER_HH

#include "Geometry.hh"

#include <cairomm/cairomm.h>
#include <atomic>
#include <cstdint>
#include <thread>

namespace std { class thread; }
class DisplayerItem;
class DisplayerImageItem;
class DisplayerTool;
class DisplayRenderer;
class Source;
namespace Ui {
class MainWindow;
}

class Displayer {
public:
	Displayer(const Ui::MainWindow& _ui);
	~Displayer() {
		setSources(std::vector<Source*>());
	}
	void setTool(DisplayerTool* tool) {
		m_tool = tool;
	}
	bool setSources(const std::vector<Source*> sources);
	bool setup(const int* page, const int* resolution = nullptr, const double* angle = nullptr);
	int getCurrentPage() const;
	int getCurrentResolution() const;
	double getCurrentAngle() const;
	double getCurrentScale() const {
		return m_scale;
	}
	bool resolvePage(int page, std::string& source, int& sourcePage) const;
	std::string getCurrentImage(int& page) const;
	int getNPages() const;
	int getNSources() const { return m_sourceRenderers.size(); }

	Cairo::RefPtr<Cairo::ImageSurface> getImage(const Geometry::Rectangle& rect) const;
	Geometry::Rectangle getSceneBoundingRect() const;
	Geometry::Point mapToSceneClamped(const Geometry::Point& p) const;
	Geometry::Point mapToView(const Geometry::Point& p) const;
	bool hasMultipleOCRAreas();
	std::vector<Cairo::RefPtr<Cairo::ImageSurface>> getOCRAreas();
	bool allowAutodetectOCRAreas() const;
	void autodetectOCRAreas();
	void setDefaultCursor(Glib::RefPtr<Gdk::Cursor> cursor);
	void setCursor(Glib::RefPtr<Gdk::Cursor> cursor);
	void ensureVisible(double evx, double evy);
	void ensureVisible(const Geometry::Rectangle& rect);

	void addItem(DisplayerItem* item);
	void removeItem(DisplayerItem* item);
	void invalidateRect(const Geometry::Rectangle& rect);
	void resortItems();
	void setBlockAutoscale(bool block);

	sigc::signal<void> signal_imageChanged() const { return m_signalImageChanged; }


private:
	enum class Zoom { In, Out, Fit, One };
	enum class RotateMode { CurrentPage, AllPages } m_rotateMode;

	const Ui::MainWindow& ui;
	ClassData m_classdata;

	Glib::RefPtr<Gtk::Adjustment> m_hadj;
	Glib::RefPtr<Gtk::Adjustment> m_vadj;

	std::vector<Source*> m_sources;
	std::map<Source*, DisplayRenderer*> m_sourceRenderers;
	std::map<int, std::pair<Source*, int>> m_pageMap;
	Source* m_currentSource = nullptr;
	Cairo::RefPtr<Cairo::ImageSurface> m_image;
	double m_scale = 1.0;
	double m_scrollPos[2] = {0.5, 0.5};
	DisplayerTool* m_tool = nullptr;
	DisplayerImageItem* m_imageItem = nullptr;
	std::vector<DisplayerItem*> m_items;
	DisplayerItem* m_activeItem = nullptr;
	double m_panPos[2] = {0., 0.};
	Glib::RefPtr<Gdk::Cursor> m_defaultCursor;

	sigc::connection m_renderTimer;
	sigc::connection m_connection_pageSpinChanged;
	sigc::connection m_connection_rotSpinChanged;
	sigc::connection m_connection_resSpinChanged;
	sigc::connection m_connection_briSpinChanged;
	sigc::connection m_connection_conSpinChanged;
	sigc::connection m_connection_invcheckToggled;
	sigc::connection m_connection_zoomfitClicked;
	sigc::connection m_connection_zoomoneClicked;
	sigc::connection m_connection_setScaledImage;
	sigc::connection m_connection_thumbClicked;

	sigc::signal<void> m_signalImageChanged;

	void resizeEvent();
	bool keyPressEvent(GdkEventKey* ev);
	bool mouseMoveEvent(GdkEventMotion* ev);
	bool mousePressEvent(GdkEventButton* ev);
	bool mouseReleaseEvent(GdkEventButton* ev);
	bool scrollEvent(GdkEventScroll* ev);

	void setZoom(Zoom zoom);
	void generateThumbnails();
	void thumbnailsToggled();

	bool renderImage();
	void drawCanvas(const Cairo::RefPtr<Cairo::Context>& ctx);
	void positionCanvas();
	void queueRenderImage();
	void setAngle(double angle);
	void setRotateMode(RotateMode mode, const std::string& iconName);
	std::pair<int, int> getPointVisible(const Geometry::Point& p) const;
	void waitForThread(std::thread*& thread, std::atomic<bool>& cancelFlag, std::atomic<int>& idleJobCount, sigc::connection* conn = nullptr);
	void adjustBrightness();
	void adjustContrast();
	void adjustResolution();
	void setInvertColors();

	bool m_autoScaleBlocked = false;
	sigc::connection m_scaleTimer;
	std::thread* m_scaleThread = nullptr;
	std::atomic<bool> m_scaleThreadCanceled;
	std::atomic<int> m_scaleThreadIdleJobsCount;
	void scaleImage();

	class ThumbListViewColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> pixbuf;
		Gtk::TreeModelColumn<Glib::ustring> label;
		ThumbListViewColumns() {
			add(pixbuf);
			add(label);
		}
	} m_thumbListViewCols;
	std::thread* m_thumbThread = nullptr;
	std::atomic<bool> m_thumbThreadCanceled;
	std::atomic<int> m_thumbThreadIdleJobsCount;
	void thumbnailThread();
};

class DisplayerItem : public sigc::trackable {
public:
	friend class Displayer;
	virtual ~DisplayerItem() {}

	Displayer* displayer() const {
		return m_displayer;
	}

	void setZIndex(int zIndex);
	double zIndex() const {
		return m_zIndex;
	}
	void setRect(const Geometry::Rectangle& rect);
	const Geometry::Rectangle& rect() const {
		return m_rect;
	}
	void setVisible(bool visible);
	bool visible() const {
		return m_visible;
	}
	void update();

	virtual void draw(Cairo::RefPtr<Cairo::Context> ctx) const = 0;
	virtual bool mousePressEvent(GdkEventButton* /*event*/) {
		return false;
	}
	virtual bool mouseMoveEvent(GdkEventMotion* /*event*/) {
		return false;
	}
	virtual bool mouseReleaseEvent(GdkEventButton* /*event*/) {
		return false;
	}

	static bool zIndexCmp(const DisplayerItem* lhs, const DisplayerItem* rhs) {
		return lhs->m_zIndex < rhs->m_zIndex;
	}

private:
	Displayer* m_displayer = nullptr;
	Geometry::Rectangle m_rect;
	int m_zIndex = 0;
	bool m_visible = true;
};

class DisplayerImageItem : public DisplayerItem {
public:
	using DisplayerItem::DisplayerItem;
	void draw(Cairo::RefPtr<Cairo::Context> ctx) const override;
	void setImage(Cairo::RefPtr<Cairo::ImageSurface> image) {
		m_image = image;
	}
	void setRotation(double rotation) {
		m_rotation = rotation;
	}

protected:
	Cairo::RefPtr<Cairo::ImageSurface> m_image;
	double m_rotation = 0.;
};

class DisplayerSelection : public DisplayerItem {
public:
	DisplayerSelection(DisplayerTool* tool, const Geometry::Point& anchor)
		: m_tool(tool), m_anchor(anchor), m_point(anchor) {
		setRect(Geometry::Rectangle(anchor, anchor));
		setZIndex(10);
	}
	void setPoint(const Geometry::Point& point) {
		m_point = point;
		setRect(Geometry::Rectangle(m_anchor, m_point));
	}
	void setAnchorAndPoint(const Geometry::Point& anchor, const Geometry::Point& point) {
		m_anchor = anchor;
		m_point = point;
		setRect(Geometry::Rectangle(m_anchor, m_point));
	}
	void setMinimumRect(const Geometry::Rectangle& rect) {
		m_minRect = rect;
	}
	void rotate(const Geometry::Rotation& R) {
		m_anchor = R.rotate(m_anchor);
		m_point = R.rotate(m_point);
		setRect(Geometry::Rectangle(m_anchor, m_point));
	}
	void scale(double factor) {
		m_anchor = Geometry::Point(m_anchor.x * factor, m_anchor.y * factor);
		m_point = Geometry::Point(m_point.x * factor, m_point.y * factor);
	}
	sigc::signal<void, Geometry::Rectangle> signal_geometry_changed() {
		return m_signalGeometryChanged;
	}

	void draw(Cairo::RefPtr<Cairo::Context> ctx) const override;
	bool mousePressEvent(GdkEventButton* event) override;
	bool mouseReleaseEvent(GdkEventButton* event) override;
	bool mouseMoveEvent(GdkEventMotion* event) override;

protected:
	DisplayerTool* m_tool;

	virtual void showContextMenu(GdkEventButton* /*event*/) {}

private:
	typedef void(*ResizeHandler)(const Geometry::Point&, Geometry::Point&, Geometry::Point&);

	Geometry::Point m_anchor;
	Geometry::Point m_point;
	Geometry::Rectangle m_minRect;
	std::vector<ResizeHandler> m_resizeHandlers;
	Geometry::Point m_mouseMoveOffset;
	bool m_translating = false;
	sigc::signal<void, Geometry::Rectangle> m_signalGeometryChanged;

	static void resizeAnchorX(const Geometry::Point& pos, Geometry::Point& anchor, Geometry::Point& /*point*/) {
		anchor.x = pos.x;
	}
	static void resizeAnchorY(const Geometry::Point& pos, Geometry::Point& anchor, Geometry::Point& /*point*/) {
		anchor.y = pos.y;
	}
	static void resizePointX(const Geometry::Point& pos, Geometry::Point& /*anchor*/, Geometry::Point& point) {
		point.x = pos.x;
	}
	static void resizePointY(const Geometry::Point& pos, Geometry::Point& /*anchor*/, Geometry::Point& point) {
		point.y = pos.y;
	}
};


class DisplayerTool {
public:
	DisplayerTool(Displayer* displayer) : m_displayer(displayer) {}
	virtual ~DisplayerTool() {}
	virtual bool mousePressEvent(GdkEventButton* /*event*/) {
		return false;
	}
	virtual bool mouseMoveEvent(GdkEventMotion* /*event*/) {
		return false;
	}
	virtual bool mouseReleaseEvent(GdkEventButton* /*event*/) {
		return false;
	}
	virtual void pageChanged() {}
	virtual void resolutionChanged(double /*factor*/) {}
	virtual void rotationChanged(double /*delta*/) {}
	virtual std::vector<Cairo::RefPtr<Cairo::ImageSurface>> getOCRAreas() = 0;
	virtual bool hasMultipleOCRAreas() const {
		return false;
	}
	virtual bool allowAutodetectOCRAreas() const {
		return false;
	}
	virtual void autodetectOCRAreas() {}
	virtual void reset() {}

protected:
	Displayer* m_displayer;
};

#endif // IMAGEDISPLAYER_HH

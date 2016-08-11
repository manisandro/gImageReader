/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.hh
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

#ifndef DISPLAYER_HH
#define DISPLAYER_HH

#include "common.hh"
#include "Geometry.hh"

#include <cairomm/cairomm.h>
#include <cstdint>
#include <queue>
#include <vector>

class DisplayerItem;
class DisplayerImageItem;
class DisplayerTool;
class DisplayRenderer;
class Source;

class Displayer {
public:
	Displayer();
	~Displayer(){ setSources(std::vector<Source*>()); }
	void setTool(DisplayerTool* tool) { m_tool = tool; }
	bool setSources(const std::vector<Source*> sources);
	int getCurrentPage() const{ return m_pagespin->get_value_as_int(); }
	bool setCurrentPage(int page);
	double getCurrentAngle() const{ return m_rotspin->get_value(); }
	double getCurrentScale() const{ return m_scale; }
	void setAngle(double angle);
	int getCurrentResolution(){ return m_resspin->get_value_as_int(); }
	void setResolution(int resolution);
	std::string getCurrentImage(int& page) const;
	Cairo::RefPtr<Cairo::ImageSurface> getImage(const Geometry::Rectangle& rect) const;
	Geometry::Rectangle getSceneBoundingRect() const;
	Geometry::Point mapToSceneClamped(const Geometry::Point& p) const;
	int getNPages(){ double min, max; m_pagespin->get_range(min, max); return int(max); }
	bool hasMultipleOCRAreas();
	std::vector<Cairo::RefPtr<Cairo::ImageSurface>> getOCRAreas();
	void autodetectOCRAreas();
	void setCursor(Glib::RefPtr<Gdk::Cursor> cursor);
	void ensureVisible(double evx, double evy);

	void addItem(DisplayerItem* item);
	void removeItem(DisplayerItem* item);
	void invalidateRect(const Geometry::Rectangle& rect);
	void resortItems();


private:
	enum class Zoom { In, Out, Fit, One };

	Gtk::DrawingArea* m_canvas;
	Gtk::Viewport* m_viewport;
	Glib::RefPtr<Gtk::Adjustment> m_hadj;
	Glib::RefPtr<Gtk::Adjustment> m_vadj;
	Gtk::Button* m_zoominbtn;
	Gtk::Button* m_zoomoutbtn;
	Gtk::ToggleButton* m_zoomfitbtn;
	Gtk::ToggleButton* m_zoomonebtn;
	Gtk::SpinButton* m_rotspin;
	Gtk::SpinButton* m_pagespin;
	Gtk::SpinButton* m_resspin;
	Gtk::SpinButton* m_brispin;
	Gtk::SpinButton* m_conspin;
	Gtk::ScrolledWindow* m_scrollwin;
	Gtk::CheckButton* m_invcheck;

	std::vector<Source*> m_sources;
	std::map<int, std::pair<Source*, int>> m_pageMap;
	Source* m_currentSource = nullptr;
	DisplayRenderer* m_renderer = nullptr;
	Cairo::RefPtr<Cairo::ImageSurface> m_image;
	double m_scale = 1.0;
	double m_scrollPos[2] = {0.5, 0.5};
	DisplayerTool* m_tool = nullptr;
	DisplayerImageItem* m_imageItem = nullptr;
	std::vector<DisplayerItem*> m_items;
	DisplayerItem* m_activeItem = nullptr;
	double m_panPos[2] = {0., 0.};

	sigc::connection m_renderTimer;
	sigc::connection m_connection_pageSpinChanged;
	sigc::connection m_connection_rotSpinChanged;
	sigc::connection m_connection_resSpinChanged;
	sigc::connection m_connection_briSpinChanged;
	sigc::connection m_connection_conSpinChanged;
	sigc::connection m_connection_invcheckToggled;
	sigc::connection m_connection_zoomfitClicked;
	sigc::connection m_connection_zoomoneClicked;

	void resizeEvent();
	bool mouseMoveEvent(GdkEventMotion* ev);
	bool mousePressEvent(GdkEventButton* ev);
	bool mouseReleaseEvent(GdkEventButton* ev);
	bool scrollEvent(GdkEventScroll* ev);

	void setZoom(Zoom zoom);

	bool renderImage();
	void drawCanvas(const Cairo::RefPtr<Cairo::Context>& ctx);
	void positionCanvas();
	void queueRenderImage();

	struct ScaleRequest {
		enum Request { Scale, Abort, Quit } type;
		double scale;
		int resolution;
		int page;
		int brightness;
		int contrast;
		bool invert;

		ScaleRequest(Request _type, double _scale = 0., int _resolution = 0, int _page = 0, int _brightness = 0, int _contrast = 0, bool _invert = 0)
			: type(_type), scale(_scale), resolution(_resolution), page(_page), brightness(_brightness), contrast(_contrast), invert(_invert) {}
	};
	Glib::Threads::Thread* m_scaleThread = nullptr;
	Glib::Threads::Mutex m_scaleMutex;
	Glib::Threads::Cond m_scaleCond;
	std::queue<ScaleRequest> m_scaleRequests;
	sigc::connection m_scaleTimer;

	void sendScaleRequest(const ScaleRequest& request);
	void scaleThread();
	void setScaledImage(Cairo::RefPtr<Cairo::ImageSurface> image, double scale);
};

class DisplayerItem {
public:
	friend class Displayer;
	virtual ~DisplayerItem(){}

	Displayer* displayer() const{ return m_displayer; }

	void setZIndex(int zIndex);
	double zIndex() const{ return m_zIndex; }
	void setRect(const Geometry::Rectangle& rect);
	const Geometry::Rectangle& rect() const{ return m_rect; }
	void update();

	virtual void draw(Cairo::RefPtr<Cairo::Context> ctx) const = 0;
	virtual bool mousePressEvent(GdkEventButton */*event*/) { return false; }
	virtual bool mouseMoveEvent(GdkEventMotion */*event*/) { return false; }
	virtual bool mouseReleaseEvent(GdkEventButton */*event*/) { return false; }

	static bool zIndexCmp(const DisplayerItem* lhs, const DisplayerItem* rhs) {
		return lhs->m_zIndex < rhs->m_zIndex;
	}

private:
	Displayer* m_displayer = nullptr;
	Geometry::Rectangle m_rect;
	int m_zIndex = 0;
};

class DisplayerImageItem : public DisplayerItem
{
public:
	using DisplayerItem::DisplayerItem;
	void draw(Cairo::RefPtr<Cairo::Context> ctx) const override;
	void setImage(Cairo::RefPtr<Cairo::ImageSurface> image) { m_image = image; }
	void setRotation(double rotation) { m_rotation = rotation; }

protected:
	Cairo::RefPtr<Cairo::ImageSurface> m_image;
	double m_rotation = 0.;
};

class DisplayerTool {
public:
	DisplayerTool(Displayer* displayer) : m_displayer(displayer) {}
	virtual ~DisplayerTool(){}
	virtual bool mousePressEvent(GdkEventButton */*event*/){ return false; }
	virtual bool mouseMoveEvent(GdkEventMotion */*event*/){ return false; }
	virtual bool mouseReleaseEvent(GdkEventButton */*event*/){ return false; }
	virtual void pageChanged(){}
	virtual void resolutionChanged(double /*factor*/){}
	virtual void rotationChanged(double /*delta*/){}
	virtual std::vector<Cairo::RefPtr<Cairo::ImageSurface>> getOCRAreas() = 0;
	virtual bool hasMultipleOCRAreas() const{ return false; }
	virtual void autodetectOCRAreas(){}

protected:
	Displayer* m_displayer;
};

#endif // IMAGEDISPLAYER_HH

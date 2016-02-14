/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.hh
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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
#include "DisplaySelection.hh"

#include <cairomm/cairomm.h>
#include <cstdint>
#include <queue>
#include <vector>

class DisplayRenderer;
class Source;

class Displayer {
public:
	Displayer();
	~Displayer(){ setSources(std::vector<Source*>()); }
	bool setSources(const std::vector<Source*> sources);
	std::vector<Cairo::RefPtr<Cairo::ImageSurface>> getSelections() const;
	bool getHasSelections() const{ return !m_selections.empty(); }
	bool setCurrentPage(int page);
	int getCurrentPage() const{ return m_pagespin->get_value_as_int(); }
	int getNPages(){ double min, max; m_pagespin->get_range(min, max); return int(max); }
	void autodetectLayout(bool rotated = false);
	sigc::signal<void, bool> signal_selectionChanged(){ return m_signal_selectionChanged; }

private:
	enum class Zoom { In, Out, Fit, One };
	struct Geo {
		double sx, sy;  // Scroll x, y
		double s;       // Scale
	};

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
	Gtk::Window* m_selmenu;
	Gtk::CheckButton* m_invcheck;

	std::vector<Source*> m_sources;
	std::map<int, std::pair<Source*, int>> m_pageMap;
	Source* m_currentSource = nullptr;
	DisplayRenderer* m_renderer = nullptr;
	Cairo::RefPtr<Cairo::ImageSurface> m_image;
	int m_scrollspeed[2];
	Geo m_geo;
	DisplaySelection::Handle* m_curSel = nullptr;
	std::vector<DisplaySelection*> m_selections;
	double m_panPos[2];

	sigc::signal<void, bool> m_signal_selectionChanged;

	sigc::connection m_renderTimer;
	sigc::connection m_scrollTimer;
	sigc::connection m_connection_pageSpinChanged;
	sigc::connection m_connection_rotSpinChanged;
	sigc::connection m_connection_resSpinChanged;
	sigc::connection m_connection_briSpinChanged;
	sigc::connection m_connection_conSpinChanged;
	sigc::connection m_connection_invcheckToggled;
	sigc::connection m_connection_zoomfitClicked;
	sigc::connection m_connection_zoomoneClicked;
	std::vector<sigc::connection> m_selmenuConnections;

	bool renderImage();
	void drawCanvas(const Cairo::RefPtr<Cairo::Context>& ctx);
	void positionCanvas();
	bool panViewport();
	void setZoom(Zoom zoom);
	Geometry::Size getImageBoundingBox() const;
	void setRotation(double angle);
	void queueRenderImage();
	Geometry::Point mapToSceneClamped(double evx, double evy) const;
	Cairo::RefPtr<Cairo::ImageSurface> getImage(const Geometry::Rectangle& rect) const;

	void resizeEvent();
	bool mouseMoveEvent(GdkEventMotion* ev);
	bool mousePressEvent(GdkEventButton* ev);
	bool mouseReleaseEvent(GdkEventButton* ev);
	bool scrollEvent(GdkEventScroll* ev);

	void clearSelections();
	void removeSelection(const DisplaySelection* sel);
	void saveSelection(const Geometry::Rectangle& rect);
	void showSelectionMenu(GdkEventButton* ev, int i);
	void hideSelectionMenu();

	struct ScaleRequest {
		enum Request { Scale, Abort, Quit } type;
		double scale;
		int resolution;
		int page;
		int brightness;
		int contrast;
		bool invert;
	};
	Cairo::RefPtr<Cairo::ImageSurface> m_blurImage;
	double m_blurScale;
	Glib::Threads::Thread* m_scaleThread = nullptr;
	Glib::Threads::Mutex m_scaleMutex;
	Glib::Threads::Cond m_scaleCond;
	std::queue<ScaleRequest> m_scaleRequests;
	sigc::connection m_scaleTimer;

	void sendScaleRequest(const ScaleRequest& request);
	void scaleThread();
	void setScaledImage(Cairo::RefPtr<Cairo::ImageSurface> image, double scale);
};

#endif // IMAGEDISPLAYER_HH

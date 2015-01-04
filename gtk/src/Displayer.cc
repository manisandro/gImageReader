/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.cc
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

#include "MainWindow.hh"
#include "Config.hh"
#include "Displayer.hh"
#include "DisplayRenderer.hh"
#include "DisplaySelection.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include "FileDialogs.hh"

#include <tesseract/baseapi.h>

Displayer::Displayer()
{
	m_canvas = Builder("drawingarea:display");
	m_viewport = Builder("viewport:display");
	m_scrollwin = Builder("scrollwin:display");
	m_hadj = m_scrollwin->get_hadjustment();
	m_vadj = m_scrollwin->get_vadjustment();
	m_zoominbtn = Builder("tbbutton:main.zoomin");
	m_zoomoutbtn = Builder("tbbutton:main.zoomout");
	m_zoomonebtn = Builder("tbbutton:main.normsize");
	m_zoomfitbtn = Builder("tbbutton:main.bestfit");
	m_rotspin = Builder("spin:display.rotate");
	m_pagespin = Builder("spin:display.page");
	m_resspin = Builder("spin:display.resolution");
	m_brispin = Builder("spin:display.brightness");
	m_conspin = Builder("spin:display.contrast");
	m_selmenu = Builder("window:selectionmenu");
	m_invcheck = Builder("check:display.invert");

#if GTKMM_CHECK_VERSION(3,12,0)
	m_rotspin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/angle.png"));
	m_pagespin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/page.png"));
	m_resspin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/resolution.png"));
	m_brispin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/brightness.png"));
	m_conspin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/contrast.png"));
#else
	gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(m_rotspin->gobj()), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/angle.png", 0));
	gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(m_pagespin->gobj()), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/page.png", 0));
	gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(m_resspin->gobj()), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/resolution.png", 0));
	gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(m_brispin->gobj()), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/brightness.png", 0));
	gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(m_conspin->gobj()), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/contrast.png", 0));
#endif

	m_viewport->override_background_color(Gdk::RGBA("#a0a0a4"));

	m_connection_rotSpinChanged = CONNECT(m_rotspin, value_changed, [this]{ setRotation(m_rotspin->get_value()); });
	m_connection_pageSpinChanged = CONNECT(m_pagespin, value_changed, [this]{ clearSelections(); queueRenderImage(); });
	m_connection_briSpinChanged = CONNECT(m_brispin, value_changed, [this]{ queueRenderImage(); });
	m_connection_conSpinChanged = CONNECT(m_conspin, value_changed, [this]{ queueRenderImage(); });
	m_connection_resSpinChanged = CONNECT(m_resspin, value_changed, [this]{ queueRenderImage(); });
	m_connection_invcheckToggled = CONNECT(m_invcheck, toggled, [this]{ queueRenderImage(); });
	CONNECT(m_viewport, size_allocate, [this](Gdk::Rectangle&){ resizeEvent(); });
	CONNECT(m_viewport, motion_notify_event, [this](GdkEventMotion* ev){ return mouseMoveEvent(ev); });
	CONNECT(m_viewport, button_press_event, [this](GdkEventButton* ev){ return mousePressEvent(ev); });
	CONNECT(m_viewport, button_release_event, [this](GdkEventButton* ev){ return mouseReleaseEvent(ev); });
	CONNECT(m_viewport, scroll_event, [this](GdkEventScroll* ev){ return scrollEvent(ev); });
	CONNECT(m_canvas, draw, [this](const Cairo::RefPtr<Cairo::Context>& ctx){ drawCanvas(ctx); return false; });
	CONNECT(m_zoominbtn, clicked, [this]{ setZoom(Zoom::In); });
	CONNECT(m_zoomoutbtn, clicked, [this]{ setZoom(Zoom::Out); });
	m_connection_zoomfitClicked = CONNECT(m_zoomfitbtn, clicked, [this]{ setZoom(Zoom::Fit); });
	m_connection_zoomoneClicked = CONNECT(m_zoomonebtn, clicked, [this]{ setZoom(Zoom::One); });
	CONNECT(Builder("tbbutton:main.rotright").as<Gtk::ToolButton>(), clicked, [this]{ setRotation(m_rotspin->get_value() + 90.); });
	CONNECT(Builder("tbbutton:main.rotleft").as<Gtk::ToolButton>(), clicked, [this]{ setRotation(m_rotspin->get_value() - 90.); });
	CONNECT(Builder("tbbutton:main.autolayout").as<Gtk::ToolButton>(), clicked, [this]{ autodetectLayout(); });
	CONNECT(m_selmenu, button_press_event, [this](GdkEventButton* ev){
		Gtk::Allocation a = m_selmenu->get_allocation();
		if(ev->x < a.get_x() || ev->x > a.get_x() + a.get_width() || ev->y < a.get_y() || ev->y > a.get_y() + a.get_height()){
			hideSelectionMenu();
		}
		return true; });
	CONNECT(m_selmenu, key_press_event, [this](GdkEventKey* ev){
		if(ev->keyval == GDK_KEY_Escape) hideSelectionMenu(); return true;
	});
	CONNECT(Builder("applicationwindow:main").as<Gtk::Window>()->get_style_context(), changed, [this]{ m_canvas->queue_draw(); });

	MAIN->getConfig()->addSetting(new VarSetting<Glib::ustring>("selectionsavefile"));
	std::string selectionsavefile = MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("selectionsavefile")->getValue();
	if(selectionsavefile.empty()) {
		selectionsavefile = Glib::build_filename(Utils::get_documents_dir(), _("selection.png"));
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("selectionsavefile")->setValue(selectionsavefile);
	}
}

void Displayer::drawCanvas(const Cairo::RefPtr<Cairo::Context> &ctx)
{
	if(!m_image){
		return;
	}
	Gtk::Allocation alloc = m_canvas->get_allocation();
	ctx->save();
	// Set up transformations
	ctx->translate(0.5 * alloc.get_width(), 0.5 * alloc.get_height());
	ctx->rotate(m_source->angle);
	// Set source and apply all transformations to it
	if(!m_blurImage){
		ctx->scale(m_geo.s, m_geo.s);
		ctx->translate(-0.5 * m_image->get_width(), -0.5 * m_image->get_height());
		ctx->set_source(m_image, 0, 0);
	}else{
		ctx->scale(m_geo.s / m_blurScale, m_geo.s / m_blurScale);
		ctx->translate(-0.5 * m_blurImage->get_width(), -0.5 * m_blurImage->get_height());
		ctx->set_source(m_blurImage, 0, 0);
	}
	ctx->paint();
	// Draw selections
	ctx->restore();
	ctx->translate(Utils::round(0.5 * alloc.get_width()), Utils::round(0.5 * alloc.get_height()));
	ctx->scale(m_geo.s, m_geo.s);
	int i = 0;
	for(const DisplaySelection* sel : m_selections){
		sel->draw(ctx, m_geo.s, Glib::ustring::compose("%1", ++i));
	}
	return;
}

void Displayer::positionCanvas()
{
	Geometry::Size bb = getImageBoundingBox();
	m_canvas->set_size_request(Utils::round(bb.x * m_geo.s), Utils::round(bb.y * m_geo.s));
	// Immediately resize viewport, so that adjustment values are correct below
	m_viewport->size_allocate(m_viewport->get_allocation());
	m_viewport->set_allocation(m_viewport->get_allocation());
	m_hadj->set_value(m_geo.sx *(m_hadj->get_upper() - m_hadj->get_page_size()));
	m_vadj->set_value(m_geo.sy *(m_vadj->get_upper() - m_vadj->get_page_size()));
	m_canvas->queue_draw();
}

bool Displayer::setCurrentPage(int page)
{
	Utils::set_spin_blocked(m_pagespin, page, m_connection_pageSpinChanged);
	return renderImage();
}

bool Displayer::setSource(Source* source)
{
	m_scaleTimer.disconnect();
	if(m_scaleThread){
		sendScaleRequest({ScaleRequest::Quit});
		m_scaleThread->join();
		m_scaleThread = nullptr;
	}
	m_geo = {0.5, 0.5, 1.0};
	clearSelections();
	m_renderTimer.disconnect();
	m_scrollTimer.disconnect();
	m_image.clear();
	m_blurImage.clear();
	delete m_renderer;
	m_renderer = 0;
	m_canvas->hide();
	m_pagespin->set_value(1);
	m_rotspin->set_value(0);
	m_brispin->set_value(0);
	m_conspin->set_value(0);
	m_resspin->set_value(100);
	m_invcheck->set_active(false);
	m_zoomfitbtn->set_active(true);
	m_zoomonebtn->set_active(false);
	m_zoominbtn->set_sensitive(true);
	m_zoomoutbtn->set_sensitive(true);
	if(m_viewport->get_window()) m_viewport->get_window()->set_cursor();

	m_source = source;
	if(!source){
		return false;
	}
	std::string filename = source->file->get_path();
#ifdef G_OS_WIN32
	if(Glib::ustring(filename.substr(filename.length() - 4)).lowercase() == ".pdf"){
#else
	if(Utils::get_content_type(filename) == "application/pdf"){
#endif
		m_renderer = new PDFRenderer(filename);
		if(source->resolution == -1) source->resolution = 300;
	}else{
		m_renderer = new ImageRenderer(filename);
		if(source->resolution == -1) source->resolution = 100;
	}
	m_pagespin->get_adjustment()->set_upper(m_renderer->getNPages());
	m_pagespin->set_visible(m_renderer->getNPages() > 1);
	Utils::set_spin_blocked(m_rotspin, source->angle, m_connection_rotSpinChanged);
	Utils::set_spin_blocked(m_pagespin, source->page, m_connection_pageSpinChanged);
	Utils::set_spin_blocked(m_brispin, source->brightness, m_connection_briSpinChanged);
	Utils::set_spin_blocked(m_conspin, source->contrast, m_connection_conSpinChanged);
	Utils::set_spin_blocked(m_resspin, source->resolution, m_connection_resSpinChanged);
	m_connection_invcheckToggled.block(true);
	m_invcheck->set_active(source->invert);
	m_connection_invcheckToggled.block(false);

	if(renderImage()){
		m_canvas->show();
		m_viewport->get_window()->set_cursor(Gdk::Cursor::create(Gdk::TCROSS));
		m_scaleThread = Glib::Threads::Thread::create(sigc::mem_fun(this, &Displayer::scaleThread));
	}else{
		setSource(nullptr);
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to load image"), Glib::ustring::compose(_("The file might not be an image or be corrupt:\n%1"), source));
		return false;
	}
	return true;
}

bool Displayer::renderImage()
{
	sendScaleRequest({ScaleRequest::Abort});
	if(m_source->resolution != m_resspin->get_value_as_int()){
		double factor = double(m_resspin->get_value_as_int()) / double(m_source->resolution);
		for(DisplaySelection* sel : m_selections){
			sel->scale(factor);
		}
	}
	m_blurImage.clear();
	m_source->page = m_pagespin->get_value_as_int();
	m_source->brightness = m_brispin->get_value_as_int();
	m_source->contrast = m_conspin->get_value_as_int();
	m_source->resolution = m_resspin->get_value_as_int();
	m_source->invert = m_invcheck->get_active();
	Cairo::RefPtr<Cairo::ImageSurface> image;
	if(!Utils::busyTask([this, &image] {
		image = m_renderer->render(m_source->page, m_source->resolution);
		m_renderer->adjustImage(image, m_source->brightness, m_source->contrast, m_source->invert);
		return bool(image);
	}, _("Rendering image..."))){
		return false;
	}
	m_image = image;
	setRotation(m_rotspin->get_value());
	if(m_geo.s < 1.0){
		m_scaleTimer.disconnect();
		ScaleRequest request = {ScaleRequest::Scale, m_geo.s, m_source->resolution, m_source->page, m_source->brightness, m_source->contrast, m_source->invert};
		m_scaleTimer = Glib::signal_timeout().connect([this,request]{ sendScaleRequest(request); return false; }, 100);
	}
	return true;
}

void Displayer::setZoom(Zoom zoom)
{
	if(!m_image){
		return;
	}
	sendScaleRequest({ScaleRequest::Abort});
	m_connection_zoomfitClicked.block(true);
	m_connection_zoomoneClicked.block(true);

	Gtk::Allocation alloc = m_viewport->get_allocation();
	Geometry::Size bb = getImageBoundingBox();
	double fit = std::min(alloc.get_width() / bb.x, alloc.get_height() / bb.y);

	if(zoom == Zoom::In){
		m_geo.s = std::min(10., m_geo.s * 1.25);
	}else if(zoom == Zoom::Out){
		m_geo.s = std::max(0.05, m_geo.s * 0.8);
	}else if(zoom == Zoom::One){
		m_geo.s = 1.0;
	}
	m_zoomfitbtn->set_active(false);
	if(zoom == Zoom::Fit || (m_geo.s / fit >= 0.9 && m_geo.s / fit <= 1.09)){
		m_geo.s = fit;
		m_zoomfitbtn->set_active(true);
	}
	bool scrollVisible = m_geo.s > fit;
	m_scrollwin->get_hscrollbar()->set_visible(scrollVisible);
	m_scrollwin->get_vscrollbar()->set_visible(scrollVisible);
	m_zoomoutbtn->set_sensitive(m_geo.s > 0.05);
	m_zoominbtn->set_sensitive(m_geo.s < 10.);
	m_zoomonebtn->set_active(m_geo.s == 1.);
	if(m_geo.s < 1.0){
		m_scaleTimer.disconnect();
		ScaleRequest request = {ScaleRequest::Scale, m_geo.s, m_source->resolution, m_source->page, m_source->brightness, m_source->contrast, m_source->invert};
		m_scaleTimer = Glib::signal_timeout().connect([this,request]{ sendScaleRequest(request); return false; }, 100);
	}else{
		m_blurImage.clear();
	}
	positionCanvas();

	m_connection_zoomfitClicked.block(false);
	m_connection_zoomoneClicked.block(false);
}

void Displayer::setRotation(double angle)
{
	if(m_image){
		angle = angle < 0 ? angle + 360. : angle >= 360 ? angle - 360 : angle,
		Utils::set_spin_blocked(m_rotspin, angle, m_connection_rotSpinChanged);
		angle *= 0.0174532925199;
		double delta = angle - m_source->angle;
		m_source->angle = angle;
		Geometry::Rotation deltaR(delta);
		for(DisplaySelection* sel : m_selections){
			sel->rotate(deltaR);
		}
		if(m_zoomfitbtn->get_active() == true){
			setZoom(Zoom::Fit);
		}else{
			positionCanvas();
		}
	}
}

void Displayer::queueRenderImage()
{
	if(m_image){
		m_renderTimer.disconnect();
		m_renderTimer = Glib::signal_timeout().connect([this]{ renderImage(); return false; }, 200);
	}
}

bool Displayer::mousePressEvent(GdkEventButton* ev)
{
	if(!m_image || m_curSel){
		return false;
	}
	Geometry::Point point = mapToSceneClamped(ev->x, ev->y);
	if(ev->button == 3){
		for(int i = m_selections.size() - 1; i >= 0; --i){
			if(m_selections[i]->getRect().contains(point)){
				showSelectionMenu(ev, i);
				return true;
			}
		}
	}else{
		for(int i = m_selections.size() - 1; i >= 0; --i){
			m_curSel = m_selections[i]->getResizeHandle(point, m_geo.s);
			if(m_curSel) break;
		}
		if(!m_curSel){
			if((ev->state & Gdk::CONTROL_MASK) == 0){
				// Clear all existing selections
				clearSelections();
				m_canvas->queue_draw();
			}
			DisplaySelection* sel = new DisplaySelection(point.x, point.y);
			m_selections.push_back(sel);
			m_curSel = sel->getResizeHandle(point, m_geo.s);
		}
	}
	return true;
}

void Displayer::resizeEvent()
{
	if(m_zoomfitbtn->get_active() == true){
		setZoom(Zoom::Fit);
	}
}

bool Displayer::mouseMoveEvent(GdkEventMotion *ev)
{
	if(!m_image){
		return false;
	}
	Geometry::Point point = mapToSceneClamped(ev->x, ev->y);
	if(m_curSel){
		m_curSel->setPoint(point);
		m_canvas->queue_draw();

		m_scrollspeed[0] = m_scrollspeed[1] = 0;
		if(ev->x < m_hadj->get_value()){
			m_scrollspeed[0] = ev->x - m_hadj->get_value();
		}else if(ev->x > m_hadj->get_value() + m_hadj->get_page_size()){
			m_scrollspeed[0] = ev->x - m_hadj->get_value() - m_hadj->get_page_size();
		}
		if(ev->y < m_vadj->get_value()){
			m_scrollspeed[1] = ev->y - m_vadj->get_value();
		}else if(ev->y > m_vadj->get_value() + m_vadj->get_page_size()){
			m_scrollspeed[1] = ev->y - m_vadj->get_value() - m_vadj->get_page_size();
		}
		if((m_scrollspeed[0] != 0 || m_scrollspeed[1] != 0) && !m_scrollTimer.connected()){
			m_scrollTimer = Glib::signal_timeout().connect([this]{ return panViewport(); }, 25);
		}
	}else{
		for(DisplaySelection* sel : Utils::reverse(m_selections)){
			Gdk::CursorType cursor = sel->getResizeCursor(point, m_geo.s);
			if(cursor != Gdk::BLANK_CURSOR){
				m_viewport->get_window()->set_cursor(Gdk::Cursor::create(cursor));
				return true;
			}
		}
		m_viewport->get_window()->set_cursor(Gdk::Cursor::create(Gdk::TCROSS));
	}
	return true;
}

bool Displayer::mouseReleaseEvent(GdkEventButton* /*ev*/)
{
	if(!m_curSel){
		return false;
	}
	if(m_curSel->sel->isEmpty()){
		removeSelection(m_curSel->sel);
	}else{
		m_signal_selectionChanged.emit(true);
	}
	delete m_curSel;
	m_curSel = nullptr;
	m_scrollTimer.disconnect();

	return true;
}

bool Displayer::scrollEvent(GdkEventScroll *ev)
{
	if((ev->state & Gdk::CONTROL_MASK) != 0){
		if((ev->direction == GDK_SCROLL_UP || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y < 0)) && m_geo.s * 1.25 < 10){
			Gtk::Allocation alloc = m_canvas->get_allocation();
			m_geo.sx = std::max(0., std::min((ev->x + m_hadj->get_value() - alloc.get_x())/alloc.get_width(), 1.0));
			m_geo.sy = std::max(0., std::min((ev->y + m_vadj->get_value() - alloc.get_y())/alloc.get_height(), 1.0));
			setZoom(Zoom::In);
		}else if((ev->direction == GDK_SCROLL_DOWN || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y > 0)) && m_geo.s * 0.8 > 0.05){
			setZoom(Zoom::Out);
		}
		return true;
	}else if((ev->state & Gdk::SHIFT_MASK) != 0){
		if(ev->direction == GDK_SCROLL_UP || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y < 0)){
			m_hadj->set_value(m_hadj->get_value() - m_hadj->get_step_increment());
		}else if(ev->direction == GDK_SCROLL_DOWN || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y > 0)){
			m_hadj->set_value(m_hadj->get_value() + m_hadj->get_step_increment());
		}
		return true;
	}
	return false;
}

bool Displayer::panViewport()
{
	if(m_scrollspeed[0] == 0 && m_scrollspeed[1] == 0){
		return false;
	}
	m_hadj->set_value(std::min(std::max(0., m_hadj->get_value() + m_scrollspeed[0] / 10.), m_hadj->get_upper() - m_hadj->get_page_size()));
	m_vadj->set_value(std::min(std::max(0., m_vadj->get_value() + m_scrollspeed[1] / 10.), m_vadj->get_upper() - m_vadj->get_page_size()));
	return true;
}
Geometry::Size Displayer::getImageBoundingBox() const
{
	int w = m_image->get_width();
	int h = m_image->get_height();
	Geometry::Rotation R(m_source->angle);
	return Geometry::Size(
			std::abs(R(0, 0) * w) + std::abs(R(0, 1) * h),
			std::abs(R(1, 0) * w) + std::abs(R(1, 1) * h)
	);
}

Geometry::Point Displayer::mapToSceneClamped(double evx, double evy) const
{
	// Selection coordinates are with respect to the center of the image in unscaled (but rotated) coordinates
	Gtk::Allocation alloc = m_canvas->get_allocation();
	double x = (std::max(0., std::min(evx - alloc.get_x(), double(alloc.get_width()))) - 0.5 * alloc.get_width()) / m_geo.s;
	double y = (std::max(0., std::min(evy - alloc.get_y(), double(alloc.get_height()))) - 0.5 * alloc.get_height()) / m_geo.s;
	return Geometry::Point(x, y);
}

void Displayer::clearSelections()
{
	for(const DisplaySelection* sel : m_selections){ delete sel; }
	m_selections.clear();
	m_signal_selectionChanged.emit(false);
}

void Displayer::removeSelection(const DisplaySelection* sel)
{
	m_selections.erase(std::find(m_selections.begin(), m_selections.end(), sel));
	delete sel;
	m_canvas->queue_draw();
}

std::vector<Cairo::RefPtr<Cairo::ImageSurface>> Displayer::getSelections() const
{
	std::vector<Geometry::Rectangle> rects;
	if(m_selections.empty()){
		Geometry::Size bb = getImageBoundingBox();
		rects.push_back(Geometry::Rectangle(-0.5 * bb.x, -0.5 * bb.y, bb.x, bb.y));
	}else{
		rects.reserve(m_selections.size());
		for(const DisplaySelection* sel : m_selections){
			rects.push_back(sel->getRect());
		}
	}
	std::vector<Cairo::RefPtr<Cairo::ImageSurface>> images;
	for(const Geometry::Rectangle& rect : rects){
		images.push_back(getImage(rect));
	}
	return images;
}

void Displayer::saveSelection(const Geometry::Rectangle& rect)
{
	Cairo::RefPtr<Cairo::ImageSurface> img = getImage(rect);
	std::string filename = Utils::make_output_filename(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("selectionsavefile")->getValue());
	FileDialogs::FileFilter filter = {_("PNG Images"), "image/png", "*.png"};
	filename = FileDialogs::save_dialog(_("Save Selection Image"), filename, filter);
	if(!filename.empty()){
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("selectionsavefile")->setValue(filename);
		img->write_to_png(filename);
	}
}

void Displayer::showSelectionMenu(GdkEventButton *ev, int i)
{
	DisplaySelection* sel = m_selections[i];
	Gtk::SpinButton* spin = Builder("spin:selectionmenu.order");
	spin->get_adjustment()->set_upper(m_selections.size());
	spin->get_adjustment()->set_value(i + 1);
	m_selmenuConnections = {
		CONNECT(spin, value_changed, [this,spin,sel]{
			m_selections.erase(std::find(m_selections.begin(), m_selections.end(), sel));
			m_selections.insert(m_selections.begin() + spin->get_value_as_int() - 1, sel);
			m_canvas->queue_draw();
		}),
		CONNECT(Builder("button:selectionmenu.delete").as<Gtk::Button>(), clicked, [this,sel]{
			hideSelectionMenu();
			removeSelection(sel);
		}),
		CONNECT(Builder("button:selectionmenu.recognize").as<Gtk::Button>(), clicked, [this, sel]{
			hideSelectionMenu();
			MAIN->getRecognizer()->recognizeImage(getImage(sel->getRect()), Recognizer::OutputDestination::Buffer);
		}),
		CONNECT(Builder("button:selectionmenu.clipboard").as<Gtk::Button>(), clicked, [this, sel]{
			hideSelectionMenu();
			MAIN->getRecognizer()->recognizeImage(getImage(sel->getRect()), Recognizer::OutputDestination::Clipboard);
		}),
		CONNECT(Builder("button:selectionmenu.save").as<Gtk::Button>(), clicked, [this, sel]{
			hideSelectionMenu();
			saveSelection(sel->getRect());
		})
	};
	Glib::RefPtr<const Gdk::Screen> screen = MAIN->getWindow()->get_screen();
	Gdk::Rectangle rect = screen->get_monitor_workarea(screen->get_monitor_at_point(ev->x_root, ev->y_root));
	m_selmenu->show_all();
	int w, h, trash;
	m_selmenu->get_preferred_width(trash, w);
	m_selmenu->get_preferred_height(trash, h);
	int x = std::min(std::max(int(ev->x_root), rect.get_x()), rect.get_x() + rect.get_width() - w);
	int y = std::min(std::max(int(ev->y_root), rect.get_y()), rect.get_y() + rect.get_height() - h);
	m_selmenu->move(x, y);
	GdkWindow* gdkwin = m_selmenu->get_window()->gobj();
	gdk_device_grab(gtk_get_current_event_device(), gdkwin, GDK_OWNERSHIP_APPLICATION, true, GDK_BUTTON_PRESS_MASK, nullptr, ev->time);
}

void Displayer::hideSelectionMenu()
{
	m_selmenu->hide();
	for(sigc::connection& conn : m_selmenuConnections){
		conn.disconnect();
	}
	gdk_device_ungrab(gtk_get_current_event_device(), gtk_get_current_event_time());
}

Cairo::RefPtr<Cairo::ImageSurface> Displayer::getImage(const Geometry::Rectangle &rect) const
{
	Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, std::ceil(rect.width), std::ceil(rect.height));
		Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surf);
		ctx->set_source_rgba(1., 1., 1., 1.);
		ctx->paint();
		ctx->translate(-rect.x, -rect.y);
		ctx->rotate(m_source->angle);
		ctx->translate(-0.5 * m_image->get_width(), -0.5 * m_image->get_height());
		ctx->set_source(m_image, 0, 0);
		ctx->paint();
		return surf;
}

void Displayer::autodetectLayout(bool rotated)
{
	clearSelections();

	double avgDeskew = 0.;
	int nDeskew = 0;
	std::vector<Geometry::Rectangle> rects;
	Geometry::Size bb = getImageBoundingBox();
	Geometry::Rectangle rect(-0.5 * bb.x, -0.5 * bb.y, bb.x, bb.y);
	Cairo::RefPtr<Cairo::ImageSurface> img = getImage(rect);

	// Perform layout analysis
	Utils::busyTask([this,&nDeskew,&avgDeskew,&rects,&img]{
		tesseract::TessBaseAPI tess;
		tess.InitForAnalysePage();
		tess.SetPageSegMode(tesseract::PSM_AUTO_ONLY);
		tess.SetImage(img->get_data(), img->get_width(), img->get_height(), 4, img->get_stride());
		tesseract::PageIterator* it = tess.AnalyseLayout();
		if(it && !it->Empty(tesseract::RIL_BLOCK)){
			do{
				int x1, y1, x2, y2;
				tesseract::Orientation orient;
				tesseract::WritingDirection wdir;
				tesseract::TextlineOrder tlo;
				float deskew;
				it->BoundingBox(tesseract::RIL_BLOCK, &x1, &y1, &x2, &y2);
				it->Orientation(&orient, &wdir, &tlo, &deskew);
				avgDeskew += deskew;
				++nDeskew;
				if(x2-x1 > 10 && y2-y1 > 10){
					rects.push_back(Geometry::Rectangle(x1 - img->get_width()*.5, y1 - img->get_height()*.5, x2-x1, y2-y1));
				}
			}while(it->Next(tesseract::RIL_BLOCK));
		}
		delete it;
		return true;
	}, _("Performing layout analysis"));

	// If a somewhat large deskew angle is detected, automatically rotate image and redetect layout,
	// unless we already attempted to rotate (to prevent endless loops)
	avgDeskew = Utils::round(((avgDeskew/nDeskew)/M_PI * 180.) * 10.) / 10.;

	if(std::abs(avgDeskew > .1) && !rotated){
		setRotation(m_rotspin->get_value() - avgDeskew);
		autodetectLayout(true);
	}else{
		// Merge overlapping rectangles
		for(unsigned i = rects.size(); i-- > 1;) {
			for(unsigned j = i; j-- > 0;) {
				if(rects[j].overlaps(rects[i])) {
					rects[j] = rects[j].unite(rects[i]);
					rects.erase(rects.begin() + i);
					break;
				}
			}
		}
		for(const Geometry::Rectangle& rect : rects) {
			m_selections.push_back(new DisplaySelection(rect.x, rect.y, rect.width, rect.height));
		}
		m_canvas->queue_draw();
		m_signal_selectionChanged.emit(true);
	}
}

void Displayer::sendScaleRequest(const ScaleRequest& request)
{
	m_scaleMutex.lock();
	m_scaleRequests.push(request);
	m_scaleCond.signal();
	m_scaleMutex.unlock();
}

void Displayer::scaleThread()
{
	m_scaleMutex.lock();
	while(true){
		while(m_scaleRequests.empty()){
			m_scaleCond.wait(m_scaleMutex);
		}
		ScaleRequest req = m_scaleRequests.front();
		m_scaleRequests.pop();
		if(req.type == ScaleRequest::Quit){
			break;
		}else if(req.type == ScaleRequest::Scale){
			m_scaleMutex.unlock();
			Cairo::RefPtr<Cairo::ImageSurface> image = m_renderer->render(req.page, req.scale * req.resolution);

			m_scaleMutex.lock();
			if(!m_scaleRequests.empty() && m_scaleRequests.front().type == ScaleRequest::Abort){
				m_scaleRequests.pop();
				continue;
			}
			m_scaleMutex.unlock();

			m_renderer->adjustImage(image, req.brightness, req.contrast, req.invert);

			m_scaleMutex.lock();
			if(!m_scaleRequests.empty() && m_scaleRequests.front().type == ScaleRequest::Abort){
				m_scaleRequests.pop();
				continue;
			}
			m_scaleMutex.unlock();

			double scale = req.scale;
			Glib::signal_idle().connect_once([this,image,scale]{ setScaledImage(image, scale); });
			m_scaleMutex.lock();
		}
	};
	m_scaleMutex.unlock();
}

void Displayer::setScaledImage(Cairo::RefPtr<Cairo::ImageSurface> image, double scale)
{
	m_scaleMutex.lock();
	if(!m_scaleRequests.empty() && m_scaleRequests.front().type == ScaleRequest::Abort){
		m_scaleRequests.pop();
	}else{
		m_blurImage = image;
		m_blurScale = scale;
		m_canvas->queue_draw();
	}
	m_scaleMutex.unlock();
}

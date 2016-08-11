/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.cc
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

#include "MainWindow.hh"
#include "Config.hh"
#include "Displayer.hh"
#include "DisplayRenderer.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include "FileDialogs.hh"

#include <tesseract/baseapi.h>

Displayer::Displayer()
{
	m_canvas = MAIN->getWidget("drawingarea:display");
	m_viewport = MAIN->getWidget("viewport:display");
	m_scrollwin = MAIN->getWidget("scrollwin:display");
	m_hadj = m_scrollwin->get_hadjustment();
	m_vadj = m_scrollwin->get_vadjustment();
	m_zoominbtn = MAIN->getWidget("button:main.zoomin");
	m_zoomoutbtn = MAIN->getWidget("button:main.zoomout");
	m_zoomonebtn = MAIN->getWidget("button:main.zoomnormsize");
	m_zoomfitbtn = MAIN->getWidget("button:main.zoomfit");
	m_rotspin = MAIN->getWidget("spin:display.rotate");
	m_pagespin = MAIN->getWidget("spin:display.page");
	m_resspin = MAIN->getWidget("spin:display.resolution");
	m_brispin = MAIN->getWidget("spin:display.brightness");
	m_conspin = MAIN->getWidget("spin:display.contrast");
	m_invcheck = MAIN->getWidget("check:display.invert");

#if GTKMM_CHECK_VERSION(3,12,0)
	m_pagespin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/page.png"));
	m_resspin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/resolution.png"));
	m_brispin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/brightness.png"));
	m_conspin->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/contrast.png"));
#else
	m_pagespin->set_icon_from_pixbuf(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/page.png", 0)));
	m_resspin->set_icon_from_pixbuf(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/resolution.png", 0)));
	m_brispin->set_icon_from_pixbuf(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/brightness.png", 0)));
	m_conspin->set_icon_from_pixbuf(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/contrast.png", 0)));
#endif

	m_scrollwin->drag_dest_set({Gtk::TargetEntry("text/uri-list")}, Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
	m_viewport->override_background_color(Gdk::RGBA("#a0a0a4"));

	m_connection_rotSpinChanged = CONNECT(m_rotspin, value_changed, [this]{ setAngle(m_rotspin->get_value()); });
	m_connection_pageSpinChanged = CONNECT(m_pagespin, value_changed, [this]{ setCurrentPage(m_pagespin->get_value_as_int()); });
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
	CONNECT(MAIN->getWidget("spin:display.rotate").as<Gtk::SpinButton>(), icon_press, [this](Gtk::EntryIconPosition pos, const GdkEventButton*) {
		setAngle(m_rotspin->get_value() + (pos == Gtk::ENTRY_ICON_PRIMARY ? -90 : 90));
	});
	CONNECT(MAIN->getWidget("applicationwindow:main").as<Gtk::Window>()->get_style_context(), changed, [this]{ m_canvas->queue_draw(); });

	CONNECT(m_scrollwin, drag_data_received, sigc::ptr_fun(Utils::handle_drag_drop));
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
	ctx->rotate(m_currentSource->angle);
	// Set source and apply all transformations to it
	if(!m_blurImage){
		ctx->scale(m_scale, m_scale);
		ctx->translate(-0.5 * m_image->get_width(), -0.5 * m_image->get_height());
		ctx->set_source(m_image, 0, 0);
	}else{
		ctx->scale(m_scale / m_blurScale, m_scale / m_blurScale);
		ctx->translate(-0.5 * m_blurImage->get_width(), -0.5 * m_blurImage->get_height());
		ctx->set_source(m_blurImage, 0, 0);
	}
	ctx->paint();
	// Draw selections
	ctx->restore();
	ctx->translate(Utils::round(0.5 * alloc.get_width()), Utils::round(0.5 * alloc.get_height()));
	ctx->scale(m_scale, m_scale);
	for(const DisplayerItem* item : m_items){
		item->draw(ctx);
	}
}

void Displayer::positionCanvas()
{
	Geometry::Rectangle bb = getImageBoundingBox();
	m_canvas->set_size_request(Utils::round(bb.width * m_scale), Utils::round(bb.height * m_scale));
	// Immediately resize viewport, so that adjustment values are correct below
	m_viewport->size_allocate(m_viewport->get_allocation());
	m_viewport->set_allocation(m_viewport->get_allocation());
	m_hadj->set_value(m_scrollPos[0] *(m_hadj->get_upper() - m_hadj->get_page_size()));
	m_vadj->set_value(m_scrollPos[1] *(m_vadj->get_upper() - m_vadj->get_page_size()));
	m_canvas->queue_draw();
}

std::string Displayer::getCurrentImage(int& page) const
{
	auto it = m_pageMap.find(m_pagespin->get_value_as_int());
	if(it != m_pageMap.end()) {
		page = it->second.second;
		return it->second.first->file->get_path();
	}
	return "";
}

bool Displayer::setCurrentPage(int page)
{
	if(m_sources.empty()) {
		return false;
	}
	if(m_tool) {
		m_tool->pageChanged();
	}
	Source* source = m_pageMap[page].first;
	if(source != m_currentSource) {
		delete m_renderer;
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
		Utils::set_spin_blocked(m_rotspin, source->angle / M_PI * 180., m_connection_rotSpinChanged);
		Utils::set_spin_blocked(m_brispin, source->brightness, m_connection_briSpinChanged);
		Utils::set_spin_blocked(m_conspin, source->contrast, m_connection_conSpinChanged);
		Utils::set_spin_blocked(m_resspin, source->resolution, m_connection_resSpinChanged);
		m_connection_invcheckToggled.block(true);
		m_invcheck->set_active(source->invert);
		m_connection_invcheckToggled.block(false);
		m_currentSource = source;
	}
	Utils::set_spin_blocked(m_pagespin, page, m_connection_pageSpinChanged);
	return renderImage();
}

bool Displayer::setSources(std::vector<Source*> sources)
{
	if(sources == m_sources) {
		return true;
	}
	if(m_scaleThread){
		sendScaleRequest({ScaleRequest::Quit});
		m_scaleThread->join();
		m_scaleThread = nullptr;
	}
	m_scale = 1.0;
	m_scrollPos[0] = m_scrollPos[1] = 0.5;
	if(m_tool) {
		m_tool->pageChanged();
	}
	m_renderTimer.disconnect();
	m_image.clear();
	m_blurImage.clear();
	delete m_renderer;
	m_renderer = 0;
	m_currentSource = nullptr;
	m_sources.clear();
	m_pageMap.clear();
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

	m_sources = sources;
	if(sources.empty()){
		return false;
	}

	int page = 0;
	for(Source* source : m_sources) {
		std::string filename = source->file->get_path();
#ifdef G_OS_WIN32
		if(Glib::ustring(filename.substr(filename.length() - 4)).lowercase() == ".pdf"){
#else
		if(Utils::get_content_type(filename) == "application/pdf"){
#endif
			PDFRenderer r(filename);
			for(int pdfPage = 1, nPdfPages = r.getNPages(); pdfPage <= nPdfPages; ++pdfPage)
			{
				m_pageMap.insert(std::make_pair(++page, std::make_pair(source, pdfPage)));
			}
		} else {
			m_pageMap.insert(std::make_pair(++page, std::make_pair(source, 1)));
		}
	}

	m_pagespin->get_adjustment()->set_upper(page);
	m_pagespin->set_visible(page > 1);
	m_viewport->get_window()->set_cursor(Gdk::Cursor::create(Gdk::TCROSS));
	m_canvas->show();
	m_scaleThread = Glib::Threads::Thread::create(sigc::mem_fun(this, &Displayer::scaleThread));

	if(!setCurrentPage(1)) {
		setSources(std::vector<Source*>());
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to load image"), Glib::ustring::compose(_("The file might not be an image or be corrupt:\n%1"), m_currentSource->displayname));
		return false;
	}
	return true;
}

bool Displayer::hasMultipleOCRAreas()
{
	return m_tool->hasMultipleOCRAreas();
}

std::vector<Cairo::RefPtr<Cairo::ImageSurface>> Displayer::getOCRAreas()
{
	return m_tool->getOCRAreas();
}

void Displayer::autodetectOCRAreas()
{
	m_tool->autodetectOCRAreas();
}

bool Displayer::renderImage()
{
	sendScaleRequest({ScaleRequest::Abort});
	if(m_currentSource->resolution != m_resspin->get_value_as_int()){
		double factor = double(m_resspin->get_value_as_int()) / double(m_currentSource->resolution);
		if(m_tool) {
			m_tool->resolutionChanged(factor);
		}
	}
	m_blurImage.clear();
	m_currentSource->page = m_pageMap[m_pagespin->get_value_as_int()].second;
	m_currentSource->brightness = m_brispin->get_value_as_int();
	m_currentSource->contrast = m_conspin->get_value_as_int();
	m_currentSource->resolution = m_resspin->get_value_as_int();
	m_currentSource->invert = m_invcheck->get_active();
	Cairo::RefPtr<Cairo::ImageSurface> image = m_renderer->render(m_currentSource->page, m_currentSource->resolution);
	m_renderer->adjustImage(image, m_currentSource->brightness, m_currentSource->contrast, m_currentSource->invert);
	if(!bool(image)) {
		return false;
	}
	m_image = image;
	setAngle(m_rotspin->get_value());
	if(m_scale < 1.0){
		ScaleRequest request = {ScaleRequest::Scale, m_scale, m_currentSource->resolution, m_currentSource->page, m_currentSource->brightness, m_currentSource->contrast, m_currentSource->invert};
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
	Geometry::Rectangle bb = getImageBoundingBox();
	double fit = std::min(alloc.get_width() / bb.width, alloc.get_height() / bb.height);

	if(zoom == Zoom::In){
		m_scale = std::min(10., m_scale * 1.25);
	}else if(zoom == Zoom::Out){
		m_scale = std::max(0.05, m_scale * 0.8);
	}else if(zoom == Zoom::One){
		m_scale = 1.0;
	}
	m_zoomfitbtn->set_active(false);
	if(zoom == Zoom::Fit || (m_scale / fit >= 0.9 && m_scale / fit <= 1.09)){
		m_scale = fit;
		m_zoomfitbtn->set_active(true);
	}
	bool scrollVisible = m_scale > fit;
	m_scrollwin->get_hscrollbar()->set_visible(scrollVisible);
	m_scrollwin->get_vscrollbar()->set_visible(scrollVisible);
	m_zoomoutbtn->set_sensitive(m_scale > 0.05);
	m_zoominbtn->set_sensitive(m_scale < 10.);
	m_zoomonebtn->set_active(m_scale == 1.);
	if(m_scale < 1.0){
		ScaleRequest request = {ScaleRequest::Scale, m_scale, m_currentSource->resolution, m_currentSource->page, m_currentSource->brightness, m_currentSource->contrast, m_currentSource->invert};
		m_scaleTimer = Glib::signal_timeout().connect([this,request]{ sendScaleRequest(request); return false; }, 100);
	}else{
		m_blurImage.clear();
	}
	positionCanvas();

	m_connection_zoomfitClicked.block(false);
	m_connection_zoomoneClicked.block(false);
}

void Displayer::setAngle(double angle)
{
	if(m_image){
		angle = angle < 0 ? angle + 360. : angle >= 360 ? angle - 360 : angle,
		Utils::set_spin_blocked(m_rotspin, angle, m_connection_rotSpinChanged);
		angle *= 0.0174532925199;
		double delta = angle - m_currentSource->angle;
		m_currentSource->angle = angle;
		if(m_tool) {
			m_tool->rotationChanged(delta);
		}
		if(m_zoomfitbtn->get_active() == true){
			setZoom(Zoom::Fit);
		}else{
			positionCanvas();
		}
	}
}

void Displayer::setResolution(int resolution)
{
	Utils::set_spin_blocked(m_resspin, resolution, m_connection_resSpinChanged);
	renderImage();
}

void Displayer::setCursor(Glib::RefPtr<Gdk::Cursor> cursor)
{
	if(cursor) {
		m_viewport->get_window()->set_cursor(cursor);
	} else {
		m_viewport->get_window()->set_cursor(Gdk::Cursor::create(Gdk::TCROSS));
	}
}

void Displayer::queueRenderImage()
{
	if(m_image){
		m_renderTimer.disconnect();
		m_renderTimer = Glib::signal_timeout().connect([this]{ renderImage(); return false; }, 200);
	}
}

void Displayer::resizeEvent()
{
	if(m_zoomfitbtn->get_active() == true){
		setZoom(Zoom::Fit);
	}
}

bool Displayer::mousePressEvent(GdkEventButton* ev)
{
	if(ev->button == 2){
		m_panPos[0] = ev->x_root;
		m_panPos[1] = ev->y_root;
		return true;
	}
	Geometry::Point scenePos = mapToSceneClamped(Geometry::Point(ev->x, ev->y));
	for(DisplayerItem* item : Utils::reverse(m_items)){
		if(item->rect().contains(scenePos)){
			m_activeItem = item;
			break;
		}
	}
	if(m_activeItem && m_activeItem->mousePressEvent(ev)) {
		return true;
	}
	if(m_tool && m_currentSource) {
		return m_tool->mousePressEvent(ev);
	}
	return false;
}

bool Displayer::mouseMoveEvent(GdkEventMotion *ev)
{
	if(ev->state & Gdk::BUTTON2_MASK) {
		double dx = m_panPos[0] - ev->x_root;
		double dy = m_panPos[1] - ev->y_root;
		m_hadj->set_value(m_hadj->get_value() + dx);
		m_vadj->set_value(m_vadj->get_value() + dy);
		m_panPos[0] = ev->x_root;
		m_panPos[1] = ev->y_root;
		return true;
	}
	if(m_activeItem && m_activeItem->mouseMoveEvent(ev)) {
		return true;
	}
	bool overItem = false;
	Geometry::Point scenePos = mapToSceneClamped(Geometry::Point(ev->x, ev->y));
	for(DisplayerItem* item : Utils::reverse(m_items)){
		if(item->rect().contains(scenePos)){
			overItem = true;
			if(item->mouseMoveEvent(ev)) {
				return true;
			}
			break;
		}
	}
	if(!overItem) {
		setCursor(Glib::RefPtr<Gdk::Cursor>(0));
	}
	if(m_tool && m_currentSource) {
		return m_tool->mouseMoveEvent(ev);
	}
	return false;
}

bool Displayer::mouseReleaseEvent(GdkEventButton* ev)
{
	if(m_activeItem) {
		bool accepted = m_activeItem->mouseReleaseEvent(ev);
		m_activeItem = nullptr;
		if(accepted) {
			return true;
		}
	}
	if(m_tool && m_currentSource) {
		return m_tool->mouseReleaseEvent(ev);
	}
	return false;
}

bool Displayer::scrollEvent(GdkEventScroll *ev)
{
	if((ev->state & Gdk::CONTROL_MASK) != 0){
		if((ev->direction == GDK_SCROLL_UP || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y < 0)) && m_scale * 1.25 < 10){
			Gtk::Allocation alloc = m_canvas->get_allocation();
			m_scrollPos[0] = std::max(0., std::min((ev->x + m_hadj->get_value() - alloc.get_x())/alloc.get_width(), 1.0));
			m_scrollPos[1] = std::max(0., std::min((ev->y + m_vadj->get_value() - alloc.get_y())/alloc.get_height(), 1.0));
			setZoom(Zoom::In);
		}else if((ev->direction == GDK_SCROLL_DOWN || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y > 0)) && m_scale * 0.8 > 0.05){
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

void Displayer::ensureVisible(double evx, double evy)
{
	if(evx - 30. < m_hadj->get_value()){
		m_hadj->set_value(std::max(0., evx - 30.));
	}else if(evx + 30. > m_hadj->get_value() + m_hadj->get_page_size()){
		m_hadj->set_value(std::min(m_hadj->get_upper(), evx + 30.) - m_hadj->get_page_size());
	}
	if(evy - 30.< m_vadj->get_value()){
		m_vadj->set_value(std::max(0., evy - 30.));
	}else if(evy + 30. > m_vadj->get_value() + m_vadj->get_page_size()){
		m_vadj->set_value(std::min(m_vadj->get_upper(), evy + 30.) - m_vadj->get_page_size());
	}
}

Geometry::Rectangle Displayer::getImageBoundingBox() const
{
	int w = m_image->get_width();
	int h = m_image->get_height();
	Geometry::Rotation R(m_currentSource->angle);
	int width = std::abs(R(0, 0) * w) + std::abs(R(0, 1) * h);
	int height = std::abs(R(1, 0) * w) + std::abs(R(1, 1) * h);
	return Geometry::Rectangle(-0.5 * width, -0.5 * height, width, height);
}

Geometry::Point Displayer::mapToSceneClamped(const Geometry::Point& p) const
{
	// Selection coordinates are with respect to the center of the image in unscaled (but rotated) coordinates
	Gtk::Allocation alloc = m_canvas->get_allocation();
	double x = (std::max(0., std::min(p.x - alloc.get_x(), double(alloc.get_width()))) - 0.5 * alloc.get_width()) / m_scale;
	double y = (std::max(0., std::min(p.y - alloc.get_y(), double(alloc.get_height()))) - 0.5 * alloc.get_height()) / m_scale;
	return Geometry::Point(x, y);
}

Cairo::RefPtr<Cairo::ImageSurface> Displayer::getImage(const Geometry::Rectangle &rect) const
{
	Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, std::ceil(rect.width), std::ceil(rect.height));
		Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surf);
		ctx->set_source_rgba(1., 1., 1., 1.);
		ctx->paint();
		ctx->translate(-rect.x, -rect.y);
		ctx->rotate(m_currentSource->angle);
		ctx->translate(-0.5 * m_image->get_width(), -0.5 * m_image->get_height());
		ctx->set_source(m_image, 0, 0);
		ctx->paint();
		return surf;
}

void Displayer::sendScaleRequest(const ScaleRequest& request)
{
	m_scaleTimer.disconnect();
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

///////////////////////////////////////////////////////////////////////////////

void DisplayerTool::addItemToCanvas(DisplayerItem* item)
{
	m_displayer->m_items.push_back(item);
	invalidateRect(item->rect());
}

void DisplayerTool::removeItemFromCanvas(DisplayerItem* item)
{
	if(item == m_displayer->m_activeItem) {
		m_displayer->m_activeItem = nullptr;
	}
	m_displayer->m_items.erase(std::remove(m_displayer->m_items.begin(), m_displayer->m_items.end(), item), m_displayer->m_items.end());
	invalidateRect(item->rect());
}

void DisplayerTool::reorderCanvasItems()
{
	std::sort(m_displayer->m_items.begin(), m_displayer->m_items.end(), DisplayerItem::zIndexCmp);
}

Geometry::Rectangle DisplayerTool::getSceneBoundingRect() const
{
	return m_displayer->getImageBoundingBox();
}

void DisplayerTool::invalidateRect(const Geometry::Rectangle& rect)
{
	Gtk::Allocation alloc = m_displayer->m_canvas->get_allocation();
	Geometry::Rectangle canvasRect = rect;
	canvasRect.x = (canvasRect.x * m_displayer->m_scale + 0.5 * alloc.get_width()) - 2;
	canvasRect.y = (canvasRect.y * m_displayer->m_scale + 0.5 * alloc.get_height()) - 2;
	canvasRect.width = canvasRect.width * m_displayer->m_scale + 4;
	canvasRect.height = canvasRect.height * m_displayer->m_scale + 4;
	m_displayer->m_canvas->queue_draw_area(canvasRect.x, canvasRect.y, canvasRect.width, canvasRect.height);
}

Geometry::Point DisplayerTool::mapToSceneClamped(const Geometry::Point& point)
{
	return m_displayer->mapToSceneClamped(point);
}

Cairo::RefPtr<Cairo::ImageSurface> DisplayerTool::getImage(const Geometry::Rectangle& rect)
{
	return m_displayer->getImage(rect);
}

double DisplayerTool::getDisplayScale() const
{
	return m_displayer->m_scale;
}

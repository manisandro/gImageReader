/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Displayer.cc
 * Copyright (C) 2013-2020 Sandro Mani <manisandro@gmail.com>
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
#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "DisplayRenderer.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include "FileDialogs.hh"

#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

Displayer::Displayer(const Ui::MainWindow& _ui)
	: ui(_ui), m_scaleThreadCanceled(false), m_scaleThreadIdleJobsCount(0), m_thumbThreadCanceled(false), m_thumbThreadIdleJobsCount(0) {
	m_defaultCursor = Gdk::Cursor::create(Gdk::ARROW);
	m_hadj = ui.scrollwinDisplay->get_hadjustment();
	m_vadj = ui.scrollwinDisplay->get_vadjustment();

	m_rotateMode = RotateMode::AllPages;

	ui.spinPage->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/page.png"));
	ui.spinResolution->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/resolution.png"));
	ui.spinBrightness->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/brightness.png"));
	ui.spinContrast->set_icon_from_pixbuf(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/contrast.png"));

	ui.scrollwinDisplay->drag_dest_set({Gtk::TargetEntry("text/uri-list")}, Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
	ui.viewportDisplay->override_background_color(Gdk::RGBA("#a0a0a4"));

	CONNECT(ui.menuitemDisplayRotateCurrent, activate, [this] { setRotateMode(RotateMode::CurrentPage, "rotate_page.png"); });
	CONNECT(ui.menuitemDisplayRotateAll, activate, [this] { setRotateMode(RotateMode::AllPages, "rotate_pages.png"); });
	CONNECT(ui.checkBoxThumbnails, toggled, [this] { thumbnailsToggled(); });
	m_connection_rotSpinChanged = CONNECT(ui.spinRotate, value_changed, [this] { setAngle(ui.spinRotate->get_value()); });
	m_connection_pageSpinChanged = CONNECT(ui.spinPage, value_changed, [this] { queueRenderImage(); });
	m_connection_briSpinChanged = CONNECT(ui.spinBrightness, value_changed, [this] { queueRenderImage(); });
	m_connection_conSpinChanged = CONNECT(ui.spinContrast, value_changed, [this] { queueRenderImage(); });
	m_connection_resSpinChanged = CONNECT(ui.spinResolution, value_changed, [this] { queueRenderImage(); });
	m_connection_invcheckToggled = CONNECT(ui.checkInvert, toggled, [this] { queueRenderImage(); });
	CONNECT(ui.viewportDisplay, size_allocate, [this](Gdk::Rectangle&) {
		resizeEvent();
	});
	CONNECT(ui.viewportDisplay, key_press_event, [this](GdkEventKey * ev) {
		return keyPressEvent(ev);
	});
	CONNECT(ui.viewportDisplay, motion_notify_event, [this](GdkEventMotion * ev) {
		return mouseMoveEvent(ev);
	});
	CONNECT(ui.viewportDisplay, button_press_event, [this](GdkEventButton * ev) {
		return mousePressEvent(ev);
	});
	CONNECT(ui.viewportDisplay, button_release_event, [this](GdkEventButton * ev) {
		return mouseReleaseEvent(ev);
	});
	CONNECT(ui.viewportDisplay, scroll_event, [this](GdkEventScroll * ev) {
		return scrollEvent(ev);
	});
	CONNECT(ui.drawingareaDisplay, draw, [this](const Cairo::RefPtr<Cairo::Context>& ctx) {
		drawCanvas(ctx);
		return false;
	});
	CONNECT(ui.buttonZoomin, clicked, [this] { setZoom(Zoom::In); });
	CONNECT(ui.buttonZoomout, clicked, [this] { setZoom(Zoom::Out); });
	m_connection_zoomfitClicked = CONNECT(ui.buttonZoomfit, clicked, [this] { setZoom(Zoom::Fit); });
	m_connection_zoomoneClicked = CONNECT(ui.buttonZoomnorm, clicked, [this] { setZoom(Zoom::One); });
	CONNECT(ui.spinRotate, icon_press, [this](Gtk::EntryIconPosition pos, const GdkEventButton*) {
		setAngle(ui.spinRotate->get_value() + (pos == Gtk::ENTRY_ICON_PRIMARY ? -90 : 90));
	});
	CONNECT(ui.windowMain->get_style_context(), changed, [this] { ui.drawingareaDisplay->queue_draw(); });

	CONNECT(ui.scrollwinDisplay, drag_data_received, sigc::ptr_fun(Utils::handle_drag_drop));
	m_connection_thumbClicked = CONNECT(ui.iconviewThumbnails, selection_changed, [this] {
		if(!ui.iconviewThumbnails->get_selected_items().empty()) {
			ui.spinPage->set_value(1 + ui.iconviewThumbnails->get_selected_items().front().front());
		}
	});
	CONNECT(ui.spinPage, value_changed, [this] {
		m_connection_thumbClicked.block(true);
		Gtk::TreePath path(1u, ui.spinPage->get_value_as_int() - 1);
		ui.iconviewThumbnails->select_path(path);
		ui.iconviewThumbnails->scroll_to_path(path, false, 0., 0.);
		m_connection_thumbClicked.block(false);
	});

	ADD_SETTING(SwitchSettingT<Gtk::ToggleButton>("thumbnails", ui.checkBoxThumbnails));
	// Adjust paned position as soon as window is visible
	Glib::signal_idle().connect_once([this] { thumbnailsToggled(); });
}

void Displayer::drawCanvas(const Cairo::RefPtr<Cairo::Context>& ctx) {
	if(!m_imageItem) {
		return;
	}
	Gtk::Allocation alloc = ui.drawingareaDisplay->get_allocation();
	ctx->translate(Utils::round(0.5 * alloc.get_width()), Utils::round(0.5 * alloc.get_height()));
	ctx->scale(m_scale, m_scale);
	m_imageItem->draw(ctx);
	for(const DisplayerItem* item : m_items) {
		if(item->visible()) {
			item->draw(ctx);
		}
	}
}

void Displayer::positionCanvas() {
	Geometry::Rectangle bb = getSceneBoundingRect();
	ui.drawingareaDisplay->set_size_request(Utils::round(bb.width * m_scale), Utils::round(bb.height * m_scale));
	// Immediately resize viewport, so that adjustment values are correct below
	ui.viewportDisplay->size_allocate(ui.viewportDisplay->get_allocation());
	ui.viewportDisplay->set_allocation(ui.viewportDisplay->get_allocation());
	m_hadj->set_value(m_scrollPos[0] * (m_hadj->get_upper() - m_hadj->get_page_size()));
	m_vadj->set_value(m_scrollPos[1] * (m_vadj->get_upper() - m_vadj->get_page_size()));
	ui.drawingareaDisplay->queue_draw();
}

int Displayer::getCurrentPage() const {
	return ui.spinPage->get_value_as_int();
}

int Displayer::getCurrentResolution() const {
	return ui.spinResolution->get_value_as_int();
}

double Displayer::getCurrentAngle() const {
	return ui.spinRotate->get_value();
}

std::string Displayer::getCurrentImage(int& page) const {
	auto it = m_pageMap.find(ui.spinPage->get_value_as_int());
	if(it != m_pageMap.end()) {
		page = it->second.second;
		return it->second.first->file->get_path();
	}
	return "";
}

int Displayer::getNPages() const {
	double min, max;
	ui.spinPage->get_range(min, max);
	return int(max);
}

bool Displayer::setSources(std::vector<Source*> sources) {
	if(sources == m_sources) {
		return true;
	}
	waitForThread(m_scaleThread, m_scaleThreadCanceled, m_scaleThreadIdleJobsCount, &m_scaleTimer);
	waitForThread(m_thumbThread, m_thumbThreadCanceled, m_thumbThreadIdleJobsCount);
	m_scale = 1.0;
	m_scrollPos[0] = m_scrollPos[1] = 0.5;
	ui.iconviewThumbnails->set_model(Gtk::ListStore::create(m_thumbListViewCols));
	ui.iconviewThumbnails->set_pixbuf_column(0);
	ui.iconviewThumbnails->set_text_column(1);
	if(m_tool) {
		m_tool->reset();
	}
	m_renderTimer.disconnect();
	delete m_imageItem;
	m_imageItem = nullptr;
	m_image.clear();
	m_currentSource = nullptr;
	std::for_each(m_sourceRenderers.begin(), m_sourceRenderers.end(), [](auto pair) { delete pair.second; });
	m_sourceRenderers.clear();
	m_sources.clear();
	m_pageMap.clear();
	ui.drawingareaDisplay->hide();
	ui.spinPage->hide();
	m_connection_pageSpinChanged.block(true);
	ui.spinPage->set_range(1, 1);
	m_connection_pageSpinChanged.block(false);
	Utils::set_spin_blocked(ui.spinRotate, 0, m_connection_rotSpinChanged);
	Utils::set_spin_blocked(ui.spinBrightness, 0, m_connection_briSpinChanged);
	Utils::set_spin_blocked(ui.spinContrast, 0, m_connection_conSpinChanged);
	Utils::set_spin_blocked(ui.spinResolution, 100, m_connection_resSpinChanged);
	m_connection_invcheckToggled.block(true);
	ui.checkInvert->set_active(false);
	m_connection_invcheckToggled.block(false);
	ui.buttonZoomfit->set_active(true);
	ui.buttonZoomnorm->set_active(false);
	ui.buttonZoomin->set_sensitive(true);
	ui.buttonZoomout->set_sensitive(true);
	if(ui.viewportDisplay->get_window()) { ui.viewportDisplay->get_window()->set_cursor(); }

	m_sources = sources;
	if(sources.empty()) {
		m_signalImageChanged.emit();
		return false;
	}

	int page = 0;
	for(Source* source : m_sources) {
		DisplayRenderer* renderer = nullptr;
		std::string filename = source->file->get_path();
#ifdef G_OS_WIN32
		if(Glib::ustring(filename.substr(filename.length() - 4)).lowercase() == ".pdf") {
#else
		if(Utils::get_content_type(filename) == "application/pdf") {
#endif
			renderer = new PDFRenderer(filename, source->password);
			if(source->resolution == -1) { source->resolution = 300; }
#ifdef G_OS_WIN32
		} else if(Glib::ustring(filename.substr(filename.length() - 4)).lowercase() == ".djvu") {
#else
		} else if(Utils::get_content_type(filename) == "image/vnd.djvu") {
#endif
			renderer = new DJVURenderer(filename);
			if(source->resolution == -1) { source->resolution = 300; }
		} else {
			renderer = new ImageRenderer(filename);
			if(source->resolution == -1) { source->resolution = 100; }
		}
		if(renderer->getNPages() >= 0) {
			source->angle.resize(renderer->getNPages(), 0.); // getNPages can potentially return -1
		}
		m_sourceRenderers[source] = renderer;
		for(int iPage = 1, nPages = renderer->getNPages(); iPage <= nPages; ++iPage) {
			m_pageMap.insert(std::make_pair(++page, std::make_pair(source, iPage)));
		}
	}
	if(page == 0) {
		return setSources(std::vector<Source*>()); // cleanup
	}

	generateThumbnails();

	m_connection_pageSpinChanged.block();
	ui.spinPage->get_adjustment()->set_upper(page);
	m_connection_pageSpinChanged.unblock();
	ui.spinPage->set_visible(page > 1);
	ui.viewportDisplay->get_window()->set_cursor(m_defaultCursor);
	ui.drawingareaDisplay->show();
	m_imageItem = new DisplayerImageItem;

	if(!renderImage()) {
		g_assert_nonnull(m_currentSource);
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Failed to load image"), Glib::ustring::compose(_("The file might not be an image or be corrupt:\n%1"), m_currentSource->displayname));
		setSources(std::vector<Source*>());
		return false;
	}
	return true;
}

bool Displayer::setup(const int* page, const int* resolution, const double* angle) {
	bool changed = false;
	if(page) {
		changed |= *page != ui.spinPage->get_value_as_int();
		Utils::set_spin_blocked(ui.spinPage, *page, m_connection_pageSpinChanged);
	}
	if(resolution) {
		changed |= *resolution != ui.spinResolution->get_value();
		Utils::set_spin_blocked(ui.spinResolution, *resolution, m_connection_resSpinChanged);
	}
	if(changed && !renderImage()) {
		return false;
	}
	if(angle && std::abs(*angle - ui.spinRotate->get_value()) > 1E-2) {
		setAngle(*angle);
	}
	return true;
}

void Displayer::queueRenderImage() {
	m_renderTimer.disconnect();
	m_renderTimer = Glib::signal_timeout().connect([this] { renderImage(); return false; }, 200);
}

bool Displayer::renderImage() {
	if(m_sources.empty()) {
		return false;
	}
	waitForThread(m_scaleThread, m_scaleThreadCanceled, m_scaleThreadIdleJobsCount, &m_scaleTimer);
	int page = ui.spinPage->get_value_as_int();

	// Set current source according to selected page
	Source* source = m_pageMap[page].first;
	if(!source) {
		return false;
	}

	int oldResolution = m_currentSource ? m_currentSource->resolution : -1;
	int oldPage = m_currentSource ? m_currentSource->page : -1;
	Source* oldSource = m_currentSource;

	if(source != m_currentSource) {
		Utils::set_spin_blocked(ui.spinBrightness, source->brightness, m_connection_briSpinChanged);
		Utils::set_spin_blocked(ui.spinContrast, source->contrast, m_connection_conSpinChanged);
		Utils::set_spin_blocked(ui.spinResolution, source->resolution, m_connection_resSpinChanged);
		m_connection_invcheckToggled.block(true);
		ui.checkInvert->set_active(source->invert);
		m_connection_invcheckToggled.block(false);
		m_currentSource = source;
	}

	// Update source struct
	m_currentSource->page = m_pageMap[ui.spinPage->get_value_as_int()].second;
	m_currentSource->brightness = ui.spinBrightness->get_value_as_int();
	m_currentSource->contrast = ui.spinContrast->get_value_as_int();
	m_currentSource->resolution = ui.spinResolution->get_value_as_int();
	m_currentSource->invert = ui.checkInvert->get_active();

	// Notify tools about changes
	if(m_tool) {
		if(m_currentSource != oldSource || m_currentSource->page != oldPage) {
			m_tool->pageChanged();
			m_signalImageChanged.emit();
		}
		if(oldResolution != m_currentSource->resolution) {
			double factor = double(m_currentSource->resolution) / double(oldResolution);
			m_tool->resolutionChanged(factor);
		}
	}

	Utils::set_spin_blocked(ui.spinRotate, m_currentSource->angle[m_currentSource->page - 1], m_connection_rotSpinChanged);

	// Render new image
	DisplayRenderer* renderer = m_sourceRenderers[m_currentSource];
	if(!renderer) {
		return false;
	}
	Cairo::RefPtr<Cairo::ImageSurface> image = renderer->render(m_currentSource->page, m_currentSource->resolution);
	if(!bool(image)) {
		return false;
	}
	renderer->adjustImage(image, m_currentSource->brightness, m_currentSource->contrast, m_currentSource->invert);
	m_image = image;
	m_imageItem->setImage(m_image);
	m_imageItem->setRect(Geometry::Rectangle(-0.5 * m_image->get_width(), -0.5 * m_image->get_height(), m_image->get_width(), m_image->get_height()));
	setAngle(ui.spinRotate->get_value());
	if(m_scale < 1.0) {
		m_scaleTimer = Glib::signal_timeout().connect([this] { scaleImage(); return false; }, 100);
	}
	return true;
}

void Displayer::setZoom(Zoom zoom) {
	if(!m_image) {
		return;
	}
	waitForThread(m_scaleThread, m_scaleThreadCanceled, m_scaleThreadIdleJobsCount, &m_scaleTimer);
	m_connection_zoomfitClicked.block(true);
	m_connection_zoomoneClicked.block(true);

	Gtk::Allocation alloc = ui.viewportDisplay->get_allocation();
	Geometry::Rectangle bb = getSceneBoundingRect();
	double fit = std::min(alloc.get_width() / bb.width, alloc.get_height() / bb.height);

	if(zoom == Zoom::In) {
		m_scale = std::min(10., m_scale * 1.25);
	} else if(zoom == Zoom::Out) {
		m_scale = std::max(0.05, m_scale * 0.8);
	} else if(zoom == Zoom::One) {
		m_scale = 1.0;
	}
	ui.buttonZoomfit->set_active(false);
	if(zoom == Zoom::Fit || (m_scale / fit >= 0.9 && m_scale / fit <= 1.09)) {
		m_scale = fit;
		ui.buttonZoomfit->set_active(true);
	}
	bool scrollVisible = m_scale > fit;
	ui.scrollwinDisplay->get_hscrollbar()->set_visible(scrollVisible);
	ui.scrollwinDisplay->get_vscrollbar()->set_visible(scrollVisible);
	ui.buttonZoomout->set_sensitive(m_scale > 0.05);
	ui.buttonZoomin->set_sensitive(m_scale < 10.);
	ui.buttonZoomnorm->set_active(m_scale == 1.);
	if(m_scale < 1.0) {
		m_scaleTimer = Glib::signal_timeout().connect([this] { scaleImage(); return false; }, 100);
	} else {
		m_imageItem->setImage(m_image);
	}
	positionCanvas();

	m_connection_zoomfitClicked.block(false);
	m_connection_zoomoneClicked.block(false);
}

void Displayer::setRotateMode(RotateMode mode, const std::string& iconName) {
	m_rotateMode = mode;
	ui.imageRotateMode->set(Gdk::Pixbuf::create_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/%1", iconName)));
}

void Displayer::setAngle(double angle) {
	if(m_image) {
		angle = angle < 0 ? angle + 360. : angle >= 360 ? angle - 360 : angle,
		Utils::set_spin_blocked(ui.spinRotate, angle, m_connection_rotSpinChanged);
		int sourcePage = m_pageMap[getCurrentPage()].second;
		double delta = angle - m_currentSource->angle[sourcePage - 1];
		if(m_rotateMode == RotateMode::CurrentPage) {
			m_currentSource->angle[sourcePage - 1] = angle;
		} else if(delta != 0) {
			for(const auto& keyval  : m_pageMap) {
				auto pair = keyval.second;
				double newangle = pair.first->angle[pair.second - 1] + delta;
				newangle = newangle < 0.0 ? newangle + 360.0 : newangle >= 360.0 ? newangle - 360.0 : newangle,
				pair.first->angle[pair.second - 1] = newangle;
			}
		}
		m_imageItem->setRotation(angle * M_PI / 180);
		if(m_tool && delta != 0) {
			m_tool->rotationChanged(delta);
		}
		if(ui.buttonZoomfit->get_active() == true) {
			setZoom(Zoom::Fit);
		} else {
			positionCanvas();
		}
	}
}

bool Displayer::hasMultipleOCRAreas() {
	return m_tool->hasMultipleOCRAreas();
}

std::vector<Cairo::RefPtr<Cairo::ImageSurface>> Displayer::getOCRAreas() {
	return m_tool->getOCRAreas();
}

bool Displayer::allowAutodetectOCRAreas() const {
	return m_tool->allowAutodetectOCRAreas();
}

void Displayer::autodetectOCRAreas() {
	m_tool->autodetectOCRAreas();
}

void Displayer::setDefaultCursor(Glib::RefPtr<Gdk::Cursor> cursor) {
	m_defaultCursor = cursor;
	setCursor(m_defaultCursor);
}

void Displayer::setCursor(Glib::RefPtr<Gdk::Cursor> cursor) {
	if(cursor) {
		if(m_imageItem && ui.viewportDisplay->get_window()) {
			ui.viewportDisplay->get_window()->set_cursor(cursor);
		}
	} else {
		if(m_imageItem && ui.viewportDisplay->get_window()) {
			ui.viewportDisplay->get_window()->set_cursor(m_defaultCursor);
		}
	}
}

void Displayer::resizeEvent() {
	if(ui.buttonZoomfit->get_active() == true) {
		setZoom(Zoom::Fit);
	}
}

bool Displayer::mousePressEvent(GdkEventButton* ev) {
	ui.viewportDisplay->grab_focus();
	if(ev->button == 2) {
		m_panPos[0] = ev->x_root;
		m_panPos[1] = ev->y_root;
		return true;
	}
	Geometry::Point scenePos = mapToSceneClamped(Geometry::Point(ev->x, ev->y));
	for(DisplayerItem* item : Utils::reverse(m_items)) {
		if(item->rect().contains(scenePos)) {
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

bool Displayer::keyPressEvent(GdkEventKey* ev) {
	if(ev->keyval == GDK_KEY_Page_Up) {
		ui.spinPage->set_value(ui.spinPage->get_value_as_int() - 1);
		return true;
	} else if(ev->keyval == GDK_KEY_Page_Down) {
		ui.spinPage->set_value(ui.spinPage->get_value_as_int() + 1);
		return true;
	} else {
		return false;
	}
}

bool Displayer::mouseMoveEvent(GdkEventMotion* ev) {
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
	for(DisplayerItem* item : Utils::reverse(m_items)) {
		if(item->rect().contains(scenePos)) {
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

bool Displayer::mouseReleaseEvent(GdkEventButton* ev) {
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

bool Displayer::scrollEvent(GdkEventScroll* ev) {
	if((ev->state & Gdk::CONTROL_MASK) != 0) {
		if((ev->direction == GDK_SCROLL_UP || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y < 0)) && m_scale * 1.25 < 10) {
			Gtk::Allocation alloc = ui.drawingareaDisplay->get_allocation();
			m_scrollPos[0] = std::max(0., std::min((ev->x + m_hadj->get_value() - alloc.get_x()) / alloc.get_width(), 1.0));
			m_scrollPos[1] = std::max(0., std::min((ev->y + m_vadj->get_value() - alloc.get_y()) / alloc.get_height(), 1.0));
			setZoom(Zoom::In);
		} else if((ev->direction == GDK_SCROLL_DOWN || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y > 0)) && m_scale * 0.8 > 0.05) {
			setZoom(Zoom::Out);
		}
		return true;
	} else if((ev->state & Gdk::SHIFT_MASK) != 0) {
		if(ev->direction == GDK_SCROLL_UP || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y < 0)) {
			m_hadj->set_value(m_hadj->get_value() - m_hadj->get_step_increment());
		} else if(ev->direction == GDK_SCROLL_DOWN || (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y > 0)) {
			m_hadj->set_value(m_hadj->get_value() + m_hadj->get_step_increment());
		}
		return true;
	}
	return false;
}

std::pair<int, int> Displayer::getPointVisible(const Geometry::Point& p) const {
	// 0 means visible, -1 means to the left/top of visible area, +1 means to the right/bottom of visible area
	int vx = 0;
	int vy = 0;
	double margin = 30.; // 30px margin
	if(p.x - margin < m_hadj->get_value()) {
		vx = -1;
	} else if(p.x + 30. > m_hadj->get_value() + m_hadj->get_page_size()) {
		vx = +1;
	}
	if(p.y - margin < m_vadj->get_value()) {
		vy = -1;
	} else if(p.y + margin > m_vadj->get_value() + m_vadj->get_page_size()) {
		vy = +1;
	}
	return std::make_pair(vx, vy);
}

void Displayer::ensureVisible(double evx, double evy) {
	double margin = 30.; // 30px margin
	if(evx - margin < m_hadj->get_value()) {
		m_hadj->set_value(std::max(0., evx - margin));
	} else if(evx + 30. > m_hadj->get_value() + m_hadj->get_page_size()) {
		m_hadj->set_value(std::min(m_hadj->get_upper(), evx + 30.) - m_hadj->get_page_size());
	}
	if(evy - margin < m_vadj->get_value()) {
		m_vadj->set_value(std::max(0., evy - margin));
	} else if(evy + margin > m_vadj->get_value() + m_vadj->get_page_size()) {
		m_vadj->set_value(std::min(m_vadj->get_upper(), evy + margin) - m_vadj->get_page_size());
	}
}

void Displayer::ensureVisible(const Geometry::Rectangle& rect) {
	Geometry::Point p1 = mapToView(Geometry::Point(rect.x, rect.y));
	Geometry::Point p2 = mapToView(Geometry::Point(rect.x + rect.width, rect.y + rect.height));
	std::pair<int, int> vis1 = getPointVisible(p1);
	std::pair<int, int> vis2 = getPointVisible(p2);
	if(vis1.first == 0 && vis1.second == 0 && vis2.first == 0 && vis2.second == 0) {
		// Both points visible, do nothing
		return;
	}

	double ox = m_hadj->get_value() + 0.5 * (m_hadj->get_page_size() - (p2.x - p1.x)) - p1.x;
	double oy = m_vadj->get_value() + 0.5 * (m_vadj->get_page_size() - (p2.y - p1.y)) - p1.y;
	m_hadj->set_value(std::min(std::max(0., m_hadj->get_value() - ox), m_hadj->get_upper() - m_hadj->get_page_size()));
	m_vadj->set_value(std::min(std::max(0., m_vadj->get_value() - oy), m_vadj->get_upper() - m_vadj->get_page_size()));
}

void Displayer::addItem(DisplayerItem* item) {
	if(!m_items.empty()) {
		item->setZIndex(m_items.back()->zIndex() + 1);
	}
	m_items.push_back(item);
	item->m_displayer = this;
	invalidateRect(item->rect());
}

void Displayer::removeItem(DisplayerItem* item) {
	if(item == m_activeItem) {
		m_activeItem = nullptr;
	}
	m_items.erase(std::remove(m_items.begin(), m_items.end(), item), m_items.end());
	item->m_displayer = nullptr;
	invalidateRect(item->rect());
}

void Displayer::invalidateRect(const Geometry::Rectangle& rect) {
	Gtk::Allocation alloc = ui.drawingareaDisplay->get_allocation();
	Geometry::Rectangle canvasRect = rect;
	canvasRect.x = (canvasRect.x * m_scale + 0.5 * alloc.get_width()) - 2;
	canvasRect.y = (canvasRect.y * m_scale + 0.5 * alloc.get_height()) - 2;
	canvasRect.width = canvasRect.width * m_scale + 4;
	canvasRect.height = canvasRect.height * m_scale + 4;
	ui.drawingareaDisplay->queue_draw_area(canvasRect.x, canvasRect.y, canvasRect.width, canvasRect.height);
}

void Displayer::resortItems() {
	std::sort(m_items.begin(), m_items.end(), DisplayerItem::zIndexCmp);
}

Geometry::Rectangle Displayer::getSceneBoundingRect() const {
	if(!m_image) {
		return Geometry::Rectangle();
	}
	int w = m_image->get_width();
	int h = m_image->get_height();
	Geometry::Rotation R(ui.spinRotate->get_value() / 180. * M_PI);
	int width = std::abs(R(0, 0) * w) + std::abs(R(0, 1) * h);
	int height = std::abs(R(1, 0) * w) + std::abs(R(1, 1) * h);
	return Geometry::Rectangle(-0.5 * width, -0.5 * height, width, height);
}

Geometry::Point Displayer::mapToSceneClamped(const Geometry::Point& p) const {
	// Selection coordinates are with respect to the center of the image in unscaled (but rotated) coordinates
	Gtk::Allocation alloc = ui.drawingareaDisplay->get_allocation();
	double x = (std::max(0., std::min(p.x - alloc.get_x(), double(alloc.get_width()))) - 0.5 * alloc.get_width()) / m_scale;
	double y = (std::max(0., std::min(p.y - alloc.get_y(), double(alloc.get_height()))) - 0.5 * alloc.get_height()) / m_scale;
	return Geometry::Point(x, y);
}

Geometry::Point Displayer::mapToView(const Geometry::Point& p) const {
	Gtk::Allocation alloc = ui.drawingareaDisplay->get_allocation();
	return Geometry::Point(alloc.get_x() + 0.5 * alloc.get_width() + p.x * m_scale,
	                       alloc.get_y() + 0.5 * alloc.get_height() + p.y * m_scale);
}

Cairo::RefPtr<Cairo::ImageSurface> Displayer::getImage(const Geometry::Rectangle& rect) const {
	Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, std::ceil(rect.width), std::ceil(rect.height));
	Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surf);
	ctx->set_source_rgba(1., 1., 1., 1.);
	ctx->paint();
	ctx->translate(-rect.x, -rect.y);
	ctx->rotate(ui.spinRotate->get_value() / 180. * M_PI);
	ctx->translate(-0.5 * m_image->get_width(), -0.5 * m_image->get_height());
	ctx->set_source(m_image, 0, 0);
	ctx->paint();
	return surf;
}

void Displayer::setBlockAutoscale(bool block) {
	m_autoScaleBlocked = block;
	if(!block) {
		m_scaleTimer = Glib::signal_timeout().connect([this] { scaleImage(); return false; }, 100);
	}
}

void Displayer::scaleImage() {
	if(m_autoScaleBlocked) {
		return;
	}
	int page = m_currentSource->page;
	double resolution = m_scale * m_currentSource->resolution;
	int brightness = m_currentSource->brightness;
	int contrast = m_currentSource->contrast;
	bool invert = m_currentSource->invert;
	m_scaleThread = new std::thread([ = ] {
		DisplayRenderer* renderer = m_sourceRenderers[m_currentSource];
		if(!renderer) {
			return;
		}
		Cairo::RefPtr<Cairo::ImageSurface> image = renderer->render(page, resolution);
		if(!bool(image) || m_scaleThreadCanceled) {
			return;
		}
		renderer->adjustImage(image, brightness, contrast, invert);
		if(m_scaleThreadCanceled) {
			return;
		}
		m_scaleThreadIdleJobsCount += 1;
		Glib::signal_idle().connect_once([ = ] {
			m_imageItem->setImage(image);
			ui.drawingareaDisplay->queue_draw();
			m_scaleThreadIdleJobsCount -= 1;
		});
	});
}


void Displayer::thumbnailsToggled() {
	bool active = ui.checkBoxThumbnails->get_active();
	ui.scrollwinThumbnails->set_visible(active);
	ui.panedSource->set_position(active ? 0.5 * ui.panedSource->get_height() : (ui.panedSource->get_height() - ui.checkBoxThumbnails->get_height()));
	if(active) {
		generateThumbnails();
	} else {
		waitForThread(m_thumbThread, m_thumbThreadCanceled, m_thumbThreadIdleJobsCount);
		ui.iconviewThumbnails->set_model(Gtk::ListStore::create(m_thumbListViewCols));
		ui.iconviewThumbnails->set_pixbuf_column(0);
		ui.iconviewThumbnails->set_text_column(1);
	}
}

void Displayer::generateThumbnails() {
	if(ui.checkBoxThumbnails->get_active()) {
		Glib::RefPtr<Gtk::ListStore> thumbnailStore = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.iconviewThumbnails->get_model());
		for(const auto& pair : m_pageMap) {
			Gtk::TreeIter it = thumbnailStore->append();
			it->set_value(m_thumbListViewCols.pixbuf, Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/thumbnail.png"));
			it->set_value(m_thumbListViewCols.label, Glib::ustring::compose(_("Page %1"), pair.first));
		}
		m_thumbThread = new std::thread(&Displayer::thumbnailThread, this);
	}
}

void Displayer::thumbnailThread() {
	for(const auto& pair : m_pageMap) {
		DisplayRenderer* renderer = m_sourceRenderers[pair.second.first];
		if(!renderer) {
			continue;
		}
		Cairo::RefPtr<Cairo::ImageSurface> thumb = renderer->renderThumbnail(pair.second.second);
		Glib::RefPtr<Gdk::Pixbuf> pixbuf = thumb ? Gdk::Pixbuf::create(thumb, 0, 0, thumb->get_width(), thumb->get_height()) : Glib::RefPtr<Gdk::Pixbuf>(0);
		m_thumbThreadIdleJobsCount += 1;
		Glib::signal_idle().connect_once([ = ] {
			Glib::RefPtr<Gtk::ListStore> thumbnailStore = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.iconviewThumbnails->get_model());
			Gtk::TreePath path(1u, pair.first - 1);
			thumbnailStore->get_iter(path)->set_value(m_thumbListViewCols.pixbuf, pixbuf);
			m_thumbThreadIdleJobsCount -= 1;
		});
		if(m_thumbThreadCanceled) {
			break;
		}
	}
}

void Displayer::waitForThread(std::thread*& thread, std::atomic<bool>& cancelFlag, std::atomic<int>& idleJobCount, sigc::connection* conn) {
	if(conn) {
		conn->disconnect();
	}
	if(thread) {
		cancelFlag = true;
		thread->join();
		delete thread;
		thread = nullptr;
		cancelFlag = false;
	}
	while(idleJobCount > 0) {
		Gtk::Main::iteration();
	}
}

///////////////////////////////////////////////////////////////////////////////

void DisplayerItem::setZIndex(int zIndex) {
	m_zIndex = zIndex;
	if(m_displayer) {
		m_displayer->invalidateRect(m_rect);
		m_displayer->resortItems();
	}
}

void DisplayerItem::setRect(const Geometry::Rectangle& rect) {
	Geometry::Rectangle invalidateArea = m_rect.unite(rect);
	m_rect = rect;
	if(m_displayer) {
		m_displayer->invalidateRect(invalidateArea);
	}
}

void DisplayerItem::setVisible(bool visible) {
	m_visible = visible;
	update();
}

void DisplayerItem::update() {
	if(m_displayer) {
		m_displayer->invalidateRect(m_rect);
	}
}

void DisplayerImageItem::draw(Cairo::RefPtr<Cairo::Context> ctx) const {
	if(m_image) {
		double sx = rect().width / m_image->get_width();
		double sy = rect().height / m_image->get_height();
		ctx->save();
		ctx->rotate(m_rotation);
		ctx->scale(sx, sy);
		ctx->translate(rect().x / sx, rect().y / sy);
		ctx->set_source(m_image, 0, 0);
		ctx->paint();
		ctx->restore();
	}
}


void DisplayerSelection::draw(Cairo::RefPtr<Cairo::Context> ctx) const {
	Gdk::RGBA bgcolor("#4A90D9");

	double scale = displayer()->getCurrentScale();

	double d = 0.5 / scale;
	double x1 = Utils::round(rect().x * scale) / scale + d;
	double y1 = Utils::round(rect().y * scale) / scale + d;
	double x2 = Utils::round((rect().x + rect().width) * scale) / scale - d;
	double y2 = Utils::round((rect().y + rect().height) * scale) / scale - d;
	Geometry::Rectangle paintrect(x1, y1, x2 - x1, y2 - y1);
	ctx->save();
	// Semitransparent rectangle with frame
	ctx->set_line_width(2. * d);
	ctx->rectangle(paintrect.x, paintrect.y, paintrect.width, paintrect.height);
	ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 0.25);
	ctx->fill_preserve();
	ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 1.0);
	ctx->stroke();
	ctx->restore();
}

bool DisplayerSelection::mousePressEvent(GdkEventButton* event) {
	if(event->button == 1) {
		Geometry::Point p = displayer()->mapToSceneClamped(Geometry::Point(event->x, event->y));
		double tol = 10.0 / displayer()->getCurrentScale();
		m_resizeOffset = Geometry::Point(0., 0.);
		if(std::abs(m_point.x - p.x) < tol) { // pointx
			m_resizeHandlers.push_back(resizePointX);
			m_resizeOffset.x = p.x - m_point.x;
		} else if(std::abs(m_anchor.x - p.x) < tol) { // anchorx
			m_resizeHandlers.push_back(resizeAnchorX);
			m_resizeOffset.x = p.x - m_anchor.x;
		}
		if(std::abs(m_point.y - p.y) < tol) { // pointy
			m_resizeHandlers.push_back(resizePointY);
			m_resizeOffset.y = p.y - m_point.y;
		} else if(std::abs(m_anchor.y - p.y) < tol) { // anchory
			m_resizeHandlers.push_back(resizeAnchorY);
			m_resizeOffset.y = p.y - m_anchor.y;
		}
		return !m_resizeHandlers.empty();
	} else if(event->button == 3) {
		showContextMenu(event);
	}
	return false;
}

bool DisplayerSelection::mouseReleaseEvent(GdkEventButton* /*event*/) {
	if(!m_resizeHandlers.empty()) {
		m_signalGeometryChanged.emit(rect());
		m_resizeHandlers.clear();
	}
	return false;
}

bool DisplayerSelection::mouseMoveEvent(GdkEventMotion* event) {
	Geometry::Point p = displayer()->mapToSceneClamped(Geometry::Point(event->x, event->y));
	if(m_resizeHandlers.empty()) {
		double tol = 10.0 / displayer()->getCurrentScale();

		bool left = std::abs(rect().x - p.x) < tol;
		bool right = std::abs(rect().x + rect().width - p.x) < tol;
		bool top = std::abs(rect().y - p.y) < tol;
		bool bottom = std::abs(rect().y + rect().height - p.y) < tol;

		if((top && left) || (bottom && right)) {
			displayer()->setCursor(Gdk::Cursor::create(MAIN->getWindow()->get_display(), "nwse-resize"));
		} else if((top && right) || (bottom && left)) {
			displayer()->setCursor(Gdk::Cursor::create(MAIN->getWindow()->get_display(), "nesw-resize"));
		} else if(top || bottom) {
			displayer()->setCursor(Gdk::Cursor::create(MAIN->getWindow()->get_display(), "ns-resize"));
		} else if(left || right) {
			displayer()->setCursor(Gdk::Cursor::create(MAIN->getWindow()->get_display(), "ew-resize"));
		} else {
			displayer()->setCursor(Glib::RefPtr<Gdk::Cursor>(0));
		}
	}
	Geometry::Point movePos(p.x - m_resizeOffset.x, p.y - m_resizeOffset.y);
	Geometry::Rectangle bb = displayer()->getSceneBoundingRect();
	movePos.x = std::min(std::max(bb.x, movePos.x), bb.x + bb.width);
	movePos.y = std::min(std::max(bb.y, movePos.y), bb.y + bb.height);
	if(!m_resizeHandlers.empty()) {
		for(const ResizeHandler& handler : m_resizeHandlers) {
			handler(movePos, m_anchor, m_point);
		}
		Geometry::Rectangle newRect = Geometry::Rectangle(m_anchor, m_point).unite(m_minRect);
		setRect(newRect);
		displayer()->ensureVisible(event->x, event->y);
		return true;
	}
	return false;
}


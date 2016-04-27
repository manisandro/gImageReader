/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayerToolSelect.cc
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

#include "DisplayerToolSelect.hh"
#include "Displayer.hh"
#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"
#include "Utils.hh"

#include <cmath>
#include <tesseract/baseapi.h>


DisplayerToolSelect::DisplayerToolSelect(Displayer *displayer)
	: DisplayerTool(displayer)
{
	Gtk::Button* autolayoutButton = MAIN->getWidget("button:main.autolayout");
	m_connectionAutolayout = CONNECT(autolayoutButton, clicked, [this]{ autodetectLayout(); });

	MAIN->getConfig()->addSetting(new VarSetting<Glib::ustring>("selectionsavefile"));
	std::string selectionsavefile = MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("selectionsavefile")->getValue();
	if(selectionsavefile.empty()) {
		selectionsavefile = Glib::build_filename(Utils::get_documents_dir(), _("selection.png"));
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("selectionsavefile")->setValue(selectionsavefile);
	}

	autolayoutButton->set_visible(true);
	updateRecognitionModeLabel();
}

DisplayerToolSelect::~DisplayerToolSelect()
{
	clearSelections();
	MAIN->getWidget("button:main.autolayout").as<Gtk::Button>()->set_visible(false);
	m_connectionAutolayout.disconnect();
}

bool DisplayerToolSelect::mousePressEvent(GdkEventButton* event)
{
	if(event->button == 1 &&  m_curSel == nullptr){
		if((event->state & Gdk::CONTROL_MASK) == 0){
			clearSelections();
		}
		m_curSel = new DisplaySelection(this, 1 + m_selections.size(), mapToSceneClamped(Geometry::Point(event->x, event->y)));
		m_curSel->setZIndex(1 + m_selections.size());
		reorderCanvasItems();
		addItemToCanvas(m_curSel);
		return true;
	}
	return false;
}

bool DisplayerToolSelect::mouseMoveEvent(GdkEventMotion* event)
{
	if(m_curSel){
		Geometry::Point p = mapToSceneClamped(Geometry::Point(event->x, event->y));
		Geometry::Rectangle oldRect = m_curSel->rect();
		m_curSel->setPoint(p);
		m_displayer->ensureVisible(event->x, event->y);
		invalidateRect(oldRect.unite(m_curSel->rect()));
		return true;
	}
	return false;
}

bool DisplayerToolSelect::mouseReleaseEvent(GdkEventButton* /*event*/)
{
	if(m_curSel){
		if(m_curSel->rect().width < 5. || m_curSel->rect().height < 5.){
			removeItemFromCanvas(m_curSel);
			delete m_curSel;
		}else{
			m_selections.push_back(m_curSel);
			updateRecognitionModeLabel();
		}
		m_curSel = nullptr;
		return true;
	}
	return false;
}

void DisplayerToolSelect::pageChanged()
{
	clearSelections();
}

void DisplayerToolSelect::resolutionChanged(double factor)
{
	for(DisplaySelection* sel : m_selections){
		sel->scale(factor);
	}
}

void DisplayerToolSelect::rotationChanged(double delta)
{
	Geometry::Rotation R(delta);
	for(DisplaySelection* sel : m_selections){
		sel->rotate(R);
	}
}

std::vector<Cairo::RefPtr<Cairo::ImageSurface>> DisplayerToolSelect::getOCRAreas()
{
	std::vector<Cairo::RefPtr<Cairo::ImageSurface>> images;
	if(m_selections.empty()){
		images.push_back(getImage(getSceneBoundingRect()));
	}else{
		for(const DisplaySelection* sel : m_selections){
			images.push_back(getImage(sel->rect()));
		}
	}
	return images;
}

void DisplayerToolSelect::clearSelections()
{
	for(DisplaySelection* sel : m_selections) {
		removeItemFromCanvas(sel);
		delete sel;
	}
	m_selections.clear();
	updateRecognitionModeLabel();
}

void DisplayerToolSelect::removeSelection(int num)
{
	removeItemFromCanvas(m_selections[num - 1]);
	delete m_selections[num - 1];
	m_selections.erase(m_selections.begin() + num - 1);
	for(int i = 0, n = m_selections.size(); i < n; ++i){
		m_selections[i]->setNumber(1 + i);
		m_selections[i]->setZIndex(1 + i);
		reorderCanvasItems();
		invalidateRect(m_selections[i]->rect());
	}
}

void DisplayerToolSelect::reorderSelection(int oldNum, int newNum)
{
	DisplaySelection* sel = m_selections[oldNum - 1];
	m_selections.erase(m_selections.begin() + oldNum - 1);
	m_selections.insert(m_selections.begin() + newNum - 1, sel);
	for(int i = 0, n = m_selections.size(); i < n; ++i){
		m_selections[i]->setNumber(1 + i);
		m_selections[i]->setZIndex(1 + i);
		reorderCanvasItems();
		invalidateRect(m_selections[i]->rect());
	}
}

void DisplayerToolSelect::saveSelection(DisplaySelection* selection)
{
	Cairo::RefPtr<Cairo::ImageSurface> img = getImage(selection->rect());
	std::string filename = Utils::make_output_filename(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("selectionsavefile")->getValue());
	FileDialogs::FileFilter filter = {_("PNG Images"), {"image/png"}, {"*.png"}};
	filename = FileDialogs::save_dialog(_("Save Selection Image"), filename, filter);
	if(!filename.empty()){
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("selectionsavefile")->setValue(filename);
		img->write_to_png(filename);
	}
}

void DisplayerToolSelect::updateRecognitionModeLabel()
{
	MAIN->getRecognizer()->setRecognizeMode(m_selections.empty() ? _("Recognize all") : _("Recognize selection"));
}

void DisplayerToolSelect::autodetectLayout(bool noDeskew)
{
	clearSelections();

	double avgDeskew = 0.;
	int nDeskew = 0;
	std::vector<Geometry::Rectangle> rects;
	Cairo::RefPtr<Cairo::ImageSurface> img = getImage(getSceneBoundingRect());

	// Perform layout analysis
	Utils::busyTask([this,&nDeskew,&avgDeskew,&rects,&img]{
		tesseract::TessBaseAPI tess;
		tess.InitForAnalysePage();
		tess.SetPageSegMode(tesseract::PSM_AUTO_ONLY);
		tess.SetImage(img->get_data(), img->get_width(), img->get_height(), 4, 4 * img->get_width());
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
				float width = x2 - x1, height = y2 - y1;
				if(width > 10 && height > 10){
					rects.push_back(Geometry::Rectangle(x1 - 0.5 * img->get_width(), y1 - 0.5 * img->get_height(), width, height));
				}
			}while(it->Next(tesseract::RIL_BLOCK));
		}
		delete it;
		return true;
	}, _("Performing layout analysis"));

	// If a somewhat large deskew angle is detected, automatically rotate image and redetect layout,
	// unless we already attempted to rotate (to prevent endless loops)
	avgDeskew = Utils::round(((avgDeskew/nDeskew)/M_PI * 180.) * 10.) / 10.;
	if(std::abs(avgDeskew > .1) && !noDeskew){
		m_displayer->setAngle(m_displayer->getCurrentAngle() - avgDeskew);
		autodetectLayout(true);
	}else{
		// Merge overlapping rectangles
		for(int i = rects.size(); i-- > 1;) {
			for(int j = i; j-- > 0;) {
				if(rects[j].overlaps(rects[i])) {
					rects[j] = rects[j].unite(rects[i]);
					rects.erase(rects.begin() + i);
					break;
				}
			}
		}
		for(int i = 0, n = rects.size(); i < n; ++i){
			m_selections.push_back(new DisplaySelection(this, 1 + i, Geometry::Point(rects[i].x, rects[i].y)));
			m_selections.back()->setPoint(Geometry::Point(rects[i].x + rects[i].width, rects[i].y + rects[i].height));
			addItemToCanvas(m_selections.back());
		}
		updateRecognitionModeLabel();
	}
}

///////////////////////////////////////////////////////////////////////////////

void DisplaySelection::showContextMenu(GdkEventButton* event){
	Gtk::Window* selmenu = MAIN->getWidget("window:selectionmenu");
	Gtk::SpinButton* spin = MAIN->getWidget("spin:selectionmenu.order");
	spin->get_adjustment()->set_upper(m_selectTool->m_selections.size());
	spin->get_adjustment()->set_value(m_number);
	Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
	std::vector<sigc::connection> selmenuConnections = {
		CONNECT(spin, value_changed, [&]{ reorderSelection(spin->get_value()); }),
		CONNECT(MAIN->getWidget("button:selectionmenu.delete").as<Gtk::Button>(), clicked, [&]{
			loop->quit();
			m_selectTool->removeSelection(m_number);
		}),
		CONNECT(MAIN->getWidget("button:selectionmenu.recognize").as<Gtk::Button>(), clicked, [&]{
			loop->quit();
			MAIN->getRecognizer()->recognizeImage(m_selectTool->getImage(rect()), Recognizer::OutputDestination::Buffer);
		}),
		CONNECT(MAIN->getWidget("button:selectionmenu.clipboard").as<Gtk::Button>(), clicked, [&]{
			loop->quit();
			MAIN->getRecognizer()->recognizeImage(m_selectTool->getImage(rect()), Recognizer::OutputDestination::Clipboard);
		}),
		CONNECT(MAIN->getWidget("button:selectionmenu.save").as<Gtk::Button>(), clicked, [&]{
			loop->quit();
			selmenu->hide(); // Explicitly hide here to avoid conflicts with file dialog which pops up
			m_selectTool->saveSelection(this);
		}),
		CONNECT(selmenu, button_press_event, [&](GdkEventButton* ev){
			Gtk::Allocation a = selmenu->get_allocation();
			if(ev->x < a.get_x() || ev->x > a.get_x() + a.get_width() || ev->y < a.get_y() || ev->y > a.get_y() + a.get_height()){
				loop->quit();
			}
			return true; }),
		CONNECT(selmenu, key_press_event, [&](GdkEventKey* ev){
			if(ev->keyval == GDK_KEY_Escape) loop->quit(); return true;
		})
	};
	Glib::RefPtr<const Gdk::Screen> screen = MAIN->getWindow()->get_screen();
	Gdk::Rectangle rect = screen->get_monitor_workarea(screen->get_monitor_at_point(event->x_root, event->y_root));
	selmenu->show_all();
	int w, h, trash;
	selmenu->get_preferred_width(trash, w);
	selmenu->get_preferred_height(trash, h);
	int x = std::min(std::max(int(event->x_root), rect.get_x()), rect.get_x() + rect.get_width() - w);
	int y = std::min(std::max(int(event->y_root), rect.get_y()), rect.get_y() + rect.get_height() - h);
	selmenu->move(x, y);
	GdkWindow* gdkwin = selmenu->get_window()->gobj();
#if GTK_CHECK_VERSION(3,20,0)
	gdk_seat_grab(gdk_device_get_seat(gtk_get_current_event_device()), gdkwin, GDK_SEAT_CAPABILITY_ALL, true, nullptr, nullptr, nullptr, nullptr);
#else
	gdk_device_grab(gtk_get_current_event_device(), gdkwin, GDK_OWNERSHIP_APPLICATION, true, GDK_BUTTON_PRESS_MASK, nullptr, event->time);
#endif

	loop->run();

	selmenu->hide();
	for(sigc::connection& conn : selmenuConnections){
		conn.disconnect();
	}
#if GTK_CHECK_VERSION(3,20,0)
	gdk_seat_ungrab(gdk_device_get_seat(gtk_get_current_event_device()));
#else
	gdk_device_ungrab(gtk_get_current_event_device(), gtk_get_current_event_time());
#endif
}

void DisplaySelection::reorderSelection(int newNumber)
{
	m_selectTool->reorderSelection(m_number, newNumber);
}

void DisplaySelection::draw(Cairo::RefPtr<Cairo::Context> ctx) const
{
	Gdk::RGBA fgcolor("#FFFFFF");
	Gdk::RGBA bgcolor("#4A90D9");

	double scale = m_selectTool->getDisplayScale();
	Glib::ustring idx = Glib::ustring::compose("%1", m_number);

	double d = 0.5 / scale;
	double x1 = Utils::round(m_rect.x * scale) / scale + d;
	double y1 = Utils::round(m_rect.y * scale) / scale + d;
	double x2 = Utils::round((m_rect.x + m_rect.width) * scale) / scale - d;
	double y2 = Utils::round((m_rect.y + m_rect.height) * scale) / scale - d;
	Geometry::Rectangle rect(x1, y1, x2 - x1, y2 - y1);
	ctx->save();
	// Semitransparent rectangle with frame
	ctx->set_line_width(2. * d);
	ctx->rectangle(rect.x, rect.y, rect.width, rect.height);
	ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 0.25);
	ctx->fill_preserve();
	ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 1.0);
	ctx->stroke();
	// Text box
	double w = std::min(std::min(40. * d, rect.width), rect.height);
	ctx->rectangle(rect.x, rect.y, w - d, w - d);
	ctx->set_source_rgba(bgcolor.get_red(), bgcolor.get_green(), bgcolor.get_blue(), 1.0);
	ctx->fill();
	// Text
	ctx->select_font_face("sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
	ctx->set_font_size(0.7 * w);
	Cairo::TextExtents ext; ctx->get_text_extents(idx, ext);
	ctx->translate(rect.x + .5 * w, rect.y + .5 * w);
	ctx->translate(-ext.x_bearing - .5 * ext.width, -ext.y_bearing - .5 * ext.height);
	ctx->set_source_rgba(fgcolor.get_red(), fgcolor.get_green(), bgcolor.get_blue(), 1.0);
	ctx->show_text(idx);
	ctx->restore();
}

bool DisplaySelection::mousePressEvent(GdkEventButton *event)
{
	if(event->button == 1) {
		Geometry::Point p = m_selectTool->mapToSceneClamped(Geometry::Point(event->x, event->y));
		double tol = 10.0 / m_selectTool->getDisplayScale();
		m_resizeOffset = Geometry::Point(0., 0.);
		if(std::abs(m_point.x - p.x) < tol){ // pointx
			m_resizeHandlers.push_back(resizePointX);
			m_resizeOffset.x = p.x - m_point.x;
		}else if(std::abs(m_anchor.x - p.x) < tol){ // anchorx
			m_resizeHandlers.push_back(resizeAnchorX);
			m_resizeOffset.x = p.x - m_anchor.x;
		}
		if(std::abs(m_point.y - p.y) < tol){ // pointy
			m_resizeHandlers.push_back(resizePointY);
			m_resizeOffset.y = p.y - m_point.y;
		}else if(std::abs(m_anchor.y - p.y) < tol){ // anchory
			m_resizeHandlers.push_back(resizeAnchorY);
			m_resizeOffset.y = p.y - m_anchor.y;
		}
		return true;
	} else if(event->button == 3) {
		showContextMenu(event);
	}
	return false;
}

bool DisplaySelection::mouseReleaseEvent(GdkEventButton */*event*/)
{
	m_resizeHandlers.clear();
	return false;
}

bool DisplaySelection::mouseMoveEvent(GdkEventMotion *event)
{
	Geometry::Point p = m_selectTool->mapToSceneClamped(Geometry::Point(event->x, event->y));
	if(m_resizeHandlers.empty()) {
		double tol = 10.0 / m_selectTool->getDisplayScale();

		bool left = std::abs(m_rect.x - p.x) < tol;
		bool right = std::abs(m_rect.x + m_rect.width - p.x) < tol;
		bool top = std::abs(m_rect.y - p.y) < tol;
		bool bottom = std::abs(m_rect.y + m_rect.height - p.y) < tol;

		if((top && left) || (bottom && right)){
			m_selectTool->m_displayer->setCursor(Gdk::Cursor::create(MAIN->getWindow()->get_display(), "nwse-resize"));
		}else if((top && right) || (bottom && left)){
			m_selectTool->m_displayer->setCursor(Gdk::Cursor::create(MAIN->getWindow()->get_display(), "nesw-resize"));
		}else if(top || bottom){
			m_selectTool->m_displayer->setCursor(Gdk::Cursor::create(MAIN->getWindow()->get_display(), "ns-resize"));
		}else if(left || right){
			m_selectTool->m_displayer->setCursor(Gdk::Cursor::create(MAIN->getWindow()->get_display(), "ew-resize"));
		}else{
			m_selectTool->m_displayer->setCursor(Glib::RefPtr<Gdk::Cursor>(0));
		}
	}
	Geometry::Point movePos(p.x - m_resizeOffset.x, p.y - m_resizeOffset.y);
	Geometry::Rectangle bb = m_selectTool->getSceneBoundingRect();
	movePos.x = std::min(std::max(bb.x, movePos.x), bb.x + bb.width);
	movePos.y = std::min(std::max(bb.y, movePos.y), bb.y + bb.height);
	if(!m_resizeHandlers.empty()){
		Geometry::Rectangle oldRect = m_rect;
		for(const ResizeHandler& handler : m_resizeHandlers){
			handler(movePos, m_anchor, m_point);
		}
		m_rect = Geometry::Rectangle(m_anchor, m_point);
		m_selectTool->invalidateRect(m_rect.unite(oldRect));
		m_selectTool->m_displayer->ensureVisible(event->x, event->y);
		return true;
	}
	return false;
}

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRProofReadWidget.cc
 * Copyright (C) 2022 Sandro Mani <manisandro@gmail.com>
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

#include "HOCRProofReadWidget.hh"

#include "Displayer.hh"
#include "ConfigSettings.hh"
#include "HOCRDocument.hh"
#include "HOCRProofReadWidget.hh"
#include "HOCRSpellChecker.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include <iostream>

class HOCRProofReadWidget::LineEdit : public Gtk::Entry {
public:
	LineEdit(HOCRProofReadWidget* proofReadWidget, HOCRItem* wordItem) :
		m_proofReadWidget(proofReadWidget), m_wordItem(wordItem) {
		set_text(wordItem->text());
		CONNECT(this, changed, [this] {onTextChanged(); });

		Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(m_proofReadWidget->documentTree()->get_model());
		CONNECT(document, row_changed, [this](const Gtk::TreePath & path, const Gtk::TreeIter & iter) { onModelDataChanged(path, iter); });
		CONNECT(document, item_attribute_changed, [this](const Gtk::TreeIter & iter, const Glib::ustring & name, const Glib::ustring & value) { onAttributeChanged(iter, name, value); });
		CONNECT(this, key_press_event, [this](GdkEventKey * ev) { return key_press_event(ev); }, false);
		CONNECT(this, focus_in_event, [this](GdkEventFocus * ev) { return focus_in_event(ev); }, false);

		Gtk::TreeIter index = document->indexAtItem(m_wordItem);
		if(document->indexIsMisspelledWord(index)) {
			Utils::set_error_state(this);
		}

		Pango::FontDescription font;
		font.set_weight(m_wordItem->fontBold() ? Pango::WEIGHT_BOLD : Pango::WEIGHT_NORMAL);
		font.set_style(m_wordItem->fontItalic() ? Pango::STYLE_ITALIC : Pango::STYLE_NORMAL);
		override_font(font);

		set_width_chars(1);
		get_style_context()->add_provider(entryStyleProvider(), 1000);
		get_style_context()->add_class("nopadding");
	}
	const HOCRItem* item() const { return m_wordItem; }

private:
	ClassData m_classdata;

	HOCRProofReadWidget* m_proofReadWidget = nullptr;
	HOCRItem* m_wordItem = nullptr;
	bool m_blockSetText = false;

	static Glib::RefPtr<Gtk::CssProvider> entryStyleProvider() {
		static Glib::RefPtr<Gtk::CssProvider> provider = createEntryStyleProvider();
		return provider;
	}
	static Glib::RefPtr<Gtk::CssProvider> createEntryStyleProvider() {
		Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
		provider->load_from_data(".nopadding { padding: 1px; border: 1px solid #ddd; }");
		return provider;
	}

	void onTextChanged() {
		Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(m_proofReadWidget->documentTree()->get_model());

		// Update data in document
		Gtk::TreeIter index = document->indexAtItem(m_wordItem);
		m_blockSetText = true;
		document->editItemText(index, get_text());
		m_blockSetText = false;
	}
	void onModelDataChanged(const Gtk::TreePath& /*path*/, const Gtk::TreeIter& iter) {
		Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(m_proofReadWidget->documentTree()->get_model());
		Gtk::TreeIter index = document->indexAtItem(m_wordItem);
		if(iter == index) {
			set_text(m_wordItem->text());
			if(document->indexIsMisspelledWord(index)) {
				Utils::set_error_state(this);
			} else {
				Utils::clear_error_state(this);
			}
		}
	}
	void onAttributeChanged(const Gtk::TreeIter& iter, const Glib::ustring& name, const Glib::ustring& /*value*/) {
		Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(m_proofReadWidget->documentTree()->get_model());
		if(document->itemAtIndex(iter) == m_wordItem) {
			if(name == "bold" || name == "italic") {
				Pango::FontDescription font;
				font.set_weight(m_wordItem->fontBold() ? Pango::WEIGHT_BOLD : Pango::WEIGHT_NORMAL);
				font.set_style(m_wordItem->fontItalic() ? Pango::STYLE_ITALIC : Pango::STYLE_NORMAL);
				override_font(font);
			} else if(name == "title:bbox") {
				Geometry::Point sceneCorner = MAIN->getDisplayer()->getSceneBoundingRect().topLeft();
				Geometry::Rectangle sceneBBox = m_wordItem->bbox().translate(sceneCorner.x, sceneCorner.y);
				Geometry::Point bottomLeft = MAIN->getDisplayer()->mapToView(sceneBBox.bottomLeft());
				Geometry::Point bottomRight = MAIN->getDisplayer()->mapToView(sceneBBox.bottomRight());
				// TODO?
//				int frameX = parentWidget()->parentWidget()->parentWidget()->pos().x();
//				move(bottomLeft.x - frameX, 0);
				int min_height, nat_height;
				get_preferred_height(min_height, nat_height);
				set_size_request(bottomRight.x - bottomLeft.x + 8, nat_height); // 8: border + padding
			}
		}
	}
	bool key_press_event(GdkEventKey* ev) {
		Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(m_proofReadWidget->documentTree()->get_model());

		bool nextLine = (ev->state == 0 && ev->keyval == GDK_KEY_Down) || (ev->keyval == GDK_KEY_Tab && m_wordItem == m_wordItem->parent()->children().back());
		bool prevLine = (ev->state == 0 && ev->keyval == GDK_KEY_Up) || (ev->state == GdkModifierType::GDK_SHIFT_MASK && ev->keyval == GDK_KEY_Tab && m_wordItem == m_wordItem->parent()->children().front());
		if(nextLine || prevLine) {
			bool next = false;
			Gtk::TreeIter index;
			if(nextLine) {
				next = true;
				index = document->indexAtItem(m_wordItem);
				// Move to first word of next line
				index = document->prevOrNextIndex(next, index, "ocr_line");
				index = document->prevOrNextIndex(true, index, "ocrx_word");
			} else if(prevLine) {
				index = document->indexAtItem(m_wordItem);
				// Move to last word of prev line
				index = document->prevOrNextIndex(false, index, "ocr_line");
				index = document->prevOrNextIndex(false, index, "ocrx_word");
			}
			m_proofReadWidget->documentTree()->get_selection()->unselect_all();
			m_proofReadWidget->documentTree()->get_selection()->select(index);
		} else if(ev->keyval == GDK_KEY_space && ev->state == GdkModifierType::GDK_CONTROL_MASK) {
			// Spelling menu
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			Gtk::Menu menu;
			document->addSpellingActions(&menu, index);
			auto positioner = sigc::bind(sigc::ptr_fun(Utils::popup_positioner), this, &menu, false, true);
			menu.popup(positioner, 0, gtk_get_current_event_time());
		} else if((ev->keyval == GDK_KEY_Return || ev->keyval == GDK_KEY_KP_Enter) && ev->state == GdkModifierType::GDK_CONTROL_MASK) {
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			document->addWordToDictionary(index);
		} else if(ev->keyval == GDK_KEY_b && ev->state == GdkModifierType::GDK_CONTROL_MASK) {
			// Bold
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			document->editItemAttribute(index, "bold", m_wordItem->fontBold() ? "0" : "1");
		} else if(ev->keyval == GDK_KEY_i && ev->state == GdkModifierType::GDK_CONTROL_MASK) {
			// Italic
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			document->editItemAttribute(index, "italic", m_wordItem->fontItalic() ? "0" : "1");
		} else if((ev->keyval == GDK_KEY_Up || ev->keyval == GDK_KEY_Down) && ev->state & GdkModifierType::GDK_CONTROL_MASK) {
			// Adjust bbox top/bottom
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			Geometry::Rectangle bbox = m_wordItem->bbox();
			if(ev->state & GdkModifierType::GDK_SHIFT_MASK) {
				bbox.setBottom(bbox.bottom() + (ev->keyval == GDK_KEY_Up ? -1 : 1));
			} else {
				bbox.setTop(bbox.top() + (ev->keyval == GDK_KEY_Up ? -1 : 1));
			}
			Glib::ustring bboxstr = Glib::ustring::compose("%1 %2 %3 %4", bbox.left(), bbox.top(), bbox.right(), bbox.bottom());
			document->editItemAttribute(index, "title:bbox", bboxstr);
		} else if((ev->keyval == GDK_KEY_Left || ev->keyval == GDK_KEY_Right) && ev->state & GdkModifierType::GDK_CONTROL_MASK) {
			// Adjust bbox left/right
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			Geometry::Rectangle bbox = m_wordItem->bbox();
			if(ev->state & GdkModifierType::GDK_SHIFT_MASK) {
				bbox.setRight(bbox.right() + (ev->keyval == GDK_KEY_Left ? -1 : 1));
			} else {
				bbox.setLeft(bbox.left() + (ev->keyval == GDK_KEY_Left ? -1 : 1));
			}
			Glib::ustring bboxstr = Glib::ustring::compose("%1 %2 %3 %4", bbox.left(), bbox.top(), bbox.right(), bbox.bottom());
			document->editItemAttribute(index, "title:bbox", bboxstr);
		} else if(ev->keyval == GDK_KEY_D && ev->state == GdkModifierType::GDK_CONTROL_MASK) {
			// Divide
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			document->splitItemText(index, property_cursor_position(), get_pango_context());
		} else if(ev->keyval == GDK_KEY_M && ev->state & GdkModifierType::GDK_CONTROL_MASK) {
			// Merge
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			document->mergeItemText(index, (ev->state & GdkModifierType::GDK_SHIFT_MASK) != 0);
		} else if(ev->keyval == GDK_KEY_Delete && ev->state == GdkModifierType::GDK_CONTROL_MASK) {
			Gtk::TreeIter index = document->indexAtItem(m_wordItem);
			document-> removeItem(index);
		} else if(ev->keyval == GDK_KEY_plus && ev->state & GdkModifierType::GDK_CONTROL_MASK) {
			m_proofReadWidget->adjustFontSize(+1);
		} else if((ev->keyval == GDK_KEY_minus || ev->keyval == GDK_KEY_underscore) && ev->state & GdkModifierType::GDK_CONTROL_MASK) {
			m_proofReadWidget->adjustFontSize(-1);
		} else {
			return false;
		}
		return true;
	}
	bool focus_in_event(GdkEventFocus* ev) {
		Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(m_proofReadWidget->documentTree()->get_model());
		m_proofReadWidget->documentTree()->get_selection()->unselect_all();
		m_proofReadWidget->documentTree()->get_selection()->select(document->indexAtItem(m_wordItem));
		m_proofReadWidget->setConfidenceLabel(std::atoi(m_wordItem->getTitleAttribute("x_wconf").data()));
		return false;
		// TODO
//		if(ev->reason() != Qt::MouseFocusReason) {
//			deselect();
//			setCursorPosition(0);
//		}
	}
};


HOCRProofReadWidget::HOCRProofReadWidget(Gtk::TreeView* treeView)
	: Gtk::Window(Gtk::WINDOW_POPUP), m_treeView(treeView) {
	m_mainLayout = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
	m_mainLayout->set_margin_left(2);
	m_mainLayout->set_margin_right(2);
	m_mainLayout->set_margin_top(2);
	m_mainLayout->set_margin_bottom(2);
	m_mainLayout->set_spacing(2);
	add(*m_mainLayout);
	set_transient_for(*MAIN->getWindow());

	m_linesLayout = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
	m_mainLayout->pack_start(*m_linesLayout, false, true);

	m_controlsWidget = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
	m_controlsWidget->set_spacing(2);
	m_mainLayout->pack_start(*m_controlsWidget, false, true);

	Pango::FontDescription smallFont;
	smallFont.set_size(0.8 * smallFont.get_size());

	m_confidenceLabel = Gtk::manage(new Gtk::Label());
	m_confidenceLabel->override_font(smallFont);
	m_controlsWidget->pack_start(*m_confidenceLabel, true, true);

	Gtk::Button* helpButton = Gtk::manage(new Gtk::Button(_("Keyboard shortcuts")));
	helpButton->set_relief(Gtk::RELIEF_NONE);
	CONNECT(helpButton, clicked, [this] { showShortcutsDialog(); });
	m_controlsWidget->pack_start(*helpButton, false, true);

	m_settingsButton = Gtk::manage(new Gtk::Button());
	m_settingsButton->set_relief(Gtk::RELIEF_NONE);
	m_settingsButton->set_image_from_icon_name("preferences-system");
	CONNECT(m_settingsButton, clicked, [this] { showSettingsMenu(); });
	m_controlsWidget->pack_start(*m_settingsButton, false, true);


	m_settingsMenu = new Gtk::Window(Gtk::WINDOW_POPUP);
	m_settingsMenu->set_type_hint(Gdk::WINDOW_TYPE_HINT_POPUP_MENU);
	m_settingsMenu->set_attached_to(*this);

	Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
	grid->set_column_spacing(2);
	grid->set_row_spacing(2);
	m_settingsMenu->add(*grid);

	grid->attach(*Gtk::manage(new Gtk::Label(_("Lines before:"))), 0, 0);

	m_spinLinesBefore = Gtk::manage(new Gtk::SpinButton());
	m_spinLinesBefore->set_range(0, 10);
	m_spinLinesBefore->set_increments(1, 1);
	CONNECT(m_spinLinesBefore, value_changed, [this] { updateWidget(true); });
	grid->attach(*m_spinLinesBefore, 1, 0);

	grid->attach(*Gtk::manage(new Gtk::Label(_("Lines after:"))), 0, 0);

	m_spinLinesAfter = Gtk::manage(new Gtk::SpinButton());
	m_spinLinesAfter->set_range(0, 10);
	m_spinLinesAfter->set_increments(1, 1);
	CONNECT(m_spinLinesAfter, value_changed, [this] { updateWidget(true); });
	grid->attach(*m_spinLinesAfter, 1, 0);

	m_settingsMenu->show_all();
	m_settingsMenu->hide();

	ADD_SETTING(SpinSetting("proofreadlinesbefore", m_spinLinesBefore));
	ADD_SETTING(SpinSetting("proofreadlinesafter", m_spinLinesAfter));

	CONNECT(m_treeView->get_selection(), changed, [this] { updateWidget(); });

	Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(documentTree()->get_model());

	// Clear for rebuild if structure changes
	CONNECT(document, row_deleted, [this](const Gtk::TreePath&) { updateWidget(); });
	CONNECT(document, row_inserted, [this](const Gtk::TreePath&, const Gtk::TreeIter&) { updateWidget(); });
	CONNECT(document, rows_reordered, [this](const Gtk::TreePath&, const Gtk::TreeIter&, int*) { updateWidget(); });
//	CONNECT(MAIN->getDisplayer(), imageChanged, [this] { updateWidget(true); });
	CONNECT(MAIN->getDisplayer(), viewportChanged, [this] { repositionWidget(); });
	CONNECT(MAIN->getWindow(), window_state_event, [this](GdkEventWindowState * ev) {
		set_visible(ev->new_window_state == GDK_WINDOW_STATE_FOCUSED);
		return false;
	});

	Glib::RefPtr<Gtk::CssProvider> wconfStyleProvider = Gtk::CssProvider::create();
	wconfStyleProvider->load_from_data(
	    ".wconfLt70 { background-color: #ffb2b2; }"
	    ".wconfLt80 { background-color: #ffdab0; }"
	    ".wconfLt90 { background-color: #fffdb4; }"
	);
	m_confidenceLabel->get_style_context()->add_provider(wconfStyleProvider, 10000);

	// Show all children, but start hidden
	show_all();
	hide();
}

HOCRProofReadWidget::~HOCRProofReadWidget() {
	delete m_settingsMenu;
}

void HOCRProofReadWidget::showSettingsMenu() {
	int x_root, y_root;
	m_settingsButton->get_window()->get_origin(x_root, y_root);
	x_root += m_settingsButton->get_allocation().get_x();
	y_root += m_settingsButton->get_allocation().get_y();

	Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
	Glib::RefPtr<const Gdk::Screen> screen = MAIN->getWindow()->get_screen();
	Gdk::Rectangle rect = screen->get_monitor_workarea(screen->get_monitor_at_point(x_root, y_root));

	int w, h, trash;
	m_settingsMenu->get_preferred_width(trash, w);
	m_settingsMenu->get_preferred_height(trash, h);
	int x = std::min(std::max(int(x_root), rect.get_x()), rect.get_x() + rect.get_width() - w);
	int y = std::min(std::max(int(y_root), rect.get_y()), rect.get_y() + rect.get_height() - h);
	m_settingsMenu->move(x, y);
	m_settingsMenu->show();
	GdkWindow* gdkwin = m_settingsMenu->get_window()->gobj();
#if GTK_CHECK_VERSION(3,20,0)
	gdk_seat_grab(gdk_device_get_seat(gtk_get_current_event_device()), gdkwin, GDK_SEAT_CAPABILITY_ALL, true, nullptr, nullptr, nullptr, nullptr);
#else
	gdk_device_grab(gtk_get_current_event_device(), gdkwin, GDK_OWNERSHIP_APPLICATION, true, GDK_BUTTON_PRESS_MASK, nullptr, event->time);
#endif

	loop->run();
	m_settingsMenu->hide();

#if GTK_CHECK_VERSION(3,20,0)
	gdk_seat_ungrab(gdk_device_get_seat(gtk_get_current_event_device()));
#else
	gdk_device_ungrab(gtk_get_current_event_device(), gtk_get_current_event_time());
#endif
}

void HOCRProofReadWidget::clear() {
	for(auto it = m_currentLines.begin(), itEnd = m_currentLines.end(); it != itEnd; ++it) {
		delete it->second;
	}
	m_currentLines.clear();
	m_currentLine = nullptr;
	m_confidenceLabel->set_text("");
	for(const Glib::ustring& className : m_confidenceLabel->get_style_context()->list_classes()) {
		m_confidenceLabel->get_style_context()->remove_class(className);
	}
	hide();
}

void HOCRProofReadWidget::updateWidget(bool force) {
	Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(m_treeView->get_model());
	std::vector<Gtk::TreePath> items = m_treeView->get_selection()->get_selected_rows();
	Gtk::TreeIter current = items.empty() ? Gtk::TreeIter() : document->get_iter(items[0]);

	int nrLinesBefore = m_spinLinesBefore->get_value_as_int();
	int nrLinesAfter = m_spinLinesAfter->get_value_as_int();

	const HOCRItem* item = document->itemAtIndex(current);
	if(!item) {
		clear();
		return;
	}
	const HOCRItem* lineItem = nullptr;
	const HOCRItem* wordItem = nullptr;
	if(item->itemClass() == "ocrx_word") {
		lineItem = item->parent();
		wordItem = item;
	} else if(item->itemClass() == "ocr_line") {
		lineItem = item;
	} else {
		clear();
		return;
	}

	const std::vector<HOCRItem*>& siblings = lineItem->parent()->children();
	int targetLine = lineItem->index();
	if(lineItem != m_currentLine || force) {
		// Rebuild widget
		std::map<const HOCRItem*, Gtk::Box*> newLines;
		int insPos = 0;
		for(int i = std::max(0, targetLine - nrLinesBefore), j = std::min(int(siblings.size()) - 1, targetLine + nrLinesAfter); i <= j; ++i) {
			HOCRItem* linei = siblings[i];
			auto it = m_currentLines.find(linei);
			if(it != m_currentLines.end()) {
				newLines[linei] = it->second;
				m_currentLines.erase(it);
				insPos = Utils::vector_index_of(m_linesLayout->get_children(), static_cast<Gtk::Widget*>(newLines[linei])) + 1;
			} else {
				Gtk::Box* lineWidget = Gtk::manage(new Gtk::Box);
				for(HOCRItem* word : siblings[i]->children()) {
					lineWidget->pack_start(*Gtk::manage(new LineEdit(this, word)), false, true);
				}
				m_linesLayout->pack_start(*lineWidget, false, true);
				m_linesLayout->reorder_child(*lineWidget, insPos++);
				newLines.insert(std::make_pair(linei, lineWidget));
			}
		}
		for(auto it = m_currentLines.begin(), itEnd = m_currentLines.end(); it != itEnd; ++it) {
			delete it->second;
		}
		m_currentLines.clear();
		m_currentLines = newLines;
		m_currentLine = lineItem;
		repositionWidget();
	}

	// Select selected word or first item of middle line
	LineEdit* focusLineEdit = static_cast<LineEdit*>(m_currentLines[lineItem]->get_children()[wordItem ? wordItem->index() : 0]);
	if(focusLineEdit && !m_treeView->has_focus()) {
		focusLineEdit->grab_focus();
	}

}

void HOCRProofReadWidget::repositionWidget() {

	if(m_currentLines.empty()) {
		return;
	}

	// Position frame
	Displayer* displayer = MAIN->getDisplayer();
	int frameXmin = std::numeric_limits<int>::max();
	int frameXmax = 0;
	Geometry::Point sceneCorner = displayer->getSceneBoundingRect().topLeft();
	for(auto pair : m_currentLines) {
		Gtk::Box* lineWidget = pair.second;
		if(lineWidget->get_children().empty()) {
			continue;
		}
		// First word
		LineEdit* lineEdit = static_cast<LineEdit*>(lineWidget->get_children()[0]);
		Geometry::Point bottomLeft = displayer->mapToView(lineEdit->item()->bbox().translate(sceneCorner.x, sceneCorner.y).bottomLeft());
		frameXmin = std::min(frameXmin, int(bottomLeft.x));
	}
	Geometry::Point bottomLeft = displayer->mapToView(m_currentLine->bbox().translate(sceneCorner.x, sceneCorner.y).bottomLeft());
	Geometry::Point topLeft = displayer->mapToView(m_currentLine->bbox().translate(sceneCorner.x, sceneCorner.y).topLeft());
	int frameY = bottomLeft.y;

	// Recompute font sizes so that text matches original as closely as possible
	Pango::FontDescription font = get_style_context()->get_font();
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(get_pango_context());
	double avgFactor = 0.0;
	int nFactors = 0;
	// First pass: min scaling factor, move to correct location
	for(auto pair : m_currentLines) {
		Gtk::Box* lineWidget = pair.second;
		int prevRight = frameXmin;
		for(int i = 0, n = lineWidget->get_children().size(); i < n; ++i) {
			LineEdit* lineEdit = static_cast<LineEdit*>(lineWidget->get_children()[i]);
			Geometry::Rectangle sceneBBox = lineEdit->item()->bbox().translate(sceneCorner.x, sceneCorner.y);
			Geometry::Point bottomLeft = displayer->mapToView(sceneBBox.bottomLeft());
			Geometry::Point bottomRight = displayer->mapToView(sceneBBox.bottomRight());
			// Factor weighed by length
			layout->set_text(lineEdit->get_text());
			int width, height;
			layout->get_pixel_size(width, height);
			double factor = (bottomRight.x - bottomLeft.x) / double(width);
			avgFactor += lineEdit->get_text().length() * factor;
			nFactors += lineEdit->get_text().length();
			lineEdit->set_margin_left(bottomLeft.x - prevRight - 6); // 4: border + padding
			prevRight = bottomRight.x;
			int min_height, nat_height;
			lineEdit->get_preferred_height(min_height, nat_height);
			lineEdit->set_size_request(bottomRight.x - bottomLeft.x + 8, nat_height); // 8: border + padding
			frameXmax = std::max(frameXmax, int(bottomRight.x) + 8);
		}
	}
	avgFactor = avgFactor > 0 ? avgFactor / nFactors : 1.;

	// Second pass: apply font sizes, set line heights
	font.set_size(font.get_size() * avgFactor);
	layout->set_font_description(font);
	int fontWidth, fontHeight;
	layout->get_pixel_size(fontWidth, fontHeight);
	for(auto pair : m_currentLines) {
		Gtk::Box* lineWidget = pair.second;
		for(int i = 0, n = lineWidget->get_children().size(); i < n; ++i) {
			LineEdit* lineEdit = static_cast<LineEdit*>(lineWidget->get_children()[i]);

			Pango::FontDescription lineEditFont = lineEdit->get_style_context()->get_font();
			lineEditFont.set_size(font.get_size() + m_fontSizeDiff);
			lineEdit->override_font(lineEditFont);
			int width, height;
			lineEdit->get_size_request(width, height);
			lineEdit->set_size_request(width, fontHeight + 5);
		}
		int width, height;
		lineWidget->get_size_request(width, height);
		lineWidget->set_size_request(width, fontHeight + 10);
	}
	frameY += 10;
	resize(frameXmax - frameXmin + 2 + 2 * m_mainLayout->get_spacing(), m_currentLines.size() * (fontHeight + 10) + 2 * m_mainLayout->get_spacing() + m_controlsWidget->get_allocated_height());

	// Place widget above line if it overflows page
	Glib::RefPtr<HOCRDocument> document = Glib::RefPtr<HOCRDocument>::cast_static(m_treeView->get_model());
	std::vector<Gtk::TreePath> items = m_treeView->get_selection()->get_selected_rows();
	Gtk::TreeIter current = items.empty() ? Gtk::TreeIter() : document->get_iter(items[0]);
	const HOCRItem* item = document->itemAtIndex(current);
	Geometry::Rectangle sceneBBox = item->page()->bbox().translate(sceneCorner.x, sceneCorner.y);
	double maxy = displayer->mapToView(sceneBBox.bottomLeft()).y;
	if(frameY + get_allocated_height() - maxy > 0) {
		frameY = topLeft.y - get_allocated_height();
	}

	int x_root, y_root;
	displayer->mapViewToRoot(frameXmin - m_mainLayout->get_spacing(), frameY, x_root, y_root);
	move(x_root, y_root);
	show_all();
}

void HOCRProofReadWidget::showShortcutsDialog() {
	Glib::ustring text = _(
	                         "<table>"
	                         "<tr><td>Tab</td><td>Next field</td></tr>"
	                         "<tr><td>Shift+Tab</td><td>Previous field</td></tr>"
	                         "<tr><td>Down</td><td>Next line</td></tr>"
	                         "<tr><td>Up</td><td>Previous line</td></tr>"
	                         "<tr><td>Ctrl+Space</td><td>Spelling suggestions</td></tr>"
	                         "<tr><td>Ctrl+Enter</td><td>Add word to dictionary</td></tr>"
	                         "<tr><td>Ctrl+B</td><td>Toggle bold</td></tr>"
	                         "<tr><td>Ctrl+I</td><td>Toggle italic</td></tr>"
	                         "<tr><td>Ctrl+D</td><td>Divide word at cursor position</td></tr>"
	                         "<tr><td>Ctrl+M</td><td>Merge with previous word</td></tr>"
	                         "<tr><td>Ctrl+Shift+M</td><td>Merge with next word</td></tr>"
	                         "<tr><td>Ctrl+Delete</td><td>Delete word</td></tr>"
	                         "<tr><td>Ctrl+{Left,Right}</td><td>Adjust left bounding box edge</td></tr>"
	                         "<tr><td>Ctrl+Shift+{Left,Right}</td><td>Adjust right bounding box edge</td></tr>"
	                         "<tr><td>Ctrl+{Up,Down}</td><td>Adjust top bounding box edge</td></tr>"
	                         "<tr><td>Ctrl+Shift+{Up,Down}</td><td>Adjust bottom bounding box edge</td></tr>"
	                         "<tr><td>Ctrl++</td><td>Increase font size</td></tr>"
	                         "<tr><td>Ctrl+-</td><td>Decrease font size</td></tr>"
	                         "</table>"
	                     );
	// Just to keep the same translation string with the qt interface
	Utils::string_replace(text, "<table>", "", true);
	Utils::string_replace(text, "<tr><td>", "", true);
	Utils::string_replace(text, "</td><td>", "\t\t", true);
	Utils::string_replace(text, "</td></tr>", "\n", true);
	Utils::string_replace(text, "</table>", "", true);
	Utils::messageBox(Gtk::MESSAGE_INFO, _("Keyboard Shortcuts"), text, "", Utils::Button::Ok, MAIN->getWindow());
}

Glib::ustring HOCRProofReadWidget::confidenceStyle(int wconf) const {
	if(wconf < 70) {
		return "wconfLt70";
	} else if(wconf < 80) {
		return "wconfLt80";
	} else if(wconf < 90) {
		return "wconfLt90;";
	}
	return Glib::ustring();
}

void HOCRProofReadWidget::setConfidenceLabel(int wconf) {
	m_confidenceLabel->set_text(Glib::ustring::compose(_("Confidence: %1"), wconf));
	Glib::ustring style = confidenceStyle(wconf);
	if(style.empty()) {
		while(!m_currentLines.empty()) {
			delete m_currentLines.begin()->second;
		}
	} else {
		m_confidenceLabel->get_style_context()->add_class(style);
	}
}

void HOCRProofReadWidget::adjustFontSize(int diff) {
	m_fontSizeDiff += diff;
	repositionWidget();
}

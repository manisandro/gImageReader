/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
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

#include <fstream>
#include <cmath>
#include <cstring>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#undef USE_STD_NAMESPACE
#include <libxml++/libxml++.h>

#include "ConfigSettings.hh"
#include "DisplayerToolHOCR.hh"
#include "FileDialogs.hh"
#include "HOCRDocument.hh"
#include "HOCROdtExporter.hh"
#include "HOCRPdfExporter.hh"
#include "HOCRTextExporter.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SearchReplaceFrame.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include "XmlUtils.hh"


class OutputEditorHOCR::HOCRCellRendererText : public Gtk::CellRendererText {
public:
	HOCRCellRendererText() : Glib::ObjectBase("CellRendererTextHOCR") {}
	Gtk::Entry* get_entry() const {
		return m_entry;
	}
	const Glib::ustring& get_current_path() const {
		return m_path;
	}

protected:
	Gtk::Entry* m_entry = nullptr;
	Glib::ustring m_path;
	Gtk::CellEditable* start_editing_vfunc(GdkEvent* /*event*/, Gtk::Widget& /*widget*/, const Glib::ustring& path, const Gdk::Rectangle& /*background_area*/, const Gdk::Rectangle& /*cell_area*/, Gtk::CellRendererState /*flags*/) override {
		if(!property_editable()) {
			return nullptr;
		}
		m_entry = new Gtk::Entry;
		m_entry->set_text(property_text());
		m_entry->set_has_frame(false);
		m_entry->signal_editing_done().connect([ = ] {
			if(!m_entry->property_editing_canceled()) {
				edited(path, m_entry->get_text());
			}
			m_entry = nullptr;
			m_path.clear();
		});
		m_entry->show();
		m_path = path;
		return Gtk::manage(m_entry);
	}
};

class OutputEditorHOCR::HOCRAttribRenderer : public Gtk::CellRendererText {
public:
	HOCRAttribRenderer(Glib::RefPtr<Gtk::TreeStore> propStore, const OutputEditorHOCR::PropStoreColumns& cols)
		: Glib::ObjectBase("HOCRAttribRenderer"), m_propStore(propStore), m_cols(cols) {}

protected:
	Glib::RefPtr<Gtk::TreeStore> m_propStore;
	const OutputEditorHOCR::PropStoreColumns& m_cols;

	Gtk::CellEditable* start_editing_vfunc(GdkEvent* /*event*/, Gtk::Widget& /*widget*/, const Glib::ustring& path, const Gdk::Rectangle& /*background_area*/, const Gdk::Rectangle& /*cell_area*/, Gtk::CellRendererState /*flags*/) override {
		if(!property_editable()) {
			return nullptr;
		}
		Gtk::TreeIter it = m_propStore->get_iter(path);
		if(it && (*it)[m_cols.attr] == "title:x_font") {
			FontComboBox* combo = new FontComboBox();
			Glib::ustring curFont = property_text();
			combo->set_active_font(property_text());
			combo->signal_editing_done().connect([ = ] {
				Glib::ustring newFont = combo->get_active_font();
				if(!combo->property_editing_canceled() && newFont != curFont) {
					edited(path, combo->get_active_font());
				}
			});
			combo->show();
			return Gtk::manage(combo);
		} else if(it && ((*it)[m_cols.attr] == "bold" || (*it)[m_cols.attr] == "italic")) {
			Gtk::ComboBoxText* combo = new Gtk::ComboBoxText();
			combo->insert(0, "no", "no");
			combo->insert(1, "yes", "yes");
			if((*it)[m_cols.multiple] == true) {
				combo->set_active(-1);
			} else {
				combo->set_active_id((*it)[m_cols.value]);
			}
			combo->signal_editing_done().connect([ = ] {
				edited(path, combo->get_active_id());
			});
			combo->show();
			return Gtk::manage(combo);
		} else {
			Gtk::Entry* entry = new Gtk::Entry;
			entry->set_text(property_text());
			entry->set_has_frame(false);
			entry->signal_editing_done().connect([ = ] {
				if(!entry->property_editing_canceled()) {
					edited(path, entry->get_text());
				}
			});
			entry->show();
			return Gtk::manage(entry);
		}
	}
};


class OutputEditorHOCR::HOCRTreeView : public Gtk::TreeView {
public:
	HOCRTreeView(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& /*builder*/)
		: Gtk::TreeView(cobject) {
		CONNECT(get_selection(), changed, [this] {
			Gtk::TreeIter oldIndex = m_currentIndex;
			m_currentIndex = currentIndex();
			m_signal_current_index_changed(m_currentIndex, oldIndex);
		});
	}
	sigc::signal<void, Gtk::TreeIter, Gtk::TreeIter> signal_current_index_changed() {
		return m_signal_current_index_changed;
	}
	sigc::signal<void, GdkEventButton*> signal_context_menu() {
		return m_signal_context_menu;
	}
	sigc::signal<void> signal_delete() {
		return m_signal_delete;
	}

	Gtk::TreeIter currentIndex() {
		std::vector<Gtk::TreePath> items = get_selection()->get_selected_rows();
		if(!items.empty()) {
			return get_model()->get_iter(items[0]);
		}
		return Gtk::TreeIter();
	}
	Gtk::TreeIter indexAtPos(int evx, int evy) {
		Gtk::TreePath path;
		Gtk::TreeViewColumn* col;
		int cell_x, cell_y;
		get_path_at_pos(evx, evy, path, col, cell_x, cell_y);
		if(!path) {
			return Gtk::TreeIter();
		}
		return get_model()->get_iter(path);
	}

protected:
	bool on_button_press_event(GdkEventButton* button_event) {
		bool rightclick = (button_event->type == GDK_BUTTON_PRESS && button_event->button == 3);

		Gtk::TreePath path;
		Gtk::TreeViewColumn* col;
		int cell_x, cell_y;
		get_path_at_pos(int(button_event->x), int(button_event->y), path, col, cell_x, cell_y);
		if(!path) {
			return false;
		}
		std::vector<Gtk::TreePath> selection = get_selection()->get_selected_rows();
		bool selected = std::find(selection.begin(), selection.end(), path) != selection.end();

		if(!rightclick || (rightclick && !selected)) {
			Gtk::TreeView::on_button_press_event(button_event);
		}
		if(rightclick) {
			m_signal_context_menu.emit(button_event);
		}
		return true;
	}
	bool on_key_press_event(GdkEventKey* key_event) {
		if(key_event->keyval == GDK_KEY_Delete) {
			m_signal_delete.emit();
			return true;
		}
		return Gtk::TreeView::on_key_press_event(key_event);
	}

private:
	ClassData m_classdata;
	Gtk::TreeIter m_currentIndex;
	sigc::signal<void, Gtk::TreeIter, Gtk::TreeIter> m_signal_current_index_changed;
	sigc::signal<void, GdkEventButton*> m_signal_context_menu;
	sigc::signal<void> m_signal_delete;
};

///////////////////////////////////////////////////////////////////////////////

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool) {
	ui.builder->get_widget_derived("treeviewItems", m_treeView);
	ui.setupUi();
	ui.dialogAddWord->set_transient_for(*MAIN->getWindow());

	m_preview = new DisplayerImageItem();
	m_preview->setZIndex(2);
	MAIN->getDisplayer()->addItem(m_preview);

	m_tool = tool;

	m_document = Glib::RefPtr<HOCRDocument>(new HOCRDocument(&m_spell));

	// HOCR tree view
	m_treeView->set_model(m_document);
	m_treeView->get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	// First column: [checkbox][icon][text]
	m_treeViewTextCell = Gtk::manage(new HOCRCellRendererText);
	Gtk::TreeViewColumn* itemViewCol1 = Gtk::manage(new Gtk::TreeViewColumn(""));
	Gtk::CellRendererToggle& checkRenderer = *Gtk::manage(new Gtk::CellRendererToggle);
	Gtk::CellRendererPixbuf& iconRenderer = *Gtk::manage(new Gtk::CellRendererPixbuf);
	Gtk::CellRendererText& textRenderer = *m_treeViewTextCell;
	itemViewCol1->pack_start(checkRenderer, false);
	itemViewCol1->pack_start(iconRenderer, false);
	itemViewCol1->pack_start(textRenderer, true);
	itemViewCol1->add_attribute(checkRenderer, "active", HOCRDocument::COLUMN_CHECKED);
	itemViewCol1->add_attribute(iconRenderer, "pixbuf", HOCRDocument::COLUMN_ICON);
	itemViewCol1->add_attribute(textRenderer, "text", HOCRDocument::COLUMN_TEXT);
	itemViewCol1->add_attribute(textRenderer, "editable", HOCRDocument::COLUMN_EDITABLE);
	itemViewCol1->add_attribute(textRenderer, "foreground", HOCRDocument::COLUMN_TEXT_COLOR);
	CONNECT(&checkRenderer, toggled, [this](const Glib::ustring & path) {
		Gtk::TreeIter it = m_document->get_iter(path);
		bool prev;
		it->get_value<bool>(HOCRDocument::COLUMN_CHECKED, prev);
		it->set_value(HOCRDocument::COLUMN_CHECKED, !prev);
	});
	CONNECT(&textRenderer, edited, [this](const Glib::ustring & path, const Glib::ustring & text) {
		m_document->get_iter(path)->set_value(HOCRDocument::COLUMN_TEXT, text);
	});
	m_treeView->append_column(*itemViewCol1);
	m_treeView->set_expander_column(*itemViewCol1);
	// Second column: wconf, hidden by default
	Gtk::TreeViewColumn* itemViewCol2 = Gtk::manage(new Gtk::TreeViewColumn(""));
	Gtk::CellRendererText& wconfRenderer = *Gtk::manage(new Gtk::CellRendererText);
	itemViewCol2->pack_start(wconfRenderer, true);
	itemViewCol2->add_attribute(wconfRenderer, "text", HOCRDocument::COLUMN_WCONF);
	m_treeView->append_column(*itemViewCol2);
	itemViewCol2->set_visible(false);
	itemViewCol2->set_fixed_width(32);

	// Navigation target combo box
	Glib::RefPtr<Gtk::ListStore> navigationComboModel = Gtk::ListStore::create(m_navigationComboCols);
	ui.comboNavigation->set_model(navigationComboModel);
	Gtk::TreeRow row = *(navigationComboModel->append());
	row[m_navigationComboCols.label] = _("Page");
	row[m_navigationComboCols.itemClass] = "ocr_page";
	row = *(navigationComboModel->append());
	row[m_navigationComboCols.label] = _("Block");
	row[m_navigationComboCols.itemClass] = "ocr_carea";
	row = *(navigationComboModel->append());
	row[m_navigationComboCols.label] = _("Paragraph");
	row[m_navigationComboCols.itemClass] = "ocr_par";
	row = *(navigationComboModel->append());
	row[m_navigationComboCols.label] = _("Line");
	row[m_navigationComboCols.itemClass] = "ocr_line";
	row = *(navigationComboModel->append());
	row[m_navigationComboCols.label] = _("Word");
	row[m_navigationComboCols.itemClass] = "ocrx_word";
	row = *(navigationComboModel->append());
	row[m_navigationComboCols.label] = _("Misspelled word");
	row[m_navigationComboCols.itemClass] = "ocrx_word_bad";
	ui.comboNavigation->pack_start(m_navigationComboCols.label);
	ui.comboNavigation->set_active(0);

	// HOCR item property table
	m_propStore = Gtk::TreeStore::create(m_propStoreCols);
	ui.treeviewProperties->set_model(m_propStore);
	// First column: attrib name
	Gtk::TreeViewColumn* nameCol = Gtk::manage(new Gtk::TreeViewColumn(""));
	Gtk::CellRendererText& nameRenderer = *Gtk::manage(new Gtk::CellRendererText());
	nameRenderer.property_ellipsize() = Pango::ELLIPSIZE_END;
	nameRenderer.property_editable() = false;
	nameCol->pack_start(nameRenderer);
	nameCol->add_attribute(nameRenderer, "text", m_propStoreCols.name);
	nameCol->add_attribute(nameRenderer, "background", m_propStoreCols.background);
	nameCol->add_attribute(nameRenderer, "weight", m_propStoreCols.weight);
	nameCol->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	nameCol->set_expand(true);
	ui.treeviewProperties->append_column(*nameCol);
	// Second column: attrib value
	Gtk::TreeViewColumn* valueCol = Gtk::manage(new Gtk::TreeViewColumn(""));
	Gtk::CellRendererText& valueRenderer = *Gtk::manage(new HOCRAttribRenderer(m_propStore, m_propStoreCols));
	valueRenderer.property_ellipsize() = Pango::ELLIPSIZE_END;
	valueCol->pack_start(valueRenderer);
	valueCol->add_attribute(valueRenderer, "text", m_propStoreCols.value);
	valueCol->add_attribute(valueRenderer, "placeholder_text", m_propStoreCols.placeholder);
	valueCol->add_attribute(valueRenderer, "editable", m_propStoreCols.editable);
	valueCol->add_attribute(valueRenderer, "background", m_propStoreCols.background);
	valueCol->add_attribute(valueRenderer, "weight", m_propStoreCols.weight);
	valueCol->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	valueCol->set_expand(true);
	CONNECT(&valueRenderer, edited, sigc::mem_fun(this, &OutputEditorHOCR::editAttribute));
	ui.treeviewProperties->append_column(*valueCol);

	// HOCR item source view
	Glib::RefPtr<Gsv::Buffer> buffer = Gsv::Buffer::create();
	buffer->set_highlight_syntax(true);
	buffer->set_language(Gsv::LanguageManager::get_default()->get_language("xml"));
	ui.textviewSource->set_buffer(buffer);

	// Search replace frame
	m_searchFrame = new SearchReplaceFrame();
	ui.boxSearch->pack_start(*Gtk::manage(m_searchFrame->getWidget()));
	m_searchFrame->getWidget()->set_visible(false);


	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	ui.buttonSave->add_accelerator("clicked", group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonFindreplace->add_accelerator("clicked", group, GDK_KEY_F, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonNavigationNext->add_accelerator("clicked", group, GDK_KEY_F3, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	ui.buttonNavigationPrev->add_accelerator("clicked", group, GDK_KEY_F3, Gdk::SHIFT_MASK, Gtk::AccelFlags(0));

	ui.buttonSave->set_sensitive(false);
	ui.buttonExport->set_sensitive(false);

	CONNECT(ui.menuItemInsertAppend, activate, [this] { setInsertMode(InsertMode::Append, "ins_hocr_append.png"); });
	CONNECT(ui.menuItemInsertBefore, activate, [this] { setInsertMode(InsertMode::InsertBefore, "ins_hocr_before.png"); });
	CONNECT(ui.buttonOpen, clicked, [this] { open(InsertMode::Replace); });
	CONNECT(ui.menuItemOpenAppend, activate, [this] { open(InsertMode::Append); });
	CONNECT(ui.menuItemOpenInsert, activate, [this] { open(InsertMode::InsertBefore); });
	CONNECT(ui.buttonSave, clicked, [this] { save(); });
	CONNECT(ui.menuitemExportText, activate, [this] { exportToText(); });
	CONNECT(ui.menuitemExportPdf, activate, [this] { exportToPDF(); });
	CONNECT(ui.menuitemExportOdt, activate, [this] { exportToODT(); });
	CONNECT(ui.buttonClear, clicked, [this] { clear(); });
	CONNECT(ui.buttonFindreplace, toggled, [this] { m_searchFrame->clear(); m_searchFrame->getWidget()->set_visible(ui.buttonFindreplace->get_active()); });
	CONNECT(ui.buttonWconf, toggled, [this] { m_treeView->get_column(1)->set_visible(ui.buttonWconf->get_active());});
	CONNECT(ui.buttonPreview, toggled, [this] { updatePreview(); });
	CONNECT(m_treeView, context_menu, [this](GdkEventButton * ev) {
		showTreeWidgetContextMenu(ev);
	});
	CONNECT(m_treeView, delete, [this] {
		m_document->removeItem(m_treeView->currentIndex());
	});
	CONNECT(m_searchFrame, find_replace, sigc::mem_fun(this, &OutputEditorHOCR::findReplace));
	CONNECT(m_searchFrame, replace_all, sigc::mem_fun(this, &OutputEditorHOCR::replaceAll));
	CONNECT(m_searchFrame, apply_substitutions, sigc::mem_fun(this, &OutputEditorHOCR::applySubstitutions));
	m_connectionCustomFont = CONNECT(ConfigSettings::get<FontSetting>("customoutputfont"), changed, [this] { setFont(); });
	m_connectionDefaultFont = CONNECT(ConfigSettings::get<SwitchSetting>("systemoutputfont"), changed, [this] { setFont(); });
	m_connectionSelectionChanged = CONNECT(m_treeView, current_index_changed, sigc::mem_fun(this, &OutputEditorHOCR::showItemProperties));
	CONNECT(ui.notebook, switch_page, [this](Gtk::Widget*, guint) {
		updateSourceText();
	});
	CONNECT(m_tool, bbox_changed, sigc::mem_fun(this, &OutputEditorHOCR::updateCurrentItemBBox));
	CONNECT(m_tool, bbox_drawn, sigc::mem_fun(this, &OutputEditorHOCR::bboxDrawn));
	CONNECT(m_tool, position_picked, sigc::mem_fun(this, &OutputEditorHOCR::pickItem));
	CONNECTX(m_document, row_inserted, [this](const Gtk::TreePath&, const Gtk::TreeIter&) {
		setModified();
	});
	CONNECTX(m_document, row_deleted, [this](const Gtk::TreePath&) {
		setModified();
	});
	CONNECTX(m_document, row_changed, [this](const Gtk::TreePath&, const Gtk::TreeIter&) {
		setModified();
		updateSourceText();
	});
	CONNECTX(m_document, item_attribute_changed, [this](const Gtk::TreeIter&, const Glib::ustring&, const Glib::ustring&) {
		setModified();
	});
	CONNECTX(m_document, item_attribute_changed, [this](const Gtk::TreeIter & it, const Glib::ustring & attr, const Glib::ustring & value) {
		updateAttributes(it, attr, value);
		updateSourceText();
	});
	CONNECT(ui.comboNavigation, changed, [this] { navigateTargetChanged(); });
	CONNECT(ui.buttonNavigationNext, clicked, [this] { navigateNextPrev(true); });
	CONNECT(ui.buttonNavigationPrev, clicked, [this] { navigateNextPrev(false); });
	CONNECT(ui.buttonExpandAll, clicked, [this] { expandCollapseItemClass(true); });
	CONNECT(ui.buttonCollapseAll, clicked, [this] { expandCollapseItemClass(false); });

	setFont();
}

OutputEditorHOCR::~OutputEditorHOCR() {
	MAIN->getDisplayer()->removeItem(m_preview);
	delete m_preview;
	delete m_searchFrame;
	m_connectionCustomFont.disconnect();
	m_connectionDefaultFont.disconnect();
}

void OutputEditorHOCR::setFont() {
	if(ConfigSettings::get<SwitchSetting>("systemoutputfont")->getValue()) {
		ui.textviewSource->unset_font();
	} else {
		Glib::ustring fontName = ConfigSettings::get<FontSetting>("customoutputfont")->getValue();
		ui.textviewSource->override_font(Pango::FontDescription(fontName));
	}
}

void OutputEditorHOCR::setInsertMode(InsertMode mode, const std::string& iconName) {
	m_insertMode = mode;
	ui.imageInsert->set(Gdk::Pixbuf::create_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/%1", iconName)));
}

void OutputEditorHOCR::setModified() {
	ui.buttonSave->set_sensitive(m_document->pageCount() > 0);
	ui.buttonExport->set_sensitive(m_document->pageCount() > 0);
	ui.boxNavigation->set_sensitive(m_document->pageCount() > 0);
	m_preview->setVisible(false);
	m_connectionPreviewTimer = Glib::signal_timeout().connect([this] { updatePreview(); return false; }, 100); // Use a timer because setModified is potentially called a large number of times when the HOCR tree changes
	m_modified = true;
}

OutputEditorHOCR::ReadSessionData* OutputEditorHOCR::initRead(tesseract::TessBaseAPI& tess) {
	tess.SetPageSegMode(tesseract::PSM_AUTO_ONLY);
	HOCRReadSessionData* data = new HOCRReadSessionData;
	data->insertIndex = m_insertMode == InsertMode::Append ? m_document->pageCount() : currentPage();
	return data;
}

void OutputEditorHOCR::read(tesseract::TessBaseAPI& tess, ReadSessionData* data) {
	tess.SetVariable("hocr_font_info", "true");
	char* text = tess.GetHOCRText(data->page);
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	Utils::runInMainThreadBlocking([&] { addPage(text, *hdata); });
	delete[] text;
	++hdata->insertIndex;
}

void OutputEditorHOCR::readError(const Glib::ustring& errorMsg, ReadSessionData* data) {
	static_cast<HOCRReadSessionData*>(data)->errors.push_back(Glib::ustring::compose("%1[%2]: %3", data->file, data->page, errorMsg));
}

void OutputEditorHOCR::finalizeRead(ReadSessionData* data) {
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	if(!hdata->errors.empty()) {
		Glib::ustring message = Glib::ustring::compose(_("The following pages could not be processed:\n%1"), Utils::string_join(hdata->errors, "\n"));
		Utils::message_dialog(Gtk::MESSAGE_WARNING, _("Recognition errors"), message);
	}
	OutputEditor::finalizeRead(data);
}

void OutputEditorHOCR::addPage(const Glib::ustring& hocrText, HOCRReadSessionData data) {
	xmlpp::DomParser parser;
	parser.parse_memory(hocrText);
	xmlpp::Document* doc = parser.get_document();

	xmlpp::Element* pageDiv = doc->get_root_node();
	if(!pageDiv || pageDiv->get_name() != "div") {
		return;
	}
	std::map<Glib::ustring, Glib::ustring> attrs = HOCRItem::deserializeAttrGroup(pageDiv->get_attribute_value("title"));
	attrs["image"] = Glib::ustring::compose("'%1'", data.file);
	attrs["ppageno"] = Glib::ustring::compose("%1", data.page);
	attrs["rot"] = Glib::ustring::compose("%1", data.angle);
	attrs["res"] = Glib::ustring::compose("%1", data.resolution);
	pageDiv->set_attribute("title", HOCRItem::serializeAttrGroup(attrs));

	Gtk::TreeIter index = m_document->insertPage(data.insertIndex, pageDiv, true);
	expandCollapseChildren(index, true);
	m_treeView->expand_to_path(m_document->get_path(index));
	m_treeView->expand_row(m_document->get_path(index), true);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
}

void OutputEditorHOCR::navigateTargetChanged() {
	Glib::ustring target = (*ui.comboNavigation->get_active())[m_navigationComboCols.itemClass];
	bool allowExpandCollapse = target.substr(0, 9) != "ocrx_word";
	ui.buttonExpandAll->set_sensitive(allowExpandCollapse);
	ui.buttonCollapseAll->set_sensitive(allowExpandCollapse);
}

void OutputEditorHOCR::expandCollapseItemClass(bool expand) {
	Glib::ustring target = (*ui.comboNavigation->get_active())[m_navigationComboCols.itemClass];
	Gtk::TreeIter start = m_document->get_iter("0");
	Gtk::TreeIter next = start;
	do {
		const HOCRItem* item = m_document->itemAtIndex(next);
		if(item && item->itemClass() == target) {
			if(expand) {
				m_treeView->expand_to_path(m_document->get_path(next));
				for(const Gtk::TreeIter& child : next->children()) {
					m_treeView->collapse_row(m_document->get_path(child));
				}
			} else {
				m_treeView->collapse_row(m_document->get_path(next));
			}
		}
		next = m_document->nextIndex(next);
	} while(next != start);
	Gtk::TreeIter it = m_treeView->currentIndex();
	if(it) {
		m_treeView->scroll_to_row(m_document->get_path(it));
	}
}

void OutputEditorHOCR::navigateNextPrev(bool next) {
	Glib::ustring target = (*ui.comboNavigation->get_active())[m_navigationComboCols.itemClass];
	bool misspelled = false;
	if(target == "ocrx_word_bad") {
		target = "ocrx_word";
		misspelled = true;
	}
	Gtk::TreeIter start = m_treeView->currentIndex();
	if(!start) {
		start = m_document->get_iter("0");
	}
	Gtk::TreeIter curr = next ? m_document->nextIndex(start) : m_document->prevIndex(start);
	while(curr != start) {
		const HOCRItem* item = m_document->itemAtIndex(curr);
		if(item && item->itemClass() == target && (!misspelled || m_document->indexIsMisspelledWord(curr))) {
			break;
		}
		curr = next ? m_document->nextIndex(curr) : m_document->prevIndex(curr);
	};
	// Sets current index
	showItemProperties(curr);
}

void OutputEditorHOCR::expandCollapseChildren(const Gtk::TreeIter& index, bool expand) const {
	if(expand) {
		m_treeView->expand_to_path(m_document->get_path(index));
		m_treeView->expand_row(m_document->get_path(index), true);
	} else {
		m_treeView->collapse_row(m_document->get_path(index));
	}
}

bool OutputEditorHOCR::showPage(const HOCRPage* page) {
	return page && MAIN->getSourceManager()->addSource(Gio::File::create_for_path(page->sourceFile()), true) && MAIN->getDisplayer()->setup(&page->pageNr(), &page->resolution(), &page->angle());
}

int OutputEditorHOCR::currentPage() {
	std::vector<Gtk::TreeModel::Path> selected = m_treeView->get_selection()->get_selected_rows();
	if(selected.empty()) {
		return m_document->pageCount();
	}
	Gtk::TreeModel::Path path = selected.front();
	if(path.empty()) {
		return m_document->pageCount();
	}
	return path.front();
}

void OutputEditorHOCR::showItemProperties(const Gtk::TreeIter& index, const Gtk::TreeIter& prev) {
	m_tool->setAction(DisplayerToolHOCR::ACTION_NONE);
	const HOCRItem* prevItem = m_document->itemAtIndex(prev);
	Gtk::TreeIter current = m_treeView->currentIndex();
	if(!current || current != index) {
		m_connectionSelectionChanged.block(true);
		m_treeView->get_selection()->unselect_all();
		if(index) {
			m_treeView->expand_to_path(m_document->get_path(index));
			m_treeView->get_selection()->select(index);
			m_treeView->set_cursor(m_document->get_path(index));
			m_treeView->scroll_to_row(m_document->get_path(index));
		}
		m_connectionSelectionChanged.block(false);
	}
	m_propStore->clear();
	ui.textviewSource->get_buffer()->set_text("");

	const HOCRItem* currentItem = m_document->itemAtIndex(index);
	if(!currentItem) {
		m_tool->clearSelection();
		return;
	}
	const HOCRPage* page = currentItem->page();

	std::map<Glib::ustring, Glib::ustring> attrs = currentItem->getAllAttributes();
	for(auto it = attrs.begin(), itEnd = attrs.end(); it != itEnd; ++it) {
		Glib::ustring attrName = it->first;
		if(attrName == "class" || attrName == "id") {
			continue;
		}
		std::vector<Glib::ustring> parts = Utils::string_split(attrName, ':');
		Gtk::TreeIter item = m_propStore->append();
		item->set_value(m_propStoreCols.attr, attrName);
		item->set_value(m_propStoreCols.name, parts[parts.size() - 1]);
		item->set_value(m_propStoreCols.value, it->second);
		item->set_value(m_propStoreCols.editable, attributeEditable(attrName));
		item->set_value(m_propStoreCols.background, Glib::ustring("#FFF"));
		item->set_value(m_propStoreCols.weight, static_cast<int>(Pango::WEIGHT_NORMAL));
	}

	// ocr_class:attr_key:attr_values
	std::map<Glib::ustring, std::map<Glib::ustring, std::set<Glib::ustring>>> occurrences;
	currentItem->getPropagatableAttributes(occurrences);
	for(auto it = occurrences.begin(), itEnd = occurrences.end(); it != itEnd; ++it) {
		Gtk::TreeIter item = m_propStore->append();
		Glib::ustring itemClass = it->first;
		item->set_value(m_propStoreCols.name, itemClass);
		item->set_value(m_propStoreCols.editable, false);
		item->set_value(m_propStoreCols.background, Glib::ustring("#C0C0C0"));
		item->set_value(m_propStoreCols.weight, static_cast<int>(Pango::WEIGHT_BOLD));
		for(auto attrIt = it->second.begin(), attrItEnd = it->second.end(); attrIt != attrItEnd; ++attrIt) {
			const Glib::ustring& attrName = attrIt->first;
			const std::set<Glib::ustring>& attrValues = attrIt->second;
			int attrValueCount = attrValues.size();
			Gtk::TreeIter item = m_propStore->append();
			std::vector<Glib::ustring> parts = Utils::string_split(attrName, ':');
			item->set_value(m_propStoreCols.attr, attrName);
			item->set_value(m_propStoreCols.name, parts[parts.size() - 1]);
			item->set_value(m_propStoreCols.value, attrValueCount > 1 ? "" : * (attrValues.begin()));
			item->set_value(m_propStoreCols.itemClass, itemClass);
			item->set_value(m_propStoreCols.editable, attributeEditable(attrName));
			item->set_value(m_propStoreCols.placeholder, attrValueCount > 1 ? _("Multiple values") : Glib::ustring());
			item->set_value(m_propStoreCols.background, Glib::ustring("#FFF"));
			item->set_value(m_propStoreCols.weight, static_cast<int>(Pango::WEIGHT_NORMAL));
		}
	}

	ui.textviewSource->get_buffer()->set_text(currentItem->toHtml());

	if(showPage(page)) {
		// Update preview if necessary
		if(!prevItem || prevItem->page() != page) {
			updatePreview();
		}
		// Minimum bounding box
		Geometry::Rectangle minBBox;
		if(currentItem->itemClass() == "ocr_page") {
			minBBox = currentItem->bbox();
		} else {
			for(const HOCRItem* child : currentItem->children()) {
				minBBox = minBBox.unite(child->bbox());
			}
		}
		m_tool->setSelection(currentItem->bbox(), minBBox);
	}
}

Glib::RefPtr<Glib::Regex> OutputEditorHOCR::attributeValidator(const Glib::ustring& attribName) const {
	static std::map<Glib::ustring, Glib::RefPtr<Glib::Regex>> validators = {
		{"title:bbox", Glib::Regex::create("^\\d+\\s+\\d+\\s+\\d+\\s+\\d+$")},
		{"lang", Glib::Regex::create("^[a-z]{2,}(?:_[A-Z]{2,})?$")},
		{"title:x_fsize", Glib::Regex::create("^\\d+$")},
		{"title:baseline", Glib::Regex::create("^[-+]?\\d+\\.?\\d*\\s[-+]?\\d+\\.?\\d*$")}
	};
	auto it = validators.find(attribName);
	if(it == validators.end()) {
		return Glib::RefPtr<Glib::Regex>();
	} else {
		return it->second;
	}
}

bool OutputEditorHOCR::attributeEditable(const Glib::ustring& attribName) const {
	static std::vector<Glib::ustring> editableAttrs = {"title:bbox", "lang", "title:x_fsize", "title:baseline", "title:x_font", "bold", "italic"};
	return std::find(editableAttrs.begin(), editableAttrs.end(), attribName) != editableAttrs.end();
}

void OutputEditorHOCR::editAttribute(const Glib::ustring& path, const Glib::ustring& value) {
	Gtk::TreeIter treeIt = m_treeView->currentIndex();
	Gtk::TreeIter propIt = m_propStore->get_iter(path);
	Glib::ustring attrName = (*propIt)[m_propStoreCols.attr];
	Glib::ustring itemClass = (*propIt)[m_propStoreCols.itemClass];
	Glib::RefPtr<Glib::Regex> validator = attributeValidator(attrName);
	if(treeIt && (!validator || validator->match(value))) {
		m_document->editItemAttribute(treeIt, attrName, value, itemClass);
	}
}

void OutputEditorHOCR::updateCurrentItemBBox(const Geometry::Rectangle& bbox) {
	Glib::ustring bboxstr = Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
	m_document->editItemAttribute(m_treeView->currentIndex(), "title:bbox", bboxstr);
}

void OutputEditorHOCR::updateAttributes(const Gtk::TreeIter& it, const Glib::ustring& attr, const Glib::ustring& value) {
	Gtk::TreeIter current = m_treeView->currentIndex();
	if(current && it == current) {
		for(const Gtk::TreeIter& propIt : m_propStore->children()) {
			if((*propIt)[m_propStoreCols.attr] == attr) {
				(*propIt)[m_propStoreCols.value] = value;
				break;
			}
		}
	}
}

void OutputEditorHOCR::updateSourceText() {
	if(ui.notebook->get_current_page() == 1) {
		const HOCRItem* currentItem = m_document->itemAtIndex(m_treeView->currentIndex());
		if(currentItem) {
			ui.textviewSource->get_buffer()->set_text(currentItem->toHtml());
		}
	}
}

void OutputEditorHOCR::bboxDrawn(const Geometry::Rectangle& bbox, int action) {
	xmlpp::Document doc;
	Gtk::TreeIter current = m_treeView->currentIndex();
	const HOCRItem* currentItem = m_document->itemAtIndex(current);
	if(!currentItem) {
		return;
	}
	std::map<Glib::ustring, std::map<Glib::ustring, std::set<Glib::ustring>>> propAttrs;
	currentItem->getPropagatableAttributes(propAttrs);
	xmlpp::Element* newElement;
	if(action == DisplayerToolHOCR::ACTION_DRAW_GRAPHIC_RECT) {
		newElement = doc.create_root_node("div");
		newElement->set_attribute("class", "ocr_graphic");
		newElement->set_attribute("title", Glib::ustring::compose("bbox %1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height));
	} else if(action == DisplayerToolHOCR::ACTION_DRAW_CAREA_RECT) {
		newElement = doc.create_root_node("div");
		newElement->set_attribute("class", "ocr_carea");
		newElement->set_attribute("title", Glib::ustring::compose("bbox %1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height));
	} else if(action == DisplayerToolHOCR::ACTION_DRAW_PAR_RECT) {
		newElement = doc.create_root_node("p");
		newElement->set_attribute("class", "ocr_par");
		newElement->set_attribute("title", Glib::ustring::compose("bbox %1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height));
	} else if(action == DisplayerToolHOCR::ACTION_DRAW_LINE_RECT) {
		newElement = doc.create_root_node("span");
		newElement->set_attribute("class", "ocr_line");
		std::set<Glib::ustring> propLineBaseline = propAttrs["ocrx_line"]["baseline"];
		// Tesseract does as follows:
		// row_height = x_height + ascenders - descenders
		// font_pt_size = row_height * 72 / dpi (72 = pointsPerInch)
		// As a first approximation, assume x_size = bbox.height() and ascenders = descenders = bbox.height() / 4
		std::map<Glib::ustring, Glib::ustring> titleAttrs;
		titleAttrs["bbox"] = Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
		titleAttrs["x_ascenders"] = Glib::ustring::compose("%1", 0.25 * bbox.height);
		titleAttrs["x_descenders"] = Glib::ustring::compose("%1", 0.25 * bbox.height);
		titleAttrs["x_size"] = Glib::ustring::compose("%1", bbox.height);
		titleAttrs["baseline"] = propLineBaseline.size() == 1 ? *propLineBaseline.begin() : Glib::ustring("0 0");
		newElement->set_attribute("title", HOCRItem::serializeAttrGroup(titleAttrs));
	} else if(action == DisplayerToolHOCR::ACTION_DRAW_WORD_RECT) {
		ui.entryAddWord->set_text("");
		int response = ui.dialogAddWord->run();
		ui.dialogAddWord->hide();
		if(response !=  Gtk::RESPONSE_OK || ui.entryAddWord->get_text().empty()) {
			return;
		}
		newElement = doc.create_root_node("span");
		newElement->set_attribute("class", "ocrx_word");
		std::map<Glib::ustring, std::set<Glib::ustring>> propWord = propAttrs["ocrx_word"];

		newElement->set_attribute("lang", propWord["lang"].size() == 1 ? *propWord["lang"].begin() : m_spell.get_language());
		std::map<Glib::ustring, Glib::ustring> titleAttrs;
		titleAttrs["bbox"] = Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
		titleAttrs["x_font"] = propWord["title:x_font"].size() == 1 ? *propWord["title:x_font"].begin() : "sans";
		titleAttrs["x_fsize"] = propWord["title:x_fsize"].size() == 1 ? *propWord["title:x_fsize"].begin() : Glib::ustring::compose("%1", std::round(bbox.height * 72. / currentItem->page()->resolution()));
		titleAttrs["x_wconf"] = "100";
		newElement->set_attribute("title", HOCRItem::serializeAttrGroup(titleAttrs));
		if (propWord["bold"].size() == 1) { newElement->set_attribute("bold", *propWord["bold"].begin()); }
		if (propWord["italic"].size() == 1) { newElement->set_attribute("italic", *propWord["italic"].begin()); }
		newElement->add_child_text(ui.entryAddWord->get_text());
	} else {
		return;
	}
	Gtk::TreeIter index = m_document->addItem(current, newElement);
	if(index) {
		m_connectionSelectionChanged.block(true);
		m_treeView->get_selection()->unselect_all();
		m_connectionSelectionChanged.block(false);
		m_treeView->expand_to_path(m_document->get_path(index));
		m_treeView->get_selection()->select(index);
		m_treeView->scroll_to_row(m_document->get_path(index));
	}
}

void OutputEditorHOCR::showTreeWidgetContextMenu(GdkEventButton* ev) {
	std::vector<Gtk::TreePath> paths = m_treeView->get_selection()->get_selected_rows();
	int nIndices = paths.size();
	if(nIndices > 1) {
		// Check if merging or swapping is allowed (items are valid siblings)
		const HOCRItem* firstItem = m_document->itemAtIndex(m_document->get_iter(paths.front()));
		if(!firstItem) {
			return;
		}
		std::set<Glib::ustring> classes;
		classes.insert(firstItem->itemClass());
		std::vector<int> rows = {paths.front().back()};
		for(int i = 1; i < nIndices; ++i) {
			const HOCRItem* item = m_document->itemAtIndex(m_document->get_iter(paths[i]));
			if(!item || item->parent() != firstItem->parent()) {
				return;
			}
			classes.insert(item->itemClass());
			rows.push_back(paths[i].back());
		}
		std::sort(rows.begin(), rows.end());
		bool consecutive = (rows.back() - rows.front()) == nIndices - 1;
		bool graphics = firstItem->itemClass() == "ocr_graphic";
		bool pages = firstItem->itemClass() == "ocr_page";
		bool sameClass = classes.size() == 1;

		Gtk::Menu menu;
		Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
		if(consecutive && !graphics && !pages && sameClass) { // Merging allowed
			Gtk::MenuItem* mergeItem = Gtk::manage(new Gtk::MenuItem(_("Merge")));
			menu.append(*mergeItem);
			CONNECT(mergeItem, activate, [&] {
				Gtk::TreeIter newIndex = m_document->mergeItems(m_document->get_iter(paths.front())->parent(), rows.front(), rows.back());
				m_treeView->get_selection()->unselect_all();
				m_treeView->get_selection()->select(newIndex);
			});
			if(firstItem->itemClass() != "ocr_carea") {
				Gtk::MenuItem* splitItem = Gtk::manage(new Gtk::MenuItem(_("Split from parent")));
				menu.append(*splitItem);
				CONNECT(splitItem, activate, [&] {
					Gtk::TreeIter newIndex = m_document->splitItem(m_document->get_iter(paths.front())->parent(), rows.front(), rows.back());
					m_treeView->get_selection()->unselect_all();
					m_treeView->get_selection()->select(newIndex);
					expandCollapseChildren(newIndex, true);
				});
			}
		}
		if(nIndices == 2) { // Swapping allowed
			Gtk::MenuItem* swapItem = Gtk::manage(new Gtk::MenuItem(_("Swap")));
			menu.append(*swapItem);
			CONNECT(swapItem, activate, [&] {
				Gtk::TreeIter newIndex = m_document->swapItems(m_document->get_iter(paths.front())->parent(), rows.front(), rows.back());
				m_treeView->get_selection()->unselect_all();
				m_treeView->get_selection()->select(newIndex);
			});
		}
		CONNECT(&menu, hide, [&] { loop->quit(); });
		menu.show_all();
		menu.popup(ev->button, ev->time);
		loop->run();
		// Nothing else is allowed with multiple items selected
		return;
	}
	Gtk::TreeIter index = m_treeView->indexAtPos(ev->x, ev->y);
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(!item) {
		return;
	}

	Gtk::Menu menu;
	Glib::ustring itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		Gtk::MenuItem* addGraphicItem = Gtk::manage(new Gtk::MenuItem(_("Add graphic region")));
		menu.append(*addGraphicItem);
		CONNECT(addGraphicItem, activate, [this] { m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_GRAPHIC_RECT); });
		Gtk::MenuItem* addTextBlockItem = Gtk::manage(new Gtk::MenuItem(_("Add text block")));
		menu.append(*addTextBlockItem);
		CONNECT(addTextBlockItem, activate, [this] { m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_CAREA_RECT); });
	} else if(itemClass == "ocr_carea") {
		Gtk::MenuItem* addParagraphItem = Gtk::manage(new Gtk::MenuItem(_("Add paragraph")));
		menu.append(*addParagraphItem);
		CONNECT(addParagraphItem, activate, [this] { m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_PAR_RECT); });
	} else if(itemClass == "ocr_par") {
		Gtk::MenuItem* addLineItem = Gtk::manage(new Gtk::MenuItem(_("Add line")));
		menu.append(*addLineItem);
		CONNECT(addLineItem, activate, [this] { m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_LINE_RECT); });
	} else if(itemClass == "ocr_line") {
		Gtk::MenuItem* addWordItem = Gtk::manage(new Gtk::MenuItem(_("Add word")));
		menu.append(*addWordItem);
		CONNECT(addWordItem, activate, [this] { m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_WORD_RECT); });
	} else if(itemClass == "ocrx_word") {
		Glib::ustring prefix, suffix, trimmedWord = HOCRItem::trimmedWord(item->text(), &prefix, &suffix);
		std::vector<Glib::ustring> suggestions;
		bool valid = m_document->checkItemSpelling(index, &suggestions, 16);
		for(const Glib::ustring& suggestion : suggestions) {
			Glib::ustring replacement = prefix + suggestion + suffix;
			Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(replacement));
			CONNECT(item, activate, [this, replacement, index] { m_document->editItemText(index, replacement); });
			menu.append(*item);
		}

		if(!trimmedWord.empty()) {
			if(menu.get_children().empty()) {
				Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(_("No suggestions")));
				item->set_sensitive(false);
				menu.append(*item);
			}
			if(!valid) {
				menu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
				Gtk::MenuItem* additem = Gtk::manage(new Gtk::MenuItem(_("Add to dictionary")));
				CONNECT(additem, activate, [this, index, trimmedWord] {
					m_spell.add_to_dictionary(trimmedWord);
					m_document->recheckSpelling();
				});
				menu.append(*additem);
				Gtk::MenuItem* ignoreitem = Gtk::manage(new Gtk::MenuItem(_("Ignore word")));
				CONNECT(ignoreitem, activate, [this, index, trimmedWord] {
					m_spell.ignore_word(trimmedWord);
					m_document->recheckSpelling();
				});
				menu.append(*ignoreitem);
			}
		}
	}
	if(!menu.get_children().empty()) {
		menu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
	}
	if(itemClass == "ocr_par" || itemClass == "ocr_line" || itemClass == "ocrx_word") {
		Gtk::MenuItem* splitItem = Gtk::manage(new Gtk::MenuItem(_("Split from parent")));
		CONNECT(splitItem, activate, [this, index, item] {
			Gtk::TreeIter newIndex = m_document->splitItem(index->parent(), item->index(), item->index());
			m_treeView->get_selection()->unselect_all();
			m_treeView->get_selection()->select(newIndex);
			expandCollapseChildren(newIndex, true);
		});
		menu.append(*splitItem);
	}
	Gtk::MenuItem* removeItem = Gtk::manage(new Gtk::MenuItem(_("Remove")));
	CONNECT(removeItem, activate, [this, index] { m_document->removeItem(index); });
	menu.append(*removeItem);
	if(!index->children().empty()) {
		Gtk::MenuItem* expandItem = Gtk::manage(new Gtk::MenuItem(_("Expand all")));
		CONNECT(expandItem, activate, [this, index] { expandCollapseChildren(index, true); });
		menu.append(*expandItem);
		Gtk::MenuItem* collapseItem = Gtk::manage(new Gtk::MenuItem(_("Collapse all")));
		CONNECT(collapseItem, activate, [this, index] { expandCollapseChildren(index, false); });
		menu.append(*collapseItem);
	}
	Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
	CONNECT(&menu, hide, [&] { loop->quit(); });
	menu.show_all();
	menu.popup(ev->button, ev->time);
	loop->run();
}

void OutputEditorHOCR::pickItem(const Geometry::Point& point) {
	int pageNr;
	Glib::ustring filename = MAIN->getDisplayer()->getCurrentImage(pageNr);
	Gtk::TreeIter pageIndex = m_document->searchPage(filename, pageNr);
	const HOCRItem* currentItem = m_document->itemAtIndex(pageIndex);
	if(!currentItem) {
		return;
	}
	const HOCRPage* page = currentItem->page();
	// Transform point in coordinate space used when page was OCRed
	double alpha = (page->angle() - MAIN->getDisplayer()->getCurrentAngle()) / 180. * M_PI;
	double scale = double(page->resolution()) / double(MAIN->getDisplayer()->getCurrentResolution());
	Geometry::Point newPoint( scale * (point.x * std::cos(alpha) - point.y * std::sin(alpha)) + 0.5 * page->bbox().width,
	                          scale * (point.x * std::sin(alpha) + point.y * std::cos(alpha)) + 0.5 * page->bbox().height);
	showItemProperties(m_document->searchAtCanvasPos(pageIndex, newPoint));
	m_treeView->grab_focus();
}

void OutputEditorHOCR::open(InsertMode mode, std::vector<Glib::RefPtr<Gio::File>> files) {
	if(mode == InsertMode::Replace && !clear(false)) {
		return;
	}
	if(files.empty()) {
		FileDialogs::FileFilter filter = {_("hOCR HTML Files"), {"text/html", "text/xml", "text/plain"}, {"*.html"}};
		files = FileDialogs::open_dialog(_("Open hOCR File"), "", "outputdir", filter, true);
	}
	if(files.empty()) {
		return;
	}
	std::vector<Glib::ustring> failed;
	std::vector<Glib::ustring> invalid;
	int added = 0;
	for(const Glib::RefPtr<Gio::File>& file : files) {
		std::string filename = file->get_path();
		std::string source;
		try {
			source = Glib::file_get_contents(filename);
		} catch(const Glib::Error&) {
			failed.push_back(filename);
			continue;
		}

		xmlpp::DomParser parser;
		try {
			parser.parse_memory(source);
		} catch(const xmlpp::exception&) {
			failed.push_back(filename);
			continue;
		}

		xmlpp::Document* doc = parser.get_document();
		const xmlpp::Element* div = XmlUtils::firstChildElement(XmlUtils::firstChildElement(doc->get_root_node(), "body"), "div");
		if(!div || div->get_attribute_value("class") != "ocr_page") {
			invalid.push_back(filename);
			continue;
		}
		int pos = mode == InsertMode::InsertBefore ? currentPage() : m_document->pageCount();
		while(div) {
			m_document->insertPage(pos++, div, false, Glib::path_get_dirname(filename));
			div = XmlUtils::nextSiblingElement(div, "div");
			++added;
		}
	}
	if(added > 0) {
		m_modified = mode != InsertMode::Replace;
		if(mode == InsertMode::Replace && m_filebasename.empty()) {
			m_filebasename = Utils::split_filename(files.front()->get_path()).first;
		}
	}
	std::vector<Glib::ustring> errorMsg;
	if(!failed.empty()) {
		errorMsg.push_back(Glib::ustring::compose(_("The following files could not be opened:\n%1"), Utils::string_join(failed, "\n")));
	}
	if(!invalid.empty()) {
		errorMsg.push_back(Glib::ustring::compose(_("The following files are not valid hOCR HTML:\n%1"), Utils::string_join(invalid, "\n")));
	}
	if(!errorMsg.empty()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Unable to open files"), Utils::string_join(errorMsg, "\n\n"));
	}
}

bool OutputEditorHOCR::save(const std::string& filename) {
	Glib::ustring outname = filename;
	if(outname.empty()) {
		Glib::ustring suggestion = m_filebasename;
		if(suggestion.empty()) {
			std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
			suggestion = !sources.empty() ? Utils::split_filename(sources.front()->displayname).first : _("output");
		}
		FileDialogs::FileFilter filter = {_("hOCR HTML Files"), {"text/html"}, {"*.html"}};
		outname = FileDialogs::save_dialog(_("Save hOCR Output..."), suggestion + ".html", "outputdir", filter);
		if(outname.empty()) {
			return false;
		}
	}
	std::ofstream file(outname);
	if(!file.is_open()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	std::string current = setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "C");
	tesseract::TessBaseAPI tess;
	setlocale(LC_ALL, current.c_str());
	Glib::ustring header = Glib::ustring::compose(
	                           "<!DOCTYPE html>\n"
	                           "<html>\n"
	                           "<head>\n"
	                           " <title>%1</title>\n"
	                           " <meta charset=\"utf-8\" /> \n"
	                           " <meta name='ocr-system' content='tesseract %2' />\n"
	                           " <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
	                           "</head>\n", Glib::path_get_basename(outname), tess.Version());
	m_document->convertSourcePaths(Glib::path_get_dirname(outname), false);
	Glib::ustring body = m_document->toHTML();
	m_document->convertSourcePaths(Glib::path_get_dirname(outname), true);
	Glib::ustring footer = "</html>\n";
	file.write(header.data(), header.bytes());
	file.write(body.data(), body.bytes());
	file.write(footer.data(), footer.bytes());
	m_modified = false;
	m_filebasename = Utils::split_filename(filename).first;
	return true;
}

bool OutputEditorHOCR::exportToODT() {
	if(m_document->pageCount() == 0) {
		return false;
	}
	MAIN->getDisplayer()->setBlockAutoscale(true);
	bool success = HOCROdtExporter(m_tool).run(m_document, m_filebasename);
	MAIN->getDisplayer()->setBlockAutoscale(false);
	return success;
}

bool OutputEditorHOCR::exportToPDF() {
	if(m_document->pageCount() == 0) {
		return false;
	}
	ui.buttonPreview->set_active(false); // Disable preview because if conflicts with preview from PDF dialog
	const HOCRItem* item = m_document->itemAtIndex(m_treeView->currentIndex());
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	bool success = false;
	if(showPage(page)) {
		MAIN->getDisplayer()->setBlockAutoscale(true);
		success = HOCRPdfExporter(m_document, page, m_tool).run(m_filebasename);
		MAIN->getDisplayer()->setBlockAutoscale(false);
	}
	return success;
}

bool OutputEditorHOCR::exportToText() {
	if(m_document->pageCount() == 0) {
		return false;
	}
	return HOCRTextExporter().run(m_document, m_filebasename);
}

bool OutputEditorHOCR::clear(bool hide) {
	m_connectionPreviewTimer.disconnect();
	m_preview->setVisible(false);
	if(!ui.boxEditorHOCR->get_visible()) {
		return true;
	}
	if(getModified()) {
		int response = Utils::question_dialog(_("Output not saved"), _("Save output before proceeding?"), Utils::Button::Save | Utils::Button::Discard | Utils::Button::Cancel);
		if(response == Utils::Button::Save) {
			if(!save()) {
				return false;
			}
		} else if(response != Utils::Button::Discard) {
			return false;
		}
	}
	m_document = Glib::RefPtr<HOCRDocument>(new HOCRDocument(&m_spell));
	CONNECTX(m_document, row_inserted, [this](const Gtk::TreePath&, const Gtk::TreeIter&) {
		setModified();
	});
	CONNECTX(m_document, row_deleted, [this](const Gtk::TreePath&) {
		setModified();
	});
	CONNECTX(m_document, row_changed, [this](const Gtk::TreePath&, const Gtk::TreeIter&) {
		setModified();
		updateSourceText();
	});
	CONNECTX(m_document, item_attribute_changed, [this](const Gtk::TreeIter&, const Glib::ustring&, const Glib::ustring&) {
		setModified();
	});
	CONNECTX(m_document, item_attribute_changed, [this](const Gtk::TreeIter & it, const Glib::ustring & attr, const Glib::ustring & value) {
		updateAttributes(it, attr, value);
		updateSourceText();
	});
	m_treeView->set_model(m_document);
	ui.textviewSource->get_buffer()->set_text("");
	m_tool->clearSelection();
	m_modified = false;
	ui.buttonSave->set_sensitive(false);
	ui.buttonExport->set_sensitive(false);
	ui.boxNavigation->set_sensitive(false);
	m_filebasename.clear();
	if(hide) {
		MAIN->setOutputPaneVisible(false);
	}
	return true;
}

void OutputEditorHOCR::setLanguage(const Config::Lang& lang) {
	m_document->setDefaultLanguage(lang.code);
}

void OutputEditorHOCR::onVisibilityChanged(bool /*visible*/) {
	m_searchFrame->hideSubstitutionsManager();
}

bool OutputEditorHOCR::findReplaceInItem(const Gtk::TreeIter& index, const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase, bool backwards, bool replace, bool& currentSelectionMatchesSearch) {
	// Check that the item is a word
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(!item || item->itemClass() != "ocrx_word") {
		return false;
	}
	Gtk::TreePath path = m_document->get_path(index);
	// If the item is already in edit mode, continue searching inside the text
	if(m_treeViewTextCell->get_entry() && m_treeViewTextCell->get_current_path() == path.to_string()) {
		Gtk::Entry* editor = m_treeViewTextCell->get_entry();
		Glib::ustring text = editor->get_text();
		int start, end;
		bool hasSel = editor->get_selection_bounds(start, end);
		bool matchesSearch = hasSel && Utils::strings_equal(text.substr(start, end - start), searchstr, matchCase);
		if(matchesSearch && replace) {
			editor->set_text(text.substr(0, start) + replacestr + text.substr(end));
			editor->select_region(start, start + replacestr.length());
			return true;
		}
		bool matchesReplace = hasSel && Utils::strings_equal(text.substr(start, end - start), replacestr, matchCase);
		int pos = -1;
		if(backwards) {
			pos = start - 1;
			pos = pos < 0 ? -1 : Utils::string_lastIndex(text, searchstr, pos, matchCase);
		} else {
			pos = matchesSearch ? start + searchstr.length() : matchesReplace ? start + replacestr.length() : start;
			pos = Utils::string_firstIndex(text, searchstr, pos, matchCase);
		}
		if(pos != -1) {
			editor->select_region(pos, pos + searchstr.length());
			return true;
		}
		currentSelectionMatchesSearch = matchesSearch;
		return false;
	}
	// Otherwise, if item contains text, set it in edit mode
	int pos = backwards ? Utils::string_lastIndex(item->text(), searchstr, -1, matchCase) : Utils::string_firstIndex(item->text(), searchstr, 0, matchCase);
	if(pos != -1) {
		if(m_treeViewTextCell->get_entry()) {
			m_treeViewTextCell->get_entry()->editing_done(); // Commit previous changes
		}
		m_treeView->get_selection()->unselect_all();
		m_treeView->get_selection()->select(path);
		m_treeView->set_cursor(path, *m_treeView->get_column(0), *m_treeViewTextCell, true);
		g_assert(m_treeViewTextCell->get_current_path() == path.to_string() && m_treeViewTextCell->get_entry());
		m_treeViewTextCell->get_entry()->select_region(pos, pos + searchstr.length());
		return true;
	}
	return false;
}

void OutputEditorHOCR::findReplace(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase, bool backwards, bool replace) {
	m_searchFrame->clearErrorState();
	Gtk::TreeIter current = m_treeView->currentIndex();
	if(!current) {
		current = m_document->get_iter(m_document->get_root_path(backwards ? (m_document->pageCount() - 1) : 0));
	}
	Gtk::TreeIter neww = current;
	bool currentSelectionMatchesSearch = false;
	while(!findReplaceInItem(neww, searchstr, replacestr, matchCase, backwards, replace, currentSelectionMatchesSearch)) {
		neww = backwards ? m_document->prevIndex(neww) : m_document->nextIndex(neww);
		if(!neww || neww == current) {
			// Break endless loop
			if(!currentSelectionMatchesSearch) {
				m_searchFrame->setErrorState();
			}
			return;
		}
	}
}

void OutputEditorHOCR::replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	Gtk::TreeIter start = m_document->get_iter(m_document->get_root_path(0));
	Gtk::TreeIter curr = start;
	int count = 0;
	do {
		const HOCRItem* item = m_document->itemAtIndex(curr);
		if(item && item->itemClass() == "ocrx_word") {
			Glib::ustring text = item->text();
			if(Utils::string_replace(text, searchstr, replacestr, matchCase)) {
				++count;
				m_document->editItemText(curr, text);
			}
		}
		curr = m_document->nextIndex(curr);
		while(Gtk::Main::events_pending()) {
			Gtk::Main::iteration();
		}
	} while(curr != start);
	if(count == 0) {
		m_searchFrame->setErrorState();
	}
	MAIN->popState();
}

void OutputEditorHOCR::applySubstitutions(const std::map<Glib::ustring, Glib::ustring>& substitutions, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	Gtk::TreeIter start = m_document->get_iter(m_document->get_root_path(0));
	for(auto it = substitutions.begin(), itEnd = substitutions.end(); it != itEnd; ++it) {
		Glib::ustring search = it->first;
		Glib::ustring replace = it->second;
		Gtk::TreeIter curr = start;
		do {
			const HOCRItem* item = m_document->itemAtIndex(curr);
			if(item && item->itemClass() == "ocrx_word") {
				Glib::ustring text = item->text();
				if(Utils::string_replace(text, search, replace, matchCase)) {
					m_document->editItemText(curr, text);
				}
			}
			curr = m_document->nextIndex(curr);
			while(Gtk::Main::events_pending()) {
				Gtk::Main::iteration();
			}
		} while(curr != start);
	}
	MAIN->popState();
}

void OutputEditorHOCR::updatePreview() {
	const HOCRItem* item = m_document->itemAtIndex(m_treeView->currentIndex());
	if(!ui.buttonPreview->get_active() || !item) {
		m_preview->setVisible(false);
		return;
	}

	const HOCRPage* page = item->page();
	const Geometry::Rectangle& bbox = page->bbox();
	int pageDpi = page->resolution();

	Cairo::RefPtr<Cairo::ImageSurface> image = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, bbox.width, bbox.height);
	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(image);
	context->save();
	context->rectangle(0, 0, image->get_width(), image->get_height());
	context->set_source_rgba(1., 1., 1., .5);
	context->fill();
	context->restore();

	drawPreview(context, page);

	m_preview->setImage(image);
	m_preview->setRect(Geometry::Rectangle(-0.5 * image->get_width(), -0.5 * image->get_height(), image->get_width(), image->get_height()));
	m_preview->setVisible(true);
}

void OutputEditorHOCR::drawPreview(Cairo::RefPtr<Cairo::Context> context, const HOCRItem* item) {
	if(!item->isEnabled()) {
		return;
	}
	Glib::ustring itemClass = item->itemClass();
	if(itemClass == "ocr_line") {
		std::pair<double, double> baseline = item->baseLine();
		const Geometry::Rectangle& lineRect = item->bbox();
		for(HOCRItem* wordItem : item->children()) {
			if(!wordItem->isEnabled()) {
				continue;
			}
			const Geometry::Rectangle& wordRect = wordItem->bbox();
			context->select_font_face(wordItem->fontFamily().empty() ? "sans" : wordItem->fontFamily(), wordItem->fontItalic() ? Cairo::FONT_SLANT_ITALIC : Cairo::FONT_SLANT_NORMAL, wordItem->fontBold() ? Cairo::FONT_WEIGHT_BOLD : Cairo::FONT_WEIGHT_NORMAL);
			context->set_font_size(wordItem->fontSize() * (item->page()->resolution() / 72.));
			// See https://github.com/kba/hocr-spec/issues/15
			double y = lineRect.y + lineRect.height + (wordRect.x + 0.5 * wordRect.width - lineRect.x) * baseline.first + baseline.second;
			context->move_to(wordRect.x, y);
			context->show_text(wordItem->text());
		}
	} else if(itemClass == "ocr_graphic") {
		const Geometry::Rectangle& bbox = item->bbox();
		context->save();
		context->move_to(bbox.x, bbox.y);
		context->set_source(m_tool->getSelection(bbox), bbox.x, bbox.y);
		context->paint();
		context->restore();
	} else {
		for(HOCRItem* childItem : item->children()) {
			drawPreview(context, childItem);;
		}
	}
}

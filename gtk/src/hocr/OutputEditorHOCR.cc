/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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

#include "DisplayerToolHOCR.hh"
#include "FileDialogs.hh"
#include "HOCRDocument.hh"
#include "HOCRPdfExporter.hh"
#include "HOCRTextExporter.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SearchReplaceFrame.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include "XmlUtils.hh"


class OutputEditorHOCR::HOCRCellRendererText : public Gtk::CellRendererText
{
public:
	HOCRCellRendererText() : Glib::ObjectBase("CellRendererTextHOCR") {}
	Gtk::Entry* get_entry() const{ return m_entry; }
	const Glib::ustring& get_current_path() const{ return m_path; }

protected:
	Gtk::Entry* m_entry = nullptr;
	Glib::ustring m_path;
	Gtk::CellEditable* start_editing_vfunc(GdkEvent* /*event*/, Gtk::Widget& /*widget*/, const Glib::ustring& path, const Gdk::Rectangle& /*background_area*/, const Gdk::Rectangle& /*cell_area*/, Gtk::CellRendererState /*flags*/) override
	{
		if(!property_editable()) {
			return nullptr;
		}
		m_entry = new Gtk::Entry;
		m_entry->set_text(property_text());
		m_entry->set_has_frame(false);
		m_entry->signal_editing_done().connect([=] {
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

	Gtk::CellEditable* start_editing_vfunc(GdkEvent* /*event*/, Gtk::Widget& /*widget*/, const Glib::ustring& path, const Gdk::Rectangle& /*background_area*/, const Gdk::Rectangle& /*cell_area*/, Gtk::CellRendererState /*flags*/) override
	{
		if(!property_editable()) {
			return nullptr;
		}
		Gtk::TreeIter it = m_propStore->get_iter(path);
		if(it && (*it)[m_cols.attr] == "title:x_font") {
			FontComboBox* combo = new FontComboBox();
			Glib::ustring curFont = property_text();
			combo->set_active_font(property_text());
			combo->signal_editing_done().connect([=] {
				Glib::ustring newFont = combo->get_active_font();
				if(!combo->property_editing_canceled() && newFont != curFont) {
					edited(path, combo->get_active_font());
				}
			});
			combo->show();
			return Gtk::manage(combo);
		} else {
			Gtk::Entry* entry = new Gtk::Entry;
			entry->set_text(property_text());
			entry->set_has_frame(false);
			entry->signal_editing_done().connect([=] {
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
			: Gtk::TreeView(cobject) {}

	sigc::signal<void, GdkEventButton*> signal_context_menu() { return m_signal_context_menu; }

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
	bool on_button_press_event(GdkEventButton *button_event) {
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

private:
	sigc::signal<void, GdkEventButton*> m_signal_context_menu;
};

///////////////////////////////////////////////////////////////////////////////

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool)
{
	ui.builder->get_widget_derived("treeviewItems", m_treeView);
	ui.setupUi();

	m_tool = tool;

	m_document = Glib::RefPtr<HOCRDocument>(new HOCRDocument(&m_spell));

	// HOCR tree view
	m_treeView->set_model(m_document);
	m_treeView->get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	// First column: [checkbox][icon][text]
	m_treeViewTextCell = Gtk::manage(new HOCRCellRendererText);
	Gtk::TreeViewColumn *itemViewCol1 = Gtk::manage(new Gtk::TreeViewColumn(""));
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
	CONNECT(&checkRenderer, toggled, [this](const Glib::ustring& path) {
		Gtk::TreeIter it = m_document->get_iter(path);
		bool prev;
		it->get_value<bool>(HOCRDocument::COLUMN_CHECKED, prev);
		it->set_value(HOCRDocument::COLUMN_CHECKED, !prev);
	});
	CONNECT(&textRenderer, edited, [this](const Glib::ustring& path, const Glib::ustring& text) {
		m_document->get_iter(path)->set_value(HOCRDocument::COLUMN_TEXT, text);
	});
	m_treeView->append_column(*itemViewCol1);
	m_treeView->set_expander_column(*itemViewCol1);
	// Second column: wconf, hidden by default
	Gtk::TreeViewColumn *itemViewCol2 = Gtk::manage(new Gtk::TreeViewColumn(""));
	Gtk::CellRendererText& wconfRenderer = *Gtk::manage(new Gtk::CellRendererText);
	itemViewCol2->pack_start(wconfRenderer, true);
	itemViewCol2->add_attribute(wconfRenderer, "text", HOCRDocument::COLUMN_WCONF);
	m_treeView->append_column(*itemViewCol2);
	itemViewCol2->set_visible(false);
	itemViewCol2->set_fixed_width(32);

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

	ui.buttonSave->set_sensitive(false);
	ui.buttonExport->set_sensitive(false);

	CONNECT(ui.buttonOpen, clicked, [this]{ open(); });
	CONNECT(ui.buttonSave, clicked, [this]{ save(); });
	CONNECT(ui.menuitemExportText, activate, [this]{ exportToText(); });
	CONNECT(ui.menuitemExportPdf, activate, [this]{ exportToPDF(); });
	CONNECT(ui.buttonClear, clicked, [this]{ clear(); });
	CONNECT(ui.buttonFindreplace, toggled, [this]{ m_searchFrame->clear(); m_searchFrame->getWidget()->set_visible(ui.buttonFindreplace->get_active()); });
	CONNECT(ui.buttonWconf, toggled, [this]{ m_treeView->get_column(1)->set_visible(ui.buttonWconf->get_active());});
	CONNECT(m_treeView, context_menu, [this](GdkEventButton* ev){ showTreeWidgetContextMenu(ev); });
	CONNECT(m_searchFrame, find_replace, sigc::mem_fun(this, &OutputEditorHOCR::findReplace));
	CONNECT(m_searchFrame, replace_all, sigc::mem_fun(this, &OutputEditorHOCR::replaceAll));
	CONNECT(m_searchFrame, apply_substitutions, sigc::mem_fun(this, &OutputEditorHOCR::applySubstitutions));
	m_connectionCustomFont = CONNECT(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont"), changed, [this] { setFont(); });
	m_connectionDefaultFont = CONNECT(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont"), changed, [this] { setFont(); });
	m_connectionSelectionChanged = CONNECT(m_treeView->get_selection(), changed, [this] { showItemProperties(m_treeView->currentIndex()); });
	CONNECT(ui.notebook, switch_page, [this](Gtk::Widget*, guint){ updateSourceText(); });
	CONNECT(m_tool, bbox_changed, sigc::mem_fun(this, &OutputEditorHOCR::updateCurrentItemBBox));
	CONNECT(m_tool, bbox_drawn, sigc::mem_fun(this, &OutputEditorHOCR::addGraphicRegion));
	CONNECT(m_tool, position_picked, sigc::mem_fun(this, &OutputEditorHOCR::pickItem));
	CONNECTX(m_document, row_inserted, [this](const Gtk::TreePath&, const Gtk::TreeIter&){ setModified(); });
	CONNECTX(m_document, row_deleted, [this](const Gtk::TreePath&){ setModified(); });
	CONNECTX(m_document, row_changed, [this](const Gtk::TreePath&, const Gtk::TreeIter&){ setModified(); updateSourceText(); });
	CONNECTX(m_document, item_attribute_changed, [this](const Gtk::TreeIter&, const Glib::ustring&, const Glib::ustring&){ setModified(); });
	CONNECTX(m_document, item_attribute_changed, [this](const Gtk::TreeIter& it, const Glib::ustring& attr, const Glib::ustring& value){ updateAttributes(it, attr, value); updateSourceText(); });

	setFont();
}

OutputEditorHOCR::~OutputEditorHOCR() {
	delete m_searchFrame;
	m_connectionCustomFont.disconnect();
	m_connectionDefaultFont.disconnect();
}

void OutputEditorHOCR::setFont() {
	if(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont")->getValue()) {
		ui.textviewSource->unset_font();
	} else {
		Glib::ustring fontName = MAIN->getConfig()->getSetting<FontSetting>("customoutputfont")->getValue();
		ui.textviewSource->override_font(Pango::FontDescription(fontName));
	}
}

void OutputEditorHOCR::setModified() {
	ui.buttonSave->set_sensitive(m_document->pageCount() > 0);
	ui.buttonExport->set_sensitive(m_document->pageCount() > 0);
	m_modified = true;
}

OutputEditorHOCR::ReadSessionData* OutputEditorHOCR::initRead(tesseract::TessBaseAPI &tess) {
	tess.SetPageSegMode(tesseract::PSM_AUTO_ONLY);
	return new HOCRReadSessionData;
}

void OutputEditorHOCR::read(tesseract::TessBaseAPI &tess, ReadSessionData *data) {
	tess.SetVariable("hocr_font_info", "true");
	char* text = tess.GetHOCRText(data->page);
	Utils::runInMainThreadBlocking([&] { addPage(text, *data); });
	delete[] text;
}

void OutputEditorHOCR::readError(const Glib::ustring& errorMsg, ReadSessionData *data) {
	static_cast<HOCRReadSessionData*>(data)->errors.push_back(Glib::ustring::compose("%1[%2]: %3", data->file, data->page, errorMsg));
}

void OutputEditorHOCR::finalizeRead(ReadSessionData *data) {
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	if(!hdata->errors.empty()) {
		Glib::ustring message = Glib::ustring::compose(_("The following pages could not be processed:\n%1"), Utils::string_join(hdata->errors, "\n"));
		Utils::message_dialog(Gtk::MESSAGE_WARNING, _("Recognition errors"), message);
	}
	OutputEditor::finalizeRead(data);
}

void OutputEditorHOCR::addPage(const Glib::ustring& hocrText, ReadSessionData data) {
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

	Gtk::TreeIter index = m_document->addPage(pageDiv, true);
	expandCollapseChildren(index, true);
	m_treeView->expand_to_path(m_document->get_path(index));
	m_treeView->expand_row(m_document->get_path(index), true);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
}

void OutputEditorHOCR::expandCollapseChildren(const Gtk::TreeIter& index, bool expand) const {
	if(expand) {
		m_treeView->expand_to_path(m_document->get_path(index));
		m_treeView->expand_row(m_document->get_path(index), true);
	} else {
		m_treeView->collapse_row(m_document->get_path(index));
	}
}

bool OutputEditorHOCR::showPage(const HOCRPage *page)
{
	return page && MAIN->getSourceManager()->addSource(Gio::File::create_for_path(page->sourceFile())) && MAIN->getDisplayer()->setup(&page->pageNr(), &page->resolution(), &page->angle());
}

void OutputEditorHOCR::showItemProperties(const Gtk::TreeIter& index) {
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
	showPage(page);

	std::map<Glib::ustring, Glib::ustring> attrs = currentItem->getAllAttributes();
	for(auto it = attrs.begin(), itEnd = attrs.end(); it != itEnd; ++it) {
		Glib::ustring attrName = it->first;
		if(attrName == "class" || attrName == "id"){
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
	std::map<Glib::ustring, std::map<Glib::ustring, std::set<Glib::ustring>>> occurences;
	currentItem->getPropagatableAttributes(occurences);
	for(auto it = occurences.begin(), itEnd = occurences.end(); it != itEnd; ++it) {
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
			item->set_value(m_propStoreCols.value, attrValueCount > 1 ? "" : *(attrValues.begin()));
			item->set_value(m_propStoreCols.itemClass, itemClass);
			item->set_value(m_propStoreCols.editable, attributeEditable(attrName));
			item->set_value(m_propStoreCols.placeholder, attrValueCount > 1 ?_("Multiple values") : Glib::ustring());
			item->set_value(m_propStoreCols.background, Glib::ustring("#FFF"));
			item->set_value(m_propStoreCols.weight, static_cast<int>(Pango::WEIGHT_NORMAL));
		}
	}

	ui.textviewSource->get_buffer()->set_text(currentItem->toHtml());

	m_tool->setSelection(currentItem->bbox());
}

Glib::RefPtr<Glib::Regex> OutputEditorHOCR::attributeValidator(const Glib::ustring& attribName) const
{
	static std::map<Glib::ustring, Glib::RefPtr<Glib::Regex>> validators = {
		{"title:bbox", Glib::Regex::create("^\\d+\\s+\\d+\\s+\\d+\\s+\\d+$")},
		{"lang", Glib::Regex::create("^[a-z]{2}(?:_[A-Z]{2})?$")},
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

bool OutputEditorHOCR::attributeEditable(const Glib::ustring& attribName) const
{
	static std::vector<Glib::ustring> editableAttrs = {"title:bbox", "lang", "title:x_fsize", "title:baseline", "title:x_font"};
	return std::find(editableAttrs.begin(), editableAttrs.end(), attribName) != editableAttrs.end();
}

void OutputEditorHOCR::editAttribute(const Glib::ustring &path, const Glib::ustring &value)
{
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

void OutputEditorHOCR::updateAttributes(const Gtk::TreeIter& it, const Glib::ustring& attr, const Glib::ustring& value)
{
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

void OutputEditorHOCR::addGraphicRegion(const Geometry::Rectangle& bbox) {
	Gtk::TreeIter current = m_treeView->currentIndex();
	xmlpp::Element* graphicElement = XmlUtils::createElement("div");
	graphicElement->set_attribute("class", "ocr_graphic");
	graphicElement->set_attribute("title", Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height));
	Gtk::TreeIter index = m_document->addItem(current, graphicElement);
	if(index) {
		m_connectionSelectionChanged.block(true);
		m_treeView->get_selection()->unselect_all();
		m_connectionSelectionChanged.block(false);
		m_treeView->get_selection()->select(index);
		m_treeView->scroll_to_row(m_document->get_path(index));
	}
}

void OutputEditorHOCR::showTreeWidgetContextMenu(GdkEventButton* ev) {
	std::vector<Gtk::TreePath> paths = m_treeView->get_selection()->get_selected_rows();
	int nIndices = paths.size();
	if(nIndices > 1) {
		// Check if item merging is allowed (no pages, all items with same parent and consecutive)
		const HOCRItem* firstItem = m_document->itemAtIndex(m_document->get_iter(paths.front()));
		bool ok = firstItem;
		std::set<Glib::ustring> classes;
		if(firstItem) {
			classes.insert(firstItem->itemClass());
		}
		std::vector<int> rows = {paths.front().back()};
		for(int i = 1; i < nIndices && ok; ++i) {
			const HOCRItem* item = m_document->itemAtIndex(m_document->get_iter(paths[i]));
			if(!item) {
				ok = false;
				break;
			}
			ok &= item->parent() == firstItem->parent();
			classes.insert(item->itemClass());
			rows.push_back(paths[i].back());
		}
		std::sort(rows.begin(), rows.end());
		ok &= classes.size() == 1 && *classes.begin() != "ocr_page";
		ok &= (rows.back() - rows.front()) == nIndices - 1;
		if(ok) {
			Gtk::Menu menu;
			Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
			Gtk::MenuItem* mergeItem = Gtk::manage(new Gtk::MenuItem(_("Merge")));
			menu.append(*mergeItem);
			CONNECT(mergeItem, activate, [&] {
				Gtk::TreeIter newIndex = m_document->mergeItems(m_document->get_iter(paths.front())->parent(), rows.front(), rows.back());
				m_treeView->get_selection()->unselect_all();
				m_treeView->get_selection()->select(newIndex);
			});
			CONNECT(&menu, hide, [&] { loop->quit(); });
			menu.show_all();
			menu.popup(ev->button, ev->time);
			loop->run();
		}
		return;
	}
	Gtk::TreeIter index = m_treeView->indexAtPos(ev->x, ev->y);
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(!item) {
		return;
	}

	Gtk::Menu menu;
	if(item->itemClass() == "ocr_page") {
		Gtk::MenuItem* addGraphicItem = Gtk::manage(new Gtk::MenuItem(_("Add graphic region")));
		menu.append(*addGraphicItem);
		CONNECT(addGraphicItem, activate, [this] { m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_RECT); });
	} else if(item->itemClass() == "ocrx_word") {
		Glib::ustring prefix, suffix, trimmedWord = HOCRItem::trimmedWord(item->text(), &prefix, &suffix);
		Glib::ustring spellLang = item->lang();
		bool haveLanguage = true;
		if(m_spell.get_language() != spellLang) {
			try {
				m_spell.set_language(spellLang);
			} catch(const GtkSpell::Error& /*e*/) {
				haveLanguage = false;
			}
		}
		if(haveLanguage && !trimmedWord.empty()) {
			for(const Glib::ustring& suggestion : m_spell.get_suggestions(trimmedWord)) {
				Glib::ustring replacement = prefix + suggestion + suffix;
				Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(replacement));
				CONNECT(item, activate, [this, replacement, index] { m_document->editItemText(index, replacement); });
				menu.append(*item);
			}
			if(menu.get_children().empty()) {
				Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(_("No suggestions")));
				item->set_sensitive(false);
				menu.append(*item);
			}
			if(!m_spell.check_word(trimmedWord)) {
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

void OutputEditorHOCR::pickItem(const Geometry::Point& point)
{
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

void OutputEditorHOCR::open() {
	if(!clear(false)) {
		return;
	}
	FileDialogs::FileFilter filter = {_("hOCR HTML Files"), {"text/html","text/xml", "text/plain"}, {"*.html"}};
	std::vector<Glib::RefPtr<Gio::File>> files = FileDialogs::open_dialog(_("Open hOCR File"), "", "outputdir", filter, false);
	if(files.empty()) {
		return;
	}
	std::string filename = files.front()->get_path();
	std::string source;
	try {
		source = Glib::file_get_contents(filename);
	} catch(Glib::Error&) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to open file"), Glib::ustring::compose(_("The file could not be opened: %1."), filename));
		return;
	}

	xmlpp::DomParser parser;
	parser.parse_memory(source);
	xmlpp::Document* doc = parser.get_document();
	xmlpp::Element* div = XmlUtils::firstChildElement(XmlUtils::firstChildElement(doc->get_root_node(), "body"), "div");
	if(!div || div->get_attribute_value("class") != "ocr_page") {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Invalid hOCR file"), Glib::ustring::compose(_("The file does not appear to contain valid hOCR HTML: %1"), filename));
		return;
	}
	while(div) {
		m_document->addPage(div, false);
		div = XmlUtils::nextSiblingElement(div, "div");
	}
	m_modified = false;
	m_filebasename = Utils::split_filename(filename).first;
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
	tesseract::TessBaseAPI tess;
	Glib::ustring header = Glib::ustring::compose(
						 "<!DOCTYPE html>\n"
						 "<html>\n"
						 " <head>\n"
						 "  <title>%1</title>\n"
						 "  <meta charset=\"utf-8\" /> \n"
						 "  <meta name='ocr-system' content='tesseract %2' />\n"
						 "  <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
						 " </head>\n", Glib::path_get_basename(outname), tess.Version());
	Glib::ustring body = m_document->toHTML();
	Glib::ustring footer = "</html>\n";
	file.write(header.data(), header.bytes());
	file.write(body.data(), body.bytes());
	file.write(footer.data(), footer.bytes());
	m_modified = false;
	m_filebasename = Utils::split_filename(filename).first;
	return true;
}

bool OutputEditorHOCR::exportToPDF()
{
	if(m_document->pageCount() == 0) {
		return false;
	}
	const HOCRItem* item = m_document->itemAtIndex(m_treeView->currentIndex());
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	if(showPage(page)) {
		return HOCRPdfExporter(m_document, page, m_tool).run(m_filebasename);
	}
	return false;
}

bool OutputEditorHOCR::exportToText()
{
	if(m_document->pageCount() == 0) {
		return false;
	}
	return HOCRTextExporter().run(m_document, m_filebasename);
}

bool OutputEditorHOCR::clear(bool hide)
{
	if(!ui.boxEditorHOCR->get_visible()) {
		return true;
	}
	if(getModified()) {
		int response = Utils::question_dialog(_("Output not saved"), _("Save output before proceeding?"), Utils::Button::Save|Utils::Button::Discard|Utils::Button::Cancel);
		if(response == Utils::Button::Save) {
			if(!save()) {
				return false;
			}
		} else if(response != Utils::Button::Discard) {
			return false;
		}
	}
	m_document = Glib::RefPtr<HOCRDocument>(new HOCRDocument(&m_spell));
	CONNECTX(m_document, row_inserted, [this](const Gtk::TreePath&, const Gtk::TreeIter&){ setModified(); });
	CONNECTX(m_document, row_deleted, [this](const Gtk::TreePath&){ setModified(); });
	CONNECTX(m_document, row_changed, [this](const Gtk::TreePath&, const Gtk::TreeIter&){ setModified(); updateSourceText(); });
	CONNECTX(m_document, item_attribute_changed, [this](const Gtk::TreeIter&, const Glib::ustring&, const Glib::ustring&){ setModified(); });
	CONNECTX(m_document, item_attribute_changed, [this](const Gtk::TreeIter& it, const Glib::ustring& attr, const Glib::ustring& value){ updateAttributes(it, attr, value); updateSourceText(); });
	m_treeView->set_model(m_document);
	ui.textviewSource->get_buffer()->set_text("");
	m_tool->clearSelection();
	m_modified = false;
	ui.buttonSave->set_sensitive(false);
	ui.buttonExport->set_sensitive(false);
	m_filebasename.clear();
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

void OutputEditorHOCR::setLanguage(const Config::Lang &lang) {
	m_document->setDefaultLanguage(lang.code);
}

void OutputEditorHOCR::onVisibilityChanged(bool /*visibile*/) {
	m_searchFrame->hideSubstitutionsManager();
}

bool OutputEditorHOCR::findReplaceInItem(const Gtk::TreeIter& index, const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase, bool backwards, bool replace, bool& currentSelectionMatchesSearch)
{
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

void OutputEditorHOCR::findReplace(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase, bool backwards, bool replace)
{
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

void OutputEditorHOCR::replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase)
{
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

void OutputEditorHOCR::applySubstitutions(const std::map<Glib::ustring, Glib::ustring>& substitutions, bool matchCase)
{
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

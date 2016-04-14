/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
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

#include <cstring>
#include <fstream>
#include <cairomm/cairomm.h>
#include <pangomm/font.h>
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#include <libxml++/libxml++.h>

#include "DisplayerToolHOCR.hh"
#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"


const Glib::RefPtr<Glib::Regex> OutputEditorHOCR::s_bboxRx = Glib::Regex::create("bbox\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)");
const Glib::RefPtr<Glib::Regex> OutputEditorHOCR::s_pageTitleRx = Glib::Regex::create("image\\s+'(.+)';\\s+bbox\\s+\\d+\\s+\\d+\\s+\\d+\\s+\\d+;\\s+pageno\\s+(\\d+);\\s+rot\\s+(\\d+\\.?\\d*);\\s+res\\s+(\\d+\\.?\\d*)");
const Glib::RefPtr<Glib::Regex> OutputEditorHOCR::s_idRx = Glib::Regex::create("(\\w+)_\\d+_(\\d+)");
const Glib::RefPtr<Glib::Regex> OutputEditorHOCR::s_fontSizeRx = Glib::Regex::create("x_fsize\\s+(\\d+)");

static inline Glib::ustring getAttribute(xmlpp::Element* element, const Glib::ustring& name)
{
	if(!element)
		return Glib::ustring();
	xmlpp::Attribute* attrib = element->get_attribute(name);
	return attrib ? attrib->get_value() : Glib::ustring();
}

static inline Glib::ustring getElementText(xmlpp::Element* element)
{
	if(!element)
		return Glib::ustring();
	Glib::ustring text;
	for(xmlpp::Node* node : element->get_children()) {
		if(dynamic_cast<xmlpp::TextNode*>(node)) {
			text += static_cast<xmlpp::TextNode*>(node)->get_content();
		} else if(dynamic_cast<xmlpp::Element*>(node)) {
			text += getElementText(static_cast<xmlpp::Element*>(node));
		}
	}
	return Utils::string_trim(text);
}

static inline xmlpp::Element* getFirstChildElement(xmlpp::Node* node, const Glib::ustring& name = Glib::ustring())
{
	if(!node)
		return nullptr;
	xmlpp::Node* child = node->get_first_child(name);
	while(child && !dynamic_cast<xmlpp::Element*>(child)) {
		child = child->get_next_sibling();
	}
	return child ? dynamic_cast<xmlpp::Element*>(child) : nullptr;
}

static inline xmlpp::Element* getNextSiblingElement(xmlpp::Node* node, const Glib::ustring& name = Glib::ustring())
{
	if(!node)
		return nullptr;
	xmlpp::Node* child = node->get_next_sibling();
	if(name != "") {
		while(child && (child->get_name() != name || !dynamic_cast<xmlpp::Element*>(child))) {
			 child = child->get_next_sibling();
		}
	} else {
		while(child && !dynamic_cast<xmlpp::Element*>(child)) {
			child = child->get_next_sibling();
		}
	}
	return child ? dynamic_cast<xmlpp::Element*>(child) : nullptr;
}

static inline Glib::ustring getDocumentXML(xmlpp::Document* doc)
{
	Glib::ustring xml = doc->write_to_string();
	// Strip entity declaration
	if(xml.substr(0, 5) == "<?xml") {
		std::size_t pos = xml.find("?>\n");
		xml = xml.substr(pos + 3);
	}
	return xml;
}

static inline Glib::ustring getElementXML(xmlpp::Element* element)
{
	xmlpp::Document doc;
	doc.create_root_node_by_import(element);
	return getDocumentXML(&doc);
}

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool)
	: m_builder("/org/gnome/gimagereader/editor_hocr.ui")
{
	m_tool = tool;
	m_widget = m_builder("box:hocr");

	Gtk::Button* openButton = m_builder("button:hocr.open");
	Gtk::Button* saveButton = m_builder("button:hocr.save");
	Gtk::Button* clearButton = m_builder("button:hocr.clear");
	Gtk::MenuItem* savePdfItem = m_builder("menuitem:hocr.export.pdf");
	Gtk::MenuItem* savePdfOverlayItem = m_builder("menuitem:hocr.export.pdfoverlay");
	m_itemView = m_builder("treeview:hocr.items");
	m_itemStore = Gtk::TreeStore::create(m_itemStoreCols);
	m_itemView->set_model(m_itemStore);
	Gtk::TreeViewColumn *itemViewCol = Gtk::manage(new Gtk::TreeViewColumn(""));
	itemViewCol->pack_start(m_itemStoreCols.selected, false);
	Gtk::TreeView_Private::_connect_auto_store_editable_signal_handler<bool>(m_itemView, itemViewCol->get_cells()[0], m_itemStoreCols.selected);
	itemViewCol->pack_start(m_itemStoreCols.icon, false);
	itemViewCol->pack_start(m_itemStoreCols.text, true);
	Gtk::TreeView_Private::_connect_auto_store_editable_signal_handler<Glib::ustring>(m_itemView, itemViewCol->get_cells()[2], m_itemStoreCols.text);
	m_itemView->append_column(*itemViewCol);
	m_itemView->set_expander_column(*m_itemView->get_column(0));
	Gtk::CellRendererText* textRenderer = dynamic_cast<Gtk::CellRendererText*>(itemViewCol->get_cells()[2]);
	if(textRenderer) {
		itemViewCol->add_attribute(textRenderer->property_foreground(), m_itemStoreCols.textColor);
		itemViewCol->add_attribute(textRenderer->property_editable(), m_itemStoreCols.editable);
	}

	m_propView = m_builder("treeview:hocr.properties");
	m_propStore = Gtk::TreeStore::create(m_propStoreCols);
	m_propView->set_model(m_propStore);
	Gtk::CellRendererText* nameRenderer = Gtk::manage(new Gtk::CellRendererText());
	nameRenderer->property_ellipsize() = Pango::ELLIPSIZE_END;
	Gtk::TreeViewColumn* nameCol = Gtk::manage(new Gtk::TreeViewColumn(""));
	nameCol->pack_start(*nameRenderer);
	nameCol->set_renderer(*nameRenderer, m_propStoreCols.name);
	nameCol->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	nameCol->set_expand(true);
	m_propView->append_column(*nameCol);
	Gtk::CellRendererText* valueRenderer = Gtk::manage(new Gtk::CellRendererText());
	valueRenderer->property_ellipsize() = Pango::ELLIPSIZE_END;
	Gtk::TreeViewColumn* valueCol = Gtk::manage(new Gtk::TreeViewColumn(""));
	valueCol->pack_start(*valueRenderer);
	valueCol->set_renderer(*valueRenderer, m_propStoreCols.value);
	valueCol->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	valueCol->set_expand(true);
	m_propView->append_column(*valueCol);

	m_sourceView = m_builder("textview:hocr.source");
	Glib::RefPtr<Gsv::Buffer> buffer = Gsv::Buffer::create();
	buffer->set_highlight_syntax(true);
	buffer->set_language(Gsv::LanguageManager::get_default()->get_language("xml"));
	m_sourceView->set_buffer(buffer);

	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	saveButton->add_accelerator("clicked", group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));

	m_pdfExportDialog = m_builder("dialog:pdfoptions");
	m_pdfExportDialog->set_transient_for(*MAIN->getWindow());

	CONNECT(openButton, clicked, [this]{ open(); });
	CONNECT(saveButton, clicked, [this]{ save(); });
	CONNECT(clearButton, clicked, [this]{ clear(); });
	CONNECT(savePdfItem, activate, [this]{ Glib::signal_idle().connect_once([this]{ savePDF(); }); });
	CONNECT(savePdfOverlayItem, activate, [this]{ Glib::signal_idle().connect_once([this]{ savePDF(true); }); });
	CONNECTP(MAIN->getWidget("fontbutton:config.settings.customoutputfont").as<Gtk::FontButton>(), font_name, [this]{ setFont(); });
	CONNECT(MAIN->getWidget("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>(), toggled, [this]{ setFont(); });
	m_connectionSelectionChanged = CONNECT(m_itemView->get_selection(), changed, [this]{ showItemProperties(); });
	m_connectionRowEdited = CONNECT(m_itemStore, row_changed, [this](const Gtk::TreeModel::Path&, const Gtk::TreeIter& iter){ itemChanged(iter); });
	m_itemView->signal_button_press_event().connect([this](GdkEventButton* ev){ return handleButtonEvent(ev); }, false);

	if(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue().empty()){
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Utils::get_documents_dir());
	}

	MAIN->getConfig()->addSetting(new FontSetting("hocrfont", m_builder("fontbutton:pdfoptions")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckButton>("hocrusedetectedfontsizes", m_builder("checkbox:pdfoptions.usedetectedfontsizes")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckButton>("hocruniformizelinespacing", m_builder("checkbox:pdfoptions.uniformlinespacing")));

	setFont();
}

void OutputEditorHOCR::setFont()
{
	if(MAIN->getWidget("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>()->get_active()){
		m_builder("textview:hocr.source").as<Gtk::TextView>()->unset_font();
	}else{
		Gtk::FontButton* fontBtn = MAIN->getWidget("fontbutton:config.settings.customoutputfont");
		m_builder("textview:hocr.source").as<Gtk::TextView>()->override_font(Pango::FontDescription(fontBtn->get_font_name()));
	}
}

void OutputEditorHOCR::read(tesseract::TessBaseAPI &tess, ReadSessionData *data)
{
	tess.SetVariable("hocr_font_info", "true");
	char* text = tess.GetHOCRText(data->page);
	Utils::runInMainThreadBlocking([&]{ addPage(text, *data); });
	delete[] text;
}

void OutputEditorHOCR::readError(const Glib::ustring &errorMsg, ReadSessionData *data)
{
	static_cast<HOCRReadSessionData*>(data)->errors.push_back(Glib::ustring::compose("%1[%2]: %3", data->file, data->page, errorMsg));
}

void OutputEditorHOCR::finalizeRead(ReadSessionData *data)
{
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	if(!hdata->errors.empty()) {
		Glib::ustring message = Glib::ustring::compose(_("The following pages could not be processed:\n%1"), Utils::string_join(hdata->errors, "\n"));
		Utils::message_dialog(Gtk::MESSAGE_WARNING, _("Recognition errors"), message);
	}
	OutputEditor::finalizeRead(data);
}

void OutputEditorHOCR::addPage(const Glib::ustring& hocrText, ReadSessionData data)
{
	xmlpp::DomParser parser;
	parser.parse_memory(hocrText);
	xmlpp::Document* doc = parser.get_document();
	if(!doc || !doc->get_root_node())
		return;
	xmlpp::Element* pageDiv = dynamic_cast<xmlpp::Element*>(doc->get_root_node());
	if(!pageDiv || pageDiv->get_name() != "div")
		return;

	Glib::MatchInfo matchInfo;
	Glib::ustring titleAttr = getAttribute(pageDiv, "title");
	s_bboxRx->match(titleAttr, matchInfo);
	int x1 = std::atoi(matchInfo.fetch(1).c_str());
	int y1 = std::atoi(matchInfo.fetch(2).c_str());
	int x2 = std::atoi(matchInfo.fetch(3).c_str());
	int y2 = std::atoi(matchInfo.fetch(4).c_str());
	Glib::ustring pageTitle = Glib::ustring::compose("image '%1'; bbox %2 %3 %4 %5; pageno %6; rot %7; res %8",
			data.file, x1, y1, x2, y2, data.page, data.angle, data.resolution);
	pageDiv->set_attribute("title", pageTitle);
	addPage(pageDiv, Gio::File::create_for_path(data.file)->get_basename(), data.page);
}

void OutputEditorHOCR::addPage(xmlpp::Element* pageDiv, const Glib::ustring& filename, int page)
{
	m_connectionRowEdited.block(true);
	pageDiv->set_attribute("id", Glib::ustring::compose("page_%1", ++m_idCounter));

	Glib::MatchInfo matchInfo;
	Glib::ustring titleAttr = getAttribute(pageDiv, "title");
	s_bboxRx->match(titleAttr, matchInfo);
	int x1 = std::atoi(matchInfo.fetch(1).c_str());
	int y1 = std::atoi(matchInfo.fetch(2).c_str());
	int x2 = std::atoi(matchInfo.fetch(3).c_str());
	int y2 = std::atoi(matchInfo.fetch(4).c_str());

	Gtk::TreeIter pageItem = m_itemStore->append();
	pageItem->set_value(m_itemStoreCols.text, Glib::ustring::compose("%1 [%2]", filename, page));
	pageItem->set_value(m_itemStoreCols.id, getAttribute(pageDiv, "id"));
	pageItem->set_value(m_itemStoreCols.bbox, Geometry::Rectangle(x1, y1, x2-x1, y2-y1));
	pageItem->set_value(m_itemStoreCols.itemClass, Glib::ustring("ocr_page"));
	pageItem->set_value(m_itemStoreCols.icon, Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_page.png"));
	pageItem->set_value(m_itemStoreCols.selected, true);
	pageItem->set_value(m_itemStoreCols.editable, false);
	pageItem->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));

	std::map<Glib::ustring,Glib::ustring> langCache;

	xmlpp::Element* element = getFirstChildElement(pageDiv, "div");
	while(element) {
		// Boxes without text are images
		titleAttr = getAttribute(element, "title");
		if(!addChildItems(getFirstChildElement(element), pageItem, langCache) && s_bboxRx->match(titleAttr, matchInfo)) {
			Gtk::TreeIter item = m_itemStore->append(pageItem->children());
			item->set_value(m_itemStoreCols.text, Glib::ustring(_("Graphic")));
			item->set_value(m_itemStoreCols.selected, true);
			item->set_value(m_itemStoreCols.editable, false);
			item->set_value(m_itemStoreCols.icon, Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_halftone.png"));
			item->set_value(m_itemStoreCols.id, getAttribute(element, "id"));
			item->set_value(m_itemStoreCols.itemClass, Glib::ustring("ocr_graphic"));
			item->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
			x1 = std::atoi(matchInfo.fetch(1).c_str());
			y1 = std::atoi(matchInfo.fetch(2).c_str());
			x2 = std::atoi(matchInfo.fetch(3).c_str());
			y2 = std::atoi(matchInfo.fetch(4).c_str());
			item->set_value(m_itemStoreCols.bbox, Geometry::Rectangle(x1, y1, x2-x1, y2-y1));
		}
		element = getNextSiblingElement(element);
	}
	pageItem->set_value(m_itemStoreCols.source, getElementXML(pageDiv));
	m_itemView->expand_row(Gtk::TreePath(pageItem), true);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
	m_connectionRowEdited.block(false);
}

bool OutputEditorHOCR::addChildItems(xmlpp::Element* element, Gtk::TreeIter parentItem, std::map<Glib::ustring,Glib::ustring>& langCache)
{
	bool haveWord = false;
	while(element) {
		Glib::MatchInfo matchInfo;
		Glib::ustring idAttr = getAttribute(element, "id");
		if(s_idRx->match(idAttr, matchInfo)) {
			Glib::ustring newId = Glib::ustring::compose("%1_%2_%3", matchInfo.fetch(1), m_idCounter, matchInfo.fetch(2));
			element->set_attribute("id", newId);
		}
		Glib::ustring titleAttr = getAttribute(element, "title");
		if(s_bboxRx->match(titleAttr, matchInfo)) {
			Glib::ustring type = getAttribute(element, "class");
			Glib::ustring icon;
			Glib::ustring title;
			int x1 = std::atoi(matchInfo.fetch(1).c_str());
			int y1 = std::atoi(matchInfo.fetch(2).c_str());
			int x2 = std::atoi(matchInfo.fetch(3).c_str());
			int y2 = std::atoi(matchInfo.fetch(4).c_str());
			if(type == "ocr_par") {
				title = _("Paragraph");
				icon = "par";
			} else if(type == "ocr_line") {
				title = _("Textline");
				icon = "line";
			} else if(type == "ocrx_word") {
				title = getElementText(element);
				icon = "word";
			}
			if(title != "") {
				Gtk::TreeIter item = m_itemStore->append(parentItem->children());
				item->set_value(m_itemStoreCols.text, title);
				if(type == "ocrx_word" || addChildItems(getFirstChildElement(element), item, langCache)) {
					item->set_value(m_itemStoreCols.selected, true);
					item->set_value(m_itemStoreCols.id, getAttribute(element, "id"));
					if(!icon.empty())
						item->set_value(m_itemStoreCols.icon, Gdk::Pixbuf::create_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/item_%1.png", icon)));
					item->set_value(m_itemStoreCols.bbox, Geometry::Rectangle(x1, y1, x2 - x1, y2 - y1));
					item->set_value(m_itemStoreCols.itemClass, type);
					item->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
					item->set_value(m_itemStoreCols.editable, type == "ocrx_word");
					haveWord = true;
					if(type == "ocrx_word") {
						if(s_fontSizeRx->match(titleAttr, matchInfo)) {
							item->set_value(m_itemStoreCols.fontSize, std::atof(matchInfo.fetch(1).c_str()));
						}
						Glib::ustring lang = getAttribute(element, "lang");
						auto it = langCache.find(lang);
						if(it == langCache.end()) {
							it = langCache.insert(std::make_pair(lang, Utils::getSpellingLanguage(lang))).first;
						}
						Glib::ustring spellingLang = it->second;
						if(m_spell.get_language() != spellingLang) {
							m_spell.set_language(spellingLang);
						}
						if(!m_spell.check_word(title)) {
							item->set_value(m_itemStoreCols.textColor, Glib::ustring("#F00"));
						}
					}
				} else {
					m_itemStore->erase(item);
				}
			}
		}
		element = getNextSiblingElement(element);
	}
	return haveWord;
}

xmlpp::Element* OutputEditorHOCR::getHOCRElementForItem(Gtk::TreeIter item, xmlpp::DomParser& parser) const
{
	if(!item) {
		return 0;
	}

	Glib::ustring id = (*item)[m_itemStoreCols.id];
	while(item->parent()) {
		item = item->parent();
	}
	Glib::ustring text = (*item)[m_itemStoreCols.source];
	parser.parse_memory(text);
	xmlpp::Document* doc = parser.get_document();
	if(doc->get_root_node()) {
		xmlpp::NodeSet nodes = doc->get_root_node()->find(Glib::ustring::compose("//*[@id='%1']", id));
		return nodes.empty() ? nullptr : dynamic_cast<xmlpp::Element*>(nodes.front());
	} else {
		return nullptr;
	}
}

void OutputEditorHOCR::showItemProperties()
{
	Gtk::TreeIter item = m_itemView->get_selection()->get_selected();
	if(!item) {
		return;
	}
	m_propStore->clear();
	xmlpp::DomParser parser;
	xmlpp::Element* element = getHOCRElementForItem(item, parser);
	if(element) {
		for(xmlpp::Attribute* attrib : element->get_attributes()) {
			if(attrib->get_name() == "title") {
				for(Glib::ustring attr : Utils::string_split(attrib->get_value(), ';')) {
					attr = Utils::string_trim(attr);
					std::size_t splitPos = attr.find(" ");
					Gtk::TreeIter item = m_propStore->append();
					item->set_value(m_propStoreCols.name, Utils::string_trim(attr.substr(0, splitPos)));
					item->set_value(m_propStoreCols.value, Utils::string_trim(attr.substr(splitPos + 1)));
				}
			} else {
				Gtk::TreeIter item = m_propStore->append();
				item->set_value(m_propStoreCols.name, attrib->get_name());
				item->set_value(m_propStoreCols.value, attrib->get_value());
			}
		}
		m_sourceView->get_buffer()->set_text(getElementXML(element));

		Glib::MatchInfo matchInfo;
		xmlpp::Element* pageElement = dynamic_cast<xmlpp::Element*>(parser.get_document()->get_root_node());
		Glib::ustring titleAttr = getAttribute(element, "title");
		if(pageElement && pageElement->get_name() == "div" && setCurrentSource(pageElement) && s_bboxRx->match(titleAttr, matchInfo)) {
			int x1 = std::atoi(matchInfo.fetch(1).c_str());
			int y1 = std::atoi(matchInfo.fetch(2).c_str());
			int x2 = std::atoi(matchInfo.fetch(3).c_str());
			int y2 = std::atoi(matchInfo.fetch(4).c_str());
			m_tool->setSelection(Geometry::Rectangle(x1, y1, x2-x1, y2-y1));
		}
	} else {
		m_tool->clearSelection();
	}
}

bool OutputEditorHOCR::setCurrentSource(xmlpp::Element* pageElement, int* pageDpi) const
{
	Glib::MatchInfo matchInfo;
	Glib::ustring titleAttr = getAttribute(pageElement, "title");
	if(s_pageTitleRx->match(titleAttr, matchInfo)) {
		Glib::ustring filename = matchInfo.fetch(1);
		int page = std::atoi(matchInfo.fetch(2).c_str());
		double angle = std::atof(matchInfo.fetch(3).c_str());
		int res = std::atoi(matchInfo.fetch(4).c_str());
		if(pageDpi) {
			*pageDpi = res;
		}

		MAIN->getSourceManager()->addSources({Gio::File::create_for_path(filename)});
		while(Gtk::Main::events_pending()){
			Gtk::Main::iteration();
		}
		int dummy;
		// TODO: Handle this better
		if(MAIN->getDisplayer()->getCurrentImage(dummy) != filename) {
			return false;
		}
		if(MAIN->getDisplayer()->getCurrentPage() != page) {
			MAIN->getDisplayer()->setCurrentPage(page);
		}
		if(MAIN->getDisplayer()->getCurrentAngle() != angle) {
			MAIN->getDisplayer()->setAngle(angle);
		}
		if(MAIN->getDisplayer()->getCurrentResolution() != res) {
			MAIN->getDisplayer()->setResolution(res);
		}
		return true;
	}
	return false;
}

void OutputEditorHOCR::itemChanged(const Gtk::TreeIter& iter)
{
	m_connectionRowEdited.block(true);
	bool isWord = (*iter)[m_itemStoreCols.itemClass] == "ocrx_word";
	bool selected = (*iter)[m_itemStoreCols.selected];
	if( isWord && selected) {
		// Update text
		updateItemText(iter);
	} else if(!selected) {
		m_itemView->collapse_row(m_itemStore->get_path(iter));
	}
	m_connectionRowEdited.block(false);
}

void OutputEditorHOCR::updateItemText(Gtk::TreeIter item)
{
	Glib::ustring newText = (*item)[m_itemStoreCols.text];
	xmlpp::DomParser parser;
	xmlpp::Element* element = getHOCRElementForItem(item, parser);
	element->remove_child(element->get_first_child());
	element->add_child_text(newText);

	Glib::ustring spellLang = Utils::getSpellingLanguage(getAttribute(element, "lang"));
	if(m_spell.get_language() != spellLang) {
		m_spell.set_language(spellLang);
	}
	// TODO
	if(m_spell.check_word(newText)) {
		item->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
	} else {
		item->set_value(m_itemStoreCols.textColor, Glib::ustring("#F00"));
	}

	Gtk::TreeIter toplevelItem = item;
	while(toplevelItem->parent()) {
		toplevelItem = toplevelItem->parent();
	}
	toplevelItem->set_value(m_itemStoreCols.source, getDocumentXML(parser.get_document()));

	m_sourceView->get_buffer()->set_text(getElementXML(element));
	m_modified = true;
}

bool OutputEditorHOCR::handleButtonEvent(GdkEventButton* ev)
{
	Gtk::TreePath path;
	Gtk::TreeViewColumn* col;
	int cell_x, cell_y;
	m_itemView->get_path_at_pos(int(ev->x), int(ev->y), path, col, cell_x, cell_y);
	if(!path) {
		return false;
	}
	Gtk::TreeIter it = m_itemStore->get_iter(path);
	if(!it) {
		return false;
	}
	Glib::ustring itemClass = (*it)[m_itemStoreCols.itemClass];

	if(itemClass == "ocr_page" && ev->type == GDK_BUTTON_PRESS && ev->button == 3) {
		// Context menu on page items with Remove option
		m_itemView->get_selection()->select(path);
		Gtk::Menu menu;
		Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
		Gtk::MenuItem* removeItem = Gtk::manage(new Gtk::MenuItem(_("Remove")));
		menu.append(*removeItem);
		CONNECT(removeItem, activate, [&]{ m_itemStore->erase(it); });
		CONNECT(&menu, hide, [&]{ loop->quit(); });
		menu.show_all();
		menu.popup(ev->button, ev->time);
		loop->run();
		return true;
	} else if(itemClass == "ocrx_word" && ev->type == GDK_BUTTON_PRESS && ev->button == 3) {
		// Context menu on word items with spelling suggestions, if any
		m_itemView->get_selection()->select(path);
		Gtk::Menu menu;
		Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
		for(const Glib::ustring& suggestion : m_spell.get_suggestions((*it)[m_itemStoreCols.text])) {
			Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(suggestion));
			CONNECT(item, activate, [this, suggestion, it] { (*it)[m_itemStoreCols.text] = suggestion; });
			menu.append(*item);
		}
		if(menu.get_children().empty()) {
			Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(_("No suggestions")));
			item->set_sensitive(false);
			menu.append(*item);
		}
		CONNECT(&menu, hide, [&]{ loop->quit(); });
		menu.show_all();
		menu.popup(ev->button, ev->time);
		loop->run();
		return true;
	}
	return false;
}

void OutputEditorHOCR::checkCellEditable(const Glib::ustring& path, Gtk::CellRenderer* renderer)
{
	Gtk::TreeIter it = m_itemStore->get_iter(path);
	if((*it)[m_itemStoreCols.itemClass] != "ocrx_word") {
		renderer->stop_editing(true);
	}
}

void OutputEditorHOCR::open()
{
	if(!clear(false)) {
		return;
	}
	Glib::ustring dir = MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue();
	FileDialogs::FileFilter filter = {_("hOCR HTML Files"), {"text/html"}, {"*.html"}};
	std::vector<Glib::RefPtr<Gio::File>> files = FileDialogs::open_dialog(_("Open hOCR File"), dir, filter, false);
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
	xmlpp::Element* div = getFirstChildElement(getFirstChildElement(doc->get_root_node(), "body"), "div");
	if(!div || getAttribute(div, "class") != "ocr_page") {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Invalid hOCR file"), Glib::ustring::compose(_("The file does not appear to contain valid hOCR HTML: %1"), filename));
		return;
	}
	int page = 0;
	while(div) {
		++page;
		addPage(div, files.front()->get_basename(), page);
		div = getNextSiblingElement(div, "div");
	}
}

bool OutputEditorHOCR::save(const std::string& filename)
{
	std::string outname = filename;
	if(outname.empty()){
		outname = Glib::build_filename(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue(), Glib::ustring(_("output")) + ".html");

		FileDialogs::FileFilter filter = {_("hOCR HTML Files"), {"text/html"}, {"*.html"}};
		outname = FileDialogs::save_dialog(_("Save hOCR Output..."), outname, filter);
		if(outname.empty()) {
			return false;
		}
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Glib::path_get_dirname(outname));
	}
	std::ofstream file(outname);
	if(!file.is_open()){
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	Glib::ustring header = Glib::ustring::compose(
			"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
			"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
			" <head>\n"
			"  <title></title>\n"
			"  <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" />\n"
			"    <meta name='ocr-system' content='tesseract %1' />\n"
			"    <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
			"  </head>\n"
			"<body>\n", TESSERACT_VERSION_STR);
	file.write(header.data(), std::strlen(header.data()));
	for(Gtk::TreeIter item : m_itemStore->children()) {
		Glib::ustring itemSource = (*item)[m_itemStoreCols.source];
		file.write(itemSource.data(), std::strlen(itemSource.data()));
	}
	Glib::ustring footer = "</body>\n</html>\n";
	file.write(footer.data(), std::strlen(footer.data()));
	m_modified = false;
	return true;
}

void OutputEditorHOCR::savePDF(bool overlay)
{
	if(!overlay) {
		int ret = m_pdfExportDialog->run();
		m_pdfExportDialog->hide();
		if(ret != Gtk::RESPONSE_OK) {
			return;
		}
	}
	std::string outname = Glib::build_filename(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue(), Glib::ustring(_("output")) + ".pdf");
	FileDialogs::FileFilter filter = {_("PDF Files"), {"application/pdf"}, {"*.pdf"}};
	outname = FileDialogs::save_dialog(_("Save PDF Output..."), outname, filter);
	if(outname.empty()){
		return;
	}
	MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Glib::path_get_dirname(outname));
	bool useDetectedFontSizes = m_builder("checkbox:pdfoptions.usedetectedfontsizes").as<Gtk::CheckButton>()->get_active();
	bool uniformizeLineSpacing = m_builder("checkbox:pdfoptions.uniformlinespacing").as<Gtk::CheckButton>()->get_active();

	int outputDpi = 300;
	Cairo::RefPtr<Cairo::PdfSurface> surface = Cairo::PdfSurface::create(outname, outputDpi, outputDpi);
	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(surface);
	std::vector<Glib::ustring> failed;
	if(!overlay) {
		Glib::ustring fontName = m_builder("fontbutton:pdfoptions").as<Gtk::FontButton>()->get_font_name();
		Pango::FontDescription fontDesc = Pango::FontDescription(fontName);
		Cairo::FontSlant fontSlant = fontDesc.get_style() == Pango::STYLE_OBLIQUE ? Cairo::FONT_SLANT_OBLIQUE : fontDesc.get_style() == Pango::STYLE_ITALIC ? Cairo::FONT_SLANT_ITALIC : Cairo::FONT_SLANT_NORMAL;
		Cairo::FontWeight fontWeight = fontDesc.get_weight() == Pango::WEIGHT_BOLD ? Cairo::FONT_WEIGHT_BOLD : Cairo::FONT_WEIGHT_NORMAL;
		double fontSize = fontDesc.get_size() / double(PANGO_SCALE);
		context->select_font_face(fontDesc.get_family(), fontSlant, fontWeight);
		context->set_font_size(fontSize * 300. / 72.);
	}
	for(Gtk::TreeIter item : m_itemStore->children()) {
		if(!(*item)[m_itemStoreCols.selected]) {
			continue;
		}
		Geometry::Rectangle bbox = (*item)[m_itemStoreCols.bbox];
		xmlpp::DomParser parser;
		parser.parse_memory((*item)[m_itemStoreCols.source]);
		xmlpp::Document* doc = parser.get_document();
		int pageDpi = 0;
		if(doc->get_root_node() && doc->get_root_node()->get_name() == "div" && setCurrentSource(doc->get_root_node(), &pageDpi)) {
			surface->set_size((bbox.width * 72.) / pageDpi, (bbox.height * 72.) / pageDpi);
			context->save();
			context->scale(72. / double(pageDpi), 72. / double(pageDpi));
			printChildren(context, item, overlay, useDetectedFontSizes, uniformizeLineSpacing);
			if(overlay) {
				Cairo::RefPtr<Cairo::ImageSurface> sel = m_tool->getSelection(bbox);
				context->save();
				context->move_to(bbox.x, bbox.y);
				context->set_source(sel, 0, 0);
				context->paint();
				context->restore();
			}
			context->restore();
			context->show_page();
		} else {
			failed.push_back((*item)[m_itemStoreCols.text]);
		}
	}
	if(!failed.empty()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Errors occurred"), Glib::ustring::compose(_("The following pages could not be rendered:\n%1"), Utils::string_join(failed, "\n")));
	}
}

void OutputEditorHOCR::printChildren(Cairo::RefPtr<Cairo::Context> context, Gtk::TreeIter item, bool overlayMode, bool useDetectedFontSizes, bool uniformizeLineSpacing) const
{
	if(!(*item)[m_itemStoreCols.selected]) {
		return;
	}
	Glib::ustring itemClass = (*item)[m_itemStoreCols.itemClass];
	Geometry::Rectangle itemRect = (*item)[m_itemStoreCols.bbox];
	if(itemClass == "ocr_line" && uniformizeLineSpacing && !overlayMode) {
		double x = itemRect.x;
		double prevWordRight = itemRect.x;
		for(Gtk::TreeIter wordItem : item->children()) {
			if((*wordItem)[m_itemStoreCols.selected]) {
				Geometry::Rectangle wordRect = (*wordItem)[m_itemStoreCols.bbox];
				if(useDetectedFontSizes) {
					context->set_font_size((*wordItem)[m_itemStoreCols.fontSize] * 300. / 72.);
				}
				// If distance from previous word is large, keep the space
				Cairo::TextExtents ext;
				context->get_text_extents(Glib::ustring((*wordItem)[m_itemStoreCols.text]) + " ", ext);
				int spaceSize = ext.x_advance - ext.width; // spaces are ignored in width but counted in advance
				if(wordRect.x - prevWordRight > 4 * spaceSize) {
					x = wordRect.x;
				}
				prevWordRight = wordRect.x + wordRect.width;
				context->move_to(x, itemRect.y + itemRect.height/*wordRect.y - ext.y_bearing*/);
				context->set_source_rgb(0, 0, 0);
				context->show_text(Glib::ustring((*wordItem)[m_itemStoreCols.text]));
				x += ext.x_advance;
			}
		}
	} else if(itemClass == "ocrx_word" && (overlayMode || !uniformizeLineSpacing)) {
		Cairo::TextExtents ext;
		if(useDetectedFontSizes) {
			context->set_font_size((*item)[m_itemStoreCols.fontSize] * 300. / 72.);
		}
		context->get_text_extents(Glib::ustring((*item)[m_itemStoreCols.text]), ext);
		if(overlayMode) {
			context->set_source_rgba(255, 255, 255, 0.01);
		}
		context->move_to(itemRect.x, itemRect.y - ext.y_bearing);
		context->show_text(Glib::ustring((*item)[m_itemStoreCols.text]));
	} else if(itemClass == "ocr_graphic" && !overlayMode) {
		Cairo::RefPtr<Cairo::ImageSurface> sel = m_tool->getSelection(itemRect);
		context->save();
		context->move_to(itemRect.x, itemRect.y);
		context->set_source(sel, itemRect.x, itemRect.y);
		context->paint();
		context->restore();
	} else {
		for(Gtk::TreeIter child : item->children()) {
			printChildren(context, child, overlayMode, useDetectedFontSizes, uniformizeLineSpacing);
		}
	}
}

bool OutputEditorHOCR::clear(bool hide)
{
	if(!m_widget->get_visible()){
		return true;
	}
	if(getModified()){
		int response = Utils::question_dialog(_("Output not saved"), _("Save output before proceeding?"), Utils::Button::Save|Utils::Button::Discard|Utils::Button::Cancel);
		if(response == Utils::Button::Save){
			if(!save()){
				return false;
			}
		}else if(response != Utils::Button::Discard){
			return false;
		}
	}
	m_idCounter = 0;
	m_connectionSelectionChanged.block();
	m_itemStore->clear();
	m_connectionSelectionChanged.unblock();
	m_propStore->clear();
	m_sourceView->get_buffer()->set_text("");
	m_tool->clearSelection();
	m_modified = false;
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

bool OutputEditorHOCR::getModified() const
{
	return m_modified;
}

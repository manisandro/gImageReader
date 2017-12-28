/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.cc
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

#include "common.hh"
#include "HOCRDocument.hh"
#include "XmlUtils.hh"
#include "Utils.hh"

#include <gtkspellmm.h>
#include <libxml++/libxml++.h>
#include <iostream>

#define DEBUG(...) //__VA_ARGS__

HOCRDocument::HOCRDocument(GtkSpell::Checker* spell)
	: Glib::ObjectBase("HOCRDocument")
	, m_spell(spell)
{
	m_document = new xmlpp::Document();
	m_document->create_root_node("body");
}

HOCRDocument::~HOCRDocument()
{
	clear();
	delete m_document;
}

void HOCRDocument::clear()
{
	for(HOCRPage* page : m_pages) {
		Gtk::TreePath pagePath = get_root_path(page->index());
		delete m_pages.back();
		m_pages.pop_back();
		row_deleted(pagePath);
	}
	m_pages.clear();
	m_pageIdCounter = 0;
	m_document->create_root_node("body");
}

void HOCRDocument::recheckSpelling()
{
	for(HOCRPage* page : m_pages) {
		recursiveDataChanged(get_iter(get_root_path(page->index())), {"ocrx_word"});
	}
}

Glib::ustring HOCRDocument::toHTML()
{
	Glib::ustring xml = m_document->write_to_string();
	// Strip entity declaration
	if(xml.substr(0, 5) == "<?xml") {
		std::size_t pos = xml.find("?>\n");
		xml = xml.substr(pos + 3);
	}
	return xml;
}

Gtk::TreeIter HOCRDocument::addPage(const xmlpp::Element* pageElement, bool cleanGraphics)
{
	xmlpp::Element* importedPageElement = static_cast<xmlpp::Element*>(m_document->get_root_node()->import_node(pageElement));
	m_pages.push_back(new HOCRPage(importedPageElement, ++m_pageIdCounter, m_defaultLanguage, cleanGraphics, m_pages.size()));
	Gtk::TreeIter iter = get_iter(get_root_path(m_pages.back()->index()));
	recursiveRowInserted(iter);
	return iter;
}

bool HOCRDocument::editItemAttribute(const Gtk::TreeIter& index, const Glib::ustring& name, const Glib::ustring& value, const Glib::ustring& attrItemClass)
{
	HOCRItem* item = mutableItemAtIndex(index);
	if(!item) {
		return false;
	}

	item->setAttribute(name, value, attrItemClass);
	row_changed(get_path(index), index);
	if(name == "lang") {
		recursiveDataChanged(index, {"ocrx_word"});
	}
	m_signal_item_attribute_changed.emit(index, name, value);
	return true;
}

bool HOCRDocument::editItemText(const Gtk::TreeIter& index, const Glib::ustring& text)
{
	HOCRItem* item = mutableItemAtIndex(index);
	if(!item) {
		return false;
	}
	item->setText(text);
	row_changed(get_path(index), index);
	return true;
}

Gtk::TreeIter HOCRDocument::mergeItems(const Gtk::TreeIter& parent, int startRow, int endRow)
{
	if(endRow - startRow <= 0) {
		return Gtk::TreeIter();
	}
	Gtk::TreeIter targetIndex = parent->children()[startRow];
	HOCRItem* targetItem = mutableItemAtIndex(targetIndex);
	if(!targetItem || targetItem->itemClass() == "ocr_page") {
		return Gtk::TreeIter();
	}

	if(targetItem->itemClass() == "ocrx_word") {
		// Merge word items: join text, merge bounding boxes
		Glib::ustring text = targetItem->text();
		Geometry::Rectangle bbox = targetItem->bbox();
		for(int row = ++startRow; row <= endRow; ++row) {
			Gtk::TreeIter childIndex = parent->children()[startRow];
			HOCRItem* item = mutableItemAtIndex(childIndex);
			g_assert(item);
			text += item->text();
			bbox = bbox.unite(item->bbox());
			Gtk::TreePath childPath = get_path(childIndex);
			deleteItem(item);
			row_deleted(childPath);
		}
		targetItem->setText(text);
		row_changed(get_path(targetIndex), targetIndex);
		Glib::ustring bboxstr = Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
		targetItem->setAttribute("title:bbox", bboxstr);
		m_signal_item_attribute_changed.emit(targetIndex, "title:bbox", bboxstr);
	} else {
		// Merge other items: merge dom trees and bounding boxes
		Geometry::Rectangle bbox = targetItem->bbox();
		std::vector<HOCRItem*> moveChilds;
		for(int row = ++startRow; row <= endRow; ++row) {
			Gtk::TreeIter childIndex = parent->children()[startRow];
			HOCRItem* item = mutableItemAtIndex(childIndex);
			g_assert(item);
			std::vector<HOCRItem*> childItems = item->takeChildren();
			moveChilds.insert(moveChilds.end(), childItems.begin(), childItems.end());
			bbox = bbox.unite(item->bbox());
			Gtk::TreePath childPath = get_path(childIndex);
			deleteItem(item);
			row_deleted(childPath);
		}
		for(HOCRItem* child : moveChilds) {
			targetItem->addChild(child);
			Gtk::TreeIter newIndex = targetIndex->children()[child->index()];
			row_inserted(get_path(newIndex), newIndex);
		}
		Glib::ustring bboxstr = Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
		targetItem->setAttribute("title:bbox", bboxstr);
		m_signal_item_attribute_changed.emit(targetIndex, "title:bbox", bboxstr);
	}
	return targetIndex;
}

Gtk::TreeIter HOCRDocument::addItem(const Gtk::TreeIter& parent, const xmlpp::Element* element)
{
	HOCRItem* parentItem = mutableItemAtIndex(parent);
	if(!parentItem) {
		return Gtk::TreeIter();
	}
	xmlpp::Element* importedElement = static_cast<xmlpp::Element*>(parentItem->importElement(element));
	HOCRItem* item = new HOCRItem(importedElement, parentItem->page(), parentItem);
	parentItem->addChild(item);
	Gtk::TreeIter child = parent->children()[item->index()];
	recursiveRowInserted(child);
	return child;
}

bool HOCRDocument::removeItem(const Gtk::TreeIter& index)
{
	HOCRItem* item = mutableItemAtIndex(index);
	if(!item) {
		return false;
	}
	Gtk::TreePath path = get_path(index);
	deleteItem(item);
	row_deleted(path);
	return true;
}

Gtk::TreeIter HOCRDocument::nextIndex(const Gtk::TreeIter& current) const
{
	Gtk::TreeIter iter = current;
	// If the current index is invalid return first index
	if(!iter) {
		return get_iter(get_root_path(0));
	}
	// If item has children, return next child
	if(!iter->children().empty()) {
		return iter->children()[0];
	}
	// Return next possible sibling
	Gtk::TreeIter parent = iter->parent();
	while(!++iter && parent) {
		iter = parent;
		parent = parent->parent();
	}
	if(!parent) {
		// Wrap around
		return get_iter(get_root_path(0));
	}
	return iter;
}

Gtk::TreeIter HOCRDocument::prevIndex(const Gtk::TreeIter& current) const
{
	Gtk::TreeIter iter = current;
	// If the current index is invalid return last index
	if(!iter) {
		return get_iter(get_root_path(m_pages.size() - 1));
	}
	// Return last possible leaf of previous sibling, if any, or parent
	Gtk::TreeIter parent = iter->parent();
	if(!--iter) {
		if(parent) {
			return parent;
		}
		// Wrap around
		iter = get_iter(get_root_path(m_pages.size() - 1));;
	}
	while(!iter->children().empty()) {
		iter = iter->children()[iter->children().size() - 1];
	}
	return iter;
}

bool HOCRDocument::referencesSource(const Glib::ustring& filename) const
{
	for(const HOCRPage* page : m_pages) {
		if(page->sourceFile() == filename) {
			return true;
		}
	}
	return false;
}

Gtk::TreeIter HOCRDocument::searchPage(const Glib::ustring& filename, int pageNr) const
{
	for(const HOCRPage* page : m_pages) {
		if(page->sourceFile() == filename && page->pageNr() == pageNr) {
			return get_iter(get_root_path(page->index()));
		}
	}
	return Gtk::TreeIter();
}

Gtk::TreeIter HOCRDocument::searchAtCanvasPos(const Gtk::TreeIter& pageIndex, const Geometry::Point& pos) const
{
	Gtk::TreeIter index = pageIndex;
	bool found = bool(index);
	while(found) {
		found = false;
		for(const Gtk::TreeIter& childIndex : index->children()) {
			if(itemAtIndex(childIndex)->bbox().contains(pos)) {
				index = childIndex;
				found = true;
				break;
			}
		}
	}
	return index;
}

void HOCRDocument::recursiveDataChanged(const Gtk::TreeIter& index, const std::vector<Glib::ustring>& itemClasses)
{
	if(index && !index->children().empty()) {
		bool emitChanged = (itemClasses.empty() || std::find(itemClasses.begin(), itemClasses.end(), itemAtIndex(index)->itemClass()) != itemClasses.end());
		for(const Gtk::TreeIter& childIndex : index->children()) {
			if(emitChanged) {
				row_changed(get_path(childIndex), childIndex);
			}
			recursiveDataChanged(childIndex);
		}
	}
}

void HOCRDocument::recursiveRowInserted(const Gtk::TreeIter& index)
{
	DEBUG(std::cout << "Inserted: " << get_path(index).to_string() << std::endl;)
	row_inserted(get_path(index), index);
	for(const Gtk::TreeIter& childIndex : index->children()) {
		recursiveRowInserted(childIndex);
	}
}

Gtk::TreeModelFlags HOCRDocument::get_flags_vfunc() const
{
	return Gtk::TREE_MODEL_ITERS_PERSIST;
}

GType HOCRDocument::get_column_type_vfunc(int index) const
{
	if(index == COLUMN_EDITABLE) {
		return Glib::Value<bool>::value_type();
	} else if(index == COLUMN_CHECKED) {
		return Glib::Value<bool>::value_type();
	} else if(index == COLUMN_ICON) {
		return Glib::Value<Glib::RefPtr<Gdk::Pixbuf>>::value_type();
	} else if(index == COLUMN_TEXT) {
		return Glib::Value<Glib::ustring>::value_type();
	} else if(index == COLUMN_TEXT_COLOR) {
		return Glib::Value<Glib::ustring>::value_type();
	} else if(index == COLUMN_WCONF) {
		return Glib::Value<Glib::ustring>::value_type();
	}
	return G_TYPE_INVALID;
}

int HOCRDocument::get_n_columns_vfunc() const
{
	return NUM_COLUMNS;
}

bool HOCRDocument::iter_next_vfunc(const iterator& iter, iterator& iter_next) const
{
	DEBUG(std::cout << "iter_next_vfunc " << get_path(iter).to_string() << ": ";)
	const HOCRItem* item = itemAtIndex(iter);
	HOCRItem* nextItem = nullptr;
	if(item) {
		if(item->parent()) {
			nextItem = item->index() < item->parent()->children().size() - 1 ? item->parent()->children()[item->index() + 1] : nullptr;
		} else {
			nextItem = item->index() < m_pages.size() - 1 ? m_pages[item->index() + 1] : nullptr;
		}
	}
	iter_next.gobj()->user_data = nextItem;
	iter_next.set_stamp(iter_next.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter_next.gobj()->user_data != nullptr) << std::endl;)
	return iter_next.gobj()->user_data != nullptr;
}

bool HOCRDocument::get_iter_vfunc(const Path& path, iterator& iter) const
{
	DEBUG(std::cout << "get_iter_vfunc " << path.to_string() << ": ";)
	if(path.empty() || m_pages.empty()) {
		DEBUG(std::cout << "0" << std::endl;)
		return false;
	}
	int idx = 0, size = path.size();
	HOCRItem* item = m_pages[path[idx++]];
	while(item && idx < size) {
		item = path[idx] >= item->children().size() ? nullptr : item->children()[path[idx++]];
	}
	iter.gobj()->user_data = item;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (item != nullptr) << std::endl;)
	return item != nullptr;
}

bool HOCRDocument::iter_children_vfunc(const iterator& parent, iterator& iter) const
{
	DEBUG(std::cout << "iter_children_vfunc " << get_path(parent).to_string() << ": ";)
	HOCRItem* parentItem = mutableItemAtIndex(parent);
	iter.gobj()->user_data = parentItem && !parentItem->children().empty() ? parentItem->children().front() : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool HOCRDocument::iter_parent_vfunc(const iterator& child, iterator& iter) const
{
	DEBUG(std::cout << "iter_parent_vfunc " << get_path(child).to_string() << ": ";)
	HOCRItem* childItem = mutableItemAtIndex(child);
	iter.gobj()->user_data = childItem ? childItem->parent() : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool HOCRDocument::iter_nth_child_vfunc(const iterator& parent, int n, iterator& iter) const
{
	DEBUG(std::cout << "iter_nth_child_vfunc " << get_path(parent).to_string() << "@" << n << ": ";)
	HOCRItem* parentItem = mutableItemAtIndex(parent);
	iter.gobj()->user_data = parentItem && n < parentItem->children().size() ? parentItem->children()[n] : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool HOCRDocument::iter_nth_root_child_vfunc(int n, iterator& iter) const
{
	DEBUG(std::cout << "iter_nth_root_child_vfunc " << n << ": ";)
	iter.gobj()->user_data =  n < m_pages.size() ? m_pages[n] : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool HOCRDocument::iter_has_child_vfunc(const iterator& iter) const
{
	DEBUG(std::cout << "iter_parent_vfunc " << get_path(iter).to_string() << ": ";)
	HOCRItem* item = mutableItemAtIndex(iter);
	DEBUG(std::cout << (item && !item->children().empty()) << std::endl;)
	return item && !item->children().empty();
}

int HOCRDocument::iter_n_children_vfunc(const iterator& iter) const
{
	DEBUG(std::cout << "iter_n_children_vfunc " << get_path(iter).to_string() << ": ";)
	HOCRItem* item = mutableItemAtIndex(iter);
	DEBUG(std::cout << (item ? item->children().size() : 0) << std::endl;)
	return item ? item->children().size() : 0;
}

int HOCRDocument::iter_n_root_children_vfunc() const
{
	DEBUG(std::cout << "iter_n_root_children_vfunc " << m_pages.size() << std::endl;)
	return m_pages.size();
}

Gtk::TreeModel::Path HOCRDocument::get_path_vfunc(const iterator& iter) const
{
	Gtk::TreeModel::Path path;
	HOCRItem* item = mutableItemAtIndex(iter);
	while(item) {
		path.push_front(item->index());
		item = item->parent();
	}
	return path;
}

template<class T>
static void setValue(Glib::ValueBase& gvalue, const T& value)
{
	Glib::Value<T> val;
	val.init(val.value_type());
	val.set(value);
	gvalue.init(val.gobj());
}

void HOCRDocument::get_value_vfunc(const iterator& iter, int column, Glib::ValueBase& value) const
{
	DEBUG(std::cout << "get_value_vfunc Column " << column << " at path " << get_path(iter).to_string() << std::endl);
	const HOCRItem* item = itemAtIndex(iter);
	if(!item) {
		return;
	} else if(column == COLUMN_CHECKED) {
		setValue(value, item->isEnabled());
	} else if(column == COLUMN_EDITABLE) {
		setValue(value, item->itemClass() == "ocrx_word");
	} else if(column == COLUMN_ICON) {
		setValue(value, decorationRoleForItem(item));
	} else if(column == COLUMN_TEXT) {
		setValue(value, displayRoleForItem(item));
	} else if(column == COLUMN_TEXT_COLOR) {
		bool enabled = item->isEnabled();
		const HOCRItem* parent = item->parent();
		while(enabled && parent) {
			enabled = parent->isEnabled();
			parent = parent->parent();
		}
		Glib::ustring color;
		if(enabled) {
			color = checkItemSpelling(item) ? "#000" : "#F00";
		} else {
			color = checkItemSpelling(item) ? "#a0a0a4" : "#d05052";
		}
		setValue(value, color);
	} else if(column == COLUMN_WCONF) {
		setValue(value, item->getTitleAttribute("x_wconf"));
	}
}

void HOCRDocument::set_value_impl(const iterator& row, int column, const Glib::ValueBase& value)
{
	HOCRItem* item = mutableItemAtIndex(row);
	if(!item) {
		return;
	} else if(column == COLUMN_TEXT) {
		item->setText(static_cast<const Glib::Value<Glib::ustring>&>(value).get());
		row_changed(get_path(row), row);
	} else if(column == COLUMN_CHECKED) {
		item->setEnabled(static_cast<const Glib::Value<bool>&>(value).get());
		Gtk::TreePath path = get_path(row);
		row_changed(path, row);
		recursiveDataChanged(row);
	}
}

void HOCRDocument::get_value_impl(const iterator& row, int column, Glib::ValueBase& value) const
{
	get_value_vfunc(row, column, value);
}

Glib::ustring HOCRDocument::displayRoleForItem(const HOCRItem* item) const
{
	Glib::ustring itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		const HOCRPage* page = static_cast<const HOCRPage*>(item);
		return page->title();
	} else if(itemClass == "ocr_carea") {
		return _("Text block");
	} else if(itemClass == "ocr_par") {
		return _("Paragraph");
	} else if(itemClass == "ocr_line") {
		return _("Textline");
	} else if(itemClass == "ocrx_word") {
		return item->text();
	} else if(itemClass == "ocr_graphic") {
		return _("Graphic");
	}
	return "";
}

Glib::RefPtr<Gdk::Pixbuf> HOCRDocument::decorationRoleForItem(const HOCRItem* item) const
{
	Glib::ustring itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		return Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_page.png");
	} else if(itemClass == "ocr_carea") {
		return Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_block.png");
	} else if(itemClass == "ocr_par") {
		return Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_par.png");
	} else if(itemClass == "ocr_line") {
		return Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_line.png");
	} else if(itemClass == "ocrx_word") {
		return Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_word.png");
	} else if(itemClass == "ocr_graphic") {
		return Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_halftone.png");
	}
	return Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 0, 0);
}

bool HOCRDocument::checkItemSpelling(const HOCRItem* item) const
{
	if(item->itemClass() == "ocrx_word") {
		Glib::ustring trimmed = HOCRItem::trimmedWord(item->text());
		if(!trimmed.empty()) {
			Glib::ustring lang = item->lang();
			if(m_spell->get_language() != lang) {
				try {
					m_spell->set_language(lang);
				} catch(const GtkSpell::Error& /*e*/) {
					return true;
				}
			}
			return m_spell->check_word(trimmed);
		}
	}
	return true;
}

void HOCRDocument::deleteItem(HOCRItem* item)
{
	if(item->parent()) {
		item->parent()->removeChild(item);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		auto it = std::find(m_pages.begin(), m_pages.end(), page);
		if(it != m_pages.end()) {
			int idx = std::distance(m_pages.begin(), it);
			delete *it;
			m_pages.erase(it);
			for(int i = idx, n = m_pages.size(); i < n; ++i) {
				m_pages[i]->m_index = i;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

std::map<Glib::ustring,Glib::ustring> HOCRItem::s_langCache = std::map<Glib::ustring,Glib::ustring>();

std::map<Glib::ustring, Glib::ustring> HOCRItem::deserializeAttrGroup(const Glib::ustring& string)
{
	std::map<Glib::ustring, Glib::ustring> attrs;
	for(const Glib::ustring& attr : Utils::string_split(string, ';', false)) {
		Glib::ustring trimmed = Utils::string_trim(attr);
		int splitPos = trimmed.find_first_of(' ');
		attrs.insert(std::make_pair(trimmed.substr(0, splitPos), Utils::string_trim(trimmed.substr(splitPos + 1))));
	}
	return attrs;
}

Glib::ustring HOCRItem::serializeAttrGroup(const std::map<Glib::ustring, Glib::ustring>& attrs)
{
	std::vector<Glib::ustring> list;
	for(auto it = attrs.begin(), itEnd = attrs.end(); it != itEnd; ++it) {
		list.push_back(Glib::ustring::compose("%1 %2", it->first, it->second));
	}
	return Utils::string_join(list, "; ");
}

Glib::ustring HOCRItem::trimmedWord(const Glib::ustring& word, Glib::ustring* prefix, Glib::ustring* suffix) {
	static const Glib::RefPtr<Glib::Regex> wordRe = Glib::Regex::create("^(\\W*)(\\w*)(\\W*)$");
	Glib::MatchInfo matchInfo;
	if(wordRe->match(word, matchInfo)) {
		if(prefix)
			*prefix = matchInfo.fetch(1);
		if(suffix)
			*suffix = matchInfo.fetch(3);
		return matchInfo.fetch(2);
	}
	return word;
}

HOCRItem::HOCRItem(xmlpp::Element* element, HOCRPage* page, HOCRItem* parent, int index)
	: m_domElement(element), m_pageItem(page), m_parentItem(parent), m_index(index)
{
	// Adjust item id based on pageId
	if(parent) {
		Glib::ustring idClass = itemClass().substr(itemClass().find_first_of('_') + 1);
		int counter = page->m_idCounters[idClass] + 1;
		page->m_idCounters[idClass] = counter;
		Glib::ustring newId = Glib::ustring::compose("%1_%2_%3", idClass, page->pageId(), counter);
		m_domElement->set_attribute("id", newId);
	}

	// Deserialize title attrs
	m_titleAttrs = deserializeAttrGroup(m_domElement->get_attribute_value("title"));

	// Parse item bbox
	std::vector<Glib::ustring> bbox = Utils::string_split(m_titleAttrs["bbox"], ' ', false);
	if(bbox.size() == 4) {
		m_bbox.setCoords(std::atof(bbox[0].c_str()), std::atof(bbox[1].c_str()), std::atof(bbox[2].c_str()), std::atof(bbox[3].c_str()));
	}

	// For the last word items of the line, ensure the correct hyphen is used
	if(itemClass() == "ocrx_word") {
		xmlpp::Element* nextElement = XmlUtils::nextSiblingElement(m_domElement);
		if(!nextElement) {
			Glib::ustring text = XmlUtils::elementText(m_domElement);
			setText(Glib::Regex::create("[-\u2014]\\s*$")->replace(text, 0, "-", static_cast<Glib::RegexMatchFlags>(0)));
		}
	}
}

HOCRItem::~HOCRItem()
{
	std::for_each( m_childItems.begin(), m_childItems.end(), [](HOCRItem* item){ delete item; });
	m_domElement->get_parent()->remove_child(m_domElement);
}

void HOCRItem::addChild(HOCRItem* child)
{
	xmlpp::Element* newElem = static_cast<xmlpp::Element*>(m_domElement->import_node(child->m_domElement));
	child->m_domElement->get_parent()->remove_child(child->m_domElement);
	child->m_domElement = newElem;
	m_childItems.push_back(child);
	child->m_parentItem = this;
	child->m_pageItem = m_pageItem;
	child->m_index = m_childItems.size() - 1;
}

void HOCRItem::removeChild(HOCRItem *child)
{
	child->m_domElement = XmlUtils::takeChild(m_domElement, child->m_domElement);
	auto it = std::find(m_childItems.begin(), m_childItems.end(), child);
	if(it != m_childItems.end()) {
		int idx = std::distance(m_childItems.begin(), it);
		delete *it;
		m_childItems.erase(it);
		for(int i = idx, n = m_childItems.size(); i < n ; ++i) {
			m_childItems[i]->m_index = i;
		}
	}
}

std::vector<HOCRItem*> HOCRItem::takeChildren()
{
	std::vector<HOCRItem*> children;
	m_childItems.swap(children);
	return children;
}

void HOCRItem::setText(const Glib::ustring& newText)
{
	m_domElement->remove_child(m_domElement->get_first_child());
	m_domElement->add_child_text(newText);
}


Glib::ustring HOCRItem::itemClass() const {
	return m_domElement->get_attribute_value("class");
}

Glib::ustring HOCRItem::text() const
{
	return XmlUtils::elementText(m_domElement);
}

Glib::ustring HOCRItem::lang() const
{
	return m_domElement->get_attribute_value("lang");
}

std::map<Glib::ustring,Glib::ustring> HOCRItem::getAllAttributes() const
{
	std::map<Glib::ustring,Glib::ustring> attrValues;
	for(const xmlpp::Attribute* attribute : m_domElement->get_attributes()) {
		Glib::ustring attrName = attribute->get_name();
		if(attrName == "title") {
			for(auto it = m_titleAttrs.begin(), itEnd = m_titleAttrs.end(); it != itEnd; ++it) {
				attrValues.insert(std::make_pair(Glib::ustring::compose("title:%1", it->first), it->second));
			}
		} else {
			attrValues.insert(std::make_pair(attrName, attribute->get_value()));
		}
	}
	return attrValues;
}

std::map<Glib::ustring,Glib::ustring> HOCRItem::getAttributes(const std::vector<Glib::ustring>& names) const
{
	std::map<Glib::ustring,Glib::ustring> attrValues;
	for(const Glib::ustring& attrName : names) {
		std::vector<Glib::ustring> parts = Utils::string_split(attrName, ':');
		if(parts.size() > 1) {
			g_assert(parts[0] == "title");
			attrValues.insert(std::make_pair(attrName, getTitleAttribute(parts[1])));
		} else {
			attrValues.insert(std::make_pair(attrName, m_domElement->get_attribute_value(attrName)));
		}
	}
	return attrValues;
}

void HOCRItem::getPropagatableAttributes(std::map<Glib::ustring, std::map<Glib::ustring, std::set<Glib::ustring>>>& occurences) const
{
	static std::map<Glib::ustring,std::vector<Glib::ustring>> s_propagatableAttributes = {
		{"ocr_line", {"title:baseline"}},
		{"ocrx_word", {"lang", "title:x_fsize", "title:x_font"}}
	};

	Glib::ustring childClass = m_childItems.empty() ? "" : m_childItems.front()->itemClass();
	auto it = s_propagatableAttributes.find(childClass);
	if(it != s_propagatableAttributes.end()) {
		for(HOCRItem* child : m_childItems) {
			std::map<Glib::ustring, Glib::ustring> attrs = child->getAttributes(it->second);
			for(auto attrIt = attrs.begin(), attrItEnd = attrs.end(); attrIt != attrItEnd; ++attrIt) {
				occurences[childClass][attrIt->first].insert(attrIt->second);
			}
		}
	}
	if(childClass != "ocrx_word") {
		for(HOCRItem* child : m_childItems) {
			child->getPropagatableAttributes(occurences);
		}
	}
}

void HOCRItem::setAttribute(const Glib::ustring& name, const Glib::ustring& value, const Glib::ustring& attrItemClass)
{
	if(!attrItemClass.empty() && itemClass() != attrItemClass) {
		for(HOCRItem* child : m_childItems) {
			child->setAttribute(name, value, attrItemClass);
		}
		return;
	}
	std::vector<Glib::ustring> parts = Utils::string_split(name, ':');
	if(parts.size() < 2) {
		m_domElement->set_attribute(name, value);
	} else {
		g_assert(parts[0] == "title");
		m_titleAttrs[parts[1]] = value;
		m_domElement->set_attribute("title", serializeAttrGroup(m_titleAttrs));
		if(name == "title:bbox") {
			std::vector<Glib::ustring> bbox = Utils::string_split(value, ' ', false);
			g_assert(bbox.size() == 4);
			m_bbox.setCoords(std::atof(bbox[0].c_str()), std::atof(bbox[1].c_str()), std::atof(bbox[2].c_str()), std::atof(bbox[3].c_str()));
		}
	}
}

xmlpp::Element* HOCRItem::importElement(const xmlpp::Element* element)
{
	return static_cast<xmlpp::Element*>(m_domElement->import_node(element));
}

Glib::ustring HOCRItem::toHtml() const
{
	return XmlUtils::elementXML(m_domElement);
}

int HOCRItem::baseLine() const
{
	static const Glib::RefPtr<Glib::Regex> baseLineRx = Glib::Regex::create("([+-]?\\d+\\.?\\d*)\\s+([+-]?\\d+)");
	Glib::MatchInfo matchInfo;
	if(baseLineRx->match(getTitleAttribute("baseline"), matchInfo)) {
		return std::atoi(matchInfo.fetch(2).c_str());
	}
	return 0;
}

bool HOCRItem::parseChildren(Glib::ustring language)
{
	// Determine item language (inherit from parent if not specified)
	Glib::ustring elemLang = m_domElement->get_attribute_value("lang");
	if(!elemLang.empty()) {
		auto it = s_langCache.find(elemLang);
		if(it == s_langCache.end()) {
			it = s_langCache.insert(std::make_pair(elemLang, Utils::getSpellingLanguage(elemLang))).first;
		}
		m_domElement->remove_attribute("lang");
		language = it->second;
	}

	if(itemClass() == "ocrx_word") {
		m_domElement->set_attribute("lang", language);
		return !XmlUtils::elementText(m_domElement).empty();
	}
	bool haveWords = false;
	xmlpp::Element* childElement = XmlUtils::firstChildElement(m_domElement);
	while(childElement) {
		m_childItems.push_back(new HOCRItem(childElement, m_pageItem, this, m_childItems.size()));
		haveWords |= m_childItems.back()->parseChildren(language);
		childElement = XmlUtils::nextSiblingElement(childElement);
	}
	return haveWords;
}

Glib::ustring HOCRItem::getTitleAttribute(const Glib::ustring& key) const {
	auto it = m_titleAttrs.find(key);
	return it == m_titleAttrs.end() ? Glib::ustring() : it->second;
}

///////////////////////////////////////////////////////////////////////////////

HOCRPage::HOCRPage(xmlpp::Element* element, int pageId, const Glib::ustring& language, bool cleanGraphics, int index)
	: HOCRItem(element, this, nullptr, index), m_pageId(pageId)
{
	m_domElement->set_attribute("id", Glib::ustring::compose("page_%1", pageId));

	m_sourceFile = Utils::string_trim(m_titleAttrs["image"], '\'');
	m_pageNr = std::atoi(m_titleAttrs["ppageno"].c_str());
	// Code to handle pageno -> ppageno typo in previous versions of gImageReader
	if(m_pageNr == 0) {
		m_pageNr = std::atoi(m_titleAttrs["pageno"].c_str());
		m_titleAttrs["ppageno"] = m_titleAttrs["pageno"];
		m_titleAttrs.erase(m_titleAttrs.find("pageno"));
	}
	m_angle = std::atof(m_titleAttrs["rot"].c_str());
	m_resolution = std::atoi(m_titleAttrs["res"].c_str());

	xmlpp::Element* childElement = XmlUtils::firstChildElement(m_domElement, "div");
	while(childElement) {
		HOCRItem* item = new HOCRItem(childElement, this, this, m_childItems.size());
		m_childItems.push_back(item);
		if(!item->parseChildren(language)) {
			// No word children -> treat as graphic
			if(cleanGraphics && (item->bbox().width < 10 || item->bbox().height < 10))
			{
				// Ignore graphics which are less than 10 x 10
				delete m_childItems.back();
				m_childItems.pop_back();
			} else {
				childElement->set_attribute("class", "ocr_graphic");
				std::for_each(item->m_childItems.begin(), item->m_childItems.end(), [](HOCRItem* item){ delete item; });
				item->m_childItems.clear();
				// Remove any children since they are not meaningful
				for(xmlpp::Node* child : childElement->get_children()) {
					childElement->remove_child(child);
				}
			}
		}
		childElement = XmlUtils::nextSiblingElement(childElement);
	}
}

Glib::ustring HOCRPage::title() const {
	std::string basename = Gio::File::create_for_path(m_sourceFile)->get_basename();
	return Glib::ustring::compose("%1 [%2]", basename, m_pageNr);
}

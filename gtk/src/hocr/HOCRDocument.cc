/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.cc
 * Copyright (C) (\d+)-2018 Sandro Mani <manisandro@gmail.com>
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
	, m_spell(spell) {
}

HOCRDocument::~HOCRDocument() {
	for(HOCRPage* page : m_pages) {
		delete page;
	}
}

void HOCRDocument::recheckSpelling() {
	for(HOCRPage* page : m_pages) {
		recursiveDataChanged(get_iter(get_root_path(page->index())), {"ocrx_word"});
	}
}

Glib::ustring HOCRDocument::toHTML() {
	Glib::ustring html = "<body>\n";
	for(const HOCRPage* page : m_pages) {
		html += page->toHtml(1);
	}
	html += "</body>\n";
	return html;
}

Gtk::TreeIter HOCRDocument::addPage(const xmlpp::Element* pageElement, bool cleanGraphics) {
	m_pages.push_back(new HOCRPage(pageElement, ++m_pageIdCounter, m_defaultLanguage, cleanGraphics, m_pages.size()));
	Gtk::TreeIter iter = get_iter(get_root_path(m_pages.back()->index()));
	recursiveRowInserted(iter);
	for(int i = 0, n = m_pages.size(); i < n; ++i) {
		Gtk::TreePath path = get_root_path(i);
		row_changed(path, get_iter(path));
	}
	return iter;
}

bool HOCRDocument::editItemAttribute(const Gtk::TreeIter& index, const Glib::ustring& name, const Glib::ustring& value, const Glib::ustring& attrItemClass) {
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
	if(name == "title:bbox") {
		recomputeParentBBoxes(item);
	}
	return true;
}

bool HOCRDocument::editItemText(const Gtk::TreeIter& index, const Glib::ustring& text) {
	HOCRItem* item = mutableItemAtIndex(index);
	if(!item) {
		return false;
	}
	item->setText(text);
	row_changed(get_path(index), index);
	return true;
}

Gtk::TreeIter HOCRDocument::moveItem(const Gtk::TreeIter& itemIndex, const Gtk::TreeIter& newParent, int newRow) {
	HOCRItem* item = mutableItemAtIndex(itemIndex);
	HOCRItem* parentItem = mutableItemAtIndex(newParent);
	if(!item || (!parentItem && item->itemClass() != "ocr_page")) {
		return Gtk::TreeIter();
	}
	Gtk::TreeIter ancestor = newParent;
	while(ancestor) {
		if(ancestor == itemIndex) {
			return Gtk::TreeIter();
		}
		ancestor = ancestor->parent();
	}
	int oldRow = item->index();
	Gtk::TreeIter oldParent = itemIndex->parent();
	if(oldParent == newParent && oldRow < newRow) {
		--newRow;
	}
	Gtk::TreePath itemPath = get_path(itemIndex);
	takeItem(item);
	row_deleted(itemPath);
	insertItem(parentItem, item, newRow);
	Gtk::TreeIter newIndex = newParent->children()[newRow];
	recursiveRowInserted(newIndex);
	return newIndex;
}

Gtk::TreeIter HOCRDocument::swapItems(const Gtk::TreeIter& parent, int firstRow, int secondRow) {
	moveItem(parent->children()[firstRow], parent, secondRow);
	moveItem(parent->children()[secondRow], parent, firstRow);
	return parent->children()[firstRow];
}

Gtk::TreeIter HOCRDocument::mergeItems(const Gtk::TreeIter& parent, int startRow, int endRow) {
	if(endRow - startRow <= 0) {
		return Gtk::TreeIter();
	}
	Gtk::TreeIter targetIndex = parent->children()[startRow];
	HOCRItem* targetItem = mutableItemAtIndex(targetIndex);
	if(!targetItem || targetItem->itemClass() == "ocr_page") {
		return Gtk::TreeIter();
	}

	Geometry::Rectangle bbox = targetItem->bbox();
	if(targetItem->itemClass() == "ocrx_word") {
		// Merge word items: join text, merge bounding boxes
		Glib::ustring text = targetItem->text();
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
	} else {
		// Merge other items: merge dom trees and bounding boxes
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
	}
	Glib::ustring bboxstr = Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
	targetItem->setAttribute("title:bbox", bboxstr);
	m_signal_item_attribute_changed.emit(targetIndex, "title:bbox", bboxstr);
	return targetIndex;
}

Gtk::TreeIter HOCRDocument::addItem(const Gtk::TreeIter& parent, const xmlpp::Element* element) {
	HOCRItem* parentItem = mutableItemAtIndex(parent);
	if(!parentItem) {
		return Gtk::TreeIter();
	}
	HOCRItem* item = new HOCRItem(element, parentItem->page(), parentItem);
	parentItem->addChild(item);
	recomputeParentBBoxes(item);
	Gtk::TreeIter child = parent->children()[item->index()];
	recursiveRowInserted(child);
	return child;
}

bool HOCRDocument::removeItem(const Gtk::TreeIter& index) {
	HOCRItem* item = mutableItemAtIndex(index);
	if(!item) {
		return false;
	}
	Gtk::TreePath path = get_path(index);
	recomputeParentBBoxes(item);
	deleteItem(item);
	row_deleted(path);
	return true;
}

Gtk::TreeIter HOCRDocument::nextIndex(const Gtk::TreeIter& current) const {
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
	Gtk::TreeIter next = iter;
	++next;
	while(iter && !next) {
		iter = iter->parent();
		next = iter;
		++next;
	}
	if(!iter) {
		// Wrap around
		return get_iter(get_root_path(0));
	}
	return next;
}

Gtk::TreeIter HOCRDocument::prevIndex(const Gtk::TreeIter& current) const {
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

bool HOCRDocument::indexIsMisspelledWord(const Gtk::TreeIter& index) const {
	return !checkItemSpelling(itemAtIndex(index));
}

bool HOCRDocument::referencesSource(const Glib::ustring& filename) const {
	for(const HOCRPage* page : m_pages) {
		if(page->sourceFile() == filename) {
			return true;
		}
	}
	return false;
}

Gtk::TreeIter HOCRDocument::searchPage(const Glib::ustring& filename, int pageNr) const {
	for(const HOCRPage* page : m_pages) {
		if(page->sourceFile() == filename && page->pageNr() == pageNr) {
			return get_iter(get_root_path(page->index()));
		}
	}
	return Gtk::TreeIter();
}

Gtk::TreeIter HOCRDocument::searchAtCanvasPos(const Gtk::TreeIter& pageIndex, const Geometry::Point& pos) const {
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

void HOCRDocument::convertSourcePaths(const std::string& basepath, bool absolute) {
	for(HOCRPage* page : m_pages) {
		page->convertSourcePath(basepath, absolute);
	}
}

void HOCRDocument::recursiveDataChanged(const Gtk::TreeIter& index, const std::vector<Glib::ustring>& itemClasses) {
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

void HOCRDocument::recursiveRowInserted(const Gtk::TreeIter& index) {
	DEBUG(std::cout << "Inserted: " << get_path(index).to_string() << std::endl;)
	row_inserted(get_path(index), index);
	for(const Gtk::TreeIter& childIndex : index->children()) {
		recursiveRowInserted(childIndex);
	}
}

void HOCRDocument::recomputeParentBBoxes(const HOCRItem* item) {
	// Update parent bboxes (except page)
	HOCRItem* parent = item->parent();
	while(parent && parent->parent()) {
		Geometry::Rectangle bbox;
		for(const HOCRItem* child : parent->children()) {
			bbox = bbox.unite(child->bbox());
		}
		Glib::ustring bboxstr = Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
		parent->setAttribute("title:bbox", bboxstr);
		parent = parent->parent();
	}
}

Gtk::TreeModelFlags HOCRDocument::get_flags_vfunc() const {
	return Gtk::TREE_MODEL_ITERS_PERSIST;
}

GType HOCRDocument::get_column_type_vfunc(int index) const {
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

int HOCRDocument::get_n_columns_vfunc() const {
	return NUM_COLUMNS;
}

bool HOCRDocument::iter_next_vfunc(const iterator& iter, iterator& iter_next) const {
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

bool HOCRDocument::get_iter_vfunc(const Path& path, iterator& iter) const {
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

bool HOCRDocument::iter_children_vfunc(const iterator& parent, iterator& iter) const {
	DEBUG(std::cout << "iter_children_vfunc " << get_path(parent).to_string() << ": ";)
	HOCRItem* parentItem = mutableItemAtIndex(parent);
	iter.gobj()->user_data = parentItem && !parentItem->children().empty() ? parentItem->children().front() : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool HOCRDocument::iter_parent_vfunc(const iterator& child, iterator& iter) const {
	DEBUG(std::cout << "iter_parent_vfunc " << get_path(child).to_string() << ": ";)
	HOCRItem* childItem = mutableItemAtIndex(child);
	iter.gobj()->user_data = childItem ? childItem->parent() : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool HOCRDocument::iter_nth_child_vfunc(const iterator& parent, int n, iterator& iter) const {
	DEBUG(std::cout << "iter_nth_child_vfunc " << get_path(parent).to_string() << "@" << n << ": ";)
	HOCRItem* parentItem = mutableItemAtIndex(parent);
	iter.gobj()->user_data = parentItem && n < parentItem->children().size() ? parentItem->children()[n] : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool HOCRDocument::iter_nth_root_child_vfunc(int n, iterator& iter) const {
	DEBUG(std::cout << "iter_nth_root_child_vfunc " << n << ": ";)
	iter.gobj()->user_data =  n < m_pages.size() ? m_pages[n] : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool HOCRDocument::iter_has_child_vfunc(const iterator& iter) const {
	DEBUG(std::cout << "iter_parent_vfunc " << get_path(iter).to_string() << ": ";)
	HOCRItem* item = mutableItemAtIndex(iter);
	DEBUG(std::cout << (item && !item->children().empty()) << std::endl;)
	return item && !item->children().empty();
}

int HOCRDocument::iter_n_children_vfunc(const iterator& iter) const {
	DEBUG(std::cout << "iter_n_children_vfunc " << get_path(iter).to_string() << ": ";)
	HOCRItem* item = mutableItemAtIndex(iter);
	DEBUG(std::cout << (item ? item->children().size() : 0) << std::endl;)
	return item ? item->children().size() : 0;
}

int HOCRDocument::iter_n_root_children_vfunc() const {
	DEBUG(std::cout << "iter_n_root_children_vfunc " << m_pages.size() << std::endl;)
	return m_pages.size();
}

Gtk::TreeModel::Path HOCRDocument::get_path_vfunc(const iterator& iter) const {
	Gtk::TreeModel::Path path;
	HOCRItem* item = mutableItemAtIndex(iter);
	while(item) {
		path.push_front(item->index());
		item = item->parent();
	}
	return path;
}

template<class T>
static void setValue(Glib::ValueBase& gvalue, const T& value) {
	Glib::Value<T> val;
	val.init(val.value_type());
	val.set(value);
	gvalue.init(val.gobj());
}

void HOCRDocument::get_value_vfunc(const iterator& iter, int column, Glib::ValueBase& value) const {
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

void HOCRDocument::set_value_impl(const iterator& row, int column, const Glib::ValueBase& value) {
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

void HOCRDocument::get_value_impl(const iterator& row, int column, Glib::ValueBase& value) const {
	get_value_vfunc(row, column, value);
}

Glib::ustring HOCRDocument::displayRoleForItem(const HOCRItem* item) const {
	Glib::ustring itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		const HOCRPage* page = static_cast<const HOCRPage*>(item);
		return Glib::ustring::compose("%1 (%2 %3/%4)", page->title(), _("Page"), page->index() + 1, m_pages.size());
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

Glib::RefPtr<Gdk::Pixbuf> HOCRDocument::decorationRoleForItem(const HOCRItem* item) const {
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

bool HOCRDocument::checkItemSpelling(const HOCRItem* item) const {
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

void HOCRDocument::insertItem(HOCRItem* parent, HOCRItem* item, int i) {
	if(parent) {
		parent->insertChild(item, i);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		page->m_index = i;
		m_pages.insert(m_pages.begin() + i++, page);
		for(int n = m_pages.size(); i < n; ++i) {
			m_pages[i]->m_index = i;
			Gtk::TreePath path = get_root_path(i);
			row_changed(path, get_iter(path));
		}
	}
}

void HOCRDocument::deleteItem(HOCRItem* item) {
	takeItem(item);
	delete item;
}

void HOCRDocument::takeItem(HOCRItem* item) {
	if(item->parent()) {
		item->parent()->takeChild(item);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		int idx = page->index();
		m_pages.erase(m_pages.begin() + idx);
		for(int i = idx, n = m_pages.size(); i < n; ++i) {
			m_pages[i]->m_index = i;
		}
		for(int i = 0, n = m_pages.size(); i < n; ++i) {
			Gtk::TreePath path = get_root_path(i);
			row_changed(path, get_iter(path));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

std::map<Glib::ustring, Glib::ustring> HOCRItem::s_langCache = std::map<Glib::ustring, Glib::ustring>();

std::map<Glib::ustring, Glib::ustring> HOCRItem::deserializeAttrGroup(const Glib::ustring& string) {
	std::map<Glib::ustring, Glib::ustring> attrs;
	for(const Glib::ustring& attr : Utils::string_split(string, ';', false)) {
		Glib::ustring trimmed = Utils::string_trim(attr);
		int splitPos = trimmed.find_first_of(' ');
		attrs.insert(std::make_pair(trimmed.substr(0, splitPos), Utils::string_trim(trimmed.substr(splitPos + 1))));
	}
	return attrs;
}

Glib::ustring HOCRItem::serializeAttrGroup(const std::map<Glib::ustring, Glib::ustring>& attrs) {
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
		if(prefix) {
			*prefix = matchInfo.fetch(1);
		}
		if(suffix) {
			*suffix = matchInfo.fetch(3);
		}
		return matchInfo.fetch(2);
	}
	return word;
}

HOCRItem::HOCRItem(const xmlpp::Element* element, HOCRPage* page, HOCRItem* parent, int index)
	: m_pageItem(page), m_parentItem(parent), m_index(index) {

	// Read attrs
	for(const xmlpp::Attribute* attr : element->get_attributes()) {
		Glib::ustring attrName = attr->get_name();
		if(attrName == "title") {
			m_titleAttrs = deserializeAttrGroup(element->get_attribute_value("title"));
		} else {
			m_attrs[attrName] = attr->get_value();
		}
	}
	// Adjust item id based on pageId
	if(parent) {
		Glib::ustring idClass = itemClass().substr(itemClass().find_first_of('_') + 1);
		int counter = page->m_idCounters[idClass] + 1;
		page->m_idCounters[idClass] = counter;
		Glib::ustring newId = Glib::ustring::compose("%1_%2_%3", idClass, page->pageId(), counter);
		m_attrs["id"] = newId;
	}

	// Parse item bbox
	std::vector<Glib::ustring> bbox = Utils::string_split(m_titleAttrs["bbox"], ' ', false);
	if(bbox.size() == 4) {
		m_bbox.setCoords(std::atof(bbox[0].c_str()), std::atof(bbox[1].c_str()), std::atof(bbox[2].c_str()), std::atof(bbox[3].c_str()));
	}

	if(itemClass() == "ocrx_word") {
		m_text = XmlUtils::elementText(element);
		m_bold = !XmlUtils::elementsByTagName(element, "strong").empty();
		m_italic = !XmlUtils::elementsByTagName(element, "em").empty();
		// For the last word items of the line, ensure the correct hyphen is used
		const xmlpp::Element* nextElement = XmlUtils::nextSiblingElement(element);
		if(!nextElement) {
			m_text = Glib::Regex::create("[-\u2014]\\s*$")->replace(m_text, 0, "-", static_cast<Glib::RegexMatchFlags>(0));
		}
	}
}

HOCRItem::~HOCRItem() {
	std::for_each( m_childItems.begin(), m_childItems.end(), [](HOCRItem * item) {
		delete item;
	});
}

void HOCRItem::addChild(HOCRItem* child) {
	m_childItems.push_back(child);
	child->m_parentItem = this;
	child->m_pageItem = m_pageItem;
	child->m_index = m_childItems.size() - 1;
}

void HOCRItem::insertChild(HOCRItem* child, int i) {
	m_childItems.insert(m_childItems.begin() + i, child);
	child->m_parentItem = this;
	child->m_pageItem = m_pageItem;
	child->m_index = i++;
	for(int n = m_childItems.size(); i < n; ++i) {
		m_childItems[i]->m_index = i;
	}
}

void HOCRItem::removeChild(HOCRItem* child) {
	takeChild(child);
	delete child;
}

void HOCRItem::takeChild(HOCRItem* child) {
	int idx = child->index();
	m_childItems.erase(m_childItems.begin() + idx);
	for(int i = idx, n = m_childItems.size(); i < n ; ++i) {
		m_childItems[i]->m_index = i;
	}
}

std::vector<HOCRItem*> HOCRItem::takeChildren() {
	std::vector<HOCRItem*> children;
	m_childItems.swap(children);
	return children;
}

std::map<Glib::ustring, Glib::ustring> HOCRItem::getAllAttributes() const {
	std::map<Glib::ustring, Glib::ustring> attrValues;
	for(auto it = m_attrs.begin(), itEnd = m_attrs.end(); it != itEnd; ++it) {
		attrValues.insert(*it);
	}
	for(auto it = m_titleAttrs.begin(), itEnd = m_titleAttrs.end(); it != itEnd; ++it) {
		attrValues.insert(std::make_pair(Glib::ustring::compose("title:%1", it->first), it->second));
	}
	if(itemClass() == "ocrx_word") {
		if(attrValues.find("title:x_font") == attrValues.end()) {
			attrValues.insert(std::make_pair("title:x_font", ""));
		}
		attrValues.insert(std::make_pair("bold", fontBold() ? "yes" : "no"));
		attrValues.insert(std::make_pair("italic", fontItalic() ? "yes" : "no"));
	}
	return attrValues;
}

std::map<Glib::ustring, Glib::ustring> HOCRItem::getAttributes(const std::vector<Glib::ustring>& names) const {
	std::map<Glib::ustring, Glib::ustring> attrValues;
	for(const Glib::ustring& attrName : names) {
		std::vector<Glib::ustring> parts = Utils::string_split(attrName, ':');
		if(parts.size() > 1) {
			g_assert(parts[0] == "title");
			attrValues.insert(std::make_pair(attrName, getTitleAttribute(parts[1])));
		} else if(attrName == "bold") {
			attrValues.insert(std::make_pair(attrName, fontBold() ? "yes" : "no"));
		} else if(attrName == "italic") {
			attrValues.insert(std::make_pair(attrName, fontItalic() ? "yes" : "no"));
		} else {
			attrValues.insert(std::make_pair(attrName, getAttribute(attrName)));
		}
	}
	return attrValues;
}

void HOCRItem::getPropagatableAttributes(std::map<Glib::ustring, std::map<Glib::ustring, std::set<Glib::ustring>>>& occurences) const {
	static std::map<Glib::ustring, std::vector<Glib::ustring>> s_propagatableAttributes = {
		{"ocr_line", {"title:baseline"}},
		{"ocrx_word", {"lang", "title:x_fsize", "title:x_font", "bold", "italic"}}
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

void HOCRItem::setAttribute(const Glib::ustring& name, const Glib::ustring& value, const Glib::ustring& attrItemClass) {
	if(!attrItemClass.empty() && itemClass() != attrItemClass) {
		for(HOCRItem* child : m_childItems) {
			child->setAttribute(name, value, attrItemClass);
		}
		return;
	}
	std::vector<Glib::ustring> parts = Utils::string_split(name, ':');
	if(name == "bold") {
		m_bold = value == "yes";
	} else if(name == "italic") {
		m_italic = value == "yes";
	} else if(parts.size() < 2) {
		m_attrs[name] = value;
	} else {
		g_assert(parts[0] == "title");
		m_titleAttrs[parts[1]] = value;
		if(name == "title:bbox") {
			std::vector<Glib::ustring> bbox = Utils::string_split(value, ' ', false);
			g_assert(bbox.size() == 4);
			m_bbox.setCoords(std::atof(bbox[0].c_str()), std::atof(bbox[1].c_str()), std::atof(bbox[2].c_str()), std::atof(bbox[3].c_str()));
		}
	}
}

Glib::ustring HOCRItem::toHtml(int indent) const {
	Glib::ustring cls = itemClass();
	Glib::ustring tag;
	if(cls == "ocr_page" || cls == "ocr_carea" || cls == "ocr_graphic") {
		tag = "div";
	} else if(cls == "ocr_par") {
		tag = "p";
	} else {
		tag = "span";
	}
	Glib::ustring html = Glib::ustring(indent, ' ') + "<" + tag;
	html += Glib::ustring::compose(" title=\"%1\"", serializeAttrGroup(m_titleAttrs));
	for(auto it = m_attrs.begin(), itEnd = m_attrs.end(); it != itEnd; ++it) {
		html += Glib::ustring::compose(" %1=\"%2\"", it->first, it->second);
	}
	html += ">";
	if(cls == "ocrx_word") {
		if(m_bold) {
			html += "<strong>";
		}
		if(m_italic) {
			html += "<em>";
		}
		html += Utils::string_html_escape(m_text);
		if(m_italic) {
			html += "</em>";
		}
		if(m_bold) {
			html += "</strong>";
		}
	} else {
		html += "\n";
		for(const HOCRItem* child : m_childItems) {
			html += child->toHtml(indent + 1);
		}
		html += Glib::ustring(indent, ' ');
	}
	html += "</" + tag + ">\n";
	return html;
}

std::pair<double, double> HOCRItem::baseLine() const {
	static const Glib::RefPtr<Glib::Regex> baseLineRx = Glib::Regex::create("([+-]?\\d+\\.?\\d*)\\s+([+-]?\\d+\\.?\\d*)");
	Glib::MatchInfo matchInfo;
	if(baseLineRx->match(getTitleAttribute("baseline"), matchInfo)) {
		double a = std::atof(matchInfo.fetch(1).c_str());
		double b = std::atof(matchInfo.fetch(2).c_str());
		return std::make_pair(a, b);
	}
	return std::make_pair(0.0, 0.0);
}

bool HOCRItem::parseChildren(const xmlpp::Element* element, Glib::ustring language) {
	// Determine item language (inherit from parent if not specified)
	Glib::ustring elemLang = getAttribute("lang");
	if(!elemLang.empty()) {
		auto it = s_langCache.find(elemLang);
		if(it == s_langCache.end()) {
			it = s_langCache.insert(std::make_pair(elemLang, Utils::getSpellingLanguage(elemLang))).first;
		}
		m_attrs.erase(m_attrs.find("lang"));
		language = it->second;
	}

	if(itemClass() == "ocrx_word") {
		m_attrs["lang"] = language;
		return !m_text.empty();
	}
	bool haveWords = false;
	const xmlpp::Element* childElement = XmlUtils::firstChildElement(element);
	while(childElement) {
		m_childItems.push_back(new HOCRItem(childElement, m_pageItem, this, m_childItems.size()));
		haveWords |= m_childItems.back()->parseChildren(childElement, language);
		childElement = XmlUtils::nextSiblingElement(childElement);
	}
	return haveWords;
}

Glib::ustring HOCRItem::getAttribute(const Glib::ustring& key) const {
	auto it = m_attrs.find(key);
	return it == m_attrs.end() ? Glib::ustring() : it->second;
}

Glib::ustring HOCRItem::getTitleAttribute(const Glib::ustring& key) const {
	auto it = m_titleAttrs.find(key);
	return it == m_titleAttrs.end() ? Glib::ustring() : it->second;
}

///////////////////////////////////////////////////////////////////////////////

HOCRPage::HOCRPage(const xmlpp::Element* element, int pageId, const Glib::ustring& language, bool cleanGraphics, int index)
	: HOCRItem(element, this, nullptr, index), m_pageId(pageId) {
	m_attrs["id"] = Glib::ustring::compose("page_%1", pageId);

	m_sourceFile = Utils::string_trim(m_titleAttrs["image"], "'\"");
	m_pageNr = std::atoi(m_titleAttrs["ppageno"].c_str());
	// Code to handle pageno -> ppageno typo in previous versions of gImageReader
	if(m_pageNr == 0) {
		m_pageNr = std::atoi(m_titleAttrs["pageno"].c_str());
		m_titleAttrs["ppageno"] = m_titleAttrs["pageno"];
		m_titleAttrs.erase(m_titleAttrs.find("pageno"));
	}
	m_angle = std::atof(m_titleAttrs["rot"].c_str());
	m_resolution = std::atoi(m_titleAttrs["res"].c_str());

	const xmlpp::Element* childElement = XmlUtils::firstChildElement(element, "div");
	while(childElement) {
		// Need to query here because "delete m_childItems.back();" may delete the item
		const xmlpp::Element* nextSibling = XmlUtils::nextSiblingElement(childElement);
		HOCRItem* item = new HOCRItem(childElement, this, this, m_childItems.size());
		m_childItems.push_back(item);
		if(!item->parseChildren(childElement, language)) {
			// No word children -> treat as graphic
			if(cleanGraphics && (item->bbox().width < 10 || item->bbox().height < 10)) {
				// Ignore graphics which are less than 10 x 10
				delete m_childItems.back();
				m_childItems.pop_back();
			} else {
				item->setAttribute("itemClass", "ocr_graphic");
				std::for_each(item->m_childItems.begin(), item->m_childItems.end(), [](HOCRItem * item) {
					delete item;
				});
				item->m_childItems.clear();
			}
		}
		childElement = nextSibling;
	}
}

Glib::ustring HOCRPage::title() const {
	std::string basename = Gio::File::create_for_path(m_sourceFile)->get_basename();
	return Glib::ustring::compose("%1 [%2]", basename, m_pageNr);
}

void HOCRPage::convertSourcePath(const std::string& basepath, bool absolute) {
	m_sourceFile = absolute ? Utils::make_absolute_path(m_sourceFile, basepath) : Utils::make_relative_path(m_sourceFile, basepath);
	m_titleAttrs["image"] = Glib::ustring::compose("'%1'", m_sourceFile);
}

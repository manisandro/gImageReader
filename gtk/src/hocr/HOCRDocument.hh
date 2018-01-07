/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.hh
 * Copyright (C) 2013-2018 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRDOCUMENT_HH
#define HOCRDOCUMENT_HH

#include "common.hh"
#include "Geometry.hh"
#include <map>
#include <set>

namespace GtkSpell {
class Checker;
}
namespace xmlpp {
class Document;
class Element;
}

class HOCRItem;
class HOCRPage;


class HOCRDocument : public Gtk::TreeModel, public Glib::Object {
public:
	enum Columns { COLUMN_EDITABLE, COLUMN_CHECKED, COLUMN_ICON, COLUMN_TEXT, COLUMN_TEXT_COLOR, COLUMN_WCONF, NUM_COLUMNS};

	HOCRDocument(GtkSpell::Checker* spell);
	~HOCRDocument();

	void setDefaultLanguage(const Glib::ustring& language) {
		m_defaultLanguage = language;
	}
	void recheckSpelling();

	xmlpp::Document* getDomDocument() {
		return m_document;
	}
	Glib::ustring toHTML();

	Gtk::TreeIter addPage(const xmlpp::Element* pageElement, bool cleanGraphics);
	const HOCRPage* page(int i) const {
		return m_pages[i];
	}
	int pageCount() const {
		return m_pages.size();
	}

	const HOCRItem* itemAtIndex(const Gtk::TreeIter& index) const {
		return static_cast<HOCRItem*>(index.gobj()->user_data);
	}
	bool editItemAttribute(const Gtk::TreeIter& index, const Glib::ustring& name, const Glib::ustring& value, const Glib::ustring& attrItemClass = Glib::ustring());
	bool editItemText(const Gtk::TreeIter& index, const Glib::ustring& text);
	Gtk::TreeIter mergeItems(const Gtk::TreeIter& parent, int startRow, int endRow);
	Gtk::TreeIter addItem(const Gtk::TreeIter& parent, const xmlpp::Element* element);
	bool removeItem(const Gtk::TreeIter& index);

	Gtk::TreeIter nextIndex(const Gtk::TreeIter& current) const;
	Gtk::TreeIter prevIndex(const Gtk::TreeIter& current) const;
	bool indexIsMisspelledWord(const Gtk::TreeIter& index) const;

	bool referencesSource(const Glib::ustring& filename) const;
	Gtk::TreeIter searchPage(const Glib::ustring& filename, int pageNr) const;
	Gtk::TreeIter searchAtCanvasPos(const Gtk::TreeIter& pageIndex, const Geometry::Point& pos) const;
	void convertSourcePaths(const std::string& basepath, bool absolute);

	Gtk::TreePath get_root_path(int idx) const {
		Gtk::TreePath path;
		path.push_back(idx);
		return path;
	}
	// Upstream forgot the consts...
	Gtk::TreeIter get_iter(const Gtk::TreePath& path) const {
		return static_cast<Gtk::TreeModel*>(const_cast<HOCRDocument*>(this))->get_iter(path);
	}
	Gtk::TreeIter get_iter(const Glib::ustring& path) const {
		return static_cast<Gtk::TreeModel*>(const_cast<HOCRDocument*>(this))->get_iter(path);
	}

	sigc::signal<void, const Gtk::TreeIter&, const Glib::ustring&, const Glib::ustring&> signal_item_attribute_changed() {
		return m_signal_item_attribute_changed;
	}

private:
	int m_pageIdCounter = 0;
	xmlpp::Document* m_document;
	Glib::ustring m_defaultLanguage = "en_US";
	GtkSpell::Checker* m_spell;

	std::vector<HOCRPage*> m_pages;

	sigc::signal<void, const Gtk::TreeIter&, const Glib::ustring&, const Glib::ustring&> m_signal_item_attribute_changed;

	Gtk::TreeModelFlags get_flags_vfunc() const override;
	GType get_column_type_vfunc(int index) const override;
	int get_n_columns_vfunc() const override;
	bool iter_next_vfunc(const iterator& iter, iterator& iter_next) const override;
	bool get_iter_vfunc(const Path& path, iterator& iter) const override;
	bool iter_children_vfunc(const iterator& parent, iterator& iter) const override;
	bool iter_parent_vfunc(const iterator& child, iterator& iter) const override;
	bool iter_nth_child_vfunc(const iterator& parent, int n, iterator& iter) const override;
	bool iter_nth_root_child_vfunc(int n, iterator& iter) const override;
	bool iter_has_child_vfunc(const iterator& iter) const override;
	int iter_n_children_vfunc(const iterator& iter) const override;
	int iter_n_root_children_vfunc() const override;
//	void ref_node_vfunc(const iterator& iter) const override;
//	void unref_node_vfunc(const iterator& iter) const override;
	Gtk::TreeModel::Path get_path_vfunc(const iterator& iter) const override;
	void get_value_vfunc(const iterator& iter, int column, Glib::ValueBase& value) const override;
	void set_value_impl(const iterator& row, int column, const Glib::ValueBase& value) override;
	void get_value_impl(const iterator& row, int column, Glib::ValueBase& value) const override;

	Glib::ustring displayRoleForItem(const HOCRItem* item) const;
	Glib::RefPtr<Gdk::Pixbuf> decorationRoleForItem(const HOCRItem* item) const;

	bool checkItemSpelling(const HOCRItem* item) const;
	void deleteItem(HOCRItem* item);
	void recursiveDataChanged(const Gtk::TreeIter& index, const std::vector<Glib::ustring>& itemClasses = {});
	void recursiveRowInserted(const Gtk::TreeIter& index);
	void recomputeParentBBoxes(const HOCRItem* item);

	HOCRItem* mutableItemAtIndex(const Gtk::TreeIter& index) const {
		return static_cast<HOCRItem*>(index.gobj()->user_data);
	}
};


class HOCRItem {
public:
	// attrname : attrvalue : occurences
	typedef std::map<Glib::ustring, std::map<Glib::ustring, int>> AttrOccurenceMap_t;

	HOCRItem(xmlpp::Element* element, HOCRPage* page, HOCRItem *parent, int index = -1);
	virtual ~HOCRItem();
	HOCRPage* page() const {
		return m_pageItem;
	}
	const std::vector<HOCRItem*>& children() const {
		return m_childItems;
	}
	HOCRItem* parent() const {
		return m_parentItem;
	}
	int index() const {
		return m_index;
	}
	const xmlpp::Element* element() const {
		return m_domElement;
	}
	bool isEnabled() const {
		return m_enabled;
	}

	// HOCR specific convenience getters
	Glib::ustring itemClass() const;
	const Geometry::Rectangle& bbox() const {
		return m_bbox;
	}
	Glib::ustring text() const;
	Glib::ustring lang() const;
	const std::map<Glib::ustring, Glib::ustring> getTitleAttributes() const {
		return m_titleAttrs;
	}
	Glib::ustring getTitleAttribute(const Glib::ustring& key) const;
	std::map<Glib::ustring,Glib::ustring> getAllAttributes() const;
	std::map<Glib::ustring,Glib::ustring> getAttributes(const std::vector<Glib::ustring>& names) const;
	void getPropagatableAttributes(std::map<Glib::ustring, std::map<Glib::ustring, std::set<Glib::ustring> > >& occurences) const;
	Glib::ustring toHtml() const;
	int baseLine() const;
	Glib::ustring fontFamily() const {
		return getTitleAttribute("x_font");
	}
	double fontSize() const {
		return std::atof(getTitleAttribute("x_fsize").c_str());
	}
	bool fontBold() const;
	bool fontItalic() const;

	void addChild(HOCRItem* child);
	void removeChild(HOCRItem* child);
	std::vector<HOCRItem*> takeChildren();
	void setEnabled(bool enabled) {
		m_enabled = enabled;
	}
	void setText(const Glib::ustring& newText);
	void setAttribute(const Glib::ustring& name, const Glib::ustring& value, const Glib::ustring& attrItemClass = Glib::ustring());

	xmlpp::Element* importElement(const xmlpp::Element* element);

	static std::map<Glib::ustring, Glib::ustring> deserializeAttrGroup(const Glib::ustring& string);
	static Glib::ustring serializeAttrGroup(const std::map<Glib::ustring, Glib::ustring>& attrs);
	static Glib::ustring trimmedWord(const Glib::ustring& word, Glib::ustring* prefix = nullptr, Glib::ustring* suffix = nullptr);

protected:
	friend class HOCRPage;

	static std::map<Glib::ustring,Glib::ustring> s_langCache;

	xmlpp::Element* m_domElement;
	std::map<Glib::ustring, Glib::ustring> m_titleAttrs;
	std::vector<HOCRItem*> m_childItems;
	HOCRPage* m_pageItem = nullptr;
	HOCRItem* m_parentItem = nullptr;
	int m_index;
	bool m_enabled = true;

	Geometry::Rectangle m_bbox;

	bool parseChildren(Glib::ustring language);
};


class HOCRPage : public HOCRItem {
public:
	HOCRPage(xmlpp::Element* element, int pageId, const Glib::ustring& language, bool cleanGraphics, int index);

	const Glib::ustring& sourceFile() const {
		return m_sourceFile;
	}
	// const-refs here to avoid taking reference from temporaries
	const int& pageNr() const {
		return m_pageNr;
	}
	const double& angle() const {
		return m_angle;
	}
	const int& resolution() const {
		return m_resolution;
	}
	int pageId() const {
		return m_pageId;
	}
	Glib::ustring title() const;

private:
	friend class HOCRItem;
	friend class HOCRDocument;

	int m_pageId;
	std::map<Glib::ustring, int> m_idCounters;
	Glib::ustring m_sourceFile;
	int m_pageNr;
	double m_angle;
	int m_resolution;

	void convertSourcePath(const std::string& basepath, bool absolute);
};


#endif // HOCRDOCUMENT_HH

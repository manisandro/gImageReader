/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.hh
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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

#ifndef OUTPUTEDITORHOCR_HH
#define OUTPUTEDITORHOCR_HH

#include <gtksourceviewmm.h>

#include "Config.hh"
#include "Geometry.hh"
#include "OutputEditor.hh"
#include "ui_OutputEditorHOCR.hh"

class DisplayerImageItem;
class DisplayerToolHOCR;
namespace Geometry {
class Rectangle;
}
class HOCRDocument;
class HOCRItem;
class HOCRPage;
class SearchReplaceFrame;

class OutputEditorHOCR : public OutputEditor {
public:
	class HOCRBatchProcessor : public BatchProcessor {
	public:
		std::string fileSuffix() const override { return std::string(".html"); }
		void writeHeader(std::ostream& dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo) const override;
		void writeFooter(std::ostream& dev) const override;
		void appendOutput(std::ostream& dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfos, bool firstArea) const override;
	};

	enum class InsertMode { Replace, Append, InsertBefore };

	OutputEditorHOCR(DisplayerToolHOCR* tool);
	~OutputEditorHOCR();

	Gtk::Box* getUI() const override {
		return ui.boxEditorHOCR;
	}
	ReadSessionData* initRead(tesseract::TessBaseAPI& tess) override;
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const Glib::ustring& errorMsg, ReadSessionData* data) override;
	void finalizeRead(ReadSessionData* data) override;
	BatchProcessor* createBatchProcessor(const std::map<Glib::ustring, Glib::ustring>& /*options*/) const override { return new HOCRBatchProcessor; }
	bool containsSource(const std::string& source, int sourcePage) const override;
	void onVisibilityChanged(bool /*visible*/) override;

	bool clear(bool hide = true) override;
	void setLanguage(const Config::Lang& lang) override;
	bool open(const std::string& file) override { return open(InsertMode::Replace, {Gio::File::create_for_path(file)}); }
	std::string crashSave(const std::string& filename) const override;
	bool open(InsertMode mode, std::vector<Glib::RefPtr<Gio::File>> files = std::vector<Glib::RefPtr<Gio::File>>());
	bool save(const std::string& filename = "");
	bool exportToPDF();
	bool exportToText();
	bool exportToODT();
	HOCRDocument* getDocument() const { return m_document.get(); }
	DisplayerToolHOCR* getTool() const { return m_tool; }
	bool selectPage(int nr);

private:
	class HOCRTreeView;
	class HOCRCellRendererText;
	class HOCRAttribRenderer;

	struct HOCRReadSessionData : ReadSessionData {
		int insertIndex;
		std::vector<Glib::ustring> errors;
	};

	struct NavigationComboColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> label;
		Gtk::TreeModelColumn<Glib::ustring> itemClass;
		NavigationComboColumns() {
			add(label);
			add(itemClass);
		}
	} m_navigationComboCols;

	struct PropStoreColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> attr;
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<Glib::ustring> value;
		Gtk::TreeModelColumn<bool> multiple;
		Gtk::TreeModelColumn<Glib::ustring> placeholder;
		Gtk::TreeModelColumn<Glib::ustring> itemClass;
		Gtk::TreeModelColumn<bool> editable;
		Gtk::TreeModelColumn<Glib::ustring> background;
		Gtk::TreeModelColumn<int> weight;
		PropStoreColumns() {
			add(attr);
			add(name);
			add(value);
			add(multiple);
			add(placeholder);
			add(itemClass);
			add(editable);
			add(background);
			add(weight);
		}
	} m_propStoreCols;

	Ui::OutputEditorHOCR ui;
	ClassData m_classdata;

	DisplayerImageItem* m_preview = nullptr;
	sigc::connection m_connectionPreviewTimer;
	HOCRTreeView* m_treeView;
	HOCRCellRendererText* m_treeViewTextCell;
	Glib::RefPtr<Gtk::TreeStore> m_propStore;
	SearchReplaceFrame* m_searchFrame;
	DisplayerToolHOCR* m_tool;
	bool m_modified = false;
	std::string m_filebasename;
	InsertMode m_insertMode = InsertMode::Append;

	Glib::RefPtr<HOCRDocument> m_document;

	sigc::connection m_connectionCustomFont;
	sigc::connection m_connectionDefaultFont;
	sigc::connection m_connectionSourceChanged;

	Glib::RefPtr<Glib::Regex> attributeValidator(const Glib::ustring& attribName) const;
	bool attributeEditable(const Glib::ustring& attribName) const;
	void expandCollapseChildren(const Gtk::TreeIter& index, bool expand) const;
	bool findReplaceInItem(const Gtk::TreeIter& index, const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase, bool backwards, bool replace, bool& currentSelectionMatchesSearch);
	bool showPage(const HOCRPage* page);
	int currentPage();

	void bboxDrawn(const Geometry::Rectangle& bbox, int action);
	void addPage(const Glib::ustring& hocrText, HOCRReadSessionData data);
	void editAttribute(const Glib::ustring& path, const Glib::ustring& value);
	void expandCollapseItemClass(bool expand);
	void navigateNextPrev(bool next);
	void navigateTargetChanged();
	void pickItem(const Geometry::Point& point);
	void setFont();
	void setInsertMode(InsertMode mode, const std::string& iconName);
	void setModified();
	void showItemProperties(const Gtk::TreeIter& index, const Gtk::TreeIter& prev = Gtk::TreeIter());
	void showTreeWidgetContextMenu(GdkEventButton* ev);
	void itemAttributeChanged(const Gtk::TreeIter& itemIndex, const Glib::ustring& name, const Glib::ustring& value);
	void updateSourceText();
	void updateAttributes(const Gtk::TreeIter& it, const Glib::ustring& attr, const Glib::ustring& value);
	void updateCurrentItemBBox(const Geometry::Rectangle& bbox);
	void findReplace(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase, bool backwards, bool replace);
	void replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase);
	void applySubstitutions(const std::map<Glib::ustring, Glib::ustring>& substitutions, bool matchCase);
	void sourceChanged();
	void previewToggled();
	void updatePreview();
	void drawPreview(Cairo::RefPtr<Cairo::Context> context, const HOCRItem* item);
};

#endif // OUTPUTEDITORHOCR_HH

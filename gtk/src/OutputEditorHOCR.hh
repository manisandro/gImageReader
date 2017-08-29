/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.hh
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

#ifndef OUTPUTEDITORHOCR_HH
#define OUTPUTEDITORHOCR_HH

#include "OutputEditor.hh"
#include "Geometry.hh"
#include "Image.hh"

#include <gtksourceviewmm.h>
#include <gtkspellmm.h>

class DisplayerImageItem;
class DisplayerToolHOCR;
namespace xmlpp {
class DomParser;
class Element;
}

class OutputEditorHOCR : public OutputEditor {
public:
	OutputEditorHOCR(DisplayerToolHOCR* tool);
	~OutputEditorHOCR();

	Gtk::Box* getUI() override {
		return m_widget;
	}
	ReadSessionData* initRead(tesseract::TessBaseAPI& tess) override;
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const Glib::ustring& errorMsg, ReadSessionData* data) override;
	void finalizeRead(ReadSessionData *data) override;
	bool getModified() const override;

	bool clear(bool hide = true) override;
	void open();
	bool save(const std::string& filename = "") override;
	void savePDF();

private:
	class TreeView;

	static const Glib::RefPtr<Glib::Regex> s_bboxRx;
	static const Glib::RefPtr<Glib::Regex> s_pageTitleRx;
	static const Glib::RefPtr<Glib::Regex> s_idRx;
	static const Glib::RefPtr<Glib::Regex> s_fontSizeRx;
	static const Glib::RefPtr<Glib::Regex> s_baseLineRx;

	struct HOCRReadSessionData : ReadSessionData {
		std::vector<Glib::ustring> errors;
	};

	struct PDFSettings {
		Image::Format colorFormat;
		Image::ConversionFlags conversionFlags;
		enum Compression { CompressZip=0, CompressFax4=1, CompressJpeg=2 } compression; // This order needs to be the same as combo:pdfoptions.compression
		int compressionQuality;
		bool useDetectedFontSizes;
		bool uniformizeLineSpacing;
		int preserveSpaceWidth;
		bool overlay;
		double detectedFontScaling;
	};

	class PDFPainter {
	public:
		virtual void setFontSize(double pointSize) = 0;
		virtual void drawText(double x, double y, const Glib::ustring& text) = 0;
		virtual void drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const PDFSettings& settings) = 0;
		virtual double getAverageCharWidth() const = 0;
		virtual double getTextWidth(const Glib::ustring& text) const = 0;
	};
	class PoDoFoPDFPainter;
	class CairoPDFPainter;


	struct ItemStoreColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<bool> selected;
		Gtk::TreeModelColumn<bool> editable;
		Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
		Gtk::TreeModelColumn<Glib::ustring> text;
		Gtk::TreeModelColumn<Glib::ustring> id;
		Gtk::TreeModelColumn<Glib::ustring> source;
		Gtk::TreeModelColumn<Geometry::Rectangle> bbox;
		Gtk::TreeModelColumn<Glib::ustring> itemClass;
		Gtk::TreeModelColumn<double> fontSize;
		Gtk::TreeModelColumn<int> baseLine;
		Gtk::TreeModelColumn<Glib::ustring> textColor;
		Gtk::TreeModelColumn<bool> checkboxVisible;
		Gtk::TreeModelColumn<bool> iconVisible;
		ItemStoreColumns() {
			add(selected);
			add(editable);
			add(icon);
			add(text);
			add(id);
			add(source);
			add(bbox);
			add(itemClass);
			add(fontSize);
			add(baseLine);
			add(textColor);
			add(checkboxVisible);
			add(iconVisible);
		}
	} m_itemStoreCols;

	struct PropStoreColumns : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<Glib::ustring> parentAttr;
		Gtk::TreeModelColumn<Glib::ustring> value;
		PropStoreColumns() {
			add(name);
			add(parentAttr);
			add(value);
		}
	} m_propStoreCols;

	struct FormatComboColums : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Image::Format> format;
		Gtk::TreeModelColumn<Glib::ustring> label;
		FormatComboColums() {
			add(format);
			add(label);
		}
	} m_formatComboCols;

	struct DitheringComboColums : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Image::ConversionFlags> conversionFlags;
		Gtk::TreeModelColumn<Glib::ustring> label;
		DitheringComboColums() {
			add(conversionFlags);
			add(label);
		}
	} m_ditheringComboCols;

	struct CompressionComboColums : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<PDFSettings::Compression> mode;
		Gtk::TreeModelColumn<Glib::ustring> label;
		Gtk::TreeModelColumn<bool> sensitive;
		CompressionComboColums() {
			add(mode);
			add(label);
			add(sensitive);
		}
	} m_compressionComboCols;

	Builder m_builder;
	Gtk::Box* m_widget = nullptr;
	TreeView* m_itemView;
	Glib::RefPtr<Gtk::TreeStore> m_itemStore;
	Gtk::TreeView* m_propView;
	Glib::RefPtr<Gtk::TreeStore> m_propStore;
	Gsv::View* m_sourceView;
	int m_idCounter = 0;
	DisplayerToolHOCR* m_tool;
	GtkSpell::Checker m_spell;
	bool m_modified = false;
	Gtk::Dialog* m_pdfExportDialog = nullptr;
	DisplayerImageItem* m_preview = nullptr;

	Gtk::TreePath m_rootItem;
	Gtk::TreePath m_currentItem;
	Gtk::TreePath m_currentPageItem;
	xmlpp::DomParser* m_currentParser = nullptr;
	xmlpp::Element* m_currentElement = nullptr;

	sigc::connection m_connectionCustomFont;
	sigc::connection m_connectionDefaultFont;
	sigc::connection m_connectionSelectionChanged;
	sigc::connection m_connectionItemViewRowEdited;
	sigc::connection m_connectionPropViewRowEdited;

	Gtk::TreeIter currentItem();
	void addPage(xmlpp::Element* pageDiv, const Glib::ustring& filename, int page, bool cleanGraphics);
	bool addChildItems(xmlpp::Element* element, Gtk::TreeIter parentItem, std::map<Glib::ustring, Glib::ustring>& langCache);
	void printChildren(PDFPainter& painter, Gtk::TreeIter item, const PDFSettings& pdfSettings, double imgScale = 1.) const;
	bool setCurrentSource(xmlpp::Element* pageElement, int* pageDpi = 0, int* overrideDpi = 0) const;
	void updateCurrentItemText();
	void updateCurrentItemAttribute(const Glib::ustring& key, const Glib::ustring& subkey, const Glib::ustring& newvalue, bool update=true);
	void updateCurrentItem();
	void removeCurrentItem();
	Glib::ustring trimWord(const Glib::ustring& word, Glib::ustring* prefix = nullptr, Glib::ustring* suffix = nullptr);
	void mergeItems(const std::vector<Gtk::TreePath>& items);

	void addGraphicRection(const Geometry::Rectangle& rect);
	void addPage(const Glib::ustring& hocrText, ReadSessionData data);
	void setFont();
	void imageFormatChanged();
	void imageCompressionChanged();
	void showItemProperties(Gtk::TreeIter item);
	void itemChanged(const Gtk::TreeIter& iter);
	void propertyCellChanged(const Gtk::TreeIter& iter);
	void showContextMenu(GdkEventButton* ev);
	void checkCellEditable(const Glib::ustring& path, Gtk::CellRenderer* renderer);
	void updatePreview();
	void updateCurrentItemBBox(const Geometry::Rectangle& rect);
};

#endif // OUTPUTEDITORHOCR_HH

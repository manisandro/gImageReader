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
#include <podofo/base/PdfDictionary.h>
#include <podofo/base/PdfFilter.h>
#include <podofo/base/PdfStream.h>
#include <podofo/doc/PdfFont.h>
#include <podofo/doc/PdfIdentityEncoding.h>
#include <podofo/doc/PdfImage.h>
#include <podofo/doc/PdfPage.h>
#include <podofo/doc/PdfPainter.h>
#include <podofo/doc/PdfStreamedDocument.h>

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
const Glib::RefPtr<Glib::Regex> OutputEditorHOCR::s_baseLineRx = Glib::Regex::create("baseline\\s+(-?\\d+\\.?\\d*)\\s+(-?\\d+)");

static inline Glib::ustring getAttribute(const xmlpp::Element* element, const Glib::ustring& name) {
	if(!element)
		return Glib::ustring();
	xmlpp::Attribute* attrib = element->get_attribute(name);
	return attrib ? attrib->get_value() : Glib::ustring();
}

static inline Glib::ustring getElementText(const xmlpp::Element* element) {
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

static inline xmlpp::Element* getFirstChildElement(xmlpp::Node* node, const Glib::ustring& name = Glib::ustring()) {
	if(!node)
		return nullptr;
	xmlpp::Node* child = node->get_first_child(name);
	while(child && !dynamic_cast<xmlpp::Element*>(child)) {
		child = child->get_next_sibling();
	}
	return child ? dynamic_cast<xmlpp::Element*>(child) : nullptr;
}

static inline xmlpp::Element* getNextSiblingElement(xmlpp::Node* node, const Glib::ustring& name = Glib::ustring()) {
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

static inline Glib::ustring getDocumentXML(xmlpp::Document* doc) {
	Glib::ustring xml = doc->write_to_string();
	// Strip entity declaration
	if(xml.substr(0, 5) == "<?xml") {
		std::size_t pos = xml.find("?>\n");
		xml = xml.substr(pos + 3);
	}
	return xml;
}

static inline Glib::ustring getElementXML(const xmlpp::Element* element) {
	xmlpp::Document doc;
	doc.create_root_node_by_import(element);
	return getDocumentXML(&doc);
}


class OutputEditorHOCR::TreeView : public Gtk::TreeView {
public:
	TreeView(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& /*builder*/)
		: Gtk::TreeView(cobject) {}

	sigc::signal<void, GdkEventButton*> signal_context_menu_requested() {
		return m_signal_context_menu;
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


class OutputEditorHOCR::CairoPDFPainter : public OutputEditorHOCR::PDFPainter {
public:
	CairoPDFPainter(Cairo::RefPtr<Cairo::Context> context) : m_context(context) {
	}
	void setFontSize(double pointSize) override {
		m_context->set_font_size(pointSize);
	}
	void drawText(double x, double y, const Glib::ustring& text) override {
		m_context->move_to(x, y/* - ext.y_bearing*/);
		m_context->show_text(text);
	}
	void drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const PDFSettings& settings) override {
		m_context->save();
		m_context->move_to(bbox.x, bbox.y);
		Cairo::RefPtr<Cairo::ImageSurface> img = Image::simulateFormat(image, settings.colorFormat);
		m_context->set_source(img, bbox.x, bbox.y);
		m_context->paint();
		m_context->restore();
	}
	double getAverageCharWidth() const override {
		Cairo::TextExtents ext;
		m_context->get_text_extents("x ", ext);
		return ext.x_advance - ext.width; // spaces are ignored in width but counted in advance
	}
	double getTextWidth(const Glib::ustring& text) const override {
		Cairo::TextExtents ext;
		m_context->get_text_extents(text, ext);
		return ext.x_advance;
	}

private:
	Cairo::RefPtr<Cairo::Context> m_context;
};

#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0,9,3)
namespace PoDoFo {
class PdfImageCompat : public PoDoFo::PdfImage {
	using PdfImage::PdfImage;
public:
	void SetImageDataRaw( unsigned int nWidth, unsigned int nHeight,
	                      unsigned int nBitsPerComponent, PdfInputStream* pStream ) {
		m_rRect.SetWidth( nWidth );
		m_rRect.SetHeight( nHeight );

		this->GetObject()->GetDictionary().AddKey( "Width",  PdfVariant( static_cast<pdf_int64>(nWidth) ) );
		this->GetObject()->GetDictionary().AddKey( "Height", PdfVariant( static_cast<pdf_int64>(nHeight) ) );
		this->GetObject()->GetDictionary().AddKey( "BitsPerComponent", PdfVariant( static_cast<pdf_int64>(nBitsPerComponent) ) );

		PdfVariant var;
		m_rRect.ToVariant( var );
		this->GetObject()->GetDictionary().AddKey( "BBox", var );

		this->GetObject()->GetStream()->SetRawData( pStream, -1 );
	}
};
}
#endif

class OutputEditorHOCR::PoDoFoPDFPainter : public OutputEditorHOCR::PDFPainter {
public:
	PoDoFoPDFPainter(PoDoFo::PdfDocument* document, PoDoFo::PdfPainter* painter, double scaleFactor, double imageScale)
		: m_document(document), m_painter(painter), m_scaleFactor(scaleFactor), m_imageScale(imageScale) {
		m_pageHeight = m_painter->GetPage()->GetPageSize().GetHeight();
	}
	void setFontSize(double pointSize) override {
		m_painter->GetFont()->SetFontSize(pointSize);
	}
	void drawText(double x, double y, const Glib::ustring& text) override {
		PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*>(text.c_str()));
		m_painter->DrawText(x * m_scaleFactor, m_pageHeight - y * m_scaleFactor, pdfString);
	}
	void drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const PDFSettings& settings) override {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
		PoDoFo::PdfImage pdfImage(m_document);
#else
		PoDoFo::PdfImageCompat pdfImage(m_document);
#endif
		Cairo::RefPtr<Cairo::ImageSurface> scaledImage = Image::scale(image, m_imageScale);
		pdfImage.SetImageColorSpace(settings.colorFormat == Image::Format_RGB24 ? PoDoFo::ePdfColorSpace_DeviceRGB : PoDoFo::ePdfColorSpace_DeviceGray);
		Image img(scaledImage, settings.colorFormat);
		if(settings.compression == PDFSettings::CompressZip) {
			PoDoFo::PdfMemoryInputStream is(reinterpret_cast<const char*>(img.data), img.bytesPerLine * img.height);
			pdfImage.SetImageData(img.width, img.height, img.sampleSize, &is, {PoDoFo::ePdfFilter_FlateDecode});
		} else {
			PoDoFo::PdfName dctFilterName(PoDoFo::PdfFilterFactory::FilterTypeToName(PoDoFo::ePdfFilter_DCTDecode));
			pdfImage.GetObject()->GetDictionary().AddKey(PoDoFo::PdfName::KeyFilter, dctFilterName);
			uint8_t* buf = nullptr;
			unsigned long bufLen = 0;
			img.writeJpeg(settings.compressionQuality, buf, bufLen);
			PoDoFo::PdfMemoryInputStream is(reinterpret_cast<const char*>(buf), bufLen);
			pdfImage.SetImageDataRaw(img.width, img.height, img.sampleSize, &is);
			std::free(buf);
		}
		m_painter->DrawImage(bbox.x * m_scaleFactor, m_pageHeight - (bbox.y + bbox.height) * m_scaleFactor, &pdfImage, m_scaleFactor / m_imageScale, m_scaleFactor / m_imageScale);
	}
	double getAverageCharWidth() const override {
		return m_painter->GetFont()->GetFontMetrics()->CharWidth(static_cast<unsigned char>('x')) / m_scaleFactor;
	}
	double getTextWidth(const Glib::ustring& text) const override {
		PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*>(text.c_str()));
		return m_painter->GetFont()->GetFontMetrics()->StringWidth(pdfString) / m_scaleFactor;
	}

private:
	PoDoFo::PdfDocument* m_document;
	PoDoFo::PdfPainter* m_painter;
	double m_scaleFactor;
	double m_imageScale;
	double m_pageHeight;
};


OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool)
	: m_builder("/org/gnome/gimagereader/editor_hocr.ui") {
	m_tool = tool;
	m_widget = m_builder("box:hocr");

	Gtk::Button* openButton = m_builder("button:hocr.open");
	Gtk::Button* saveButton = m_builder("button:hocr.save");
	Gtk::Button* clearButton = m_builder("button:hocr.clear");
	Gtk::Button* exportButton = m_builder("button:hocr.export");
	m_itemView = nullptr;
	m_builder.get_derived("treeview:hocr.items", m_itemView);
	m_itemStore = Gtk::TreeStore::create(m_itemStoreCols);
	m_itemView->set_model(m_itemStore);
	m_itemView->get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
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
	Gtk::TreeView_Private::_connect_auto_store_editable_signal_handler<Glib::ustring>(m_propView, valueRenderer, m_propStoreCols.value);
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

	Gtk::ComboBox* imageFormatCombo = m_builder("combo:pdfoptions.imageformat");
	Glib::RefPtr<Gtk::ListStore> formatComboModel = Gtk::ListStore::create(m_formatComboCols);
	imageFormatCombo->set_model(formatComboModel);
	Gtk::TreeModel::Row row = *(formatComboModel->append());
	row[m_formatComboCols.format] = Image::Format_RGB24;
	row[m_formatComboCols.label] = _("Color");
	row = *(formatComboModel->append());
	row[m_formatComboCols.format] = Image::Format_Gray8;
	row[m_formatComboCols.label] = _("Grayscale");
	row = *(formatComboModel->append());
	row[m_formatComboCols.format] = Image::Format_Mono;
	row[m_formatComboCols.label] = _("Monochrome");
	imageFormatCombo->pack_start(m_formatComboCols.label);
	imageFormatCombo->set_active(-1);

	Gtk::ComboBox* compressionCombo = m_builder("combo:pdfoptions.compression");
	Glib::RefPtr<Gtk::ListStore> compressionModel = Gtk::ListStore::create(m_compressionComboCols);
	compressionCombo->set_model(compressionModel);
	row = *(compressionModel->append());
	row[m_compressionComboCols.mode] = PDFSettings::CompressZip;
	row[m_compressionComboCols.label] = _("Zip (lossless)");
	row = *(compressionModel->append());
	row[m_compressionComboCols.mode] = PDFSettings::CompressJpeg;
	row[m_compressionComboCols.label] = _("Jpeg (lossy)");
	compressionCombo->pack_start(m_compressionComboCols.label);
	compressionCombo->set_active(-1);

	CONNECT(openButton, clicked, [this] { open(); });
	CONNECT(saveButton, clicked, [this] { save(); });
	CONNECT(clearButton, clicked, [this] { clear(); });
	CONNECT(exportButton, clicked, [this] { savePDF(); });
	m_connectionCustomFont = CONNECTP(MAIN->getWidget("fontbutton:config.settings.customoutputfont").as<Gtk::FontButton>(), font_name, [this] { setFont(); });
	m_connectionDefaultFont = CONNECT(MAIN->getWidget("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>(), toggled, [this] { setFont(); });
	m_connectionSelectionChanged = CONNECT(m_itemView->get_selection(), changed, [this] { showItemProperties(currentItem()); });
	m_connectionItemViewRowEdited = CONNECT(m_itemStore, row_changed, [this](const Gtk::TreeModel::Path&, const Gtk::TreeIter& iter) {
		itemChanged(iter);
	});
	m_connectionPropViewRowEdited = CONNECT(m_propStore, row_changed, [this](const Gtk::TreeModel::Path&, const Gtk::TreeIter& iter) {
		propertyCellChanged(iter);
	});
	CONNECT(m_itemView, context_menu_requested, [this](GdkEventButton* ev) {
		showContextMenu(ev);
	});
	CONNECT(m_tool, selection_geometry_changed, [this](const Geometry::Rectangle& rect) {
		updateCurrentItemBBox(rect);
	});
	CONNECT(m_tool, selection_drawn, [this](const Geometry::Rectangle& rect) {
		addGraphicRection(rect);
	});

	CONNECT(m_builder("combo:pdfoptions.mode").as<Gtk::ComboBox>(), changed, [this] { updatePreview(); });
	CONNECT(imageFormatCombo, changed, [this] { imageFormatChanged(); updatePreview(); });
	CONNECT(compressionCombo, changed, [this] { imageCompressionChanged(); });
	CONNECT(m_builder("fontbutton:pdfoptions").as<Gtk::FontButton>(), font_set, [this] { updatePreview(); });
	CONNECT(m_builder("checkbox:pdfoptions.usedetectedfontsizes").as<Gtk::CheckButton>(), toggled, [this] { updatePreview(); });
	CONNECTS(m_builder("checkbox:pdfoptions.usedetectedfontsizes").as<Gtk::CheckButton>(), toggled, [this](Gtk::CheckButton* button) {
		updatePreview();
		m_builder("box:pdfoptions.fontscale")->set_sensitive(button->get_active());
	});
	CONNECT(m_builder("spin:pdfoptions.fontscale").as<Gtk::SpinButton>(), value_changed, [this] { updatePreview(); });
	CONNECTS(m_builder("checkbox:pdfoptions.uniformlinespacing").as<Gtk::CheckButton>(), toggled, [this](Gtk::CheckButton* button) {
		updatePreview();
		m_builder("box:pdfoptions.preserve")->set_sensitive(button->get_active());
	});
	CONNECT(m_builder("spin:pdfoptions.preserve").as<Gtk::SpinButton>(), value_changed, [this] { updatePreview(); });
	CONNECT(m_builder("checkbox:pdfoptions.preview").as<Gtk::CheckButton>(), toggled, [this] { updatePreview(); });

	if(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue().empty()) {
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Utils::get_documents_dir());
	}

	MAIN->getConfig()->addSetting(new ComboSetting("pdfexportmode", m_builder("combo:pdfoptions.mode")));
	MAIN->getConfig()->addSetting(new SpinSetting("pdfimagecompressionquality", m_builder("spin:pdfoptions.quality")));
	MAIN->getConfig()->addSetting(new ComboSetting("pdfimagecompression", m_builder("combo:pdfoptions.compression")));
	MAIN->getConfig()->addSetting(new ComboSetting("pdfimageformat", m_builder("combo:pdfoptions.imageformat")));
	MAIN->getConfig()->addSetting(new SpinSetting("pdfimagedpi", m_builder("spin:pdfoptions.dpi")));
	MAIN->getConfig()->addSetting(new FontSetting("pdffont", m_builder("fontbutton:pdfoptions")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckButton>("pdfusedetectedfontsizes", m_builder("checkbox:pdfoptions.usedetectedfontsizes")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckButton>("pdfuniformizelinespacing", m_builder("checkbox:pdfoptions.uniformlinespacing")));
	MAIN->getConfig()->addSetting(new SpinSetting("pdfpreservespaces", m_builder("spin:pdfoptions.preserve")));
	MAIN->getConfig()->addSetting(new SpinSetting("pdffontscale", m_builder("spin:pdfoptions.fontscale")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckButton>("pdfpreview", m_builder("checkbox:pdfoptions.preview")));

#ifndef MAKE_VERSION
#define MAKE_VERSION(...) 0
#endif
#if !defined(TESSERACT_VERSION) || TESSERACT_VERSION < MAKE_VERSION(3,04,00)
	m_builder("checkbox:pdfoptions.usedetectedfontsizes").as<Gtk::CheckButton>()->set_active(false);
	m_builder("checkbox:pdfoptions.usedetectedfontsizes")->set_visible(false);
	m_builder("box:pdfoptions.fontscale")->set_visible(false);
#endif

	setFont();
}

OutputEditorHOCR::~OutputEditorHOCR() {
	delete m_currentParser;
	m_connectionCustomFont.disconnect();
	m_connectionDefaultFont.disconnect();
	MAIN->getConfig()->removeSetting("pdfexportmode");
	MAIN->getConfig()->removeSetting("pdfimagecompressionquality");
	MAIN->getConfig()->removeSetting("pdfimagecompression");
	MAIN->getConfig()->removeSetting("pdfimageformat");
	MAIN->getConfig()->removeSetting("pdfimagedpi");
	MAIN->getConfig()->removeSetting("pdffont");
	MAIN->getConfig()->removeSetting("pdfusedetectedfontsizes");
	MAIN->getConfig()->removeSetting("pdfuniformizelinespacing");
	MAIN->getConfig()->removeSetting("pdfpreservespaces");
	MAIN->getConfig()->removeSetting("pdffontscale");
	MAIN->getConfig()->removeSetting("pdfpreview");
}

void OutputEditorHOCR::setFont() {
	if(MAIN->getWidget("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>()->get_active()) {
		m_builder("textview:hocr.source").as<Gtk::TextView>()->unset_font();
	} else {
		Gtk::FontButton* fontBtn = MAIN->getWidget("fontbutton:config.settings.customoutputfont");
		m_builder("textview:hocr.source").as<Gtk::TextView>()->override_font(Pango::FontDescription(fontBtn->get_font_name()));
	}
}

void OutputEditorHOCR::imageFormatChanged() {
	Image::Format format = (*m_builder("combo:pdfoptions.imageformat").as<Gtk::ComboBox>()->get_active())[m_formatComboCols.format];
	if(format == Image::Format_Mono) {
		m_builder("combo:pdfoptions.compression").as<Gtk::ComboBox>()->set_active(0);
		m_builder("combo:pdfoptions.compression").as<Gtk::Widget>()->set_sensitive(false);
		m_builder("label:pdfoptions.compression").as<Gtk::Widget>()->set_sensitive(false);
	} else {
		m_builder("combo:pdfoptions.compression").as<Gtk::Widget>()->set_sensitive(true);
		m_builder("label:pdfoptions.compression").as<Gtk::Widget>()->set_sensitive(true);
	}
}

void OutputEditorHOCR::imageCompressionChanged() {
	PDFSettings::Compression compression = (*m_builder("combo:pdfoptions.compression").as<Gtk::ComboBox>()->get_active())[m_compressionComboCols.mode];
	bool zipCompression = compression == PDFSettings::CompressZip;
	m_builder("spin:pdfoptions.quality").as<Gtk::Widget>()->set_sensitive(!zipCompression);
	m_builder("label:pdfoptions.quality").as<Gtk::Widget>()->set_sensitive(!zipCompression);
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

void OutputEditorHOCR::readError(const Glib::ustring &errorMsg, ReadSessionData *data) {
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
	addPage(pageDiv, Gio::File::create_for_path(data.file)->get_basename(), data.page, true);
}

void OutputEditorHOCR::addPage(xmlpp::Element* pageDiv, const Glib::ustring& filename, int page, bool cleanGraphics) {
	m_connectionItemViewRowEdited.block(true);
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
#if GTKMM_CHECK_VERSION(3,12,0)
	pageItem->set_value(m_itemStoreCols.icon, Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_page.png"));
#else
	pageItem->set_value(m_itemStoreCols.icon, Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/item_page.png", 0)));
#endif
	pageItem->set_value(m_itemStoreCols.selected, true);
	pageItem->set_value(m_itemStoreCols.editable, false);
	pageItem->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));

	std::map<Glib::ustring,Glib::ustring> langCache;

	std::vector<std::pair<xmlpp::Element*,Geometry::Rectangle>> graphicElements;
	xmlpp::Element* element = getFirstChildElement(pageDiv, "div");
	while(element) {
		// Boxes without text are images
		titleAttr = getAttribute(element, "title");
		if(!addChildItems(getFirstChildElement(element), pageItem, langCache) && s_bboxRx->match(titleAttr, matchInfo)) {
			x1 = std::atoi(matchInfo.fetch(1).c_str());
			y1 = std::atoi(matchInfo.fetch(2).c_str());
			x2 = std::atoi(matchInfo.fetch(3).c_str());
			y2 = std::atoi(matchInfo.fetch(4).c_str());
			graphicElements.push_back(std::make_pair(element, Geometry::Rectangle(x1, y1, x2-x1, y2-y1)));
		}
		element = getNextSiblingElement(element);
	}

	// Discard graphic elements which intersect with text block or which are too small
	int numTextBlocks = pageItem->children().size();
	for(const std::pair<xmlpp::Element*,Geometry::Rectangle>& pair : graphicElements) {
		xmlpp::Element* element = pair.first;
		const Geometry::Rectangle& bbox = pair.second;
		bool deleteGraphic = false;
		if(cleanGraphics) {
			if(bbox.width < 10 || bbox.height < 10) {
				deleteGraphic = true;
			} else {
				for(int i = 0; i < numTextBlocks; ++i) {
					if(bbox.overlaps((*pageItem->children()[i])[m_itemStoreCols.bbox])) {
						deleteGraphic = true;
						break;
					}
				}
			}
		}
		if(!deleteGraphic) {
			Gtk::TreeIter item = m_itemStore->append(pageItem->children());
			item->set_value(m_itemStoreCols.text, Glib::ustring(_("Graphic")));
			item->set_value(m_itemStoreCols.selected, true);
			item->set_value(m_itemStoreCols.editable, false);
#if GTKMM_CHECK_VERSION(3,12,0)
			item->set_value(m_itemStoreCols.icon, Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_halftone.png"));
#else
			item->set_value(m_itemStoreCols.icon, Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/item_halftone.png", 0)));
#endif
			item->set_value(m_itemStoreCols.id, getAttribute(element, "id"));
			item->set_value(m_itemStoreCols.itemClass, Glib::ustring("ocr_graphic"));
			item->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
			item->set_value(m_itemStoreCols.bbox, Geometry::Rectangle(x1, y1, x2-x1, y2-y1));
		} else {
			element->get_parent()->remove_child(element);
		}
	}
	pageItem->set_value(m_itemStoreCols.source, getElementXML(pageDiv));
	m_itemView->expand_row(Gtk::TreePath(pageItem), true);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
	m_connectionItemViewRowEdited.block(false);
	m_builder("button:hocr.save")->set_sensitive(true);
	m_builder("button:hocr.export")->set_sensitive(true);
}

Gtk::TreeIter OutputEditorHOCR::currentItem() {
	std::vector<Gtk::TreePath> items = m_itemView->get_selection()->get_selected_rows();
	if(!items.empty()) {
		return m_itemStore->get_iter(items[0]);
	}
	return Gtk::TreeIter();
}

bool OutputEditorHOCR::addChildItems(xmlpp::Element* element, Gtk::TreeIter parentItem, std::map<Glib::ustring,Glib::ustring>& langCache) {
	bool haveWord = false;
	while(element) {
		xmlpp::Element* nextElement = getNextSiblingElement(element);
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
				if(type == "ocrx_word" || addChildItems(getFirstChildElement(element), item, langCache)) {
					item->set_value(m_itemStoreCols.selected, true);
					item->set_value(m_itemStoreCols.id, getAttribute(element, "id"));
					if(!icon.empty()) {
#if GTKMM_CHECK_VERSION(3,12,0)
						item->set_value(m_itemStoreCols.icon, Gdk::Pixbuf::create_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/item_%1.png", icon)));
#else
						item->set_value(m_itemStoreCols.icon, Glib::wrap(gdk_pixbuf_new_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/item_%1.png", icon).c_str(), 0)));
#endif
					}
					item->set_value(m_itemStoreCols.bbox, Geometry::Rectangle(x1, y1, x2 - x1, y2 - y1));
					item->set_value(m_itemStoreCols.itemClass, type);
					item->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
					item->set_value(m_itemStoreCols.editable, type == "ocrx_word");
					haveWord = true;
					if(type == "ocr_line") {
						if(s_baseLineRx->match(titleAttr, matchInfo)) {
							item->set_value(m_itemStoreCols.baseLine, std::atoi(matchInfo.fetch(2).c_str()));
						}
					} else if(type == "ocrx_word") {
						// Ensure correct hyphen char is used on last word of line
						if(!nextElement) {
							title = Glib::Regex::create("[-\u2014]\\s*$")->replace(title, 0, "-", static_cast<Glib::RegexMatchFlags>(0));
							element->remove_child(element->get_first_child());
							element->add_child_text(title);
						}

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
						if(!m_spell.check_word(trimWord(title))) {
							item->set_value(m_itemStoreCols.textColor, Glib::ustring("#F00"));
						}
					}
					item->set_value(m_itemStoreCols.text, title);
				} else {
					m_itemStore->erase(item);
				}
			}
		}
		element = nextElement;
	}
	return haveWord;
}

void OutputEditorHOCR::showItemProperties(Gtk::TreeIter item) {
	m_connectionPropViewRowEdited.block(true);
	m_propStore->clear();
	m_connectionPropViewRowEdited.block(false);
	m_sourceView->get_buffer()->set_text("");
	m_tool->clearSelection();
	m_currentPageItem = Gtk::TreePath();
	m_currentItem = Gtk::TreePath();
	m_currentElement = nullptr;
	delete m_currentParser;
	m_currentParser = nullptr;
	if(!item) {
		return;
	}
	m_currentItem = m_itemStore->get_path(item);
	Gtk::TreeIter parentItem = item;
	while(parentItem->parent()) {
		parentItem = parentItem->parent();
	}
	m_currentPageItem = m_itemStore->get_path(parentItem);
	Glib::ustring id = (*item)[m_itemStoreCols.id];
	m_currentParser = new xmlpp::DomParser();
	m_currentParser->parse_memory((*parentItem)[m_itemStoreCols.source]);
	xmlpp::Document* doc = m_currentParser->get_document();
	if(doc->get_root_node()) {
		xmlpp::NodeSet nodes = doc->get_root_node()->find(Glib::ustring::compose("//*[@id='%1']", id));
		m_currentElement = nodes.empty() ? nullptr : dynamic_cast<xmlpp::Element*>(nodes.front());
	}
	if(!m_currentElement) {
		delete m_currentParser;
		m_currentParser = nullptr;
		m_currentPageItem = Gtk::TreePath();
		m_currentItem = Gtk::TreePath();
	}

	m_connectionPropViewRowEdited.block(true);
	for(xmlpp::Attribute* attrib : m_currentElement->get_attributes()) {
		if(attrib->get_name() == "title") {
			for(Glib::ustring attr : Utils::string_split(attrib->get_value(), ';')) {
				attr = Utils::string_trim(attr);
				std::size_t splitPos = attr.find(" ");
				Gtk::TreeIter item = m_propStore->append();
				item->set_value(m_propStoreCols.parentAttr, Glib::ustring("title"));
				item->set_value(m_propStoreCols.name, Utils::string_trim(attr.substr(0, splitPos)));
				item->set_value(m_propStoreCols.value, Utils::string_trim(attr.substr(splitPos + 1)));
			}
		} else {
			Gtk::TreeIter item = m_propStore->append();
			item->set_value(m_propStoreCols.name, attrib->get_name());
			item->set_value(m_propStoreCols.value, attrib->get_value());
		}
	}
	m_connectionPropViewRowEdited.block(false);
	m_sourceView->get_buffer()->set_text(getElementXML(m_currentElement));

	Glib::MatchInfo matchInfo;
	xmlpp::Element* pageElement = dynamic_cast<xmlpp::Element*>(m_currentParser->get_document()->get_root_node());
	Glib::ustring titleAttr = getAttribute(m_currentElement, "title");
	if(pageElement && pageElement->get_name() == "div" && setCurrentSource(pageElement) && s_bboxRx->match(titleAttr, matchInfo)) {
		int x1 = std::atoi(matchInfo.fetch(1).c_str());
		int y1 = std::atoi(matchInfo.fetch(2).c_str());
		int x2 = std::atoi(matchInfo.fetch(3).c_str());
		int y2 = std::atoi(matchInfo.fetch(4).c_str());
		m_tool->setSelection(Geometry::Rectangle(x1, y1, x2-x1, y2-y1));
	}
}

bool OutputEditorHOCR::setCurrentSource(xmlpp::Element* pageElement, int* pageDpi) const {
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
		while(Gtk::Main::events_pending()) {
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

void OutputEditorHOCR::itemChanged(const Gtk::TreeIter& iter) {
	if(m_itemStore->get_path(iter) != m_currentItem) {
		return;
	}
	m_connectionItemViewRowEdited.block(true);
	bool isWord = (*iter)[m_itemStoreCols.itemClass] == "ocrx_word";
	bool selected = (*iter)[m_itemStoreCols.selected];
	if( isWord && selected) {
		// Update text
		updateCurrentItemText();
	} else if(!selected) {
		m_itemView->collapse_row(m_itemStore->get_path(iter));
	}
	m_connectionItemViewRowEdited.block(false);
}

void OutputEditorHOCR::propertyCellChanged(const Gtk::TreeIter &iter) {
	Glib::ustring parentAttr = (*iter)[m_propStoreCols.parentAttr];
	Glib::ustring key = (*iter)[m_propStoreCols.name];
	Glib::ustring value = (*iter)[m_propStoreCols.value];
	if(!parentAttr.empty()) {
		updateCurrentItemAttribute(parentAttr, key, value);
	} else {
		updateCurrentItemAttribute(key, "", value);
	}
}

void OutputEditorHOCR::updateCurrentItemText() {
	if(m_currentItem) {
		Gtk::TreeIter item = m_itemStore->get_iter(m_currentItem);
		Glib::ustring newText = (*item)[m_itemStoreCols.text];
		m_currentElement->remove_child(m_currentElement->get_first_child());
		m_currentElement->add_child_text(newText);
		updateCurrentItem();
	}
}

void OutputEditorHOCR::updateCurrentItemAttribute(const Glib::ustring& key, const Glib::ustring& subkey, const Glib::ustring& newvalue, bool update) {
	if(m_currentItem) {
		if(subkey.empty()) {
			m_currentElement->set_attribute(key, newvalue);
		} else {
			Glib::ustring value = getAttribute(m_currentElement, key);
			std::vector<Glib::ustring> subattrs = Utils::string_split(value, ';');
			for(int i = 0, n = subattrs.size(); i < n; ++i) {
				Glib::ustring attr = Utils::string_trim(subattrs[i]);
				std::size_t splitPos = attr.find(" ");
				if(attr.substr(0, splitPos) == subkey) {
					subattrs[i] = subkey + " " + newvalue;
					break;
				}
			}
			m_currentElement->set_attribute(key, Utils::string_join(subattrs, ";"));
		}
		if(update) {
			updateCurrentItem();
		}
	}
}

void OutputEditorHOCR::updateCurrentItemBBox(const Geometry::Rectangle &rect) {
	if(m_currentItem) {
		Gtk::TreeIter item = m_itemStore->get_iter(m_currentItem);
		m_connectionItemViewRowEdited.block(true);
		item->set_value(m_itemStoreCols.bbox, rect);
		m_connectionItemViewRowEdited.block(false);
		Glib::ustring bboxstr = Glib::ustring::compose("%1 %2 %3 %4", rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
		for(Gtk::TreeIter it : m_propStore->children()) {
			if((*it)[m_propStoreCols.name] == "bbox" and (*it)[m_propStoreCols.parentAttr] == "title") {
				m_connectionPropViewRowEdited.block(true);
				(*it)[m_propStoreCols.value] = bboxstr;
				m_connectionPropViewRowEdited.block(false);
				break;
			}
		}
		updateCurrentItemAttribute("title", "bbox", bboxstr, false);
		Gtk::TreeIter toplevelItem = m_itemStore->get_iter(m_currentPageItem);
		toplevelItem->set_value(m_itemStoreCols.source, getDocumentXML(m_currentParser->get_document()));

		m_sourceView->get_buffer()->set_text(getElementXML(m_currentElement));
	}
}

void OutputEditorHOCR::updateCurrentItem() {
	Gtk::TreeIter item = m_itemStore->get_iter(m_currentItem);
	Glib::ustring spellLang = Utils::getSpellingLanguage(getAttribute(m_currentElement, "lang"));
	if(m_spell.get_language() != spellLang) {
		m_spell.set_language(spellLang);
	}
	m_connectionItemViewRowEdited.block(true); // prevent row edited signal
	if(m_spell.check_word(trimWord((*item)[m_itemStoreCols.text]))) {
		item->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
	} else {
		item->set_value(m_itemStoreCols.textColor, Glib::ustring("#F00"));
	}
	m_connectionItemViewRowEdited.block(false);

	Gtk::TreeIter toplevelItem = m_itemStore->get_iter(m_currentPageItem);
	toplevelItem->set_value(m_itemStoreCols.source, getDocumentXML(m_currentParser->get_document()));

	m_sourceView->get_buffer()->set_text(getElementXML(m_currentElement));

	Glib::MatchInfo matchInfo;
	xmlpp::Element* pageElement = dynamic_cast<xmlpp::Element*>(m_currentParser->get_document()->get_root_node());
	Glib::ustring titleAttr = getAttribute(m_currentElement, "title");
	if(pageElement && pageElement->get_name() == "div" && setCurrentSource(pageElement) && s_bboxRx->match(titleAttr, matchInfo)) {
		int x1 = std::atoi(matchInfo.fetch(1).c_str());
		int y1 = std::atoi(matchInfo.fetch(2).c_str());
		int x2 = std::atoi(matchInfo.fetch(3).c_str());
		int y2 = std::atoi(matchInfo.fetch(4).c_str());
		m_tool->setSelection(Geometry::Rectangle(x1, y1, x2-x1, y2-y1));
	}

	m_modified = true;
}

void OutputEditorHOCR::removeCurrentItem() {
	if(m_currentItem) {
		g_assert_nonnull(m_currentElement);
		m_currentElement->get_parent()->remove_child(m_currentElement);
		Gtk::TreeIter toplevelItem = m_itemStore->get_iter(m_currentPageItem);
		toplevelItem->set_value(m_itemStoreCols.source, getDocumentXML(m_currentParser->get_document()));

		m_itemStore->erase(m_itemStore->get_iter(m_currentItem));
		// m_currentItem updated by m_itemView->get_selection()->signal_changed()
	}
}

void OutputEditorHOCR::addGraphicRection(const Geometry::Rectangle &rect) {
	if(!m_currentParser) {
		return;
	}
	xmlpp::Document* doc = m_currentParser->get_document();
	xmlpp::Element* pageDiv = dynamic_cast<xmlpp::Element*>(doc ? doc->get_root_node() : nullptr);
	if(!pageDiv || pageDiv->get_name() != "div")
		return;

	// Determine a free block id
	int pageId = 0;
	int blockId = 0;
	xmlpp::Element* blockEl = getFirstChildElement(pageDiv, "div");
	while(blockEl) {
		Glib::MatchInfo matchInfo;
		Glib::ustring idAttr = getAttribute(blockEl, "id");
		if(s_idRx->match(idAttr, matchInfo)) {
			pageId = std::max(pageId, std::atoi(matchInfo.fetch(1).c_str()) + 1);
			blockId = std::max(blockId, std::atoi(matchInfo.fetch(2).c_str()) + 1);
		}
		blockEl = getNextSiblingElement(blockEl);
	}

	// Add html element
	xmlpp::Element* graphicElement = pageDiv->add_child("div");
	graphicElement->set_attribute("title", Glib::ustring::compose("bbox %1 %2 %3 %4", rect.x, rect.y, rect.x + rect.width, rect.y + rect.height));
	graphicElement->set_attribute("class", "ocr_carea");
	graphicElement->set_attribute("id", Glib::ustring::compose("block_%1_%2", pageId, blockId));
	Gtk::TreeIter toplevelItem = m_itemStore->get_iter(m_currentPageItem);
	toplevelItem->set_value(m_itemStoreCols.source, getDocumentXML(m_currentParser->get_document()));

	// Add tree item
	Gtk::TreeIter item = m_itemStore->append(toplevelItem->children());
	item->set_value(m_itemStoreCols.text, Glib::ustring(_("Graphic")));
	item->set_value(m_itemStoreCols.selected, true);
	item->set_value(m_itemStoreCols.editable, false);
#if GTKMM_CHECK_VERSION(3,12,0)
	item->set_value(m_itemStoreCols.icon, Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/item_halftone.png"));
#else
	item->set_value(m_itemStoreCols.icon, Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/item_halftone.png", 0)));
#endif
	item->set_value(m_itemStoreCols.id, getAttribute(graphicElement, "id"));
	item->set_value(m_itemStoreCols.itemClass, Glib::ustring("ocr_graphic"));
	item->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
	item->set_value(m_itemStoreCols.bbox, rect);

	m_itemView->get_selection()->unselect_all();
	m_itemView->get_selection()->select(item);
	m_itemView->scroll_to_row(m_itemStore->get_path(item));
}

Glib::ustring OutputEditorHOCR::trimWord(const Glib::ustring& word, Glib::ustring* prefix, Glib::ustring* suffix) {
	Glib::RefPtr<Glib::Regex> re = Glib::Regex::create("^(\\W*)(.*\\w)(\\W*)$");
	Glib::MatchInfo match_info;
	if(re->match(word, -1, 0, match_info, static_cast<Glib::RegexMatchFlags>(0))) {
		if(prefix)
			*prefix = match_info.fetch(1);
		if(suffix)
			*suffix = match_info.fetch(3);
		return match_info.fetch(2);
	}
	return word;
}

void OutputEditorHOCR::mergeItems(const std::vector<Gtk::TreePath>& items) {
	Gtk::TreeIter it = m_itemStore->get_iter(items.front());
	if(!it) {
		return;
	}

	Geometry::Rectangle bbox = (*it)[m_itemStoreCols.bbox];
	Glib::ustring text = (*it)[m_itemStoreCols.text];

	xmlpp::Document* doc = m_currentParser->get_document();

	for(int i = 1, n = items.size(); i < n; ++i) {
		it = m_itemStore->get_iter(items[i]);
		if(it) {
			bbox = bbox.unite((*it)[m_itemStoreCols.bbox]);
			text += (*it)[m_itemStoreCols.text];
			Glib::ustring id = (*it)[m_itemStoreCols.id];
			xmlpp::NodeSet nodes = doc->get_root_node()->find(Glib::ustring::compose("//*[@id='%1']", id));
			xmlpp::Element* element = nodes.empty() ? nullptr : dynamic_cast<xmlpp::Element*>(nodes.front());
			element->get_parent()->remove_child(element);
		}
	}

	for(int i = 1, n = items.size(); i < n; ++i) {
		it = m_itemStore->get_iter(items[n - i]);
		if(it) {
			m_itemStore->erase(it);
		}
	}

	m_itemView->get_selection()->unselect_all();
	m_itemView->get_selection()->select(items.front());

	it = m_itemStore->get_iter(items.front());
	(*it)[m_itemStoreCols.text] = text;
	(*it)[m_itemStoreCols.bbox] = bbox;
	updateCurrentItemText();
	updateCurrentItemAttribute("title", "bbox", Glib::ustring::compose("%1 %2 %3 %4", bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height));
	showItemProperties(m_itemStore->get_iter(m_currentItem));
}

void OutputEditorHOCR::showContextMenu(GdkEventButton* ev) {
	std::vector<Gtk::TreePath> items = m_itemView->get_selection()->get_selected_rows();
	bool wordsSelected = true;
	for(const Gtk::TreePath& path : items) {
		Gtk::TreeIter it = m_itemStore->get_iter(path);
		if(it) {
			Glib::ustring itemClass = (*it)[m_itemStoreCols.itemClass];
			if(itemClass != "ocrx_word") {
				wordsSelected = false;
				break;
			}
		}
	}
	bool consecutive = true;
	for(int i = 1, n = items.size(); i < n; ++i) {
		Gtk::TreePath path = items[i];
		path.prev();
		if(path != items[i - 1]) {
			consecutive = false;
			break;
		}
	}
	if(items.size() > 1 && consecutive && wordsSelected) {
		Gtk::Menu menu;
		Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
		Gtk::MenuItem* mergeItem = Gtk::manage(new Gtk::MenuItem(_("Merge")));
		menu.append(*mergeItem);
		CONNECT(mergeItem, activate, [&] { mergeItems(items); });
		CONNECT(&menu, hide, [&] { loop->quit(); });
		menu.show_all();
		menu.popup(ev->button, ev->time);
		loop->run();
		return;
	} else if(items.size() > 1) {
		return;
	}
	Gtk::TreePath path;
	Gtk::TreeViewColumn* col;
	int cell_x, cell_y;
	m_itemView->get_path_at_pos(int(ev->x), int(ev->y), path, col, cell_x, cell_y);
	if(!path) {
		return;
	}
	Gtk::TreeIter it = m_itemStore->get_iter(path);
	if(!it) {
		return;
	}
	Glib::ustring itemClass = (*it)[m_itemStoreCols.itemClass];

	if(itemClass == "ocr_page") {
		// Context menu on page items with Remove option
		Gtk::Menu menu;
		Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
		Gtk::MenuItem* addGraphicItem = Gtk::manage(new Gtk::MenuItem(_("Add graphic region")));
		menu.append(*addGraphicItem);
		menu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
		Gtk::MenuItem* removeItem = Gtk::manage(new Gtk::MenuItem(_("Remove")));
		menu.append(*removeItem);
		CONNECT(removeItem, activate, [&] {
			m_itemStore->erase(it);
			m_connectionPropViewRowEdited.block(true);
			m_propStore->clear();
			m_connectionPropViewRowEdited.block(false);
			m_builder("button:hocr.save")->set_sensitive(!m_itemStore->children().empty());
			m_builder("button:hocr.export")->set_sensitive(!m_itemStore->children().empty());
		});
		CONNECT(addGraphicItem, activate, [this] {
			m_tool->clearSelection();
			m_tool->activateDrawSelection();
		});
		CONNECT(&menu, hide, [&] { loop->quit(); });
		menu.show_all();
		menu.popup(ev->button, ev->time);
		loop->run();
		return;
	} else {
		// Context menu on word items with spelling suggestions, if any
		Gtk::Menu menu;
		Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
		if(itemClass == "ocrx_word") {
			Glib::ustring prefix, suffix, trimmed = trimWord((*it)[m_itemStoreCols.text], &prefix, &suffix);
			for(const Glib::ustring& suggestion : m_spell.get_suggestions(trimmed)) {
				Glib::ustring replacement = prefix + suggestion + suffix;
				Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(replacement));
				CONNECT(item, activate, [this, replacement, it] { (*it)[m_itemStoreCols.text] = replacement; });
				menu.append(*item);
			}
			if(menu.get_children().empty()) {
				Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(_("No suggestions")));
				item->set_sensitive(false);
				menu.append(*item);
			}
			if(!m_spell.check_word(trimWord((*it)[m_itemStoreCols.text]))) {
				menu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
				Gtk::MenuItem* additem = Gtk::manage(new Gtk::MenuItem(_("Add to dictionary")));
				CONNECT(additem, activate, [this, it] {
					m_spell.add_to_dictionary((*it)[m_itemStoreCols.text]);
					it->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
				});
				menu.append(*additem);
				Gtk::MenuItem* ignoreitem = Gtk::manage(new Gtk::MenuItem(_("Ignore word")));
				CONNECT(ignoreitem, activate, [this, it] {
					m_spell.ignore_word((*it)[m_itemStoreCols.text]);
					it->set_value(m_itemStoreCols.textColor, Glib::ustring("#000"));
				});
				menu.append(*ignoreitem);
			}
			menu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
		}
		Gtk::MenuItem* removeItem = Gtk::manage(new Gtk::MenuItem(_("Remove")));
		menu.append(*removeItem);
		CONNECT(removeItem, activate, [this] { removeCurrentItem(); });

		CONNECT(&menu, hide, [&] { loop->quit(); });
		menu.show_all();
		menu.popup(ev->button, ev->time);
		loop->run();
		return;
	}
	return;
}

void OutputEditorHOCR::checkCellEditable(const Glib::ustring& path, Gtk::CellRenderer* renderer) {
	Gtk::TreeIter it = m_itemStore->get_iter(path);
	if((*it)[m_itemStoreCols.itemClass] != "ocrx_word") {
		renderer->stop_editing(true);
	}
}

void OutputEditorHOCR::open() {
	if(!clear(false)) {
		return;
	}
	Glib::ustring dir = MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue();
	FileDialogs::FileFilter filter = {_("hOCR HTML Files"), {"text/html","text/xml", "text/plain"}, {"*.html"}};
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
		addPage(div, files.front()->get_basename(), page, false);
		div = getNextSiblingElement(div, "div");
	}
}

bool OutputEditorHOCR::save(const std::string& filename) {
	std::string outname = filename;
	if(outname.empty()) {
		std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		std::string ext, base;
		std::string name = !sources.empty() ? sources.front()->displayname : _("output");
		Utils::get_filename_parts(name, base, ext);
		outname = Glib::build_filename(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue(), base + ".html");

		FileDialogs::FileFilter filter = {_("hOCR HTML Files"), {"text/html"}, {"*.html"}};
		outname = FileDialogs::save_dialog(_("Save hOCR Output..."), outname, filter);
		if(outname.empty()) {
			return false;
		}
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Glib::path_get_dirname(outname));
	}
	std::ofstream file(outname);
	if(!file.is_open()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	tesseract::TessBaseAPI tess;
	Glib::ustring header = Glib::ustring::compose(
	                           "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
	                           "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
	                           " <head>\n"
	                           "  <title></title>\n"
	                           "  <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" />\n"
	                           "    <meta name='ocr-system' content='tesseract %1' />\n"
	                           "    <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
	                           "  </head>\n"
	                           "<body>\n", tess.Version());
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

void OutputEditorHOCR::savePDF() {
	m_preview = new DisplayerImageItem();
	updatePreview();
	MAIN->getDisplayer()->addItem(m_preview);
	bool accepted = false;
	PoDoFo::PdfStreamedDocument* document = nullptr;
	PoDoFo::PdfFont* font = nullptr;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
	const PoDoFo::PdfEncoding* pdfEncoding = PoDoFo::PdfEncodingFactory::GlobalIdentityEncodingInstance();
#else
	const PoDoFo::PdfEncoding* pdfEncoding = new PoDoFo::PdfIdentityEncoding;
#endif
	double fontSize = 0;
	while(true) {
		accepted = m_pdfExportDialog->run() ==  Gtk::RESPONSE_OK;
		m_pdfExportDialog->hide();
		if(!accepted) {
			break;
		}

		std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		std::string ext, base;
		std::string name = !sources.empty() ? sources.front()->displayname : _("output");
		Utils::get_filename_parts(name, base, ext);
		std::string outname = Glib::build_filename(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue(), base + ".pdf");
		FileDialogs::FileFilter filter = {_("PDF Files"), {"application/pdf"}, {"*.pdf"}};
		outname = FileDialogs::save_dialog(_("Save PDF Output..."), outname, filter);
		if(outname.empty()) {
			accepted = false;
			break;
		}
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Glib::path_get_dirname(outname));

		try {
			document = new PoDoFo::PdfStreamedDocument(outname.c_str());
		} catch(...) {
			Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
			continue;
		}
		try {
			Glib::ustring fontName = m_builder("fontbutton:pdfoptions").as<Gtk::FontButton>()->get_font_name();
			Pango::FontDescription fontDesc = Pango::FontDescription(fontName);
			bool italic = fontDesc.get_style() == Pango::STYLE_OBLIQUE;
			bool bold = fontDesc.get_weight() == Pango::WEIGHT_BOLD;
			fontSize = fontDesc.get_size() / double(PANGO_SCALE);
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
			font = document->CreateFontSubset(Utils::resolveFontName(fontDesc.get_family()).c_str(), bold, italic, false, pdfEncoding);
#else
			font = document->CreateFontSubset(Utils::resolveFontName(fontDesc.get_family()).c_str(), bold, italic, pdfEncoding);
#endif
		} catch(...) {
			font = nullptr;
		}
		if(!font) {
			Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), _("The PDF library does not support the selected font."));
			document->Close();
			delete document;
			continue;
		}
		break;
	}

	MAIN->getDisplayer()->removeItem(m_preview);
	delete m_preview;
	m_preview = nullptr;

	if(!accepted) {
		return;
	}

	PoDoFo::PdfPainter painter;

	PDFSettings pdfSettings;
	pdfSettings.colorFormat = (*m_builder("combo:pdfoptions.imageformat").as<Gtk::ComboBox>()->get_active())[m_formatComboCols.format];
	pdfSettings.compression = (*m_builder("combo:pdfoptions.compression").as<Gtk::ComboBox>()->get_active())[m_compressionComboCols.mode];
	pdfSettings.compressionQuality = m_builder("spin:pdfoptions.quality").as<Gtk::SpinButton>()->get_value();
	pdfSettings.useDetectedFontSizes = m_builder("checkbox:pdfoptions.usedetectedfontsizes").as<Gtk::CheckButton>()->get_active();
	pdfSettings.uniformizeLineSpacing = m_builder("checkbox:pdfoptions.uniformlinespacing").as<Gtk::CheckButton>()->get_active();
	pdfSettings.preserveSpaceWidth = m_builder("spin:pdfoptions.preserve").as<Gtk::SpinButton>()->get_value();
	pdfSettings.overlay = m_builder("combo:pdfoptions.mode").as<Gtk::ComboBox>()->get_active_row_number() == 1;
	pdfSettings.detectedFontScaling = m_builder("spin:pdfoptions.fontscale").as<Gtk::SpinButton>()->get_value() / 100.;
	std::vector<Glib::ustring> failed;
	for(Gtk::TreeIter item : m_itemStore->children()) {
		if(!(*item)[m_itemStoreCols.selected]) {
			continue;
		}
		Geometry::Rectangle bbox = (*item)[m_itemStoreCols.bbox];
		xmlpp::DomParser parser;
		parser.parse_memory((*item)[m_itemStoreCols.source]);
		xmlpp::Document* doc = parser.get_document();
		int pageDpi = 72;
		if(doc->get_root_node() && doc->get_root_node()->get_name() == "div" && setCurrentSource(doc->get_root_node(), &pageDpi)) {
			double dpiScale = 72. / pageDpi;
			double imageScale = m_builder("spin:pdfoptions.dpi").as<Gtk::SpinButton>()->get_value() / double(pageDpi);
			PoDoFo::PdfPage* page = document->CreatePage(PoDoFo::PdfRect(0, 0, bbox.width * dpiScale, bbox.height * dpiScale));
			painter.SetPage(page);
			painter.SetFont(font);

			PoDoFoPDFPainter pdfprinter(document, &painter, dpiScale, imageScale);
			pdfprinter.setFontSize(fontSize);
			printChildren(pdfprinter, item, pdfSettings);
			if(pdfSettings.overlay) {
				pdfprinter.drawImage(bbox, m_tool->getSelection(bbox), pdfSettings);
			}
			painter.FinishPage();
		} else {
			failed.push_back((*item)[m_itemStoreCols.text]);
		}
	}
	if(!failed.empty()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Errors occurred"), Glib::ustring::compose(_("The following pages could not be rendered:\n%1"), Utils::string_join(failed, "\n")));
	}
	document->Close();
	delete document;
}

void OutputEditorHOCR::printChildren(PDFPainter& painter, Gtk::TreeIter item, const PDFSettings& pdfSettings) const {
	if(!(*item)[m_itemStoreCols.selected]) {
		return;
	}
	Glib::ustring itemClass = (*item)[m_itemStoreCols.itemClass];
	Geometry::Rectangle itemRect = (*item)[m_itemStoreCols.bbox];
	if(itemClass == "ocr_par" && pdfSettings.uniformizeLineSpacing) {
		double yInc = double(itemRect.height) / item->children().size();
		double y = itemRect.y + yInc;
		int baseLine = item->children().empty() ? 0 : (*(*item->children().begin()))[m_itemStoreCols.baseLine];
		for(Gtk::TreeIter lineItem : item->children()) {
			double x = itemRect.x;
			double prevWordRight = itemRect.x;
			for(Gtk::TreeIter wordItem : lineItem->children()) {
				if((*wordItem)[m_itemStoreCols.selected]) {
					Geometry::Rectangle wordRect = (*wordItem)[m_itemStoreCols.bbox];
					if(pdfSettings.useDetectedFontSizes) {
						painter.setFontSize((*wordItem)[m_itemStoreCols.fontSize] * pdfSettings.detectedFontScaling);
					}
					// If distance from previous word is large, keep the space
					if(wordRect.x - prevWordRight > pdfSettings.preserveSpaceWidth * painter.getAverageCharWidth()) {
						x = wordRect.x;
					}
					prevWordRight = wordRect.x + wordRect.width;
					painter.drawText(x, y + baseLine, Glib::ustring((*wordItem)[m_itemStoreCols.text]));
					x += painter.getTextWidth(Glib::ustring((*wordItem)[m_itemStoreCols.text]) + " ");
				}
			}
			y += yInc;
		}
	} else if(itemClass == "ocr_line" && !pdfSettings.uniformizeLineSpacing) {
		int baseLine = (*item)[m_itemStoreCols.baseLine];
		double y = itemRect.y + itemRect.height + baseLine;
		for(Gtk::TreeIter wordItem : item->children()) {
			Geometry::Rectangle wordRect = (*wordItem)[m_itemStoreCols.bbox];
			if(pdfSettings.useDetectedFontSizes) {
				painter.setFontSize((*wordItem)[m_itemStoreCols.fontSize] * pdfSettings.detectedFontScaling);
			}
			painter.drawText(wordRect.x, y, Glib::ustring((*wordItem)[m_itemStoreCols.text]));
		}
	} else if(itemClass == "ocr_graphic" && !pdfSettings.overlay) {
		Cairo::RefPtr<Cairo::ImageSurface> sel = m_tool->getSelection(itemRect);
		painter.drawImage(itemRect, sel, pdfSettings);
	} else {
		for(Gtk::TreeIter child : item->children()) {
			printChildren(painter, child, pdfSettings);
		}
	}
}

void OutputEditorHOCR::updatePreview() {
	if(!m_preview) {
		return;
	}
	bool visible = m_builder("checkbox:pdfoptions.preview").as<Gtk::CheckButton>()->get_active();
	m_preview->setVisible(visible);
	if(m_itemStore->children().empty()|| !visible) {
		return;
	}
	Gtk::TreeIter item = currentItem();
	if(!item) {
		item = *m_itemStore->children().begin();
	} else {
		while(item->parent()) {
			item = item->parent();
		}
	}

	Geometry::Rectangle bbox = (*item)[m_itemStoreCols.bbox];
	xmlpp::DomParser parser;
	parser.parse_memory((*item)[m_itemStoreCols.source]);
	xmlpp::Document* doc = parser.get_document();
	int pageDpi = 72;
	setCurrentSource(doc->get_root_node(), &pageDpi);

	Cairo::RefPtr<Cairo::ImageSurface> image = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, bbox.width, bbox.height);

	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(image);
	Glib::ustring fontName = m_builder("fontbutton:pdfoptions").as<Gtk::FontButton>()->get_font_name();
	Pango::FontDescription fontDesc = Pango::FontDescription(fontName);
	Cairo::FontSlant fontSlant = fontDesc.get_style() == Pango::STYLE_OBLIQUE ? Cairo::FONT_SLANT_OBLIQUE : fontDesc.get_style() == Pango::STYLE_ITALIC ? Cairo::FONT_SLANT_ITALIC : Cairo::FONT_SLANT_NORMAL;
	Cairo::FontWeight fontWeight = fontDesc.get_weight() == Pango::WEIGHT_BOLD ? Cairo::FONT_WEIGHT_BOLD : Cairo::FONT_WEIGHT_NORMAL;
	double fontSize = fontDesc.get_size() / double(PANGO_SCALE);
	context->select_font_face(fontDesc.get_family(), fontSlant, fontWeight);
	context->set_font_size(fontSize * pageDpi / 72.);

	PDFSettings pdfSettings;
	pdfSettings.colorFormat = (*m_builder("combo:pdfoptions.imageformat").as<Gtk::ComboBox>()->get_active())[m_formatComboCols.format];
	pdfSettings.compression = (*m_builder("combo:pdfoptions.compression").as<Gtk::ComboBox>()->get_active())[m_compressionComboCols.mode];
	pdfSettings.compressionQuality = m_builder("spin:pdfoptions.quality").as<Gtk::SpinButton>()->get_value();
	pdfSettings.useDetectedFontSizes = m_builder("checkbox:pdfoptions.usedetectedfontsizes").as<Gtk::CheckButton>()->get_active();
	pdfSettings.uniformizeLineSpacing = m_builder("checkbox:pdfoptions.uniformlinespacing").as<Gtk::CheckButton>()->get_active();
	pdfSettings.preserveSpaceWidth = m_builder("spin:pdfoptions.preserve").as<Gtk::SpinButton>()->get_value();
	pdfSettings.overlay = m_builder("combo:pdfoptions.mode").as<Gtk::ComboBox>()->get_active_row_number() == 1;
	pdfSettings.detectedFontScaling = (pageDpi / 72.) * m_builder("spin:pdfoptions.fontscale").as<Gtk::SpinButton>()->get_value() / 100.;
	CairoPDFPainter painter(context);
	if(pdfSettings.overlay) {
		painter.drawImage(bbox, m_tool->getSelection(bbox), pdfSettings);
		context->save();
		context->rectangle(0, 0, image->get_width(), image->get_height());
		context->set_source_rgba(1., 1., 1., 0.5);
		context->fill();
		context->restore();
	} else {
		context->save();
		context->rectangle(0, 0, image->get_width(), image->get_height());
		context->set_source_rgba(1., 1., 1., 1.);
		context->fill();
		context->restore();
	}
	printChildren(painter, item, pdfSettings);
	m_preview->setImage(image);
	m_preview->setRect(Geometry::Rectangle(-0.5 * image->get_width(), -0.5 * image->get_height(), image->get_width(), image->get_height()));
}

bool OutputEditorHOCR::clear(bool hide) {
	if(!m_widget->get_visible()) {
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

bool OutputEditorHOCR::getModified() const {
	return m_modified;
}

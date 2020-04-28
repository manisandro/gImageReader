/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExporter.cc
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

#include <cstring>
#include <iomanip>
#include <podofo/base/PdfDictionary.h>
#include <podofo/base/PdfFilter.h>
#include <podofo/base/PdfStream.h>
#include <podofo/doc/PdfFont.h>
#include <podofo/doc/PdfIdentityEncoding.h>
#include <podofo/doc/PdfImage.h>
#include <podofo/doc/PdfPage.h>
#include <podofo/doc/PdfPainter.h>
#include <podofo/doc/PdfStreamedDocument.h>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "CCITTFax4Encoder.hh"
#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "DisplayerToolHOCR.hh"
#include "FileDialogs.hh"
#include "FontComboBox.hh"
#include "HOCRDocument.hh"
#include "HOCRPdfExporter.hh"
#include "MainWindow.hh"
#include "PaperSize.hh"
#include "SourceManager.hh"
#include "Utils.hh"

class HOCRPdfExporter::CairoPDFPainter : public HOCRPdfExporter::PDFPainter {
public:
	CairoPDFPainter(Cairo::RefPtr<Cairo::Context> context, const Glib::ustring& defaultFont, const std::vector<Glib::ustring>& fontFamilies)
		: m_context(context), m_defaultFont(defaultFont), m_fontFamilies(fontFamilies) {
		m_curFont = m_defaultFont;
		m_context->select_font_face(m_curFont, Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
	}
	void setFontFamily(const Glib::ustring& family, bool bold, bool italic) override {
		if(family != m_curFont) {
			if(std::find(m_fontFamilies.begin(), m_fontFamilies.end(), family) != m_fontFamilies.end()) {
				m_curFont = family;
			}  else {
				m_curFont = m_defaultFont;
			}
		}
		m_context->select_font_face(m_curFont, italic ? Cairo::FONT_SLANT_ITALIC : Cairo::FONT_SLANT_NORMAL, bold ? Cairo::FONT_WEIGHT_BOLD : Cairo::FONT_WEIGHT_NORMAL);
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
		if(settings.compression == PDFSettings::CompressJpeg) {
			Image img(image, settings.colorFormat, settings.conversionFlags);
			uint8_t* buf = nullptr;
			unsigned long bufLen = 0;
			img.writeJpeg(settings.compressionQuality, buf, bufLen);
			Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create("jpeg");
			loader->write(buf, bufLen);
			loader->close();
			std::free(buf);
			Gdk::Cairo::set_source_pixbuf(m_context, loader->get_pixbuf(), bbox.x, bbox.y);
		} else {
			Cairo::RefPtr<Cairo::ImageSurface> img = Image::simulateFormat(image, settings.colorFormat, settings.conversionFlags);
			m_context->set_source(img, bbox.x, bbox.y);
		}
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
	Glib::ustring m_curFont;
	Glib::ustring m_defaultFont;
	const std::vector<Glib::ustring>& m_fontFamilies;
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

class HOCRPdfExporter::PoDoFoPDFPainter : public HOCRPdfExporter::PDFPainter {
public:
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
	PoDoFoPDFPainter(PoDoFo::PdfStreamedDocument* document, const PoDoFo::PdfEncoding* fontEncoding, PoDoFo::PdfFont* defaultFont, const Glib::ustring& defaultFontFamily, double defaultFontSize, const std::vector<Glib::ustring>& fontFamilies)
#else
	PoDoFoPDFPainter(PoDoFo::PdfStreamedDocument* document, PoDoFo::PdfEncoding* fontEncoding, PoDoFo::PdfFont* defaultFont, const Glib::ustring& defaultFontFamily, double defaultFontSize, const std::vector<Glib::ustring>& fontFamilies)
#endif
		: m_fontFamilies(fontFamilies), m_document(document), m_pdfFontEncoding(fontEncoding), m_defaultFont(defaultFont), m_defaultFontFamily(defaultFontFamily), m_defaultFontSize(defaultFontSize) {
	}
	~PoDoFoPDFPainter() {
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0,9,3)
		delete m_pdfFontEncoding;
#endif
		delete m_document;
		// Fonts are deleted by the internal PoDoFo font cache of the document
	}
	bool createPage(double width, double height, double offsetX, double offsetY, Glib::ustring& /*errMsg*/) override {
		PoDoFo::PdfPage* pdfpage = m_document->CreatePage(PoDoFo::PdfRect(0, 0, width, height));
		m_painter.SetPage(pdfpage);
		m_pageHeight = m_painter.GetPage()->GetPageSize().GetHeight();
		m_painter.SetFont(m_defaultFont);
		if(m_defaultFontSize > 0) {
			m_painter.GetFont()->SetFontSize(m_defaultFontSize);
		}
		m_offsetX = offsetX;
		m_offsetY = offsetY;
		return true;
	}
	void finishPage() override {
		m_painter.FinishPage();
	}
	bool finishDocument(Glib::ustring& errMsg) override {
		try {
			m_document->Close();
		} catch(PoDoFo::PdfError& e) {
			errMsg = e.what();
			return false;
		}
		return true;
	}
	void setFontFamily(const Glib::ustring& family, bool bold, bool italic) override {
		float curSize = m_painter.GetFont()->GetFontSize();
		m_painter.SetFont(getFont(family, bold, italic));
		m_painter.GetFont()->SetFontSize(curSize);
	}
	void setFontSize(double pointSize) override {
		m_painter.GetFont()->SetFontSize(pointSize);
	}
	void drawText(double x, double y, const Glib::ustring& text) override {
		PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*>(text.c_str()));
		m_painter.DrawText(m_offsetX + x, m_pageHeight - m_offsetY - y, pdfString);
	}
	void drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const PDFSettings& settings) override {
		Image img(image, settings.colorFormat, settings.conversionFlags);
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
		PoDoFo::PdfImage pdfImage(m_document);
#else
		PoDoFo::PdfImageCompat pdfImage(m_document);
#endif
		pdfImage.SetImageColorSpace(settings.colorFormat == Image::Format_RGB24 ? PoDoFo::ePdfColorSpace_DeviceRGB : PoDoFo::ePdfColorSpace_DeviceGray);
		if(settings.compression == PDFSettings::CompressZip) {
			PoDoFo::PdfMemoryInputStream is(reinterpret_cast<const char*>(img.data), PoDoFo::pdf_long(img.bytesPerLine) * img.height);
			pdfImage.SetImageData(img.width, img.height, img.sampleSize, &is, {PoDoFo::ePdfFilter_FlateDecode});
		} else if(settings.compression == PDFSettings::CompressJpeg) {
			PoDoFo::PdfName dctFilterName(PoDoFo::PdfFilterFactory::FilterTypeToName(PoDoFo::ePdfFilter_DCTDecode));
			pdfImage.GetObject()->GetDictionary().AddKey(PoDoFo::PdfName::KeyFilter, dctFilterName);
			uint8_t* buf = nullptr;
			unsigned long bufLen = 0;
			img.writeJpeg(settings.compressionQuality, buf, bufLen);
			PoDoFo::PdfMemoryInputStream is(reinterpret_cast<const char*>(buf), bufLen);
			pdfImage.SetImageDataRaw(img.width, img.height, img.sampleSize, &is);
			std::free(buf);
		} else if(settings.compression == PDFSettings::CompressFax4) {
			PoDoFo::PdfName faxFilterName(PoDoFo::PdfFilterFactory::FilterTypeToName(PoDoFo::ePdfFilter_CCITTFaxDecode));
			pdfImage.GetObject()->GetDictionary().AddKey(PoDoFo::PdfName::KeyFilter, faxFilterName);
			PoDoFo::PdfDictionary decodeParams;
			decodeParams.AddKey("Columns", PoDoFo::PdfObject(PoDoFo::pdf_int64(img.width)));
			decodeParams.AddKey("Rows", PoDoFo::PdfObject(PoDoFo::pdf_int64(img.height)));
			decodeParams.AddKey("K", PoDoFo::PdfObject(PoDoFo::pdf_int64(-1))); // K < 0 --- Pure two-dimensional encoding (Group 4)
			pdfImage.GetObject()->GetDictionary().AddKey("DecodeParms", PoDoFo::PdfObject(decodeParams));
			CCITTFax4Encoder encoder;
			uint32_t encodedLen = 0;
			uint8_t* encoded = encoder.encode(img.data, img.width, img.height, img.bytesPerLine, encodedLen);
			PoDoFo::PdfMemoryInputStream is(reinterpret_cast<char*>(encoded), encodedLen);
			pdfImage.SetImageDataRaw(img.width, img.height, img.sampleSize, &is);
		}
		m_painter.DrawImage(m_offsetX + bbox.x, m_pageHeight - m_offsetY - (bbox.y + bbox.height),
		                    &pdfImage, bbox.width / double(image->get_width()), bbox.height / double(image->get_height()));
	}
	double getAverageCharWidth() const override {
		return m_painter.GetFont()->GetFontMetrics()->CharWidth(static_cast<unsigned char>('x'));
	}
	double getTextWidth(const Glib::ustring& text) const override {
		PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*>(text.c_str()));
		return m_painter.GetFont()->GetFontMetrics()->StringWidth(pdfString);
	}

private:
	const std::vector<Glib::ustring>& m_fontFamilies;
	std::map<Glib::ustring, PoDoFo::PdfFont*> m_fontCache;
	PoDoFo::PdfStreamedDocument* m_document;
	PoDoFo::PdfPainter m_painter;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
	const PoDoFo::PdfEncoding* m_pdfFontEncoding;
#else
	PoDoFo::PdfEncoding* m_pdfFontEncoding;
#endif
	PoDoFo::PdfFont* m_defaultFont;
	Glib::ustring m_defaultFontFamily;
	double m_defaultFontSize = -1.0;
	double m_pageHeight = 0.0;
	double m_offsetX = 0.0;
	double m_offsetY = 0.0;

	PoDoFo::PdfFont* getFont(Glib::ustring family, bool bold, bool italic) {
		Glib::ustring key = family + (bold ? "@bold" : "") + (italic ? "@italic" : "");
		auto it = m_fontCache.find(key);
		if(it == m_fontCache.end()) {
			if(family.empty() || std::find(m_fontFamilies.begin(), m_fontFamilies.end(), family) == m_fontFamilies.end()) {
				family = m_defaultFontFamily;
			}
			PoDoFo::PdfFont* font = nullptr;
			try {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
				font = m_document->CreateFontSubset(Utils::resolveFontName(family).c_str(), bold, italic, false, m_pdfFontEncoding);
#else
				font = document->CreateFontSubset(Utils::resolveFontName(family).c_str(), bold, italic, m_pdfFontEncoding);
#endif
				it = m_fontCache.insert(std::make_pair(key, font)).first;
			} catch(PoDoFo::PdfError& /*err*/) {
				it = m_fontCache.insert(std::make_pair(key, m_defaultFont)).first;
			}
		}
		return it->second;
	}
};


HOCRPdfExporter::HOCRPdfExporter(const Glib::RefPtr<HOCRDocument>& hocrdocument, const HOCRPage* previewPage, DisplayerToolHOCR* displayerTool)
	: m_hocrdocument(hocrdocument), m_previewPage(previewPage), m_displayerTool(displayerTool) {
	ui.builder->get_widget_derived("comboOverridefontfamily", m_comboOverrideFont);
	ui.builder->get_widget_derived("comboFallbackfontfamily", m_comboFallbackFont);
	ui.setupUi();
	ui.dialogPdfExport->set_transient_for(*MAIN->getWindow());

	// Paper format combo
	Glib::RefPtr<Gtk::ListStore> paperFormatComboModel = Gtk::ListStore::create(m_paperFormatComboCols);
	ui.comboPaperFormat->set_model(paperFormatComboModel);
	Gtk::TreeModel::Row row = *(paperFormatComboModel->append());
	row[m_paperFormatComboCols.label] = _("Same as source");
	row[m_paperFormatComboCols.format] = "source";
	row = *(paperFormatComboModel->append());
	row[m_paperFormatComboCols.label] = _("Custom");
	row[m_paperFormatComboCols.format] = "custom";
	for(const auto& size : PaperSize::paperSizes) {
		row = *(paperFormatComboModel->append());
		row[m_paperFormatComboCols.label] = size.first;
		row[m_paperFormatComboCols.format] = size.first;
	}
	ui.comboPaperFormat->pack_start(m_paperFormatComboCols.label);
	ui.comboPaperFormat->set_active(-1);

	// Paper size combo
	Glib::RefPtr<Gtk::ListStore> sizeUnitComboModel = Gtk::ListStore::create(m_sizeUnitComboCols);
	ui.comboPaperSizeUnit->set_model(sizeUnitComboModel);
	row = *(sizeUnitComboModel->append());
	row[m_sizeUnitComboCols.label] = _("cm");
	row[m_sizeUnitComboCols.unit] = static_cast<int>(PaperSize::cm);
	row = *(sizeUnitComboModel->append());
	row[m_sizeUnitComboCols.label] = _("in");
	row[m_sizeUnitComboCols.unit] = static_cast<int>(PaperSize::inch);
	ui.comboPaperSizeUnit->pack_start(m_sizeUnitComboCols.label);
	ui.comboPaperSizeUnit->set_active(0);

	// Image format combo
	Glib::RefPtr<Gtk::ListStore> formatComboModel = Gtk::ListStore::create(m_formatComboCols);
	ui.comboImageformat->set_model(formatComboModel);
	row = *(formatComboModel->append());
	row[m_formatComboCols.format] = Image::Format_RGB24;
	row[m_formatComboCols.label] = _("Color");
	row = *(formatComboModel->append());
	row[m_formatComboCols.format] = Image::Format_Gray8;
	row[m_formatComboCols.label] = _("Grayscale");
	row = *(formatComboModel->append());
	row[m_formatComboCols.format] = Image::Format_Mono;
	row[m_formatComboCols.label] = _("Monochrome");
	ui.comboImageformat->pack_start(m_formatComboCols.label);
	ui.comboImageformat->set_active(-1);

	// Dithering algorithm combo
	Glib::RefPtr<Gtk::ListStore> ditheringComboModel = Gtk::ListStore::create(m_ditheringComboCols);
	ui.comboDithering->set_model(ditheringComboModel);
	row = *(ditheringComboModel->append());
	row[m_ditheringComboCols.conversionFlags] = Image::ThresholdDithering;
	row[m_ditheringComboCols.label] = _("Threshold (closest color)");
	row = *(ditheringComboModel->append());
	row[m_ditheringComboCols.conversionFlags] = Image::DiffuseDithering;
	row[m_ditheringComboCols.label] = _("Diffuse");
	ui.comboDithering->pack_start(m_ditheringComboCols.label);
	ui.comboDithering->set_active(-1);

	// Image compression combo
	Glib::RefPtr<Gtk::ListStore> compressionModel = Gtk::ListStore::create(m_compressionComboCols);
	ui.comboCompression->set_model(compressionModel);
	row = *(compressionModel->append());
	row[m_compressionComboCols.mode] = PDFSettings::CompressZip;
	row[m_compressionComboCols.label] = _("Zip (lossless)");
	row[m_compressionComboCols.sensitive] = true;
	row = *(compressionModel->append());
	row[m_compressionComboCols.mode] = PDFSettings::CompressFax4;
	row[m_compressionComboCols.label] = _("CCITT Group 4 (lossless)");
	row[m_compressionComboCols.sensitive] = true;
	row = *(compressionModel->append());
	row[m_compressionComboCols.mode] = PDFSettings::CompressJpeg;
	row[m_compressionComboCols.label] = _("Jpeg (lossy)");
	row[m_compressionComboCols.sensitive] = true;
	ui.comboCompression->pack_start(m_compressionComboCols.label);
	ui.comboCompression->set_active(-1);
	Gtk::CellRendererText* compressionTextRenderer = dynamic_cast<Gtk::CellRendererText*>(ui.comboCompression->get_cells()[0]);
	if(compressionTextRenderer) {
		ui.comboCompression->add_attribute(compressionTextRenderer->property_sensitive(), m_compressionComboCols.sensitive);
	}

	Glib::RefPtr<Pango::FontMap> fontMap = Glib::wrap(pango_cairo_font_map_get_default(),  true);
	for(const Glib::RefPtr<Pango::FontFamily>& family : fontMap->list_families()) {
		m_fontFamilies.push_back(family->get_name());
	}

	Glib::RefPtr<Gtk::ListStore> pdfVersionModel = Gtk::ListStore::create(m_pdfVersionComboCols);
	ui.comboPdfVersion->set_model(pdfVersionModel);
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.0";
	row[m_pdfVersionComboCols.version] = PoDoFo::EPdfVersion::ePdfVersion_1_0;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.1";
	row[m_pdfVersionComboCols.version] = PoDoFo::EPdfVersion::ePdfVersion_1_1;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.2";
	row[m_pdfVersionComboCols.version] = PoDoFo::EPdfVersion::ePdfVersion_1_2;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.3";
	row[m_pdfVersionComboCols.version] = PoDoFo::EPdfVersion::ePdfVersion_1_3;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.4";
	row[m_pdfVersionComboCols.version] = PoDoFo::EPdfVersion::ePdfVersion_1_4;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.5";
	row[m_pdfVersionComboCols.version] = PoDoFo::EPdfVersion::ePdfVersion_1_5;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.6";
	row[m_pdfVersionComboCols.version] = PoDoFo::EPdfVersion::ePdfVersion_1_6;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.7";
	row[m_pdfVersionComboCols.version] = PoDoFo::EPdfVersion::ePdfVersion_1_7;
	ui.comboPdfVersion->pack_start(m_pdfVersionComboCols.label);
	ui.comboPdfVersion->set_active(-1);

	CONNECT(ui.comboMode, changed, [this] { updatePreview(); });
	CONNECT(ui.comboImageformat, changed, [this] { imageFormatChanged(); updatePreview(); });
	CONNECT(ui.comboDithering, changed, [this] { updatePreview(); });
	CONNECT(ui.comboCompression, changed, [this] { imageCompressionChanged(); });
	CONNECT(ui.spinQuality, value_changed, [this] { updatePreview(); });
	CONNECT(ui.checkboxOverridefontfamily, toggled, [this] {
		m_comboOverrideFont->set_sensitive(ui.checkboxOverridefontfamily->get_active());
		m_comboFallbackFont->set_sensitive(!ui.checkboxOverridefontfamily->get_active());
		ui.labelFallbackfontfamily->set_sensitive(!ui.checkboxOverridefontfamily->get_active());
		updatePreview();
	});
	CONNECT(m_comboFallbackFont, font_changed, [this](Glib::ustring) {
		updatePreview();
	});
	CONNECT(m_comboOverrideFont, font_changed, [this](Glib::ustring) {
		updatePreview();
	});
	CONNECT(ui.checkboxOverridefontsize, toggled, [this] {
		ui.spinOverridefontsize->set_sensitive(ui.checkboxOverridefontsize->get_active());
		ui.spinFontscale->set_sensitive(!ui.checkboxOverridefontsize->get_active());
		ui.labelFontscale->set_sensitive(!ui.checkboxOverridefontsize->get_active());
		updatePreview();
	});
	CONNECT(ui.spinOverridefontsize, value_changed, [this] { updatePreview(); });
	CONNECT(ui.spinFontscale, value_changed, [this] { updatePreview(); });
	CONNECT(ui.checkboxUniformlinespacing, toggled, [this] {
		ui.labelPreserve->set_sensitive(ui.checkboxUniformlinespacing->get_active());
		ui.spinPreserve->set_sensitive(ui.checkboxUniformlinespacing->get_active());
		ui.labelPreserve2->set_sensitive(ui.checkboxUniformlinespacing->get_active());
		updatePreview();
	});
	CONNECT(ui.spinPreserve, value_changed, [this] { updatePreview(); });
	CONNECT(ui.entryEncryptionPassword, changed, [this] { updateValid(); });
	CONNECT(ui.entryEncryptionConfirm, changed, [this] { updateValid(); });
	CONNECT(ui.checkboxPreview, toggled, [this] { updatePreview(); });
	CONNECT(ui.comboPaperFormat, changed, [this] { paperSizeChanged(); });
	CONNECT(ui.comboPaperSizeUnit, changed, [this] { paperSizeChanged(); });
	m_connLandscape = CONNECT(ui.buttonLandscape, toggled, [this] { paperOrientationChanged(true); });
	m_connPortrait = CONNECT(ui.buttonPortrait, toggled, [this] { paperOrientationChanged(false); });
	CONNECT(ui.entryPaperWidth, changed, [this] { paperSizeChanged(); });
	CONNECT(ui.entryPaperHeight, changed, [this] { paperSizeChanged(); });
	CONNECT(ui.buttonImportFromSource, clicked, [this] { importMetadataFromSource(); });

	ADD_SETTING(ComboSetting("pdfexportmode", ui.comboMode));
	ADD_SETTING(SpinSetting("pdfimagecompressionquality", ui.spinQuality));
	ADD_SETTING(ComboSetting("pdfimagecompression", ui.comboCompression));
	ADD_SETTING(ComboSetting("pdfimageformat", ui.comboImageformat));
	ADD_SETTING(ComboSetting("pdfimagedithering", ui.comboDithering));
	ADD_SETTING(SpinSetting("pdfimagedpi", ui.spinDpi));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("pdfoverridefontfamily", ui.checkboxOverridefontfamily));
	ADD_SETTING(FontComboSetting("pdffontfamily", m_comboOverrideFont));
	ADD_SETTING(FontComboSetting("pdffallbackfontfamily", m_comboFallbackFont));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("pdfoverridefontsizes", ui.checkboxOverridefontsize));
	ADD_SETTING(SpinSetting("pdffontsize", ui.spinOverridefontsize));
	ADD_SETTING(SpinSetting("pdffontscale", ui.spinFontscale));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("pdfuniformizelinespacing", ui.checkboxUniformlinespacing));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("pdfsanitizehyphens", ui.checkboxSanitizeHyphens));
	ADD_SETTING(SpinSetting("pdfpreservespaces", ui.spinPreserve));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("pdfpreview", ui.checkboxPreview));
	ADD_SETTING(ComboSetting("pdfexportpapersize", ui.comboPaperFormat));
	ADD_SETTING(EntrySetting("pdfexportpaperwidth", ui.entryPaperWidth));
	ADD_SETTING(EntrySetting("pdfexportpaperheight", ui.entryPaperHeight));
	ADD_SETTING(ComboSetting("pdfexportpapersizeunit", ui.comboPaperSizeUnit));
	ADD_SETTING(SwitchSettingT<Gtk::ToggleButton>("pdfexportpaperlandscape", ui.buttonLandscape));
	ADD_SETTING(EntrySetting("pdfexportinfoauthor", ui.entryMetadataAuthor));
	ADD_SETTING(EntrySetting("pdfexportinfoproducer", ui.entryMetadataProducer));
	ADD_SETTING(EntrySetting("pdfexportinfocreator", ui.entryMetadataCreator));
	ADD_SETTING(ComboSetting("pdfexportpdfversion", ui.comboPdfVersion));

#if !defined(TESSERACT_VERSION) || TESSERACT_VERSION < TESSERACT_MAKE_VERSION(3,04,00)
	checkboxFontFamily->set_active(true);
	checkboxFontFamily->set_sensitive(true);
	checkboxFontSize->set_active(true);
	checkboxFontSize->set_sensitive(true);
#endif
}

bool HOCRPdfExporter::run(const std::string& filebasename) {
	m_preview = new DisplayerImageItem();
	m_preview->setZIndex(2);
	updatePreview();
	MAIN->getDisplayer()->addItem(m_preview);

	bool accepted = false;
	PDFPainter* painter = nullptr;
	int pageCount = m_hocrdocument->pageCount();

	std::string outname;
	while(true) {
		accepted = ui.dialogPdfExport->run() ==  Gtk::RESPONSE_OK;
		ui.dialogPdfExport->hide();
		if(!accepted) {
			break;
		}

		std::string suggestion = filebasename;
		if(suggestion.empty()) {
			std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
			suggestion = !sources.empty() ? Utils::split_filename(sources.front()->displayname).first : _("output");
		}

		while(true) {
			FileDialogs::FileFilter filter = {_("PDF Files"), {"application/pdf"}, {"*.pdf"}};
			outname = FileDialogs::save_dialog(_("Save PDF Output..."), suggestion + ".pdf", "outputdir", filter);
			if(outname.empty()) {
				accepted = false;
				break;
			}
			if(m_hocrdocument->referencesSource(outname)) {
				Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Invalid Output"), _("Cannot overwrite a file which is a source image of this document."));
				continue;
			}
			break;
		}
		if(!accepted) {
			break;
		}

		Glib::ustring defaultFont = ui.checkboxOverridefontfamily->get_active() ? m_comboOverrideFont->get_active_font() : m_comboFallbackFont->get_active_font();
		double defaultFontSize = ui.checkboxOverridefontsize->get_active() ? ui.spinOverridefontsize->get_value() : 0;

		Glib::ustring errMsg;
		painter = createPoDoFoPrinter(outname, defaultFont, defaultFontSize, errMsg);
		if(!painter) {
			Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to create output"), Glib::ustring::compose(_("Failed to create output. The returned error was: %1"), errMsg));
			continue;
		}

		break;
	}

	MAIN->getDisplayer()->removeItem(m_preview);
	delete m_preview;
	m_preview = nullptr;
	if(!accepted) {
		return false;
	}

	PDFSettings pdfSettings = getPdfSettings();

	int outputDpi = ui.spinDpi->get_value();
	std::string paperSize = (*ui.comboPaperFormat->get_active())[m_paperFormatComboCols.format];

	double pageWidth, pageHeight;
	// Page dimensions are in points: 1 in = 72 pt
	if(paperSize == "custom") {
		pageWidth = std::atof(ui.entryPaperWidth->get_text().c_str()) * 72.0;
		pageHeight = std::atof(ui.entryPaperHeight->get_text().c_str()) * 72.0;

		PaperSize::Unit unit = static_cast<PaperSize::Unit>(static_cast<int>((*ui.comboPaperSizeUnit->get_active())[m_sizeUnitComboCols.unit]));
		if(unit == PaperSize::cm) {
			pageWidth /= PaperSize::CMtoInch;
			pageHeight /= PaperSize::CMtoInch;
		}
	} else if (paperSize != "source") {
		auto inchSize = PaperSize::getSize(PaperSize::inch, paperSize, ui.buttonLandscape->get_active());
		pageWidth = inchSize.width * 72.0;
		pageHeight = inchSize.height * 72.0;
	}

	MainWindow::ProgressMonitor monitor(pageCount);
	MAIN->showProgress(&monitor);
	Glib::ustring errMsg;
	bool success = Utils::busyTask([&] {
		for(int i = 0; i < pageCount; ++i) {
			if(monitor.cancelled()) {
				errMsg = _("The operation was cancelled");
				return false;
			}
			const HOCRPage* page = m_hocrdocument->page(i);
			if(page->isEnabled()) {
				Geometry::Rectangle bbox = page->bbox();
				Glib::ustring sourceFile = page->sourceFile();
				// If the source file is an image, its "resolution" is actually just the scale factor that was used for recognizing.
				bool isImage = Glib::ustring(sourceFile.substr(sourceFile.length() - 4)).lowercase() != ".pdf" && Glib::ustring(sourceFile.substr(sourceFile.length() - 5)).lowercase() != ".djvu";
				int sourceDpi = page->resolution();
				int sourceScale = sourceDpi;
				if(isImage) {
					sourceDpi *= ui.spinBoxPaperSizeDpi->get_value_as_int() / 100;
				}
				// [pt] = 72 * [in]
				// [in] = 1 / dpi * [px]
				// => [pt] = 72 / dpi * [px]
				double px2pt = (72.0 / sourceDpi);
				double imgScale = double(outputDpi) / sourceDpi;

				bool success = false;
				if(isImage) {
					Utils::runInMainThreadBlocking([&] { success = setSource(page->sourceFile(), page->pageNr(), sourceScale * imgScale, page->angle()); });
				} else {
					Utils::runInMainThreadBlocking([&] { success = setSource(page->sourceFile(), page->pageNr(), outputDpi, page->angle()); });
				}
				if(success) {
					double imgScale = double(outputDpi) / sourceDpi;
					if(paperSize == "source") {
						pageWidth = bbox.width * px2pt;
						pageHeight = bbox.height * px2pt;
					}
					double offsetX = 0.5 * (pageWidth - bbox.width * px2pt);
					double offsetY = 0.5 * (pageHeight - bbox.height * px2pt);
					if(!painter->createPage(pageWidth, pageHeight, offsetX, offsetY, errMsg)) {
						return false;
					}
					printChildren(*painter, page, pdfSettings, px2pt, imgScale, double(sourceScale) / sourceDpi, true);
					if(pdfSettings.overlay) {
						Geometry::Rectangle scaledRect(imgScale * bbox.x, imgScale * bbox.y, imgScale * bbox.width, imgScale * bbox.height);
						Geometry::Rectangle printRect(bbox.x * px2pt, bbox.y * px2pt, bbox.width * px2pt, bbox.height * px2pt);
						Cairo::RefPtr<Cairo::ImageSurface> selection;
						Utils::runInMainThreadBlocking([&] { selection = getSelection(scaledRect); });
						painter->drawImage(printRect, selection, pdfSettings);
					}
					Utils::runInMainThreadBlocking([&] { setSource(page->sourceFile(), page->pageNr(), sourceScale, page->angle()); });
					painter->finishPage();
				} else {
					errMsg = Glib::ustring::compose(_("Failed to render page %1"), page->title());
					return false;
				}
			}
			monitor.increaseProgress();
		}
		return painter->finishDocument(errMsg);
	}, _("Exporting to PDF..."));
	MAIN->hideProgress();

	bool openAfterExport = ConfigSettings::get<SwitchSettingT<Gtk::CheckButton>>("openafterexport")->getValue();
	if(!success) {
		Utils::message_dialog(Gtk::MESSAGE_WARNING, _("Export failed"), Glib::ustring::compose(_("The PDF export failed: %1."), errMsg));
	} else if(openAfterExport) {
		Utils::openUri(Glib::filename_to_uri(outname));
	}
	delete painter;

	return success;
}


HOCRPdfExporter::PDFPainter* HOCRPdfExporter::createPoDoFoPrinter(const std::string& filename, const Glib::ustring& defaultFont, double defaultFontSize, Glib::ustring& errMsg) {
	PoDoFo::PdfStreamedDocument* document = nullptr;
	PoDoFo::PdfFont* defaultPdfFont = nullptr;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
	const PoDoFo::PdfEncoding* pdfFontEncoding = PoDoFo::PdfEncodingFactory::GlobalIdentityEncodingInstance();
#else
	PoDoFo::PdfEncoding* pdfFontEncoding = new PoDoFo::PdfIdentityEncoding;
#endif

	try {
		Glib::ustring password = ui.entryEncryptionPassword->get_text();
		PoDoFo::PdfEncrypt* encrypt = nullptr;
		if(!password.empty()) {
			encrypt = PoDoFo::PdfEncrypt::CreatePdfEncrypt(password, password,
			          PoDoFo::PdfEncrypt::EPdfPermissions::ePdfPermissions_Print |
			          PoDoFo::PdfEncrypt::EPdfPermissions::ePdfPermissions_Edit |
			          PoDoFo::PdfEncrypt::EPdfPermissions::ePdfPermissions_Copy |
			          PoDoFo::PdfEncrypt::EPdfPermissions::ePdfPermissions_EditNotes |
			          PoDoFo::PdfEncrypt::EPdfPermissions::ePdfPermissions_FillAndSign |
			          PoDoFo::PdfEncrypt::EPdfPermissions::ePdfPermissions_Accessible |
			          PoDoFo::PdfEncrypt::EPdfPermissions::ePdfPermissions_DocAssembly |
			          PoDoFo::PdfEncrypt::EPdfPermissions::ePdfPermissions_HighPrint,
			          PoDoFo::PdfEncrypt::EPdfEncryptAlgorithm::ePdfEncryptAlgorithm_RC4V2);
		}

		PoDoFo::EPdfVersion pdfVersion = static_cast<PoDoFo::EPdfVersion>(static_cast<int>((*ui.comboPdfVersion->get_active())[m_pdfVersionComboCols.version]));
		document = new PoDoFo::PdfStreamedDocument(filename.c_str(), pdfVersion, encrypt);
	} catch(PoDoFo::PdfError& err) {
		errMsg = err.what();
		return nullptr;
	}

	Pango::FontDescription fontDesc = Pango::FontDescription(defaultFont);

	// Attempt to load the default/fallback font to ensure it is valid
	try {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
		defaultPdfFont = document->CreateFontSubset(Utils::resolveFontName(fontDesc.get_family()).c_str(), false, false, false, pdfFontEncoding);
#else
		defaultPdfFont = document->CreateFontSubset(Utils::resolveFontName(fontDesc.get_family()).c_str(), false, false, pdfFontEncoding);
#endif
	} catch(PoDoFo::PdfError&) {
	}
	if(!defaultPdfFont) {
		errMsg = Glib::ustring::compose(_("The PDF library could not load the font '%1'."), defaultFont);
		return nullptr;
	}

	// Set PDF info
	PoDoFo::PdfInfo* pdfInfo = document->GetInfo();
	pdfInfo->SetProducer(ui.entryMetadataProducer->get_text().c_str());
	pdfInfo->SetCreator(ui.entryMetadataCreator->get_text().c_str());
	pdfInfo->SetTitle(ui.entryMetadataTitle->get_text().c_str());
	pdfInfo->SetSubject(ui.entryMetadataSubject->get_text().c_str());
	pdfInfo->SetKeywords(ui.entryMetadataKeywords->get_text().c_str());
	pdfInfo->SetAuthor(ui.entryMetadataAuthor->get_text().c_str());

	return new PoDoFoPDFPainter(document, pdfFontEncoding, defaultPdfFont, defaultFont, defaultFontSize, m_fontFamilies);
}

HOCRPdfExporter::PDFSettings HOCRPdfExporter::getPdfSettings() const {
	PDFSettings pdfSettings;
	pdfSettings.colorFormat = (*ui.comboImageformat->get_active())[m_formatComboCols.format];
	pdfSettings.conversionFlags = pdfSettings.colorFormat == Image::Format_Mono ? (*ui.comboDithering->get_active())[m_ditheringComboCols.conversionFlags] : Image::AutoColor;
	pdfSettings.compression = (*ui.comboCompression->get_active())[m_compressionComboCols.mode];
	pdfSettings.compressionQuality = ui.spinQuality->get_value();
	pdfSettings.fontFamily = ui.checkboxOverridefontfamily->get_active() ? m_comboOverrideFont->get_active_font() : "";
	pdfSettings.fontSize = ui.checkboxOverridefontsize->get_active() ? ui.spinOverridefontsize->get_value() : -1;
	pdfSettings.uniformizeLineSpacing = ui.checkboxUniformlinespacing->get_active();
	pdfSettings.preserveSpaceWidth = ui.spinPreserve->get_value();
	pdfSettings.overlay = ui.comboMode->get_active_row_number() == 1;
	pdfSettings.detectedFontScaling = ui.spinFontscale->get_value() / 100.;
	pdfSettings.sanitizeHyphens = ui.checkboxSanitizeHyphens->get_active();
	return pdfSettings;
}

void HOCRPdfExporter::printChildren(PDFPainter& painter, const HOCRItem* item, const PDFSettings& pdfSettings, double px2pu/*pixels to printer units*/, double imgScale, double fontScale, bool inThread) {
	if(!item->isEnabled()) {
		return;
	}
	if(pdfSettings.fontSize != -1) {
		painter.setFontSize(pdfSettings.fontSize * fontScale);
	}
	Glib::ustring itemClass = item->itemClass();
	Geometry::Rectangle itemRect = item->bbox();
	int childCount = item->children().size();
	if(itemClass == "ocr_par" && pdfSettings.uniformizeLineSpacing) {
		double yInc = double(itemRect.height) / childCount;
		double y = itemRect.y + yInc;
		std::pair<double, double> baseline = childCount > 0 ? item->children()[0]->baseLine() : std::make_pair(0.0, 0.0);
		for(int iLine = 0; iLine < childCount; ++iLine, y += yInc) {
			HOCRItem* lineItem = item->children()[iLine];
			int x = itemRect.x;
			int prevWordRight = itemRect.x;
			for(int iWord = 0, nWords = lineItem->children().size(); iWord < nWords; ++iWord) {
				HOCRItem* wordItem = lineItem->children()[iWord];
				if(!wordItem->isEnabled()) {
					continue;
				}
				Geometry::Rectangle wordRect = wordItem->bbox();
				painter.setFontFamily(pdfSettings.fontFamily.empty() ? wordItem->fontFamily() : pdfSettings.fontFamily, wordItem->fontBold(), wordItem->fontItalic());
				if(pdfSettings.fontSize == -1) {
					painter.setFontSize(wordItem->fontSize() * pdfSettings.detectedFontScaling * fontScale);
				}
				// If distance from previous word is large, keep the space
				if(wordRect.x - prevWordRight > pdfSettings.preserveSpaceWidth * painter.getAverageCharWidth() / px2pu) {
					x = wordRect.x;
				}
				prevWordRight = wordRect.x + wordRect.width;
				Glib::ustring text = wordItem->text();
				if(iWord == nWords - 1 && pdfSettings.sanitizeHyphens) {
					text = Glib::Regex::create("[-\u2014]\\s*$")->replace(text, 0, "-", static_cast<Glib::RegexMatchFlags>(0));
				}
				double wordBaseline = (x - itemRect.x) * baseline.first + baseline.second;
				painter.drawText(x * px2pu, (y + wordBaseline) * px2pu, text);
				x += painter.getTextWidth(text + " ") / px2pu;
			}
		}
	} else if(itemClass == "ocr_line" && !pdfSettings.uniformizeLineSpacing) {
		std::pair<double, double> baseline = item->baseLine();
		for(int iWord = 0, nWords = item->children().size(); iWord < nWords; ++iWord) {
			HOCRItem* wordItem = item->children()[iWord];
			if(!wordItem->isEnabled()) {
				continue;
			}
			Geometry::Rectangle wordRect = wordItem->bbox();
			painter.setFontFamily(pdfSettings.fontFamily.empty() ? wordItem->fontFamily() : pdfSettings.fontFamily, wordItem->fontBold(), wordItem->fontItalic());
			if(pdfSettings.fontSize == -1) {
				painter.setFontSize(wordItem->fontSize() * pdfSettings.detectedFontScaling * fontScale);
			}
			double y = itemRect.y + itemRect.height + (wordRect.x + 0.5 * wordRect.width - itemRect.x) * baseline.first + baseline.second;
			Glib::ustring text = wordItem->text();
			if(iWord == nWords - 1 && pdfSettings.sanitizeHyphens) {
				text = Glib::Regex::create("[-\u2014]\\s*$")->replace(text, 0, "-", static_cast<Glib::RegexMatchFlags>(0));
			}
			painter.drawText(wordRect.x * px2pu, y * px2pu, text);
		}
	} else if(itemClass == "ocr_graphic" && !pdfSettings.overlay) {
		Geometry::Rectangle scaledItemRect(imgScale * itemRect.x, imgScale * itemRect.y, imgScale * itemRect.width, imgScale * itemRect.height);
		Geometry::Rectangle printRect(itemRect.x * px2pu, itemRect.y * px2pu, itemRect.width * px2pu, itemRect.height * px2pu);
		Cairo::RefPtr<Cairo::ImageSurface> selection;
		if(inThread) {
			Utils::runInMainThreadBlocking([&] { selection = getSelection(scaledItemRect); });
		} else {
			selection = getSelection(scaledItemRect);
		}
		painter.drawImage(printRect, selection, pdfSettings);
	} else {
		for(int i = 0, n = item->children().size(); i < n; ++i) {
			printChildren(painter, item->children()[i], pdfSettings, px2pu, imgScale, fontScale, inThread);
		}
	}
}

void HOCRPdfExporter::updatePreview() {
	if(!m_preview) {
		return;
	}
	m_preview->setVisible(ui.checkboxPreview->get_active());
	if(m_hocrdocument->pageCount() == 0 || !ui.checkboxPreview->get_active()) {
		return;
	}

	const HOCRPage* page = m_previewPage;
	Geometry::Rectangle bbox = page->bbox();
	int pageDpi = page->resolution();

	PDFSettings pdfSettings = getPdfSettings();
	pdfSettings.detectedFontScaling *= (pageDpi / 72.);
	pdfSettings.fontSize *= (pageDpi / 72.);

	Glib::ustring defaultFont = ui.checkboxOverridefontfamily->get_active() ? m_comboOverrideFont->get_active_font() : m_comboFallbackFont->get_active_font();

	Cairo::RefPtr<Cairo::ImageSurface> image = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, bbox.width, bbox.height);
	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(image);

	CairoPDFPainter pdfPrinter(context, defaultFont, m_fontFamilies);
	if(!pdfSettings.fontFamily.empty()) {
		pdfPrinter.setFontFamily(pdfSettings.fontFamily, false, false);
	}
	if(pdfSettings.fontSize != -1) {
		pdfPrinter.setFontSize(pdfSettings.fontSize * pageDpi / 72.);
	}

	if(pdfSettings.overlay) {
		pdfPrinter.drawImage(bbox, m_displayerTool->getSelection(bbox), pdfSettings);
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
	printChildren(pdfPrinter, page, pdfSettings, 1.);
	m_preview->setImage(image);
	m_preview->setRect(Geometry::Rectangle(-0.5 * image->get_width(), -0.5 * image->get_height(), image->get_width(), image->get_height()));
}

void HOCRPdfExporter::imageFormatChanged() {
	Image::Format format = (*ui.comboImageformat->get_active())[m_formatComboCols.format];
	Glib::RefPtr<Gtk::ListStore> compressionStore = Glib::RefPtr<Gtk::ListStore>::cast_static<Gtk::TreeModel>(ui.comboCompression->get_model());
	bool isMono = format == Image::Format_Mono;
	if(isMono && (*ui.comboCompression->get_active())[m_compressionComboCols.mode] == PDFSettings::CompressJpeg) {
		ui.comboCompression->set_active(PDFSettings::CompressZip);
	} else if(!isMono && (*ui.comboCompression->get_active())[m_compressionComboCols.mode] == PDFSettings::CompressFax4) {
		ui.comboCompression->set_active(PDFSettings::CompressZip);
	}
	(*compressionStore->children()[PDFSettings::CompressFax4])[m_compressionComboCols.sensitive] = isMono;
	(*compressionStore->children()[PDFSettings::CompressJpeg])[m_compressionComboCols.sensitive] = !isMono;
	ui.labelDithering->set_sensitive(isMono);
	ui.comboDithering->set_sensitive(isMono);
}

void HOCRPdfExporter::imageCompressionChanged() {
	PDFSettings::Compression compression = (*ui.comboCompression->get_active())[m_compressionComboCols.mode];
	bool jpegCompression = compression == PDFSettings::CompressJpeg;
	ui.spinQuality->set_sensitive(jpegCompression);
	ui.labelQuality->set_sensitive(jpegCompression);
}

bool HOCRPdfExporter::setSource(const Glib::ustring& sourceFile, int page, int dpi, double angle) {
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(sourceFile);
	if(MAIN->getSourceManager()->addSource(file, true)) {
		MAIN->getDisplayer()->setup(&page, &dpi, &angle);
		return true;
	} else {
		return false;
	}
}

Cairo::RefPtr<Cairo::ImageSurface> HOCRPdfExporter::getSelection(const Geometry::Rectangle& bbox) {
	return m_displayerTool->getSelection(bbox);
}

void HOCRPdfExporter::paperOrientationChanged(bool landscape) {
	m_connPortrait.block(true);
	m_connLandscape.block(true);
	ui.buttonPortrait->set_active(!landscape);
	ui.buttonLandscape->set_active(landscape);
	m_connPortrait.block(false);
	m_connLandscape.block(false);
	paperSizeChanged();
}

void HOCRPdfExporter::paperSizeChanged() {
	std::string paperSize = (*ui.comboPaperFormat->get_active())[m_paperFormatComboCols.format];
	if(paperSize == "custom") {
		ui.entryPaperWidth->set_sensitive(true);
		ui.entryPaperHeight->set_sensitive(true);
		ui.boxPaperSize->set_visible(true);
		ui.boxPaperOrientation->set_visible(false);
		ui.boxPaperSizeDpi->set_visible(false);
	} else if(paperSize == "source") {
		ui.entryPaperWidth->set_sensitive(true);
		ui.entryPaperHeight->set_sensitive(true);
		ui.boxPaperSize->set_visible(false);
		ui.boxPaperOrientation->set_visible(false);
		ui.boxPaperSizeDpi->set_visible(true);
	} else {
		ui.boxPaperSize->set_visible(true);
		ui.boxPaperOrientation->set_visible(true);
		ui.boxPaperSizeDpi->set_visible(false);
		ui.entryPaperWidth->set_sensitive(false);
		ui.entryPaperHeight->set_sensitive(false);
		PaperSize::Unit unit = static_cast<PaperSize::Unit>(static_cast<int>((*ui.comboPaperSizeUnit->get_active())[m_sizeUnitComboCols.unit]));
		auto outputPaperSize = PaperSize::getSize(unit, paperSize, ui.buttonLandscape->get_active());
		ui.entryPaperWidth->set_text(Glib::ustring::format(std::fixed, std::setprecision(1), outputPaperSize.width));
		ui.entryPaperHeight->set_text(Glib::ustring::format(std::fixed, std::setprecision(1), outputPaperSize.height));
	}
	updateValid();
}

void HOCRPdfExporter::updateValid() {
	bool valid = true;

	// Passwords must match
	if(ui.entryEncryptionPassword->get_text() == ui.entryEncryptionConfirm->get_text()) {
		Utils::clear_error_state(ui.entryEncryptionConfirm);
	} else {
		Utils::set_error_state(ui.entryEncryptionConfirm);
		valid = false;
	}

	// In custom paper size mode, size must be specified
	std::string paperSize = (*ui.comboPaperFormat->get_active())[m_paperFormatComboCols.format];
	Utils::clear_error_state(ui.entryPaperWidth);
	Utils::clear_error_state(ui.entryPaperHeight);
	if(paperSize == "custom") {
		if(ui.entryPaperWidth->get_text().empty()) {
			Utils::set_error_state(ui.entryPaperWidth);
			valid = false;
		}
		if(ui.entryPaperHeight->get_text().empty()) {
			Utils::set_error_state(ui.entryPaperHeight);
			valid = false;
		}
	}
	ui.buttonOk->set_sensitive(valid);
}

void HOCRPdfExporter::importMetadataFromSource() {
	std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
	if(!sources.empty()) {
		Source* source = sources.front();
		ui.entryMetadataAuthor->set_text(source->author);
		ui.entryMetadataCreator->set_text(source->creator);
		ui.entryMetadataKeywords->set_text(source->keywords);
		ui.entryMetadataTitle->set_text(source->title);
		ui.entryMetadataSubject->set_text(source->subject);
		ui.entryMetadataProducer->set_text(source->producer);

		PoDoFo::EPdfVersion sourcePdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_7;
		if(source->pdfVersionMajor == 1 && source->pdfVersionMinor <= 7) {
			sourcePdfVersion = static_cast<PoDoFo::EPdfVersion>(PoDoFo::EPdfVersion::ePdfVersion_1_0 + source->pdfVersionMinor);
		}
		ui.comboPdfVersion->set_active(sourcePdfVersion);
	}
}

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExporter.hh
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRPDFEXPORTER_HH
#define HOCRPDFEXPORTER_HH

#include "common.hh"
#include "Image.hh"
#include "HOCRExporter.hh"

class DisplayerImageItem;
class DisplayerToolHOCR;
class FontComboBox;
namespace Geometry {
class Rectangle;
}
class HOCRDocument;
class HOCRPage;
class HOCRItem;
class HOCRPdfExportWidget;
namespace PoDoFo {
class PdfEncoding;
class PdfFont;
class PdfPainter;
class PdfDocument;
}


class HOCRPdfExporter : public HOCRExporter {
public:
	struct PDFSettings : ExporterSettings {
		Image::Format colorFormat;
		Image::ConversionFlags conversionFlags;
		enum Compression { CompressZip, CompressFax4, CompressJpeg } compression;
		int compressionQuality;
		Glib::ustring fontFamily;
		int fontSize;
		Glib::ustring fallbackFontFamily;
		int fallbackFontSize;
		bool uniformizeLineSpacing;
		int preserveSpaceWidth;
		bool overlay;
		double detectedFontScaling;
		bool sanitizeHyphens;
		int assumedImageDpi;
		int outputDpi;
		Glib::ustring paperSize;
		bool paperSizeLandscape;
		double paperWidthIn;
		double paperHeightIn;
		enum Version {
			PdfVersion_1_0 = 0,
			PdfVersion_1_1,
			PdfVersion_1_2,
			PdfVersion_1_3,
			PdfVersion_1_4,
			PdfVersion_1_5,
			PdfVersion_1_6,
			PdfVersion_1_7
		} version;
		Glib::ustring password;
		Glib::ustring producer;
		Glib::ustring creator;
		Glib::ustring title;
		Glib::ustring subject;
		Glib::ustring keywords;
		Glib::ustring author;
	};

	bool run(const HOCRDocument* hocrdocument, const std::string& outname, const ExporterSettings* settings = nullptr) override;

private:
	bool setSource(const Glib::ustring& sourceFile, int page, int dpi, double angle);
};

class HOCRPdfPrinter {
public:
	HOCRPdfPrinter();
	virtual ~HOCRPdfPrinter() = default;
	virtual void setFontFamily(const Glib::ustring& family, bool bold, bool italic) = 0;
	virtual void setFontSize(double pointSize) = 0;
	virtual void drawText(double x, double y, const Glib::ustring& text) = 0;
	virtual void drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const HOCRPdfExporter::PDFSettings& settings) = 0;
	virtual double getAverageCharWidth() const = 0;
	virtual double getTextWidth(const Glib::ustring& text) const = 0;
	virtual bool createPage(double /*width*/, double /*height*/, double /*offsetX*/, double /*offsetY*/, Glib::ustring& /*errMsg*/) { return true; }
	virtual void finishPage() {}
	virtual bool finishDocument(Glib::ustring& /*errMsg*/) { return true; }

	void printChildren(const HOCRItem* item, const HOCRPdfExporter::PDFSettings& pdfSettings, double px2pu, double imgScale = 1., double fontScale = 1., bool inThread = false);

	Cairo::RefPtr<Cairo::ImageSurface> getSelection(const Geometry::Rectangle& bbox);

protected:
	std::vector<Glib::ustring> m_fontFamilies;
};

class HOCRPoDoFoPdfPrinter : public HOCRPdfPrinter {
public:
	static HOCRPoDoFoPdfPrinter* create(const std::string& filename, const HOCRPdfExporter::PDFSettings& settings, const Glib::ustring& defaultFont, int defaultFontSize, Glib::ustring& errMsg);

	HOCRPoDoFoPdfPrinter(PoDoFo::PdfDocument* document, const std::string& filename, const PoDoFo::PdfEncoding* fontEncoding, PoDoFo::PdfFont* defaultFont, const Glib::ustring& defaultFontFamily, double defaultFontSize);
	~HOCRPoDoFoPdfPrinter();
	bool createPage(double width, double height, double offsetX, double offsetY, Glib::ustring& /*errMsg*/) override;
	void finishPage() override;
	bool finishDocument(Glib::ustring& errMsg) override;
	void setFontFamily(const Glib::ustring& family, bool bold, bool italic) override;
	void setFontSize(double pointSize) override;
	void drawText(double x, double y, const Glib::ustring& text) override;
	void drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const HOCRPdfExporter::PDFSettings& settings) override;
	double getAverageCharWidth() const override;
	double getTextWidth(const Glib::ustring& text) const override;

private:
	std::map<Glib::ustring, PoDoFo::PdfFont*> m_fontCache;
	PoDoFo::PdfDocument* m_document = nullptr;
	std::string m_filename;
	PoDoFo::PdfPainter* m_painter = nullptr;
	const PoDoFo::PdfEncoding* m_pdfFontEncoding;
	PoDoFo::PdfFont* m_defaultFont;
	Glib::ustring m_defaultFontFamily;
	double m_defaultFontSize = -1.0;
	double m_pageHeight = 0.0;
	double m_offsetX = 0.0;
	double m_offsetY = 0.0;

	PoDoFo::PdfFont* getFont(Glib::ustring family, bool bold, bool italic);
};


class HOCRCairoPdfPrinter : public HOCRPdfPrinter {
public:
	HOCRCairoPdfPrinter(Cairo::RefPtr<Cairo::Context> context, const Glib::ustring& defaultFont);
	void setFontFamily(const Glib::ustring& family, bool bold, bool italic) override;
	void setFontSize(double pointSize) override;
	void drawText(double x, double y, const Glib::ustring& text) override;
	void drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const HOCRPdfExporter::PDFSettings& settings) override;
	double getAverageCharWidth() const override;
	double getTextWidth(const Glib::ustring& text) const override;

private:
	Cairo::RefPtr<Cairo::Context> m_context;
	Glib::ustring m_curFont;
	Glib::ustring m_defaultFont;
};

class HOCRPdfExportDialog : public Gtk::Dialog {
public:
	HOCRPdfExportDialog(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument, const HOCRPage* hocrpage, Gtk::Window* parent = nullptr);
	~HOCRPdfExportDialog();

	HOCRPdfExporter::PDFSettings getPdfSettings() const;

private:
	HOCRPdfExportWidget* m_widget = nullptr;
};

#endif // HOCRPDFEXPORTER_HH

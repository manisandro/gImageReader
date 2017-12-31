/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExporter.hh
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

#ifndef HOCRPDFEXPORTER_HH
#define HOCRPDFEXPORTER_HH

#include "common.hh"
#include "Image.hh"
#include "ui_PdfExportDialog.hh"

class DisplayerImageItem;
class DisplayerToolHOCR;
class FontComboBox;
namespace Geometry { class Rectangle; }
class HOCRDocument;
class HOCRPage;
class HOCRItem;


class HOCRPdfExporter {
public:
	HOCRPdfExporter(const Glib::RefPtr<HOCRDocument>& hocrdocument, const HOCRPage* previewPage, DisplayerToolHOCR* displayerTool);
	bool run(std::string& filebasename);

private:
	struct PDFSettings {
		Image::Format colorFormat;
		Image::ConversionFlags conversionFlags;
		enum Compression { CompressZip, CompressFax4, CompressJpeg } compression;
		int compressionQuality;
		Glib::ustring fontFamily;
		int fontSize;
		bool uniformizeLineSpacing;
		int preserveSpaceWidth;
		bool overlay;
		double detectedFontScaling;
	};

	class PDFPainter {
	public:
		virtual void setFontFamily(const Glib::ustring& family) = 0;
		virtual void setFontSize(double pointSize) = 0;
		virtual void drawText(double x, double y, const Glib::ustring& text) = 0;
		virtual void drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const PDFSettings& settings) = 0;
		virtual double getAverageCharWidth() const = 0;
		virtual double getTextWidth(const Glib::ustring& text) const = 0;
	};
	class PoDoFoPDFPainter;
	class CairoPDFPainter;

	struct PaperFormatComboColums : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> label;
		Gtk::TreeModelColumn<std::string> format;
		PaperFormatComboColums() {
			add(label);
			add(format);
		}
	} m_paperFormatComboCols;

	struct SizeUnitComboColums : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> label;
		Gtk::TreeModelColumn<int> unit;
		SizeUnitComboColums() {
			add(label);
			add(unit);
		}
	} m_sizeUnitComboCols;

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

	Ui::PdfExportDialog ui;
	ClassData m_classdata;

	FontComboBox* m_comboOverrideFont;
	FontComboBox* m_comboFallbackFont;
	DisplayerImageItem* m_preview = nullptr;
	Glib::RefPtr<HOCRDocument> m_hocrdocument;
	const HOCRPage* m_previewPage;
	DisplayerToolHOCR* m_displayerTool;
	std::vector<Glib::ustring> m_fontFamilies;
	sigc::connection m_connPortrait;
	sigc::connection m_connLandscape;

	PDFSettings getPdfSettings() const;
	void printChildren(PDFPainter& painter, const HOCRItem* item, const PDFSettings& pdfSettings, double imgScale = 1., bool inThread = false);

	void imageFormatChanged();
	void imageCompressionChanged();
	void paperOrientationChanged(bool landscape);
	void paperSizeChanged();
	void updatePreview();
	void updateValid();
	bool setSource(const Glib::ustring& sourceFile, int page, int dpi, double angle);
	Cairo::RefPtr<Cairo::ImageSurface> getSelection(const Geometry::Rectangle& bbox);
};

#endif // HOCRPDFEXPORTER_HH

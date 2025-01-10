/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExportWidget.hh
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

#ifndef HOCRPDFEXPORTWIDGET_HH
#define HOCRPDFEXPORTWIDGET_HH

#include "HOCRPdfExporter.hh"
#include "ui_PdfExportWidget.hh"


class DisplayerToolHOCR;
class HOCRDocument;
class HOCRPage;


class HOCRPdfExportWidget {
public:
	HOCRPdfExportWidget(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument = nullptr, const HOCRPage* hocrpage = nullptr);
	~HOCRPdfExportWidget();
	void setPreviewPage(const HOCRDocument* hocrdocument, const HOCRPage* hocrpage);
	HOCRPdfExporter::PDFSettings getPdfSettings() const;
	Gtk::Widget* getWidget() const { return ui.pdfExportWidget; }

	sigc::signal<void (bool) > signal_validChanged() const { return m_signal_valid_changed; }

private:
	ClassData m_classdata;

	Ui::PdfExportWidget ui;
	DisplayerImageItem* m_preview = nullptr;
	DisplayerToolHOCR* m_displayerTool;
	const HOCRDocument* m_document = nullptr;
	const HOCRPage* m_previewPage;

	FontComboBox* m_comboOverrideFont;
	FontComboBox* m_comboFallbackFont;
	sigc::connection m_connPortrait;
	sigc::connection m_connLandscape;
	sigc::signal<void (bool) > m_signal_valid_changed;

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
		Gtk::TreeModelColumn<HOCRPdfExporter::PDFSettings::Compression> mode;
		Gtk::TreeModelColumn<Glib::ustring> label;
		Gtk::TreeModelColumn<bool> sensitive;
		CompressionComboColums() {
			add(mode);
			add(label);
			add(sensitive);
		}
	} m_compressionComboCols;

	struct PdfVersionComboCols : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<Glib::ustring> label;
		Gtk::TreeModelColumn<int> version;
		PdfVersionComboCols() {
			add(label);
			add(version);
		}
	} m_pdfVersionComboCols;

	void importMetadataFromSource();
	void imageFormatChanged();
	void imageCompressionChanged();
	void paperOrientationChanged(bool landscape);
	void paperSizeChanged();
	void updatePreview();
	void updateValid();
};

#endif // HOCRPDFEXPORTWIDGET_HH

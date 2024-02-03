/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExportWidget.cc
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

#include <iomanip>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "DisplayerToolHOCR.hh"
#include "HOCRDocument.hh"
#include "HOCRPdfExportWidget.hh"
#include "HOCRPdfExporter.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "PaperSize.hh"
#include "Utils.hh"


HOCRPdfExportWidget::HOCRPdfExportWidget(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument, const HOCRPage* hocrpage)
	: m_displayerTool(displayerTool), m_document(hocrdocument), m_previewPage(hocrpage) {

	ui.builder->get_widget_derived("comboOverridefontfamily", m_comboOverrideFont);
	ui.builder->get_widget_derived("comboFallbackfontfamily", m_comboFallbackFont);
	ui.setupUi();

	// Image format combo
	Glib::RefPtr<Gtk::ListStore> formatComboModel = Gtk::ListStore::create(m_formatComboCols);
	ui.comboImageformat->set_model(formatComboModel);
	Gtk::TreeModel::Row row = *(formatComboModel->append());
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
	row[m_compressionComboCols.mode] = HOCRPdfExporter::PDFSettings::CompressZip;
	row[m_compressionComboCols.label] = _("Zip (lossless)");
	row[m_compressionComboCols.sensitive] = true;
	row = *(compressionModel->append());
	row[m_compressionComboCols.mode] = HOCRPdfExporter::PDFSettings::CompressFax4;
	row[m_compressionComboCols.label] = _("CCITT Group 4 (lossless)");
	row[m_compressionComboCols.sensitive] = true;
	row = *(compressionModel->append());
	row[m_compressionComboCols.mode] = HOCRPdfExporter::PDFSettings::CompressJpeg;
	row[m_compressionComboCols.label] = _("Jpeg (lossy)");
	row[m_compressionComboCols.sensitive] = true;
	ui.comboCompression->pack_start(m_compressionComboCols.label);
	ui.comboCompression->set_active(-1);
	Gtk::CellRendererText* compressionTextRenderer = dynamic_cast<Gtk::CellRendererText*>(ui.comboCompression->get_cells()[0]);
	if(compressionTextRenderer) {
		ui.comboCompression->add_attribute(compressionTextRenderer->property_sensitive(), m_compressionComboCols.sensitive);
	}

	// Paper format combo
	Glib::RefPtr<Gtk::ListStore> paperFormatComboModel = Gtk::ListStore::create(m_paperFormatComboCols);
	ui.comboPaperFormat->set_model(paperFormatComboModel);
	row = *(paperFormatComboModel->append());
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

	Glib::RefPtr<Gtk::ListStore> pdfVersionModel = Gtk::ListStore::create(m_pdfVersionComboCols);
	ui.comboPdfVersion->set_model(pdfVersionModel);
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.0";
	row[m_pdfVersionComboCols.version] = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_0;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.1";
	row[m_pdfVersionComboCols.version] = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_1;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.2";
	row[m_pdfVersionComboCols.version] = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_2;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.3";
	row[m_pdfVersionComboCols.version] = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_3;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.4";
	row[m_pdfVersionComboCols.version] = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_4;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.5";
	row[m_pdfVersionComboCols.version] = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_5;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.6";
	row[m_pdfVersionComboCols.version] = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_6;
	row = *(pdfVersionModel->append());
	row[m_pdfVersionComboCols.label] = "1.7";
	row[m_pdfVersionComboCols.version] = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_7;
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

	m_preview = new DisplayerImageItem();
	m_preview->setZIndex(2);
	updatePreview();
	MAIN->getDisplayer()->addItem(m_preview);
}

HOCRPdfExportWidget::~HOCRPdfExportWidget() {
	MAIN->getDisplayer()->removeItem(m_preview);
	delete m_preview;
}

void HOCRPdfExportWidget::setPreviewPage(const HOCRDocument* hocrdocument, const HOCRPage* hocrpage) {
	m_document = hocrdocument;
	m_previewPage = hocrpage;
	updatePreview();
}

HOCRPdfExporter::PDFSettings HOCRPdfExportWidget::getPdfSettings() const {
	HOCRPdfExporter::PDFSettings pdfSettings;
	pdfSettings.colorFormat = (*ui.comboImageformat->get_active())[m_formatComboCols.format];
	pdfSettings.conversionFlags = pdfSettings.colorFormat == Image::Format_Mono ? (*ui.comboDithering->get_active())[m_ditheringComboCols.conversionFlags] : Image::AutoColor;
	pdfSettings.compression = (*ui.comboCompression->get_active())[m_compressionComboCols.mode];
	pdfSettings.compressionQuality = ui.spinQuality->get_value();
	pdfSettings.fontFamily = ui.checkboxOverridefontfamily->get_active() ? m_comboOverrideFont->get_active_font() : "";
	pdfSettings.fontSize = ui.checkboxOverridefontfamily->get_active() ? ui.spinOverridefontsize->get_value() : -1;
	pdfSettings.fallbackFontFamily = ui.checkboxOverridefontfamily->get_active() ? m_comboOverrideFont->get_active_font() : m_comboFallbackFont->get_active_font();
	pdfSettings.fallbackFontSize = ui.checkboxOverridefontfamily->get_active() ? ui.spinOverridefontsize->get_value() : -1;
	pdfSettings.uniformizeLineSpacing = ui.checkboxUniformlinespacing->get_active();
	pdfSettings.preserveSpaceWidth = ui.spinPreserve->get_value();
	pdfSettings.overlay = ui.comboMode->get_active_row_number() == 1;
	pdfSettings.detectedFontScaling = ui.spinFontscale->get_value() / 100.;
	pdfSettings.sanitizeHyphens = ui.checkboxSanitizeHyphens->get_active();

	pdfSettings.assumedImageDpi = ui.spinBoxPaperSizeDpi->get_value_as_int();
	pdfSettings.outputDpi = ui.spinDpi->get_value_as_int();
	pdfSettings.paperSize = (*ui.comboPaperFormat->get_active())[m_paperFormatComboCols.format];
	pdfSettings.paperSizeLandscape = ui.buttonLandscape->get_active();
	pdfSettings.paperWidthIn = std::atof(ui.entryPaperWidth->get_text().c_str());
	pdfSettings.paperHeightIn = std::atof(ui.entryPaperHeight->get_text().c_str());

	PaperSize::Unit unit = static_cast<PaperSize::Unit>(static_cast<int>((*ui.comboPaperSizeUnit->get_active())[m_sizeUnitComboCols.unit]));
	if(unit == PaperSize::cm) {
		pdfSettings.paperWidthIn /= PaperSize::CMtoInch;
		pdfSettings.paperHeightIn /= PaperSize::CMtoInch;
	}

	pdfSettings.version = static_cast<HOCRPdfExporter::PDFSettings::Version>(static_cast<int>((*ui.comboPdfVersion->get_active())[m_pdfVersionComboCols.version]));
	pdfSettings.password = ui.entryEncryptionPassword->get_text();
	pdfSettings.producer = ui.entryMetadataProducer->get_text().c_str();
	pdfSettings.creator = ui.entryMetadataCreator->get_text().c_str();
	pdfSettings.title = ui.entryMetadataTitle->get_text().c_str();
	pdfSettings.subject = ui.entryMetadataSubject->get_text().c_str();
	pdfSettings.keywords = ui.entryMetadataKeywords->get_text().c_str();
	pdfSettings.author = ui.entryMetadataAuthor->get_text().c_str();

	return pdfSettings;
}


void HOCRPdfExportWidget::updatePreview() {
	if(!m_preview) {
		return;
	}
	m_preview->setVisible(ui.checkboxPreview->get_active());
	if(!m_document || m_document->pageCount() == 0 || !ui.checkboxPreview->get_active()) {
		return;
	}

	const HOCRPage* page = m_previewPage;
	Geometry::Rectangle bbox = page->bbox();
	int pageDpi = page->resolution();

	HOCRPdfExporter::PDFSettings pdfSettings = getPdfSettings();
	pdfSettings.detectedFontScaling *= (pageDpi / 72.);
	pdfSettings.fontSize *= (pageDpi / 72.);

	Glib::ustring defaultFont = ui.checkboxOverridefontfamily->get_active() ? m_comboOverrideFont->get_active_font() : m_comboFallbackFont->get_active_font();

	Cairo::RefPtr<Cairo::ImageSurface> image = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, bbox.width, bbox.height);
	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(image);

	HOCRCairoPdfPrinter pdfPrinter(context, defaultFont);
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
	pdfPrinter.printChildren(page, pdfSettings, 1.);
	m_preview->setImage(image);
	m_preview->setRect(Geometry::Rectangle(-0.5 * image->get_width(), -0.5 * image->get_height(), image->get_width(), image->get_height()));
}

void HOCRPdfExportWidget::imageFormatChanged() {
	Image::Format format = (*ui.comboImageformat->get_active())[m_formatComboCols.format];
	Glib::RefPtr<Gtk::ListStore> compressionStore = Glib::RefPtr<Gtk::ListStore>::cast_static<Gtk::TreeModel>(ui.comboCompression->get_model());
	bool isMono = format == Image::Format_Mono;
	if(isMono && (*ui.comboCompression->get_active())[m_compressionComboCols.mode] == HOCRPdfExporter::PDFSettings::CompressJpeg) {
		ui.comboCompression->set_active(HOCRPdfExporter::PDFSettings::CompressZip);
	} else if(!isMono && (*ui.comboCompression->get_active())[m_compressionComboCols.mode] == HOCRPdfExporter::PDFSettings::CompressFax4) {
		ui.comboCompression->set_active(HOCRPdfExporter::PDFSettings::CompressZip);
	}
	(*compressionStore->children()[HOCRPdfExporter::PDFSettings::CompressFax4])[m_compressionComboCols.sensitive] = isMono;
	(*compressionStore->children()[HOCRPdfExporter::PDFSettings::CompressJpeg])[m_compressionComboCols.sensitive] = !isMono;
	ui.labelDithering->set_sensitive(isMono);
	ui.comboDithering->set_sensitive(isMono);
}

void HOCRPdfExportWidget::imageCompressionChanged() {
	HOCRPdfExporter::PDFSettings::Compression compression = (*ui.comboCompression->get_active())[m_compressionComboCols.mode];
	bool jpegCompression = compression == HOCRPdfExporter::PDFSettings::CompressJpeg;
	ui.spinQuality->set_sensitive(jpegCompression);
	ui.labelQuality->set_sensitive(jpegCompression);
}

void HOCRPdfExportWidget::paperOrientationChanged(bool landscape) {
	m_connPortrait.block(true);
	m_connLandscape.block(true);
	ui.buttonPortrait->set_active(!landscape);
	ui.buttonLandscape->set_active(landscape);
	m_connPortrait.block(false);
	m_connLandscape.block(false);
	paperSizeChanged();
}

void HOCRPdfExportWidget::paperSizeChanged() {
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

void HOCRPdfExportWidget::updateValid() {
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
	m_signal_valid_changed.emit(valid);
}

void HOCRPdfExportWidget::importMetadataFromSource() {
	std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
	if(!sources.empty()) {
		Source* source = sources.front();
		ui.entryMetadataAuthor->set_text(source->author);
		ui.entryMetadataCreator->set_text(source->creator);
		ui.entryMetadataKeywords->set_text(source->keywords);
		ui.entryMetadataTitle->set_text(source->title);
		ui.entryMetadataSubject->set_text(source->subject);
		ui.entryMetadataProducer->set_text(source->producer);

		HOCRPdfExporter::PDFSettings::Version sourcePdfVersion = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_7;
		if(source->pdfVersionMajor == 1 && source->pdfVersionMinor <= 7) {
			sourcePdfVersion = static_cast<HOCRPdfExporter::PDFSettings::Version>(HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_0 + source->pdfVersionMinor);
		}
		ui.comboPdfVersion->set_active(sourcePdfVersion);
	}
}

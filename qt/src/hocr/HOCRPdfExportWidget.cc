/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExportWidget.cc
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

#include <QBuffer>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontDialog>
#include <QGraphicsPixmapItem>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPrinter>
#include <QStandardItemModel>
#include <QThread>
#include <QToolTip>
#include <QUrl>

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


HOCRPdfExportWidget::HOCRPdfExportWidget(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument, const HOCRPage* hocrpage, QWidget* parent)
	: QWidget(parent), m_displayerTool(displayerTool), m_document(hocrdocument), m_previewPage(hocrpage) {
	ui.setupUi(this);

	ui.comboBoxBackend->addItem("PoDoFo", HOCRPdfExporter::PDFSettings::BackendPoDoFo);
	ui.comboBoxBackend->addItem("QPrinter", HOCRPdfExporter::PDFSettings::BackendQPrinter);
	ui.comboBoxBackend->setCurrentIndex(-1);

	ui.comboBoxImageFormat->addItem(_("Color"), QImage::Format_RGB888);
	ui.comboBoxImageFormat->addItem(_("Grayscale"), QImage::Format_Grayscale8);
	ui.comboBoxImageFormat->addItem(_("Monochrome"), QImage::Format_Mono);
	ui.comboBoxImageFormat->setCurrentIndex(-1);
	ui.comboBoxDithering->addItem(_("Threshold (closest color)"), Qt::ThresholdDither);
	ui.comboBoxDithering->addItem(_("Diffuse"), Qt::DiffuseDither);
	ui.comboBoxImageCompression->addItem(_("Zip (lossless)"), HOCRPdfExporter::PDFSettings::CompressZip);
	ui.comboBoxImageCompression->addItem(_("CCITT Group 4 (lossless)"), HOCRPdfExporter::PDFSettings::CompressFax4);
	ui.comboBoxImageCompression->addItem(_("Jpeg (lossy)"), HOCRPdfExporter::PDFSettings::CompressJpeg);
	ui.comboBoxImageCompression->setCurrentIndex(-1);

	ui.comboBoxPaperSizeUnit->addItem(_("cm"), static_cast<int>(PaperSize::cm));
	ui.comboBoxPaperSizeUnit->addItem(_("in"), static_cast<int>(PaperSize::inch));

	ui.comboBoxPaperSize->addItem(_("Same as source"), "source");
	ui.comboBoxPaperSize->addItem(_("Custom"), "custom");
	for(const auto& size : PaperSize::paperSizes) {
		ui.comboBoxPaperSize->addItem(size.first.c_str(), size.first.c_str());
	}
	ui.comboBoxPaperSize->setCurrentIndex(-1);


	ui.lineEditPaperHeight->setValidator(new QDoubleValidator(0.0, 10000000.0, 1));
	ui.lineEditPaperWidth->setValidator(new QDoubleValidator(0.0, 10000000.0, 1));

	ui.comboBoxPdfVersion->addItem("1.0", HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_0);
	ui.comboBoxPdfVersion->addItem("1.1", HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_1);
	ui.comboBoxPdfVersion->addItem("1.2", HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_2);
	ui.comboBoxPdfVersion->addItem("1.3", HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_3);
	ui.comboBoxPdfVersion->addItem("1.4", HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_4);
	ui.comboBoxPdfVersion->addItem("1.5", HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_5);
	ui.comboBoxPdfVersion->addItem("1.6", HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_6);
	ui.comboBoxPdfVersion->addItem("1.7", HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_7);

	ui.comboBoxPdfVersion->setCurrentIndex(-1);

	connect(ui.comboBoxBackend, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRPdfExportWidget::backendChanged);
	connect(ui.toolButtonBackendHint, &QToolButton::clicked, this, &HOCRPdfExportWidget::toggleBackendHint);
	connect(ui.comboBoxOutputMode, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.comboBoxImageFormat, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.comboBoxImageFormat, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRPdfExportWidget::imageFormatChanged);
	connect(ui.comboBoxDithering, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.comboBoxImageCompression, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRPdfExportWidget::imageCompressionChanged);
	connect(ui.spinBoxCompressionQuality, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.checkBoxFontFamily, &QCheckBox::toggled, ui.comboBoxFontFamily, &QComboBox::setEnabled);
	connect(ui.checkBoxFontFamily, &QCheckBox::toggled, this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.checkBoxFontFamily, &QCheckBox::toggled, ui.labelFallbackFontFamily, &QLabel::setDisabled);
	connect(ui.checkBoxFontFamily, &QCheckBox::toggled, ui.comboBoxFallbackFontFamily, &QComboBox::setDisabled);
	connect(ui.comboBoxFontFamily, &QFontComboBox::currentFontChanged, this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.comboBoxFallbackFontFamily, &QFontComboBox::currentFontChanged, this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.checkBoxFontSize, &QCheckBox::toggled, ui.spinBoxFontSize, &QSpinBox::setEnabled);
	connect(ui.checkBoxFontSize, &QCheckBox::toggled, this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.checkBoxFontSize, &QCheckBox::toggled, ui.labelFontScaling, &HOCRPdfExportWidget::setDisabled);
	connect(ui.checkBoxFontSize, &QCheckBox::toggled, ui.spinFontScaling, &HOCRPdfExportWidget::setDisabled);
	connect(ui.spinBoxFontSize, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.spinFontScaling, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.checkBoxUniformizeSpacing, &QCheckBox::toggled, this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.spinBoxPreserve, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.checkBoxUniformizeSpacing, &QCheckBox::toggled, ui.labelPreserve, &QLabel::setEnabled);
	connect(ui.checkBoxUniformizeSpacing, &QCheckBox::toggled, ui.labelPreserveCharacters, &QLabel::setEnabled);
	connect(ui.checkBoxUniformizeSpacing, &QCheckBox::toggled, ui.spinBoxPreserve, &QSpinBox::setEnabled);
	connect(ui.lineEditPasswordOpen, &QLineEdit::textChanged, this, &HOCRPdfExportWidget::updateValid);
	connect(ui.lineEditConfirmPasswordOpen, &QLineEdit::textChanged, this, &HOCRPdfExportWidget::updateValid);
	connect(ui.checkBoxPreview, &QCheckBox::toggled, this, &HOCRPdfExportWidget::updatePreview);
	connect(ui.comboBoxPaperSize, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRPdfExportWidget::paperSizeChanged);
	connect(ui.comboBoxPaperSizeUnit, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRPdfExportWidget::paperSizeChanged);
	connect(ui.toolButtonLandscape, &QToolButton::clicked, this, &HOCRPdfExportWidget::paperSizeChanged);
	connect(ui.lineEditPaperWidth, &QLineEdit::textChanged, this, &HOCRPdfExportWidget::updateValid);
	connect(ui.lineEditPaperHeight, &QLineEdit::textChanged, this, &HOCRPdfExportWidget::updateValid);
	connect(ui.pushButtonImport, &QPushButton::clicked, this, &HOCRPdfExportWidget::importMetadataFromSource);

	ADD_SETTING(ComboSetting("pdfexportmode", ui.comboBoxOutputMode));
	ADD_SETTING(SpinSetting("pdfimagecompressionquality", ui.spinBoxCompressionQuality, 90));
	ADD_SETTING(ComboSetting("pdfimagecompression", ui.comboBoxImageCompression));
	ADD_SETTING(ComboSetting("pdfimageformat", ui.comboBoxImageFormat));
	ADD_SETTING(ComboSetting("pdfimageconversionflags", ui.comboBoxDithering));
	ADD_SETTING(SpinSetting("pdfimagedpi", ui.spinBoxDpi, 300));
	ADD_SETTING(SwitchSetting("pdfoverridefontfamily", ui.checkBoxFontFamily, true));
	ADD_SETTING(FontComboSetting("pdffontfamily", ui.comboBoxFontFamily));
	ADD_SETTING(FontComboSetting("pdffallbackfontfamily", ui.comboBoxFallbackFontFamily));
	ADD_SETTING(SwitchSetting("pdfoverridefontsizes", ui.checkBoxFontSize, true));
	ADD_SETTING(SpinSetting("pdffontsize", ui.spinBoxFontSize, 10));
	ADD_SETTING(SpinSetting("pdffontscale", ui.spinFontScaling, 100));
	ADD_SETTING(SwitchSetting("pdfuniformizelinespacing", ui.checkBoxUniformizeSpacing, false));
	ADD_SETTING(SwitchSetting("pdfsanitizehyphens", ui.checkBoxSanitizeHyphens, true));
	ADD_SETTING(SpinSetting("pdfpreservespaces", ui.spinBoxPreserve, 4));
	ADD_SETTING(SwitchSetting("pdfpreview", ui.checkBoxPreview, false));
	ADD_SETTING(ComboSetting("pdfexportpapersize", ui.comboBoxPaperSize));
	ADD_SETTING(LineEditSetting("pdfexportpaperwidth", ui.lineEditPaperWidth));
	ADD_SETTING(LineEditSetting("pdfexportpaperheight", ui.lineEditPaperHeight));
	ADD_SETTING(ComboSetting("pdfexportpapersizeunit", ui.comboBoxPaperSize));
	ADD_SETTING(SwitchSetting("pdfexportpaperlandscape", ui.toolButtonLandscape));
	ADD_SETTING(LineEditSetting("pdfexportinfoauthor", ui.lineEditAuthor, PACKAGE_NAME));
	ADD_SETTING(LineEditSetting("pdfexportinfoproducer", ui.lineEditProducer, PACKAGE_NAME));
	ADD_SETTING(LineEditSetting("pdfexportinfocreator", ui.lineEditCreator, PACKAGE_NAME));
	ADD_SETTING(ComboSetting("pdfexportpdfversion", ui.comboBoxPdfVersion, ui.comboBoxPdfVersion->findData(HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_7)));
	ADD_SETTING(ComboSetting("pdfexportbackend", ui.comboBoxBackend));

#if !defined(TESSERACT_VERSION) || TESSERACT_VERSION < TESSERACT_MAKE_VERSION(3,04,00)
	ui.checkBoxFontFamily->setChecked(true);
	ui.checkBoxFontFamily->setEnabled(false);
	ui.checkBoxFontSize->setChecked(true);
	ui.checkBoxFontSize->setEnabled(false);
#endif

	m_preview = new QGraphicsPixmapItem();
	m_preview->setTransformationMode(Qt::SmoothTransformation);
	m_preview->setZValue(2);
	updatePreview();
	MAIN->getDisplayer()->scene()->addItem(m_preview);
}

HOCRPdfExportWidget::~HOCRPdfExportWidget() {
	MAIN->getDisplayer()->scene()->removeItem(m_preview);
	delete m_preview;
}

void HOCRPdfExportWidget::setPreviewPage(const HOCRDocument* hocrdocument, const HOCRPage* hocrpage) {
	m_document = hocrdocument;
	m_previewPage = hocrpage;
	updatePreview();
}

HOCRPdfExporter::PDFSettings HOCRPdfExportWidget::getPdfSettings() const {
	HOCRPdfExporter::PDFSettings pdfSettings;
	pdfSettings.colorFormat = static_cast<QImage::Format>(ui.comboBoxImageFormat->itemData(ui.comboBoxImageFormat->currentIndex()).toInt());
	pdfSettings.conversionFlags = pdfSettings.colorFormat == QImage::Format_Mono ? static_cast<Qt::ImageConversionFlags>(ui.comboBoxDithering->itemData(ui.comboBoxDithering->currentIndex()).toInt()) : Qt::AutoColor;
	pdfSettings.compression = static_cast<HOCRPdfExporter::PDFSettings::Compression>(ui.comboBoxImageCompression->itemData(ui.comboBoxImageCompression->currentIndex()).toInt());
	pdfSettings.compressionQuality = ui.spinBoxCompressionQuality->value();
	pdfSettings.fontFamily = ui.checkBoxFontFamily->isChecked() ? ui.comboBoxFontFamily->currentFont().family() : "";
	pdfSettings.fontSize = ui.checkBoxFontSize->isChecked() ? ui.spinBoxFontSize->value() : -1;
	pdfSettings.fallbackFontFamily = ui.checkBoxFontFamily->isChecked() ? ui.comboBoxFontFamily->currentFont().family() : ui.comboBoxFallbackFontFamily->currentFont().family();
	pdfSettings.fallbackFontSize = ui.checkBoxFontSize->isChecked() ? ui.spinBoxFontSize->value() : -1;
	pdfSettings.uniformizeLineSpacing = ui.checkBoxUniformizeSpacing->isChecked();
	pdfSettings.preserveSpaceWidth = ui.spinBoxPreserve->value();
	pdfSettings.overlay = ui.comboBoxOutputMode->currentIndex() == 1;
	pdfSettings.detectedFontScaling = ui.spinFontScaling->value() / 100.;
	pdfSettings.sanitizeHyphens = ui.checkBoxSanitizeHyphens->isChecked();
	pdfSettings.assumedImageDpi = ui.spinBoxPaperSizeDpi->value();
	pdfSettings.outputDpi = ui.spinBoxDpi->value();
	pdfSettings.paperSize = ui.comboBoxPaperSize->itemData(ui.comboBoxPaperSize->currentIndex()).toString();
	pdfSettings.paperSizeLandscape = ui.toolButtonLandscape->isChecked();
	pdfSettings.paperWidthIn = ui.lineEditPaperWidth->text().toDouble();
	pdfSettings.paperHeightIn = ui.lineEditPaperHeight->text().toDouble();

	PaperSize::Unit unit = static_cast<PaperSize::Unit>(ui.comboBoxPaperSizeUnit->itemData(ui.comboBoxPaperSizeUnit->currentIndex()).toInt());
	if(unit == PaperSize::cm) {
		pdfSettings.paperWidthIn /= PaperSize::CMtoInch;
		pdfSettings.paperHeightIn /= PaperSize::CMtoInch;
	}

	pdfSettings.backend = static_cast<HOCRPdfExporter::PDFSettings::PDFBackend>(ui.comboBoxBackend->currentData().toInt());

	pdfSettings.version = static_cast<HOCRPdfExporter::PDFSettings::Version>(ui.comboBoxPdfVersion->currentData().toInt());
	pdfSettings.password = ui.lineEditPasswordOpen->text();
	pdfSettings.producer = ui.lineEditProducer->text();
	pdfSettings.creator = ui.lineEditCreator->text();
	pdfSettings.title = ui.lineEditTitle->text();
	pdfSettings.subject = ui.lineEditSubject->text();
	pdfSettings.keywords = ui.lineEditKeywords->text();
	pdfSettings.author = ui.lineEditAuthor->text();

	return pdfSettings;
}


void HOCRPdfExportWidget::updatePreview() {
	if(!m_preview) {
		return;
	}
	m_preview->setVisible(ui.checkBoxPreview->isChecked());
	if(!m_document || m_document->pageCount() == 0 || !m_previewPage || !ui.checkBoxPreview->isChecked()) {
		return;
	}

	const HOCRPage* page = m_previewPage;
	QRect bbox = page->bbox();
	int pageDpi = page->resolution();

	HOCRPdfExporter::PDFSettings pdfSettings = getPdfSettings();

	QFont defaultFont = ui.checkBoxFontFamily->isChecked() ? ui.comboBoxFontFamily->currentFont() : ui.comboBoxFallbackFontFamily->currentFont();

	QImage image(bbox.size(), QImage::Format_ARGB32);
	image.setDotsPerMeterX(pageDpi / 0.0254); // 1 in = 0.0254 m
	image.setDotsPerMeterY(pageDpi / 0.0254);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	HOCRQPainterPdfPrinter printer(&painter, defaultFont);
	if(!pdfSettings.fontFamily.isEmpty()) {
		printer.setFontFamily(pdfSettings.fontFamily, false, false);
	}
	if(pdfSettings.fontSize != -1) {
		printer.setFontSize(pdfSettings.fontSize);
	}

	if(pdfSettings.overlay) {
		printer.drawImage(bbox, m_displayerTool->getSelection(bbox), pdfSettings);
		painter.fillRect(0, 0, bbox.width(), bbox.height(), QColor(255, 255, 255, 127));
	} else {
		image.fill(Qt::white);
	}
	// Units of QPainter on pixel based devices is pixels, so px2pu = 1.
	printer.printChildren(page, pdfSettings, 1.);
	image.save("/home/sandro/Desktop/a.png");
	m_preview->setPixmap(QPixmap::fromImage(image));
	m_preview->setPos(-0.5 * bbox.width(), -0.5 * bbox.height());
}

void HOCRPdfExportWidget::backendChanged() {
	int jpegIdx = ui.comboBoxImageCompression->findData(HOCRPdfExporter::PDFSettings::CompressJpeg);
	bool podofoBackend = ui.comboBoxBackend->itemData(ui.comboBoxBackend->currentIndex()).toInt() == HOCRPdfExporter::PDFSettings::BackendPoDoFo;
	ui.groupBoxEncryption->setEnabled(podofoBackend);
	ui.labelImageCompression->setEnabled(podofoBackend);
	ui.comboBoxImageCompression->setEnabled(podofoBackend);
	if(!podofoBackend) {
		// QPainter compresses images in PDFs to JPEG at 94% quality
		ui.comboBoxImageCompression->setCurrentIndex(jpegIdx);
		ui.spinBoxCompressionQuality->setValue(94);
	}
	ui.labelAuthor->setEnabled(podofoBackend);
	ui.lineEditAuthor->setEnabled(podofoBackend);
	ui.labelTitle->setEnabled(podofoBackend);
	ui.lineEditTitle->setEnabled(podofoBackend);
	ui.labelProducer->setEnabled(podofoBackend);
	ui.lineEditProducer->setEnabled(podofoBackend);
	ui.labelKeywords->setEnabled(podofoBackend);
	ui.lineEditKeywords->setEnabled(podofoBackend);
	ui.labelSubject->setEnabled(podofoBackend);
	ui.lineEditSubject->setEnabled(podofoBackend);
	ui.labelPdfVersion->setEnabled(podofoBackend);
	ui.comboBoxPdfVersion->setEnabled(podofoBackend);
	ui.spinBoxCompressionQuality->setEnabled(podofoBackend);
	ui.labelCompressionQuality->setEnabled(podofoBackend);
}

void HOCRPdfExportWidget::toggleBackendHint() {
	QString tooltip = _("<html><head/><body><ul><li>PoDoFo: offers more image compression options, but does not handle complex scripts.</li><li>QPrinter: only supports JPEG compression for storing images, but supports complex scripts.</li></ul></body></html>");
	QRect r = ui.toolButtonBackendHint->rect();
	QToolTip::showText(ui.toolButtonBackendHint->mapToGlobal(QPoint(0, 0.5 * r.height())), tooltip, ui.toolButtonBackendHint, r);
}

void HOCRPdfExportWidget::imageFormatChanged() {
	QImage::Format format = static_cast<QImage::Format>(ui.comboBoxImageFormat->itemData(ui.comboBoxImageFormat->currentIndex()).toInt());
	QStandardItemModel* model = static_cast<QStandardItemModel*>(ui.comboBoxImageCompression->model());
	int zipIdx = ui.comboBoxImageCompression->findData(HOCRPdfExporter::PDFSettings::CompressZip);
	int ccittIdx = ui.comboBoxImageCompression->findData(HOCRPdfExporter::PDFSettings::CompressFax4);
	int jpegIdx = ui.comboBoxImageCompression->findData(HOCRPdfExporter::PDFSettings::CompressJpeg);
	QStandardItem* ccittItem = model->item(ccittIdx);
	QStandardItem* jpegItem = model->item(jpegIdx);
	if(format == QImage::Format_Mono) {
		// for monochrome images, allow zip and ccitt4 (but not jpeg, unless QPrinter is forcing us)
		if(ui.comboBoxImageCompression->currentIndex() == jpegIdx && ui.comboBoxBackend->itemData(ui.comboBoxBackend->currentIndex()).toInt() != HOCRPdfExporter::PDFSettings::BackendQPrinter) {
			ui.comboBoxImageCompression->setCurrentIndex(zipIdx);
		}
		ccittItem->setFlags(ccittItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		jpegItem->setFlags(jpegItem->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		ui.labelDithering->setEnabled(true);
		ui.comboBoxDithering->setEnabled(true);
	} else {
		// for color and grayscale images, allow jpeg and zip (but not ccitt4)
		if(ui.comboBoxImageCompression->currentIndex() == ccittIdx) {
			ui.comboBoxImageCompression->setCurrentIndex(zipIdx);
		}
		ccittItem->setFlags(ccittItem->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		jpegItem->setFlags(jpegItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		ui.labelDithering->setEnabled(false);
		ui.comboBoxDithering->setEnabled(false);
	}
}

void HOCRPdfExportWidget::imageCompressionChanged() {
	HOCRPdfExporter::PDFSettings::Compression compression = static_cast<HOCRPdfExporter::PDFSettings::Compression>(ui.comboBoxImageCompression->itemData(ui.comboBoxImageCompression->currentIndex()).toInt());
	bool jpegCompression = compression == HOCRPdfExporter::PDFSettings::CompressJpeg;
	ui.spinBoxCompressionQuality->setEnabled(jpegCompression);
	ui.labelCompressionQuality->setEnabled(jpegCompression);
}

void HOCRPdfExportWidget::paperSizeChanged() {
	QString paperSize = ui.comboBoxPaperSize->itemData(ui.comboBoxPaperSize->currentIndex()).toString();
	if(paperSize == "custom") {
		ui.lineEditPaperWidth->setEnabled(true);
		ui.lineEditPaperHeight->setEnabled(true);
		ui.widgetPaperSize->setVisible(true);
		ui.widgetPaperOrientation->setVisible(false);
		ui.widgetPaperSizeDpi->setVisible(false);
	} else if(paperSize == "source") {
		ui.lineEditPaperWidth->setEnabled(true);
		ui.lineEditPaperHeight->setEnabled(true);
		ui.widgetPaperSize->setVisible(false);
		ui.widgetPaperOrientation->setVisible(false);
		ui.widgetPaperSizeDpi->setVisible(true);
	} else {
		ui.widgetPaperSize->setVisible(true);
		ui.widgetPaperOrientation->setVisible(true);
		ui.widgetPaperSizeDpi->setVisible(false);
		ui.lineEditPaperWidth->setDisabled(true);
		ui.lineEditPaperHeight->setDisabled(true);
		PaperSize::Unit unit = static_cast<PaperSize::Unit>(ui.comboBoxPaperSizeUnit->itemData(ui.comboBoxPaperSizeUnit->currentIndex()).toInt());
		auto outputPaperSize = PaperSize::getSize(unit, paperSize.toStdString(), ui.toolButtonLandscape->isChecked());
		QLocale locale;
		ui.lineEditPaperWidth->setText(locale.toString(outputPaperSize.width, 'f', 1));
		ui.lineEditPaperHeight->setText(locale.toString(outputPaperSize.height, 'f', 1));
	}
	updateValid();
}

void HOCRPdfExportWidget::updateValid() {
	bool valid = true;

	// Passwords must match
	if(ui.lineEditPasswordOpen->text() == ui.lineEditConfirmPasswordOpen->text()) {
		ui.lineEditConfirmPasswordOpen->setStyleSheet(QStringLiteral(""));
	} else {
		ui.lineEditConfirmPasswordOpen->setStyleSheet("background: #FF7777; color: #FFFFFF;");
		valid = false;
	}

	// In custom paper size mode, size must be specified
	QString paperSize = ui.comboBoxPaperSize->itemData(ui.comboBoxPaperSize->currentIndex()).toString();
	ui.lineEditPaperWidth->setStyleSheet(QStringLiteral(""));
	ui.lineEditPaperHeight->setStyleSheet(QStringLiteral(""));
	if(paperSize == "custom") {
		if(ui.lineEditPaperWidth->text().isEmpty()) {
			ui.lineEditPaperWidth->setStyleSheet("background: #FF7777; color: #FFFFFF;");
			valid = false;
		}
		if(ui.lineEditPaperHeight->text().isEmpty()) {
			ui.lineEditPaperHeight->setStyleSheet("background: #FF7777; color: #FFFFFF;");
			valid = false;
		}
	}
	emit validChanged(valid);
//	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

void HOCRPdfExportWidget::importMetadataFromSource() {
	QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
	if(!sources.isEmpty()) {
		Source* source = sources.first();
		ui.lineEditAuthor->setText(source->author);
		ui.lineEditCreator->setText(source->creator);
		ui.lineEditKeywords->setText(source->keywords);
		ui.lineEditTitle->setText(source->title);
		ui.lineEditSubject->setText(source->subject);
		ui.lineEditProducer->setText(source->producer);

		HOCRPdfExporter::PDFSettings::Version sourcePdfVersion = HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_7;
		if(source->pdfVersionMajor == 1 && source->pdfVersionMinor >= 0 && source->pdfVersionMinor <= 7) {
			sourcePdfVersion = static_cast<HOCRPdfExporter::PDFSettings::Version>(HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_0 + source->pdfVersionMinor);
		}
		ui.comboBoxPdfVersion->setCurrentIndex(ui.comboBoxPdfVersion->findData(sourcePdfVersion));
	}
}

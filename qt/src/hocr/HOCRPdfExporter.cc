/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExporter.cc
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

#include <cstring>
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
#include <QToolTip>
#include <QUrl>
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
#include "HOCRDocument.hh"
#include "HOCRPdfExporter.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "PaperSize.hh"
#include "Utils.hh"

class HOCRPdfExporter::QPainterPDFPainter : public HOCRPdfExporter::PDFPainter {
public:
	QPainterPDFPainter(QPainter* painter, const QFont& defaultFont)
		: m_painter(painter), m_defaultFont(defaultFont) {
		m_curFont = m_defaultFont;
	}
	void setFontFamily(const QString& family, bool bold, bool italic) override {
		float curSize = m_curFont.pointSize();
		if(m_fontDatabase.hasFamily(family)) {
			m_curFont.setFamily(family);
		}  else {
			m_curFont = m_defaultFont;
		}
		m_curFont.setPointSize(curSize);
		m_curFont.setBold(bold);
		m_curFont.setItalic(italic);
		m_painter->setFont(m_curFont);
	}
	void setFontSize(double pointSize) override {
		if(pointSize != m_curFont.pointSize()) {
			m_curFont.setPointSize(pointSize);
			m_painter->setFont(m_curFont);
		}
	}
	void drawText(double x, double y, const QString& text) override {
		m_painter->drawText(m_offsetX + x, m_offsetY + y, text);
	}
	void drawImage(const QRect& bbox, const QImage& image, const PDFSettings& settings) override {
		QImage img = convertedImage(image, settings.colorFormat, settings.conversionFlags);
		if(settings.compression == PDFSettings::CompressJpeg) {
			QByteArray data;
			QBuffer buffer(&data);
			img.save(&buffer, "jpg", settings.compressionQuality);
			img = QImage::fromData(data);
		}
		m_painter->drawImage(bbox, img);
	}
	double getAverageCharWidth() const override {
		return m_painter->fontMetrics().averageCharWidth();
	}
	double getTextWidth(const QString& text) const override {
		return m_painter->fontMetrics().width(text);
	}

protected:
	QFontDatabase m_fontDatabase;
	QPainter* m_painter;
	QFont m_curFont;
	QFont m_defaultFont;
	double m_offsetX = 0.0;
	double m_offsetY = 0.0;
};

class HOCRPdfExporter::QPrinterPDFPainter : public HOCRPdfExporter::QPainterPDFPainter {
public:
	QPrinterPDFPainter(const QString& filename, const QString& creator, const QFont& defaultFont)
		: QPainterPDFPainter(nullptr, defaultFont) {
		m_printer.setOutputFormat(QPrinter::PdfFormat);
		m_printer.setOutputFileName(filename);
		m_printer.setResolution(72); // This to ensure that painter units are points - images are passed pre-scaled to drawImage, so that the resulting dpi in the box is the output dpi (the document resolution only matters for images)
		m_printer.setFontEmbeddingEnabled(true);
		m_printer.setFullPage(true);
		m_printer.setCreator(creator);
	}
	~QPrinterPDFPainter() {
		delete m_painter;
	}
	bool createPage(double width, double height, double offsetX, double offsetY, QString& errMsg) override {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
		m_printer.setPaperSize(width, pageDpi), QPrinter::Point);
#else
		m_printer.setPageSize(QPageSize(QSizeF(width, height), QPageSize::Point));
#endif
		if(!m_firstPage) {
		if(!m_printer.newPage()) {
				return false;
			}
		} else {
			// Need to create painter after setting paper size, otherwise default paper size is used for first page...
			m_painter = new QPainter();
			m_painter->setRenderHint(QPainter::Antialiasing);
			if(!m_painter->begin(&m_printer)) {
				errMsg = _("Failed to write to file");
				return false;
			}
			m_firstPage = false;
		}
		m_curFont = m_defaultFont;
		m_painter->setFont(m_curFont);
		m_offsetX = offsetX;
		m_offsetY = offsetY;
		return true;
	}
	bool finishDocument(QString& /*errMsg*/) override {
		return m_painter->end();
	}
	void drawImage(const QRect& bbox, const QImage& image, const PDFSettings& settings) override {
		QImage img = convertedImage(image, settings.colorFormat, settings.conversionFlags);
		m_painter->drawImage(bbox, img);
	}

private:
	QPrinter m_printer;
	bool m_firstPage = true;
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
	PoDoFoPDFPainter(PoDoFo::PdfStreamedDocument* document, const PoDoFo::PdfEncoding* fontEncoding, PoDoFo::PdfFont* defaultFont, const QString& defaultFontFamily, double defaultFontSize)
#else
	PoDoFoPDFPainter(PoDoFo::PdfStreamedDocument* document, PoDoFo::PdfEncoding* fontEncoding, PoDoFo::PdfFont* defaultFont, const QString& defaultFontFamily, double defaultFontSize)
#endif
		: m_document(document), m_pdfFontEncoding(fontEncoding), m_defaultFont(defaultFont), m_defaultFontFamily(defaultFontFamily), m_defaultFontSize(defaultFontSize) {
	}
	~PoDoFoPDFPainter() {
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0,9,3)
		delete m_pdfFontEncoding;
#endif
		delete m_document;
		// Fonts are deleted by the internal PoDoFo font cache of the document
	}
	bool createPage(double width, double height, double offsetX, double offsetY, QString& /*errMsg*/) override {
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
	bool finishDocument(QString& errMsg) override {
		try {
			m_document->Close();
		} catch(PoDoFo::PdfError& e) {
			errMsg = e.what();
			return false;
		}
		return true;
	}
	void setFontFamily(const QString& family, bool bold, bool italic) override {
		float curSize = m_painter.GetFont()->GetFontSize();
		m_painter.SetFont(getFont(family, bold, italic));
		m_painter.GetFont()->SetFontSize(curSize);
	}
	void setFontSize(double pointSize) override {
		m_painter.GetFont()->SetFontSize(pointSize);
	}
	void drawText(double x, double y, const QString& text) override {
		PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*>(text.toUtf8().data()));
		m_painter.DrawText(m_offsetX + x, m_pageHeight - m_offsetY - y, pdfString);
	}
	void drawImage(const QRect& bbox, const QImage& image, const PDFSettings& settings) override {
		QImage img = convertedImage(image, settings.colorFormat, settings.conversionFlags);
		if(settings.colorFormat == QImage::Format_Mono) {
			img.invertPixels();
		}
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
		PoDoFo::PdfImage pdfImage(m_document);
#else
		PoDoFo::PdfImageCompat pdfImage(m_document);
#endif
		pdfImage.SetImageColorSpace(img.format() == QImage::Format_RGB888 ? PoDoFo::ePdfColorSpace_DeviceRGB : PoDoFo::ePdfColorSpace_DeviceGray);
		int width = img.width();
		int height = img.height();
		int sampleSize = settings.colorFormat == QImage::Format_Mono ? 1 : 8;
		if(settings.compression == PDFSettings::CompressZip) {
			// QImage has 32-bit aligned scanLines, but we need a continuous buffer
			int numComponents = settings.colorFormat == QImage::Format_RGB888 ? 3 : 1;
			int bytesPerLine = numComponents * ((width * sampleSize) / 8 + ((width * sampleSize) % 8 != 0));
			QVector<char> buf(bytesPerLine * height);
			for(int y = 0; y < height; ++y) {
				std::memcpy(buf.data() + y * bytesPerLine, img.scanLine(y), bytesPerLine);
			}
			PoDoFo::PdfMemoryInputStream is(buf.data(), bytesPerLine * height);
			pdfImage.SetImageData(width, height, sampleSize, &is, {PoDoFo::ePdfFilter_FlateDecode});
		} else if(settings.compression == PDFSettings::CompressJpeg) {
			PoDoFo::PdfName dctFilterName(PoDoFo::PdfFilterFactory::FilterTypeToName(PoDoFo::ePdfFilter_DCTDecode));
			pdfImage.GetObject()->GetDictionary().AddKey(PoDoFo::PdfName::KeyFilter, dctFilterName);
			QByteArray data;
			QBuffer buffer(&data);
			img.save(&buffer, "jpg", settings.compressionQuality);
			PoDoFo::PdfMemoryInputStream is(data.data(), data.size());
			pdfImage.SetImageDataRaw(width, height, sampleSize, &is);
		} else if(settings.compression == PDFSettings::CompressFax4) {
			PoDoFo::PdfName faxFilterName(PoDoFo::PdfFilterFactory::FilterTypeToName(PoDoFo::ePdfFilter_CCITTFaxDecode));
			pdfImage.GetObject()->GetDictionary().AddKey(PoDoFo::PdfName::KeyFilter, faxFilterName);
			PoDoFo::PdfDictionary decodeParams;
			decodeParams.AddKey("Columns", PoDoFo::PdfObject(PoDoFo::pdf_int64(img.width())));
			decodeParams.AddKey("Rows", PoDoFo::PdfObject(PoDoFo::pdf_int64(img.height())));
			decodeParams.AddKey("K", PoDoFo::PdfObject(PoDoFo::pdf_int64(-1))); // K < 0 --- Pure two-dimensional encoding (Group 4)
			pdfImage.GetObject()->GetDictionary().AddKey("DecodeParms", PoDoFo::PdfObject(decodeParams));
			CCITTFax4Encoder encoder;
			uint32_t encodedLen = 0;
			uint8_t* encoded = encoder.encode(img.constBits(), img.width(), img.height(), img.bytesPerLine(), encodedLen);
			PoDoFo::PdfMemoryInputStream is(reinterpret_cast<char*>(encoded), encodedLen);
			pdfImage.SetImageDataRaw(img.width(), img.height(), sampleSize, &is);
		}
		m_painter.DrawImage(m_offsetX + bbox.x(), m_pageHeight - m_offsetY - (bbox.y() + bbox.height()),
		                    &pdfImage, bbox.width() / double(image.width()), bbox.height() / double(image.height()));
	}
	double getAverageCharWidth() const override {
		return m_painter.GetFont()->GetFontMetrics()->CharWidth(static_cast<unsigned char>('x'));
	}
	double getTextWidth(const QString& text) const override {
		PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*>(text.toUtf8().data()));
		return m_painter.GetFont()->GetFontMetrics()->StringWidth(pdfString);
	}

private:
	QFontDatabase m_fontDatabase;
	QMap<QString, PoDoFo::PdfFont*> m_fontCache;
	PoDoFo::PdfPainter m_painter;
	PoDoFo::PdfStreamedDocument* m_document;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
	const PoDoFo::PdfEncoding* m_pdfFontEncoding;
#else
	PoDoFo::PdfEncoding* m_pdfFontEncoding;
#endif
	PoDoFo::PdfFont* m_defaultFont;
	QString m_defaultFontFamily;
	double m_defaultFontSize = -1.0;
	double m_pageHeight = 0.0;
	double m_offsetX = 0.0;
	double m_offsetY = 0.0;
	PoDoFo::PdfFont* getFont(QString family, bool bold, bool italic) {
		QString key = family + (bold ? "@bold" : "") + (italic ? "@italic" : "");
		auto it = m_fontCache.find(key);
		if(it == m_fontCache.end()) {
			if(family.isEmpty() || !m_fontDatabase.hasFamily(family)) {
				family = m_defaultFontFamily;
			}
			PoDoFo::PdfFont* font = nullptr;
			try {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
				font = m_document->CreateFontSubset(family.toLocal8Bit().data(), bold, italic, false, m_pdfFontEncoding);
#else
				font = m_document->CreateFontSubset(family.toLocal8Bit().data(), bold, italic, m_pdfFontEncoding);
#endif
				it = m_fontCache.insert(key, font);
			} catch(PoDoFo::PdfError& /*err*/) {
				it = m_fontCache.insert(key, m_defaultFont);;
			}
		}
		return it.value();
	}
};


HOCRPdfExporter::HOCRPdfExporter(const HOCRDocument* hocrdocument, const HOCRPage* previewPage, DisplayerToolHOCR* displayerTool, QWidget* parent)
	: QDialog(parent), m_hocrdocument(hocrdocument), m_previewPage(previewPage), m_displayerTool(displayerTool) {
	ui.setupUi(this);

	ui.comboBoxBackend->addItem("PoDoFo", BackendPoDoFo);
	ui.comboBoxBackend->addItem("QPrinter", BackendQPrinter);
	ui.comboBoxBackend->setCurrentIndex(-1);

	ui.comboBoxImageFormat->addItem(_("Color"), QImage::Format_RGB888);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	ui.comboBoxImageFormat->addItem(_("Grayscale"), QImage::Format_Indexed8);
#else
	ui.comboBoxImageFormat->addItem(_("Grayscale"), QImage::Format_Grayscale8);
#endif
	ui.comboBoxImageFormat->addItem(_("Monochrome"), QImage::Format_Mono);
	ui.comboBoxImageFormat->setCurrentIndex(-1);
	ui.comboBoxDithering->addItem(_("Threshold (closest color)"), Qt::ThresholdDither);
	ui.comboBoxDithering->addItem(_("Diffuse"), Qt::DiffuseDither);
	ui.comboBoxImageCompression->addItem(_("Zip (lossless)"), PDFSettings::CompressZip);
	ui.comboBoxImageCompression->addItem(_("CCITT Group 4 (lossless)"), PDFSettings::CompressFax4);
	ui.comboBoxImageCompression->addItem(_("Jpeg (lossy)"), PDFSettings::CompressJpeg);
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

	ui.comboBoxPdfVersion->addItem("1.0", PoDoFo::EPdfVersion::ePdfVersion_1_0);
	ui.comboBoxPdfVersion->addItem("1.1", PoDoFo::EPdfVersion::ePdfVersion_1_1);
	ui.comboBoxPdfVersion->addItem("1.2", PoDoFo::EPdfVersion::ePdfVersion_1_2);
	ui.comboBoxPdfVersion->addItem("1.3", PoDoFo::EPdfVersion::ePdfVersion_1_3);
	ui.comboBoxPdfVersion->addItem("1.4", PoDoFo::EPdfVersion::ePdfVersion_1_4);
	ui.comboBoxPdfVersion->addItem("1.5", PoDoFo::EPdfVersion::ePdfVersion_1_5);
	ui.comboBoxPdfVersion->addItem("1.6", PoDoFo::EPdfVersion::ePdfVersion_1_6);
	ui.comboBoxPdfVersion->addItem("1.7", PoDoFo::EPdfVersion::ePdfVersion_1_7);

	ui.comboBoxPdfVersion->setCurrentIndex(-1);

	connect(ui.comboBoxBackend, SIGNAL(currentIndexChanged(int)), this, SLOT(backendChanged()));
	connect(ui.toolButtonBackendHint, SIGNAL(clicked(bool)), this, SLOT(toggleBackendHint()));
	connect(ui.comboBoxOutputMode, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
	connect(ui.comboBoxImageFormat, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
	connect(ui.comboBoxImageFormat, SIGNAL(currentIndexChanged(int)), this, SLOT(imageFormatChanged()));
	connect(ui.comboBoxDithering, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
	connect(ui.comboBoxImageCompression, SIGNAL(currentIndexChanged(int)), this, SLOT(imageCompressionChanged()));
	connect(ui.spinBoxCompressionQuality, SIGNAL(valueChanged(int)), this, SLOT(updatePreview()));
	connect(ui.checkBoxFontFamily, SIGNAL(toggled(bool)), ui.comboBoxFontFamily, SLOT(setEnabled(bool)));
	connect(ui.checkBoxFontFamily, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(ui.checkBoxFontFamily, SIGNAL(toggled(bool)), ui.labelFallbackFontFamily, SLOT(setDisabled(bool)));
	connect(ui.checkBoxFontFamily, SIGNAL(toggled(bool)), ui.comboBoxFallbackFontFamily, SLOT(setDisabled(bool)));
	connect(ui.comboBoxFontFamily, SIGNAL(currentFontChanged(QFont)), this, SLOT(updatePreview()));
	connect(ui.comboBoxFallbackFontFamily, SIGNAL(currentFontChanged(QFont)), this, SLOT(updatePreview()));
	connect(ui.checkBoxFontSize, SIGNAL(toggled(bool)), ui.spinBoxFontSize, SLOT(setEnabled(bool)));
	connect(ui.checkBoxFontSize, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(ui.checkBoxFontSize, SIGNAL(toggled(bool)), ui.labelFontScaling, SLOT(setDisabled(bool)));
	connect(ui.checkBoxFontSize, SIGNAL(toggled(bool)), ui.spinFontScaling, SLOT(setDisabled(bool)));
	connect(ui.spinBoxFontSize, SIGNAL(valueChanged(int)), this, SLOT(updatePreview()));
	connect(ui.spinFontScaling, SIGNAL(valueChanged(int)), this, SLOT(updatePreview()));
	connect(ui.checkBoxUniformizeSpacing, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(ui.spinBoxPreserve, SIGNAL(valueChanged(int)), this, SLOT(updatePreview()));
	connect(ui.checkBoxUniformizeSpacing, SIGNAL(toggled(bool)), ui.labelPreserve, SLOT(setEnabled(bool)));
	connect(ui.checkBoxUniformizeSpacing, SIGNAL(toggled(bool)), ui.labelPreserveCharacters, SLOT(setEnabled(bool)));
	connect(ui.checkBoxUniformizeSpacing, SIGNAL(toggled(bool)), ui.spinBoxPreserve, SLOT(setEnabled(bool)));
	connect(ui.lineEditPasswordOpen, SIGNAL(textChanged(const QString&)), this, SLOT(updateValid()));
	connect(ui.lineEditConfirmPasswordOpen, SIGNAL(textChanged(const QString&)), this, SLOT(updateValid()));
	connect(ui.checkBoxPreview, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(ui.comboBoxPaperSize, SIGNAL(currentIndexChanged(int)), this, SLOT(paperSizeChanged()));
	connect(ui.comboBoxPaperSizeUnit, SIGNAL(currentIndexChanged(int)), this, SLOT(paperSizeChanged()));
	connect(ui.toolButtonLandscape, SIGNAL(toggled(bool)), this, SLOT(paperSizeChanged()));
	connect(ui.lineEditPaperWidth, SIGNAL(textChanged(QString)), this, SLOT(updateValid()));
	connect(ui.lineEditPaperHeight, SIGNAL(textChanged(QString)), this, SLOT(updateValid()));
	connect(ui.pushButtonImport, SIGNAL(clicked(bool)), this, SLOT(importMetadataFromSource()));

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
	ADD_SETTING(ComboSetting("pdfexportpdfversion", ui.comboBoxPdfVersion, ui.comboBoxPdfVersion->findData(PoDoFo::EPdfVersion::ePdfVersion_1_7)));
	ADD_SETTING(ComboSetting("pdfexportbackend", ui.comboBoxBackend));

#ifndef MAKE_VERSION
#define MAKE_VERSION(...) 0
#endif
#if !defined(TESSERACT_VERSION) || TESSERACT_VERSION < MAKE_VERSION(3,04,00)
	ui.checkBoxFontFamily->setChecked(true);
	ui.checkBoxFontFamily->setEnabled(false);
	ui.checkBoxFontSize->setChecked(true);
	ui.checkBoxFontSize->setEnabled(false);
#endif
}

bool HOCRPdfExporter::run(QString& filebasename) {
	m_preview = new QGraphicsPixmapItem();
	m_preview->setTransformationMode(Qt::SmoothTransformation);
	m_preview->setZValue(2);
	updatePreview();
	MAIN->getDisplayer()->scene()->addItem(m_preview);

	bool accepted = false;
	PDFPainter* painter = nullptr;
	int pageCount = m_hocrdocument->pageCount();

	QString outname;
	while(true) {
		accepted = (exec() == QDialog::Accepted);
		if(!accepted) {
			break;
		}

		QString suggestion = filebasename;
		if(suggestion.isEmpty()) {
			QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
			suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
		}

		while(true) {
			outname = FileDialogs::saveDialog(_("Save PDF Output..."), suggestion + ".pdf", "outputdir", QString("%1 (*.pdf)").arg(_("PDF Files")));
			if(outname.isEmpty()) {
				accepted = false;
				break;
			}
			if(m_hocrdocument->referencesSource(outname)) {
				QMessageBox::warning(MAIN, _("Invalid Output"), _("Cannot overwrite a file which is a source image of this document."));
				continue;
			}
			break;
		}
		if(!accepted) {
			break;
		}
		filebasename = QFileInfo(outname).completeBaseName();

		QFont defaultFont = ui.checkBoxFontFamily->isChecked() ? ui.comboBoxFontFamily->currentFont() : ui.comboBoxFallbackFontFamily->currentFont();
		defaultFont.setPointSize(ui.checkBoxFontSize->isChecked() ? ui.spinBoxFontSize->value() : 0);

		QString errMsg;

		PDFBackend backend = static_cast<PDFBackend>(ui.comboBoxBackend->itemData(ui.comboBoxBackend->currentIndex()).toInt());
		if(backend == BackendPoDoFo) {
			painter = createPoDoFoPrinter(outname, defaultFont, errMsg);
		} else if(backend == BackendQPrinter) {
			painter = new QPrinterPDFPainter(outname, ui.lineEditCreator->text(), defaultFont);
		}

		if(!painter) {
			QMessageBox::critical(MAIN, _("Failed to create output"), _("Failed to create output. The returned error was: %1").arg(errMsg));
			continue;
		}
		break;
	}

	MAIN->getDisplayer()->scene()->removeItem(m_preview);
	delete m_preview;
	m_preview = nullptr;
	if(!accepted) {
		return false;
	}

	PDFSettings pdfSettings = getPdfSettings();
	int outputDpi = ui.spinBoxDpi->value();
	QString paperSize = ui.comboBoxPaperSize->itemData(ui.comboBoxPaperSize->currentIndex()).toString();

	double pageWidth, pageHeight;
	// Page dimensions are in points: 1 in = 72 pt
	if(paperSize == "custom") {
		pageWidth = ui.lineEditPaperWidth->text().toDouble() * 72.0;
		pageHeight = ui.lineEditPaperHeight->text().toDouble() * 72.0;

		PaperSize::Unit unit = static_cast<PaperSize::Unit>(ui.comboBoxPaperSizeUnit->itemData(ui.comboBoxPaperSizeUnit->currentIndex()).toInt());
		if(unit == PaperSize::cm) {
			pageWidth /= PaperSize::CMtoInch;
			pageHeight /= PaperSize::CMtoInch;
		}
	} else if (paperSize != "source") {
		auto inchSize = PaperSize::getSize(PaperSize::inch, paperSize.toStdString(), ui.toolButtonLandscape->isChecked());
		pageWidth = inchSize.width * 72.0;
		pageHeight = inchSize.height * 72.0;
	}

	MainWindow::ProgressMonitor monitor(pageCount);
	MAIN->showProgress(&monitor);
	QString errMsg;
	bool success = Utils::busyTask([&] {
		for(int i = 0; i < pageCount; ++i) {
			if(monitor.cancelled()) {
				errMsg = _("The operation was cancelled");
				return false;
			}
			const HOCRPage* page = m_hocrdocument->page(i);
			if(page->isEnabled()) {
				QRect bbox = page->bbox();
				int sourceDpi = page->resolution();
				bool success = false;
				QMetaObject::invokeMethod(this, "setSource", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, success), Q_ARG(QString, page->sourceFile()), Q_ARG(int, page->pageNr()), Q_ARG(int, outputDpi), Q_ARG(double, page->angle()));
				if(success) {
					// [pt] = 72 * [in]
					// [in] = 1 / dpi * [px]
					// => [pt] = 72 / dpi * [px]
					double px2pt = (72.0 / sourceDpi);
					double imgScale = double(outputDpi) / sourceDpi;
					if(paperSize == "source") {
						pageWidth = bbox.width() * px2pt;
						pageHeight = bbox.height() * px2pt;
					}
					double offsetX = 0.5 * (pageWidth - bbox.width() * px2pt);
					double offsetY = 0.5 * (pageHeight - bbox.height() * px2pt);
					if(!painter->createPage(pageWidth, pageHeight, offsetX, offsetY, errMsg)) {
						return false;
					}
					printChildren(*painter, page, pdfSettings, px2pt, imgScale);
					if(pdfSettings.overlay) {
						QRect scaledRect(imgScale * bbox.left(), imgScale * bbox.top(), imgScale * bbox.width(), imgScale * bbox.height());
						QRect printRect(bbox.left() * px2pt, bbox.top() * px2pt, bbox.width() * px2pt, bbox.height() * px2pt);
						QImage selection;
						QMetaObject::invokeMethod(this, "getSelection",  Qt::BlockingQueuedConnection, Q_RETURN_ARG(QImage, selection), Q_ARG(QRect, scaledRect));
						painter->drawImage(printRect, selection, pdfSettings);
					}
					QMetaObject::invokeMethod(this, "setSource", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, success), Q_ARG(QString, page->sourceFile()), Q_ARG(int, page->pageNr()), Q_ARG(int, sourceDpi), Q_ARG(double, page->angle()));
					painter->finishPage();
				} else {
					errMsg = _("Failed to render page %1").arg(page->title());
					return false;;
				}
			}
			monitor.increaseProgress();
		}
		return painter->finishDocument(errMsg);
	}, _("Exporting to PDF..."));
	MAIN->hideProgress();

	bool openAfterExport = ConfigSettings::get<SwitchSetting>("openafterexport")->getValue();
	if(!success) {
		QMessageBox::warning(MAIN, _("Export failed"), _("The PDF export failed: %1.").arg(errMsg));
	} else if(openAfterExport) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(outname));
	}
	delete painter;

	return success;
}

HOCRPdfExporter::PDFPainter* HOCRPdfExporter::createPoDoFoPrinter(const QString& filename, const QFont& defaultFont, QString& errMsg) {
	PoDoFo::PdfStreamedDocument* document = nullptr;
	PoDoFo::PdfFont* defaultPdfFont = nullptr;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
	const PoDoFo::PdfEncoding* pdfFontEncoding = PoDoFo::PdfEncodingFactory::GlobalIdentityEncodingInstance();
#else
	PoDoFo::PdfEncoding* pdfFontEncoding = new PoDoFo::PdfIdentityEncoding;
#endif

	try {
		const std::string password = ui.lineEditPasswordOpen->text().toStdString();
		PoDoFo::PdfEncrypt* encrypt = nullptr;
		if(!password.empty()) {
			encrypt = PoDoFo::PdfEncrypt::CreatePdfEncrypt(password,
			          password,
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
		PoDoFo::EPdfVersion pdfVersion = static_cast<PoDoFo::EPdfVersion>(ui.comboBoxPdfVersion->itemData(ui.comboBoxPdfVersion->currentIndex()).toInt());
		document = new PoDoFo::PdfStreamedDocument(filename.toLocal8Bit().data(), pdfVersion, encrypt);
	} catch(PoDoFo::PdfError& err) {
		errMsg = err.what();
		return nullptr;
	}

	QFontInfo info(defaultFont);

	// Attempt to load the default/fallback font to ensure it is valid
	try {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
		defaultPdfFont = document->CreateFontSubset(info.family().toLocal8Bit().data(), false, false, false, pdfFontEncoding);
#else
		defaultPdfFont = document->CreateFontSubset(info.family().toLocal8Bit().data(), false, false, pdfFontEncoding);
#endif
	} catch(PoDoFo::PdfError&) {
	}
	if(!defaultPdfFont) {
		errMsg = _("The PDF library could not load the font '%1'.").arg(defaultFont.family());
		return nullptr;
	}

	// Set PDF info
	PoDoFo::PdfInfo* pdfInfo = document->GetInfo();
	pdfInfo->SetProducer(ui.lineEditProducer->text().toStdString());
	pdfInfo->SetCreator(ui.lineEditCreator->text().toStdString());
	pdfInfo->SetTitle(ui.lineEditTitle->text().toStdString());
	pdfInfo->SetSubject(ui.lineEditSubject->text().toStdString());
	pdfInfo->SetKeywords(ui.lineEditKeywords->text().toStdString());
	pdfInfo->SetAuthor(ui.lineEditAuthor->text().toStdString());

	return new PoDoFoPDFPainter(document, pdfFontEncoding, defaultPdfFont, defaultFont.family(), defaultFont.pointSize());
}

HOCRPdfExporter::PDFSettings HOCRPdfExporter::getPdfSettings() const {
	PDFSettings pdfSettings;
	pdfSettings.colorFormat = static_cast<QImage::Format>(ui.comboBoxImageFormat->itemData(ui.comboBoxImageFormat->currentIndex()).toInt());
	pdfSettings.conversionFlags = pdfSettings.colorFormat == QImage::Format_Mono ? static_cast<Qt::ImageConversionFlags>(ui.comboBoxDithering->itemData(ui.comboBoxDithering->currentIndex()).toInt()) : Qt::AutoColor;
	pdfSettings.compression = static_cast<PDFSettings::Compression>(ui.comboBoxImageCompression->itemData(ui.comboBoxImageCompression->currentIndex()).toInt());
	pdfSettings.compressionQuality = ui.spinBoxCompressionQuality->value();
	pdfSettings.fontFamily = ui.checkBoxFontFamily->isChecked() ? ui.comboBoxFontFamily->currentFont().family() : "";
	pdfSettings.fontSize = ui.checkBoxFontSize->isChecked() ? ui.spinBoxFontSize->value() : -1;
	pdfSettings.uniformizeLineSpacing = ui.checkBoxUniformizeSpacing->isChecked();
	pdfSettings.preserveSpaceWidth = ui.spinBoxPreserve->value();
	pdfSettings.overlay = ui.comboBoxOutputMode->currentIndex() == 1;
	pdfSettings.detectedFontScaling = ui.spinFontScaling->value() / 100.;
	return pdfSettings;
}

void HOCRPdfExporter::printChildren(PDFPainter& painter, const HOCRItem* item, const PDFSettings& pdfSettings, double px2pu/*pixels to printer units*/, double imgScale) {
	if(!item->isEnabled()) {
		return;
	}
	QString itemClass = item->itemClass();
	QRect itemRect = item->bbox();
	int childCount = item->children().size();
	bool prevSpacedWord, currentSpacedWord;
	prevSpacedWord = currentSpacedWord = false;
	if(itemClass == "ocr_par" && pdfSettings.uniformizeLineSpacing) {
		double yInc = double(itemRect.height()) / childCount;
		double y = itemRect.top() + yInc;
		QPair<double, double> baseline = childCount > 0 ? item->children()[0]->baseLine() : qMakePair(0.0, 0.0);
		for(int iLine = 0; iLine < childCount; ++iLine, y += yInc) {
			HOCRItem* lineItem = item->children()[iLine];
			int x = itemRect.x();
			int prevWordRight = itemRect.x();
			for(int iWord = 0, nWords = lineItem->children().size(); iWord < nWords; ++iWord) {
				HOCRItem* wordItem = lineItem->children()[iWord];
				if(!wordItem->isEnabled()) {
					continue;
				}
				QRect wordRect = wordItem->bbox();
				painter.setFontFamily(pdfSettings.fontFamily.isEmpty() ? wordItem->fontFamily() : pdfSettings.fontFamily, wordItem->fontBold(), wordItem->fontItalic());
				if(pdfSettings.fontSize == -1) {
					painter.setFontSize(wordItem->fontSize() * pdfSettings.detectedFontScaling);
				}

				prevWordRight = wordRect.right();
				QString text = wordItem->text();
				currentSpacedWord = Utils::SpacedWord(text);
				// If distance from previous word is large, keep the space
				if(wordRect.x() - prevWordRight > pdfSettings.preserveSpaceWidth * painter.getAverageCharWidth() / px2pu) {
					x = wordRect.x();
				} else {
					//need space
					if(currentSpacedWord && prevSpacedWord ) {
						x += painter.getTextWidth(" ") / px2pu;
					}
				}

				double wordBaseline = (x - itemRect.x()) * baseline.first + baseline.second;
				painter.drawText(x * px2pu, (y + wordBaseline) * px2pu, text);
				x += painter.getTextWidth(text) / px2pu;
				prevSpacedWord = currentSpacedWord;
			}
		}
	} else if(itemClass == "ocr_line" && !pdfSettings.uniformizeLineSpacing) {
		QPair<double, double> baseline = item->baseLine();
		for(int iWord = 0, nWords = item->children().size(); iWord < nWords; ++iWord) {
			HOCRItem* wordItem = item->children()[iWord];
			if(!wordItem->isEnabled()) {
				continue;
			}
			QRect wordRect = wordItem->bbox();
			painter.setFontFamily(pdfSettings.fontFamily.isEmpty() ? wordItem->fontFamily() : pdfSettings.fontFamily, wordItem->fontBold(), wordItem->fontItalic());
			if(pdfSettings.fontSize == -1) {
				painter.setFontSize(wordItem->fontSize() * pdfSettings.detectedFontScaling);
			}
			double y = itemRect.bottom() + (wordRect.center().x() - itemRect.x()) * baseline.first + baseline.second;
			painter.drawText(wordRect.x() * px2pu, y * px2pu, wordItem->text());
		}
	} else if(itemClass == "ocr_graphic" && !pdfSettings.overlay) {
		QRect scaledItemRect(itemRect.left() * imgScale, itemRect.top() * imgScale, itemRect.width() * imgScale, itemRect.height() * imgScale);
		QRect printRect(itemRect.left() * px2pu, itemRect.top() * px2pu, itemRect.width() * px2pu, itemRect.height() * px2pu);
		QImage selection;
		QMetaObject::invokeMethod(this, "getSelection", QThread::currentThread() == qApp->thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection, Q_RETURN_ARG(QImage, selection), Q_ARG(QRect, scaledItemRect));
		painter.drawImage(printRect, selection, pdfSettings);
	} else {
		for(int i = 0, n = item->children().size(); i < n; ++i) {
			printChildren(painter, item->children()[i], pdfSettings, px2pu, imgScale);
		}
	}
}

void HOCRPdfExporter::updatePreview() {
	if(!m_preview) {
		return;
	}
	m_preview->setVisible(ui.checkBoxPreview->isChecked());
	if(m_hocrdocument->pageCount() == 0 || !ui.checkBoxPreview->isChecked()) {
		return;
	}

	const HOCRPage* page = m_previewPage;
	QRect bbox = page->bbox();
	int pageDpi = page->resolution();

	PDFSettings pdfSettings = getPdfSettings();

	QFont defaultFont = ui.checkBoxFontFamily->isChecked() ? ui.comboBoxFontFamily->currentFont() : ui.comboBoxFallbackFontFamily->currentFont();

	QImage image(bbox.size(), QImage::Format_ARGB32);
	image.setDotsPerMeterX(pageDpi / 0.0254); // 1 in = 0.0254 m
	image.setDotsPerMeterY(pageDpi / 0.0254);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	QPainterPDFPainter pdfPrinter(&painter, defaultFont);
	if(!pdfSettings.fontFamily.isEmpty()) {
		pdfPrinter.setFontFamily(pdfSettings.fontFamily, false, false);
	}
	if(pdfSettings.fontSize != -1) {
		pdfPrinter.setFontSize(pdfSettings.fontSize);
	}

	if(pdfSettings.overlay) {
		pdfPrinter.drawImage(bbox, m_displayerTool->getSelection(bbox), pdfSettings);
		painter.fillRect(0, 0, bbox.width(), bbox.height(), QColor(255, 255, 255, 127));
	} else {
		image.fill(Qt::white);
	}
	// Units of QPainter on pixel based devices is pixels, so px2pu = 1.
	printChildren(pdfPrinter, page, pdfSettings, 1.);
	m_preview->setPixmap(QPixmap::fromImage(image));
	m_preview->setPos(-0.5 * bbox.width(), -0.5 * bbox.height());
}

void HOCRPdfExporter::backendChanged() {
	int jpegIdx = ui.comboBoxImageCompression->findData(PDFSettings::CompressJpeg);
	bool podofoBackend = ui.comboBoxBackend->itemData(ui.comboBoxBackend->currentIndex()).toInt() == BackendPoDoFo;
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

void HOCRPdfExporter::toggleBackendHint() {
	QString tooltip = tr("<html><head/><body><ul><li>PoDoFo: offers more image compression options, but does not handle complex scripts.</li><li>QPrinter: only supports JPEG compression for storing images, but supports complex scripts.</li></ul></body></html>");
	QRect r = ui.toolButtonBackendHint->rect();
	QToolTip::showText(ui.toolButtonBackendHint->mapToGlobal(QPoint(0, 0.5 * r.height())), tooltip, ui.toolButtonBackendHint, r);
}

void HOCRPdfExporter::imageFormatChanged() {
	QImage::Format format = static_cast<QImage::Format>(ui.comboBoxImageFormat->itemData(ui.comboBoxImageFormat->currentIndex()).toInt());
	QStandardItemModel* model = static_cast<QStandardItemModel*>(ui.comboBoxImageCompression->model());
	int zipIdx = ui.comboBoxImageCompression->findData(PDFSettings::CompressZip);
	int ccittIdx = ui.comboBoxImageCompression->findData(PDFSettings::CompressFax4);
	int jpegIdx = ui.comboBoxImageCompression->findData(PDFSettings::CompressJpeg);
	QStandardItem* ccittItem = model->item(ccittIdx);
	QStandardItem* jpegItem = model->item(jpegIdx);
	if(format == QImage::Format_Mono) {
		if(ui.comboBoxImageCompression->currentIndex() == jpegIdx) {
			ui.comboBoxImageCompression->setCurrentIndex(zipIdx);
		}
		ccittItem->setFlags(ccittItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		jpegItem->setFlags(jpegItem->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		ui.labelDithering->setEnabled(true);
		ui.comboBoxDithering->setEnabled(true);
	} else {
		if(ui.comboBoxImageCompression->currentIndex() == ccittIdx) {
			ui.comboBoxImageCompression->setCurrentIndex(zipIdx);
		}
		ccittItem->setFlags(ccittItem->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		jpegItem->setFlags(jpegItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		ui.labelDithering->setEnabled(false);
		ui.comboBoxDithering->setEnabled(false);
	}
}

void HOCRPdfExporter::imageCompressionChanged() {
	PDFSettings::Compression compression = static_cast<PDFSettings::Compression>(ui.comboBoxImageCompression->itemData(ui.comboBoxImageCompression->currentIndex()).toInt());
	bool jpegCompression = compression == PDFSettings::CompressJpeg;
	ui.spinBoxCompressionQuality->setEnabled(jpegCompression);
	ui.labelCompressionQuality->setEnabled(jpegCompression);
}

bool HOCRPdfExporter::setSource(const QString& sourceFile, int page, int dpi, double angle) {
	if(MAIN->getSourceManager()->addSource(sourceFile, true)) {
		MAIN->getDisplayer()->setup(&page, &dpi, &angle);
		return true;
	} else {
		return false;
	}
}

QImage HOCRPdfExporter::getSelection(const QRect& bbox) {
	return m_displayerTool->getSelection(bbox);
}

void HOCRPdfExporter::paperSizeChanged() {
	QString paperSize = ui.comboBoxPaperSize->itemData(ui.comboBoxPaperSize->currentIndex()).toString();
	if(paperSize == "custom") {
		ui.lineEditPaperWidth->setEnabled(true);
		ui.lineEditPaperHeight->setEnabled(true);
		ui.widgetPaperSize->setVisible(true);
		ui.widgetPaperOrientation->setVisible(false);
	} else if(paperSize == "source") {
		ui.lineEditPaperWidth->setEnabled(true);
		ui.lineEditPaperHeight->setEnabled(true);
		ui.widgetPaperSize->setVisible(false);
		ui.widgetPaperOrientation->setVisible(false);
	} else {
		ui.widgetPaperSize->setVisible(true);
		ui.widgetPaperOrientation->setVisible(true);
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

void HOCRPdfExporter::updateValid() {
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
	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

void HOCRPdfExporter::importMetadataFromSource() {
	QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
	if(!sources.isEmpty()) {
		Source* source = sources.first();
		ui.lineEditAuthor->setText(source->author);
		ui.lineEditCreator->setText(source->creator);
		ui.lineEditKeywords->setText(source->keywords);
		ui.lineEditTitle->setText(source->title);
		ui.lineEditSubject->setText(source->subject);
		ui.lineEditProducer->setText(source->producer);

		PoDoFo::EPdfVersion sourcePdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_7;
		if(source->pdfVersionMajor == 1 && source->pdfVersionMinor >= 0 && source->pdfVersionMinor <= 7) {
			sourcePdfVersion = static_cast<PoDoFo::EPdfVersion>(PoDoFo::EPdfVersion::ePdfVersion_1_0 + source->pdfVersionMinor);
		}
		ui.comboBoxPdfVersion->setCurrentIndex(ui.comboBoxPdfVersion->findData(sourcePdfVersion));
	}
}

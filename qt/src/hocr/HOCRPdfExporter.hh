/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExporter.hh
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

#ifndef HOCRPDFEXPORTER_HH
#define HOCRPDFEXPORTER_HH

#include <QImage>
#include <QVector>

#include "common.hh"
#include "ui_PdfExportDialog.h"

class QFontDialog;
class QGraphicsPixmapItem;

class DisplayerToolHOCR;
class HOCRDocument;
class HOCRPage;
class HOCRItem;


class HOCRPdfExporter: public QDialog {
	Q_OBJECT
public:
	HOCRPdfExporter(const HOCRDocument* hocrdocument, const HOCRPage* previewPage, DisplayerToolHOCR* displayerTool, QWidget* parent = 0);
	bool run(QString& filebasename);

private:
	enum PDFBackend { BackendPoDoFo, BackendQPrinter};

	struct PDFSettings {
		QImage::Format colorFormat;
		Qt::ImageConversionFlags conversionFlags;
		enum Compression { CompressZip, CompressFax4, CompressJpeg } compression;
		int compressionQuality;
		QString fontFamily;
		int fontSize;
		bool uniformizeLineSpacing;
		int preserveSpaceWidth;
		bool overlay;
		double detectedFontScaling;
		bool sanitizeHyphens;
	};

	class PDFPainter {
	public:
		virtual ~PDFPainter() {}
		virtual void setFontFamily(const QString& family, bool bold, bool italic) = 0;
		virtual void setFontSize(double pointSize) = 0;
		virtual void drawText(double x, double y, const QString& text) = 0;
		virtual void drawImage(const QRect& bbox, const QImage& image, const PDFSettings& settings) = 0;
		virtual double getAverageCharWidth() const = 0;
		virtual double getTextWidth(const QString& text) const = 0;
		virtual bool createPage(double /*width*/, double /*height*/, double /*offsetX*/, double /*offsetY*/, QString& /*errMsg*/) { return true; }
		virtual void finishPage() {}
		virtual bool finishDocument(QString& /*errMsg*/) { return true; }
	protected:
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
		QVector<QRgb> createGray8Table() const {
			QVector<QRgb> colorTable(255);
			for(int i = 0; i < 255; ++i) {
				colorTable[i] = qRgb(i, i, i);
			}
			return colorTable;
		}
#endif
		QImage convertedImage(const QImage& image, QImage::Format targetFormat, Qt::ImageConversionFlags flags) const {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
			if(image.format() == targetFormat) {
				return image;
			} else if(targetFormat == QImage::Format_Indexed8) {
				static QVector<QRgb> gray8Table = createGray8Table();
				return image.convertToFormat(targetFormat, gray8Table);
			} else {
				return image.convertToFormat(targetFormat);
			}
#else
			return image.format() == targetFormat ? image : image.convertToFormat(targetFormat, flags);
#endif
		}
	};
	class PoDoFoPDFPainter;
	class QPainterPDFPainter;
	class QPrinterPDFPainter;

	Ui::PdfExportDialog ui;
	QGraphicsPixmapItem* m_preview = nullptr;
	const HOCRDocument* m_hocrdocument;
	const HOCRPage* m_previewPage;
	DisplayerToolHOCR* m_displayerTool;

	PDFSettings getPdfSettings() const;
	PDFPainter* createPoDoFoPrinter(const QString& filename, const QFont& defaultFont, QString& errMsg);
	void printChildren(PDFPainter& painter, const HOCRItem* item, const PDFSettings& pdfSettings, double px2pu, double imgScale = 1., double fontScale = 1.);

private slots:
	void backendChanged();
	void toggleBackendHint();
	void importMetadataFromSource();
	void imageFormatChanged();
	void imageCompressionChanged();
	void paperSizeChanged();
	void updatePreview();
	bool setSource(const QString& sourceFile, int page, int dpi, double angle);
	QImage getSelection(const QRect& bbox);
	void updateValid();
};

#endif // HOCRPDFEXPORTER_HH

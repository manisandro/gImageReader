/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExporter.hh
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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

#include "HOCRExporter.hh"

#include <QDialog>
#include <QFontDatabase>
#include <QImage>
#include <QMap>
#include <QPrinter>
#include <QString>

class DisplayerToolHOCR;
class HOCRItem;
class HOCRPage;
class HOCRPdfExportWidget;
namespace PoDoFo {
class PdfEncoding;
class PdfFont;
class PdfPainter;
class PdfDocument;
}

class HOCRPdfExporter : public HOCRExporter {
	Q_OBJECT

public:
	struct PDFSettings : ExporterSettings {
		QImage::Format colorFormat;
		Qt::ImageConversionFlags conversionFlags;
		enum Compression { CompressZip, CompressFax4, CompressJpeg } compression;
		int compressionQuality;
		QString fontFamily;
		int fontSize;
		QString fallbackFontFamily;
		int fallbackFontSize;
		bool uniformizeLineSpacing;
		int preserveSpaceWidth;
		bool overlay;
		double detectedFontScaling;
		bool sanitizeHyphens;
		int assumedImageDpi;
		int outputDpi;
		QString paperSize;
		bool paperSizeLandscape;
		double paperWidthIn;
		double paperHeightIn;
		enum PDFBackend { BackendPoDoFo, BackendQPrinter} backend;

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
		QString password;
		QString producer;
		QString creator;
		QString title;
		QString subject;
		QString keywords;
		QString author;
	};

	bool run(const HOCRDocument* hocrdocument, const QString& outname, const ExporterSettings* settings = nullptr) override;

private slots:
	bool setSource(const QString& sourceFile, int page, int dpi, double angle);

};

class HOCRPdfPrinter : public QObject {
	Q_OBJECT
public:
	virtual void setFontFamily(const QString& family, bool bold, bool italic) = 0;
	virtual void setFontSize(double pointSize) = 0;
	virtual void drawText(double x, double y, const QString& text) = 0;
	virtual void drawImage(const QRect& bbox, const QImage& image, const HOCRPdfExporter::PDFSettings& settings) = 0;
	virtual double getAverageCharWidth() const = 0;
	virtual double getTextWidth(const QString& text) const = 0;
	virtual bool createPage(double /*width*/, double /*height*/, double /*offsetX*/, double /*offsetY*/, QString& /*errMsg*/) { return true; }
	virtual void finishPage() {}
	virtual bool finishDocument(QString& /*errMsg*/) { return true; }

	void printChildren(const HOCRItem* item, const HOCRPdfExporter::PDFSettings& pdfSettings, double px2pu/*pixels to printer units*/, double imgScale = 1., double fontScale = 1.);

protected:
	QImage convertedImage(const QImage& image, QImage::Format targetFormat, Qt::ImageConversionFlags flags) const {
		return image.format() == targetFormat ? image : image.convertToFormat(targetFormat, flags);
	}

private slots:
	QImage getSelection(const QRect& bbox);
};

class HOCRQPainterPdfPrinter : public HOCRPdfPrinter {
public:
	HOCRQPainterPdfPrinter(QPainter* painter, const QFont& defaultFont);
	void setFontFamily(const QString& family, bool bold, bool italic) override;
	void setFontSize(double pointSize) override;
	void drawText(double x, double y, const QString& text) override;
	void drawImage(const QRect& bbox, const QImage& image, const HOCRPdfExporter::PDFSettings& settings) override;
	double getAverageCharWidth() const override;
	double getTextWidth(const QString& text) const override;

protected:
	QFontDatabase m_fontDatabase;
	QPainter* m_painter;
	QFont m_curFont;
	QFont m_defaultFont;
	double m_offsetX = 0.0;
	double m_offsetY = 0.0;
};


class HOCRQPrinterPdfPrinter : public HOCRQPainterPdfPrinter {
public:
	HOCRQPrinterPdfPrinter(const QString& filename, const QString& creator, const QFont& defaultFont);
	~HOCRQPrinterPdfPrinter();
	bool createPage(double width, double height, double offsetX, double offsetY, QString& errMsg) override;
	bool finishDocument(QString& /*errMsg*/) override;
	void drawImage(const QRect& bbox, const QImage& image, const HOCRPdfExporter::PDFSettings& settings) override;

private:
	QPrinter m_printer;
	bool m_firstPage = true;
};

class HOCRPoDoFoPdfPrinter : public HOCRPdfPrinter {
public:
	static HOCRPoDoFoPdfPrinter* create(const QString& filename, const HOCRPdfExporter::PDFSettings& settings, const QFont& defaultFont, QString& errMsg);

	HOCRPoDoFoPdfPrinter(PoDoFo::PdfDocument* document, const std::string& filename, const PoDoFo::PdfEncoding* fontEncoding, PoDoFo::PdfFont* defaultFont, const QString& defaultFontFamily, double defaultFontSize);
	~HOCRPoDoFoPdfPrinter();
	bool createPage(double width, double height, double offsetX, double offsetY, QString& /*errMsg*/) override;
	void finishPage() override;
	bool finishDocument(QString& errMsg) override;
	void setFontFamily(const QString& family, bool bold, bool italic) override;
	void setFontSize(double pointSize) override;
	void drawText(double x, double y, const QString& text) override;
	void drawImage(const QRect& bbox, const QImage& image, const HOCRPdfExporter::PDFSettings& settings) override;
	double getAverageCharWidth() const override;
	double getTextWidth(const QString& text) const override;

private:
	QFontDatabase m_fontDatabase;
	QMap<QString, PoDoFo::PdfFont*> m_fontCache;
	PoDoFo::PdfPainter* m_painter = nullptr;
	PoDoFo::PdfDocument* m_document = nullptr;
	std::string m_filename;
	const PoDoFo::PdfEncoding* m_pdfFontEncoding;
	PoDoFo::PdfFont* m_defaultFont;
	QString m_defaultFontFamily;
	double m_defaultFontSize = -1.0;
	double m_pageHeight = 0.0;
	double m_offsetX = 0.0;
	double m_offsetY = 0.0;

	PoDoFo::PdfFont* getFont(QString family, bool bold, bool italic);
};

class HOCRPdfExportDialog : public QDialog {
	Q_OBJECT
public:
	HOCRPdfExportDialog(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument, const HOCRPage* hocrpage, QWidget* parent = nullptr);

	HOCRPdfExporter::PDFSettings getPdfSettings() const;

private:
	HOCRPdfExportWidget* m_widget = nullptr;
};

#endif // HOCRPDFEXPORTER_HH

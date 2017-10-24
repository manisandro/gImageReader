/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.hh
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

#ifndef OUTPUTEDITORHOCR_HH
#define OUTPUTEDITORHOCR_HH

#include "OutputEditor.hh"
#include "Ui_OutputEditorHOCR.hh"
#include "ui_PdfExportDialog.h"

#include <QDomDocument>
#include <QDomElement>
#include <QtSpell.hpp>

#include <podofo/base/PdfDefines.h>

class DisplayerToolHOCR;
class QGraphicsPixmapItem;

class OutputEditorHOCR : public OutputEditor {
	Q_OBJECT
public:
	OutputEditorHOCR(DisplayerToolHOCR* tool);
	~OutputEditorHOCR();

	QWidget* getUI() override {
		return m_widget;
	}
	ReadSessionData* initRead(tesseract::TessBaseAPI &tess) override;
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const QString& errorMsg, ReadSessionData* data) override;
	void finalizeRead(ReadSessionData *data) override;
	bool getModified() const override;

public slots:
	bool clear(bool hide = true) override;
	void open();
	bool save(const QString& filename = "") override;
	void savePDF();

private:
	class HTMLHighlighter;

	struct HOCRReadSessionData : ReadSessionData {
		QStringList errors;
	};

	static const int IdRole = Qt::UserRole + 1;
	static const int SourceRole = Qt::UserRole + 2;
	static const int BBoxRole = Qt::UserRole + 3;
	static const int ClassRole = Qt::UserRole + 4;
	static const int FontSizeRole = Qt::UserRole + 5;
	static const int BaselineRole = Qt::UserRole + 6;
	static const int ParentAttrRole = Qt::UserRole + 1;

	static const QRegExp s_bboxRx;
	static const QRegExp s_pageTitleRx;
	static const QRegExp s_idRx;
	static const QRegExp s_fontSizeRx;
	static const QRegExp s_baseLineRx;

	struct PDFSettings {
		QImage::Format colorFormat;
		Qt::ImageConversionFlags conversionFlags;
		enum Compression { CompressZip, CompressFax4, CompressJpeg } compression;
		int compressionQuality;
		bool useDetectedFontSizes;
		bool uniformizeLineSpacing;
		int preserveSpaceWidth;
		bool overlay;
		double detectedFontScaling;
	};

	class PDFPainter {
	public:
		virtual void setFontSize(double pointSize) = 0;
		virtual void drawText(double x, double y, const QString& text) = 0;
		virtual void drawImage(const QRect& bbox, const QImage& image, const PDFSettings& settings) = 0;
		virtual double getAverageCharWidth() const = 0;
		virtual double getTextWidth(const QString& text) const = 0;
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

	int m_idCounter = 0;
	DisplayerToolHOCR* m_tool;
	QWidget* m_widget;
	UI_OutputEditorHOCR ui;
	HTMLHighlighter* m_highlighter;
	QtSpell::TextEditChecker m_spell;
	bool m_modified = false;
	QDialog* m_pdfExportDialog;
	Ui::PdfExportDialog m_pdfExportDialogUi;
	PoDoFo::EPdfVersion pdfVersion;
	QFontDialog m_pdfFontDialog;

	QTreeWidgetItem* m_rootItem = nullptr;
	QTreeWidgetItem* m_currentItem = nullptr;
	QTreeWidgetItem* m_currentPageItem = nullptr;
	QDomDocument m_currentDocument;
	QDomElement m_currentElement;

	QGraphicsPixmapItem* m_preview = nullptr;

	void findReplace(bool backwards, bool replace);
	void addPage(QDomElement pageDiv, const QString& filename, int page, bool cleanGraphics);
	bool addChildItems(QDomElement element, QTreeWidgetItem* parentItem, QMap<QString, QString>& langCache);
	QDomElement elementById(QDomElement element, const QString& id) const;
	void expandChildren(QTreeWidgetItem* item) const;
	void collapseChildren(QTreeWidgetItem* item) const;
	void printChildren(PDFPainter& painter, QTreeWidgetItem* item, const PDFSettings& pdfSettings, double imgScale = 1.) const;
	bool setCurrentSource(const QDomElement& pageElement, int* pageDpi = 0, int* overrideDpi = 0) const;
	void updateCurrentItemText();
	void updateCurrentItemAttribute(const QString& key, const QString& subkey, const QString& newvalue, bool update=true);
	void updateCurrentItem();
	void removeCurrentItem();
	QString trimWord(const QString& word, QString* prefix = nullptr, QString* suffix = nullptr);
	void mergeItems(const QList<QTreeWidgetItem*>& items);

private slots:
	void addGraphicRegion(QRect rect);
	void addPage(const QString& hocrText, ReadSessionData data);
	void setFont();
	void showItemProperties(QTreeWidgetItem* item);
	void itemChanged(QTreeWidgetItem* item, int col);
	void imageFormatChanged();
	void pdfVersionChanged();
	void imageCompressionChanged();
	void propertyCellChanged(int row, int col);
	void showTreeWidgetContextMenu(const QPoint& point);
	void updateFontButton(const QFont& font);
	void updatePreview();
	void updateCurrentItemBBox(QRect rect);
};

#endif // OUTPUTEDITORHOCR_HH

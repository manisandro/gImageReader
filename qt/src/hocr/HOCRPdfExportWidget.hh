/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExportWidget.hh
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

#ifndef HOCRPDFEXPORTWIDGET_HH
#define HOCRPDFEXPORTWIDGET_HH

#include <QImage>
#include <QVector>

#include "HOCRPdfExporter.hh"
#include "ui_PdfExportWidget.h"

class QGraphicsPixmapItem;

class DisplayerToolHOCR;
class HOCRDocument;
class HOCRPage;


class HOCRPdfExportWidget : public QWidget {
	Q_OBJECT
public:
	HOCRPdfExportWidget(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument = nullptr, const HOCRPage* hocrpage = nullptr, QWidget* parent = nullptr);
	~HOCRPdfExportWidget();
	void setPreviewPage(const HOCRDocument* hocrdocument, const HOCRPage* hocrpage);
	HOCRPdfExporter::PDFSettings getPdfSettings() const;

signals:
	void validChanged(bool valid);

private:
	Ui::PdfExportWidget ui;
	QGraphicsPixmapItem* m_preview = nullptr;
	DisplayerToolHOCR* m_displayerTool;
	const HOCRDocument* m_document = nullptr;
	const HOCRPage* m_previewPage;


private slots:
	void backendChanged();
	void toggleBackendHint();
	void importMetadataFromSource();
	void imageFormatChanged();
	void imageCompressionChanged();
	void paperSizeChanged();
	void updatePreview();
	void updateValid();
};

#endif // HOCRPDFEXPORTWIDGET_HH

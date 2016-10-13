/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QDomDocument>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QMessageBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QPainter>
#include <QSyntaxHighlighter>
#include <QTextStream>
#include <cstring>
#include <podofo/base/PdfDictionary.h>
#include <podofo/base/PdfFilter.h>
#include <podofo/base/PdfStream.h>
#include <podofo/doc/PdfFont.h>
#include <podofo/doc/PdfIdentityEncoding.h>
#include <podofo/doc/PdfImage.h>
#include <podofo/doc/PdfPage.h>
#include <podofo/doc/PdfPainter.h>
#include <podofo/doc/PdfStreamedDocument.h>
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>

#include "DisplayerToolHOCR.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include "ui_PdfExportDialog.h"


const QRegExp OutputEditorHOCR::s_bboxRx = QRegExp("bbox\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)");
const QRegExp OutputEditorHOCR::s_pageTitleRx = QRegExp("image\\s+'(.+)';\\s+bbox\\s+\\d+\\s+\\d+\\s+\\d+\\s+\\d+;\\s+pageno\\s+(\\d+);\\s+rot\\s+(\\d+\\.?\\d*);\\s+res\\s+(\\d+\\.?\\d*)");
const QRegExp OutputEditorHOCR::s_idRx = QRegExp("(\\w+)_\\d+_(\\d+)");
const QRegExp OutputEditorHOCR::s_fontSizeRx = QRegExp("x_fsize\\s+(\\d+)");
const QRegExp OutputEditorHOCR::s_baseLineRx = QRegExp("baseline\\s+(-?\\d+\\.?\\d*)\\s+(-?\\d+)");


class OutputEditorHOCR::HTMLHighlighter : public QSyntaxHighlighter
{
public:
	HTMLHighlighter(QTextDocument *document) : QSyntaxHighlighter(document)
	{
		mFormatMap[NormalState].setForeground(QColor(Qt::black));
		mFormatMap[InTag].setForeground(QColor(75, 75, 255));
		mFormatMap[InAttrKey].setForeground(QColor(75, 200, 75));
		mFormatMap[InAttrValue].setForeground(QColor(255, 75, 75));
		mFormatMap[InAttrValueDblQuote].setForeground(QColor(255, 75, 75));

		mStateMap[NormalState].append({QRegExp("<"), InTag, false});
		mStateMap[InTag].append({QRegExp(">"), NormalState, true});
		mStateMap[InTag].append({QRegExp("\\w+="), InAttrKey, false});
		mStateMap[InAttrKey].append({QRegExp("'"), InAttrValue, false});
		mStateMap[InAttrKey].append({QRegExp("\""), InAttrValueDblQuote, false});
		mStateMap[InAttrKey].append({QRegExp("\\s"), NormalState, false});
		mStateMap[InAttrValue].append({QRegExp("'[^']*'"), InTag, true});
		mStateMap[InAttrValueDblQuote].append({QRegExp("\"[^\"]*\""), InTag, true});
	}

private:
	enum State { NormalState = -1, InComment, InTag, InAttrKey, InAttrValue, InAttrValueDblQuote };
	struct Rule {
		QRegExp pattern;
		State nextState;
		bool addMatched; // add matched length to pos
	};

	QMap<State,QTextCharFormat> mFormatMap;
	QMap<State,QList<Rule>> mStateMap;

	void highlightBlock(const QString &text)
	{
		int pos = 0;
		int len = text.length();
		State state = static_cast<State>(previousBlockState());
		while(pos < len) {
			State minState = state;
			int minPos = -1;
			for(const Rule& rule : mStateMap.value(state)) {
				int matchPos = rule.pattern.indexIn(text, pos);
				if(matchPos != -1 && (minPos < 0 || matchPos < minPos)) {
					minPos = matchPos + (rule.addMatched ? rule.pattern.matchedLength() : 0);
					minState = rule.nextState;
				}
			}
			if(minPos == -1) {
				setFormat(pos, len - pos, mFormatMap[state]);
				pos = len;
			} else {
				setFormat(pos, minPos - pos, mFormatMap[state]);
				pos = minPos;
				state = minState;
			}
		}

		setCurrentBlockState(state);
	}
};


class OutputEditorHOCR::QPainterPDFPainter : public OutputEditorHOCR::PDFPainter
{
public:
	QPainterPDFPainter(QPainter* painter) : m_painter(painter) {
		m_curFontSize = m_painter->font().pointSize();
	}
	void setFontSize(double pointSize) override {
		if(pointSize != m_curFontSize) {
			QFont font = m_painter->font();
			font.setPointSize(pointSize);
			m_painter->setFont(font);
			m_curFontSize = pointSize;
		}
	}
	void drawText(double x, double y, const QString& text) override {
		m_painter->drawText(x, y, text);
	}
	void drawImage(const QRect& bbox, const QImage& image, const PDFSettings& settings) override {
		m_painter->drawImage(bbox, convertedImage(image, settings.colorFormat));
	}
	double getAverageCharWidth() const override {
		return m_painter->fontMetrics().averageCharWidth();
	}
	double getTextWidth(const QString& text) const override {
		return m_painter->fontMetrics().width(text);
	}

private:
	QPainter* m_painter;
	int m_curFontSize;
};

#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0,9,3)
namespace PoDoFo {
class PdfImageCompat : public PoDoFo::PdfImage {
	using PdfImage::PdfImage;
public:
	void SetImageDataRaw( unsigned int nWidth, unsigned int nHeight,
							  unsigned int nBitsPerComponent, PdfInputStream* pStream )
	{
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

class OutputEditorHOCR::PoDoFoPDFPainter : public OutputEditorHOCR::PDFPainter {
public:
	PoDoFoPDFPainter(PoDoFo::PdfDocument* document, PoDoFo::PdfPainter* painter, double scaleFactor, double imageScale)
		: m_document(document), m_painter(painter), m_scaleFactor(scaleFactor), m_imageScale(imageScale)
	{
		m_pageHeight = m_painter->GetPage()->GetPageSize().GetHeight();
	}
	void setFontSize(double pointSize) override {
		m_painter->GetFont()->SetFontSize(pointSize);
	}
	void drawText(double x, double y, const QString& text) override {
		PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*>(text.toUtf8().data()));
		m_painter->DrawText(x * m_scaleFactor, m_pageHeight - y * m_scaleFactor, pdfString);
	}
	void drawImage(const QRect& bbox, const QImage& image, const PDFSettings& settings) override {
		QImage scaledImage = image.scaled(image.width() * m_imageScale, image.height() * m_imageScale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		QImage img = convertedImage(scaledImage, settings.colorFormat);
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
		} else {
			PoDoFo::PdfName dctFilterName(PoDoFo::PdfFilterFactory::FilterTypeToName(PoDoFo::ePdfFilter_DCTDecode));
			pdfImage.GetObject()->GetDictionary().AddKey(PoDoFo::PdfName::KeyFilter, dctFilterName);
			QByteArray data;
			QBuffer buffer(&data);
			img.save(&buffer, "jpg", settings.compressionQuality);
			PoDoFo::PdfMemoryInputStream is(data.data(), data.size());
			pdfImage.SetImageDataRaw(width, height, sampleSize, &is);
		}
		m_painter->DrawImage(bbox.x() * m_scaleFactor, m_pageHeight - (bbox.y() + bbox.height()) * m_scaleFactor, &pdfImage, m_scaleFactor / m_imageScale, m_scaleFactor / m_imageScale);
	}
	double getAverageCharWidth() const override {
		return m_painter->GetFont()->GetFontMetrics()->CharWidth(static_cast<unsigned char>('x')) / m_scaleFactor;
	}
	double getTextWidth(const QString& text) const override {
		PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*>(text.toUtf8().data()));
		return m_painter->GetFont()->GetFontMetrics()->StringWidth(pdfString) / m_scaleFactor;
	}

private:
	PoDoFo::PdfDocument* m_document;
	PoDoFo::PdfPainter* m_painter;
	double m_scaleFactor;
	double m_imageScale;
	double m_pageHeight;
};

Q_DECLARE_METATYPE(QList<QRect>)

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool)
{
	static int reg = qRegisterMetaType<QList<QRect>>("QList<QRect>");
	Q_UNUSED(reg);

	m_tool = tool;
	m_widget = new QWidget;
	ui.setupUi(m_widget);
	m_highlighter = new HTMLHighlighter(ui.plainTextEditOutput->document());

	m_pdfExportDialog = new QDialog(m_widget);
	m_pdfExportDialogUi.setupUi(m_pdfExportDialog);
	m_pdfExportDialogUi.comboBoxImageFormat->addItem(_("Color"), QImage::Format_RGB888);
	m_pdfExportDialogUi.comboBoxImageFormat->addItem(_("Grayscale"), QImage::Format_Grayscale8);
	m_pdfExportDialogUi.comboBoxImageFormat->addItem(_("Monochrome"), QImage::Format_Mono);
	m_pdfExportDialogUi.comboBoxImageFormat->setCurrentIndex(-1);
	m_pdfExportDialogUi.comboBoxImageCompression->addItem(_("Zip (lossless)"), PDFSettings::CompressZip);
	m_pdfExportDialogUi.comboBoxImageCompression->addItem(_("Jpeg (lossy)"), PDFSettings::CompressJpeg);
	m_pdfExportDialogUi.comboBoxImageCompression->setCurrentIndex(-1);

	ui.actionOutputSaveHOCR->setShortcut(Qt::CTRL + Qt::Key_S);

	ui.treeWidgetItems->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(ui.actionOutputOpen, SIGNAL(triggered()), this, SLOT(open()));
	connect(ui.actionOutputSaveHOCR, SIGNAL(triggered()), this, SLOT(save()));
	connect(ui.actionOutputExportPDF, SIGNAL(triggered()), this, SLOT(savePDF()));
	connect(ui.actionOutputClear, SIGNAL(triggered()), this, SLOT(clear()));
	connect(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(ui.treeWidgetItems, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(showItemProperties(QTreeWidgetItem*)));
	connect(ui.treeWidgetItems, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(itemChanged(QTreeWidgetItem*,int)));
	connect(ui.treeWidgetItems, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showTreeWidgetContextMenu(QPoint)));
	connect(ui.tableWidgetProperties, SIGNAL(cellChanged(int,int)), this, SLOT(propertyCellChanged(int,int)));
	connect(m_pdfExportDialogUi.buttonFont, SIGNAL(clicked()), &m_pdfFontDialog, SLOT(exec()));
	connect(&m_pdfFontDialog, SIGNAL(fontSelected(QFont)), this, SLOT(updateFontButton(QFont)));
	connect(m_pdfExportDialogUi.comboBoxOutputMode, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.comboBoxImageFormat, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.comboBoxImageFormat, SIGNAL(currentIndexChanged(int)), this, SLOT(imageFormatChanged()));
	connect(m_pdfExportDialogUi.comboBoxImageCompression, SIGNAL(currentIndexChanged(int)), this, SLOT(imageCompressionChanged()));
	connect(m_pdfExportDialogUi.checkBoxFontSize, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.checkBoxFontSize, SIGNAL(toggled(bool)), m_pdfExportDialogUi.labelFontScaling, SLOT(setEnabled(bool)));
	connect(m_pdfExportDialogUi.checkBoxFontSize, SIGNAL(toggled(bool)), m_pdfExportDialogUi.spinFontScaling, SLOT(setEnabled(bool)));
	connect(m_pdfExportDialogUi.spinFontScaling, SIGNAL(valueChanged(int)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.checkBoxUniformizeSpacing, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.spinBoxPreserve, SIGNAL(valueChanged(int)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.checkBoxUniformizeSpacing, SIGNAL(toggled(bool)), m_pdfExportDialogUi.labelPreserve, SLOT(setEnabled(bool)));
	connect(m_pdfExportDialogUi.checkBoxUniformizeSpacing, SIGNAL(toggled(bool)), m_pdfExportDialogUi.spinBoxPreserve, SLOT(setEnabled(bool)));
	connect(m_pdfExportDialogUi.checkBoxPreview, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(m_tool, SIGNAL(selectionGeometryChanged(QRect)), this, SLOT(updateCurrentItemBBox(QRect)));
	connect(m_tool, SIGNAL(selectionDrawn(QRect)), this, SLOT(addGraphicRegion(QRect)));

	MAIN->getConfig()->addSetting(new ComboSetting("pdfexportmode", m_pdfExportDialogUi.comboBoxOutputMode));
	MAIN->getConfig()->addSetting(new FontSetting("pdffont", &m_pdfFontDialog, QFont().toString()));
	MAIN->getConfig()->addSetting(new SpinSetting("pdfimagecompressionquality", m_pdfExportDialogUi.spinBoxCompressionQuality, 90));
	MAIN->getConfig()->addSetting(new ComboSetting("pdfimagecompression", m_pdfExportDialogUi.comboBoxImageCompression));
	MAIN->getConfig()->addSetting(new ComboSetting("pdfimageformat", m_pdfExportDialogUi.comboBoxImageFormat));
	MAIN->getConfig()->addSetting(new SpinSetting("pdfimagedpi", m_pdfExportDialogUi.spinBoxDpi, 300));
	MAIN->getConfig()->addSetting(new SwitchSetting("pdfusedetectedfontsizes", m_pdfExportDialogUi.checkBoxFontSize, true));
	MAIN->getConfig()->addSetting(new SpinSetting("pdffontscale", m_pdfExportDialogUi.spinFontScaling, 100));
	MAIN->getConfig()->addSetting(new SwitchSetting("pdfuniformizelinespacing", m_pdfExportDialogUi.checkBoxUniformizeSpacing, false));
	MAIN->getConfig()->addSetting(new SpinSetting("pdfpreservespaces", m_pdfExportDialogUi.spinBoxPreserve, 4));
	MAIN->getConfig()->addSetting(new SwitchSetting("pdfpreview", m_pdfExportDialogUi.checkBoxPreview, false));

	setFont();
	updateFontButton(m_pdfFontDialog.currentFont());
}

OutputEditorHOCR::~OutputEditorHOCR()
{
	delete m_widget;
}

void OutputEditorHOCR::setFont()
{
	if(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont")->getValue()){
		ui.plainTextEditOutput->setFont(QFont());
	}else{
		ui.plainTextEditOutput->setFont(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont")->getValue());
	}
}

void OutputEditorHOCR::imageFormatChanged()
{
	QImage::Format format = static_cast<QImage::Format>(m_pdfExportDialogUi.comboBoxImageFormat->itemData(m_pdfExportDialogUi.comboBoxImageFormat->currentIndex()).toInt());
	if(format == QImage::Format_Mono) {
		m_pdfExportDialogUi.comboBoxImageCompression->setCurrentIndex(m_pdfExportDialogUi.comboBoxImageCompression->findData(PDFSettings::CompressZip));
		m_pdfExportDialogUi.comboBoxImageCompression->setEnabled(false);
		m_pdfExportDialogUi.labelImageCompression->setEnabled(false);
	} else {
		m_pdfExportDialogUi.comboBoxImageCompression->setEnabled(true);
		m_pdfExportDialogUi.labelImageCompression->setEnabled(true);
	}
}

void OutputEditorHOCR::imageCompressionChanged()
{
	PDFSettings::Compression compression = static_cast<PDFSettings::Compression>(m_pdfExportDialogUi.comboBoxImageCompression->itemData(m_pdfExportDialogUi.comboBoxImageCompression->currentIndex()).toInt());
	bool zipCompression = compression == PDFSettings::CompressZip;
	m_pdfExportDialogUi.spinBoxCompressionQuality->setEnabled(!zipCompression);
	m_pdfExportDialogUi.labelCompressionQuality->setEnabled(!zipCompression);
}

OutputEditorHOCR::ReadSessionData* OutputEditorHOCR::initRead(tesseract::TessBaseAPI &tess)
{
	tess.SetPageSegMode(tesseract::PSM_AUTO_ONLY);
	return new HOCRReadSessionData;
}

void OutputEditorHOCR::read(tesseract::TessBaseAPI &tess, ReadSessionData *data)
{
	tess.SetVariable("hocr_font_info", "true");
	char* text = tess.GetHOCRText(data->page);
	QMetaObject::invokeMethod(this, "addPage", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(text)), Q_ARG(ReadSessionData, *data));
	delete[] text;
}

void OutputEditorHOCR::readError(const QString& errorMsg, ReadSessionData *data)
{
	static_cast<HOCRReadSessionData*>(data)->errors.append(QString("%1[%2]: %3").arg(data->file).arg(data->page).arg(errorMsg));
}

void OutputEditorHOCR::finalizeRead(ReadSessionData *data)
{
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	if(!hdata->errors.isEmpty()) {
		QString message = QString(_("The following pages could not be processed:\n%1").arg(hdata->errors.join("\n")));
		QMessageBox::warning(MAIN, _("Recognition errors"), message);
	}
	OutputEditor::finalizeRead(data);
}

void OutputEditorHOCR::addPage(const QString& hocrText, ReadSessionData data)
{
	QDomDocument doc;
	doc.setContent(hocrText);
	QDomElement pageDiv = doc.firstChildElement("div");
	s_bboxRx.indexIn(pageDiv.attribute("title"));
	int x1 = s_bboxRx.cap(1).toInt();
	int y1 = s_bboxRx.cap(2).toInt();
	int x2 = s_bboxRx.cap(3).toInt();
	int y2 = s_bboxRx.cap(4).toInt();
	QString pageTitle = QString("image '%1'; bbox %2 %3 %4 %5; pageno %6; rot %7; res %8")
			.arg(data.file)
			.arg(x1).arg(y1).arg(x2).arg(y2)
			.arg(data.page)
			.arg(data.angle)
			.arg(data.resolution);
	pageDiv.setAttribute("title", pageTitle);
	addPage(pageDiv, QFileInfo(data.file).fileName(), data.page);
}

void OutputEditorHOCR::addPage(QDomElement pageDiv, const QString& filename, int page)
{
	pageDiv.setAttribute("id", QString("page_%1").arg(++m_idCounter));
	s_bboxRx.indexIn(pageDiv.attribute("title"));
	int x1 = s_bboxRx.cap(1).toInt();
	int y1 = s_bboxRx.cap(2).toInt();
	int x2 = s_bboxRx.cap(3).toInt();
	int y2 = s_bboxRx.cap(4).toInt();

	QTreeWidgetItem* pageItem = new QTreeWidgetItem(QStringList() << QString("%1 [%2]").arg(filename).arg(page));
	pageItem->setData(0, IdRole, pageDiv.attribute("id"));
	pageItem->setData(0, BBoxRole, QRect(x1, y1, x2 - x1, y2 - y1));
	pageItem->setData(0, ClassRole, "ocr_page");
	pageItem->setIcon(0, QIcon(":/icons/item_page"));
	pageItem->setCheckState(0, Qt::Checked);
	ui.treeWidgetItems->addTopLevelItem(pageItem);
	QMap<QString,QString> langCache;

	QDomElement element = pageDiv.firstChildElement("div");
	while(!element.isNull()) {
		// Boxes without text are images
		if(!addChildItems(element.firstChildElement(), pageItem, langCache) && s_bboxRx.indexIn(element.attribute("title")) != -1) {
			QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << _("Graphic"));
			item->setCheckState(0, Qt::Checked);
			item->setIcon(0, QIcon(":/icons/item_halftone"));
			item->setData(0, IdRole, element.attribute("id"));
			item->setData(0, ClassRole, "ocr_graphic");
			x1 = s_bboxRx.cap(1).toInt();
			y1 = s_bboxRx.cap(2).toInt();
			x2 = s_bboxRx.cap(3).toInt();
			y2 = s_bboxRx.cap(4).toInt();
			item->setData(0, BBoxRole, QRect(x1, y1, x2 - x1, y2 - y1));
			pageItem->addChild(item);
		}
		element = element.nextSiblingElement();
	}
	QString str;
	QTextStream ss(&str);
	pageDiv.save(ss, 1);
	pageItem->setData(0, SourceRole, str);
	expandChildren(pageItem);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
	ui.actionOutputSaveHOCR->setEnabled(true);
	ui.actionOutputExportPDF->setEnabled(true);
}

void OutputEditorHOCR::expandChildren(QTreeWidgetItem* item) const
{
	if(item->childCount() > 0) {
		item->setExpanded(true);
		for(int i = 0, n = item->childCount(); i < n; ++i) {
			expandChildren(item->child(i));
		}
	}
}

bool OutputEditorHOCR::addChildItems(QDomElement element, QTreeWidgetItem* parentItem, QMap<QString,QString>& langCache)
{
	bool haveWord = false;
	while(!element.isNull()) {
		QDomElement nextElement = element.nextSiblingElement();
		if(s_idRx.indexIn(element.attribute("id")) != -1) {
			QString newId = QString("%1_%2_%3").arg(s_idRx.cap(1)).arg(m_idCounter).arg(s_idRx.cap(2));
			element.setAttribute("id", newId);
		}

		if(s_bboxRx.indexIn(element.attribute("title")) != -1) {
			QString type = element.attribute("class");
			QString icon;
			QString title;
			int x1 = s_bboxRx.cap(1).toInt();
			int y1 = s_bboxRx.cap(2).toInt();
			int x2 = s_bboxRx.cap(3).toInt();
			int y2 = s_bboxRx.cap(4).toInt();
			if(type == "ocr_par") {
				title = _("Paragraph");
				icon = "par";
			} else if(type == "ocr_line") {
				title = _("Textline");
				icon = "line";
			} else if(type == "ocrx_word") {
				title = element.text();
				icon = "word";
			}
			if(!title.isEmpty()) {
				QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << title);
				if(type == "ocrx_word" || addChildItems(element.firstChildElement(), item, langCache)) {
					item->setCheckState(0, Qt::Checked);
					item->setData(0, IdRole, element.attribute("id"));
					item->setIcon(0, QIcon(QString(":/icons/item_%1").arg(icon)));
					item->setData(0, BBoxRole, QRect(x1, y1, x2 - x1, y2 - y1));
					item->setData(0, ClassRole, type);
					parentItem->addChild(item);
					haveWord = true;
					if(type == "ocr_line") {
						if(s_baseLineRx.indexIn(element.attribute("title")) != -1) {
							item->setData(0, BaselineRole, s_baseLineRx.cap(2).toInt());
						}
					} else if(type == "ocrx_word") {
						// Ensure correct hyphen char is used on last word of line
						if(nextElement.isNull()) {
							title.replace(QRegExp("[-\u2014]\\s*$"), "-");
							element.replaceChild(element.ownerDocument().createTextNode(title), element.firstChild());
							item->setText(0, title);
						}

						if(s_fontSizeRx.indexIn(element.attribute("title")) != -1) {
							item->setData(0, FontSizeRole, s_fontSizeRx.cap(1).toDouble());
						}
						item->setFlags(item->flags() | Qt::ItemIsEditable);
						QString lang = element.attribute("lang");
						auto it = langCache.find(lang);
						if(it == langCache.end()) {
							it = langCache.insert(lang, Utils::getSpellingLanguage(lang));
						}
						QString spellingLang = it.value();
						if(m_spell.getLanguage() != spellingLang) {
							m_spell.setLanguage(spellingLang);
						}
						if(!m_spell.checkWord(trimWord(title))) {
							item->setForeground(0, Qt::red);
						}
					}
				} else {
					delete item;
				}
			}
		}
		element = nextElement;
	}
	return haveWord;
}

QDomElement OutputEditorHOCR::elementById(QDomElement element, const QString& id) const
{
	QDomElement child;
	while(!element.isNull()) {
		if(element.attribute("id") == id) {
			return element;
		} else if(!(child = elementById(element.firstChildElement(), id)).isNull()) {
			return child;
		} else {
			element = element.nextSiblingElement();
		}
	}
	return QDomElement();
}

void OutputEditorHOCR::showItemProperties(QTreeWidgetItem* item)
{
	ui.tableWidgetProperties->blockSignals(true);
	ui.tableWidgetProperties->setRowCount(0);
	ui.tableWidgetProperties->blockSignals(false);
	ui.plainTextEditOutput->setPlainText("");
	m_tool->clearSelection();
	m_currentPageItem = nullptr;
	m_currentItem = item;
	if(!item) {
		m_currentDocument = QDomDocument();
		m_currentElement = QDomElement();
		return;
	}
	m_currentPageItem = item;
	while(m_currentPageItem->parent()) {
		m_currentPageItem = m_currentPageItem->parent();
	}
	QString id = item->data(0, IdRole).toString();
	m_currentDocument.setContent(m_currentPageItem->data(0, SourceRole).toString());
	m_currentElement = elementById(m_currentDocument.firstChildElement(), id);
	if(m_currentElement.isNull()) {
		m_currentDocument = QDomDocument();
		m_currentPageItem = nullptr;
		m_currentItem = nullptr;
		return;
	}
	QDomNamedNodeMap attributes = m_currentElement.attributes();
	int row = -1;
	ui.tableWidgetProperties->blockSignals(true);
	for(int i = 0, n = attributes.count(); i < n; ++i) {
		QDomNode attribNode = attributes.item(i);
		if(attribNode.nodeName() == "title") {
			for(const QString& attrib : attribNode.nodeValue().split(QRegExp("\\s*;\\s*"))) {
				int splitPos = attrib.indexOf(QRegExp("\\s+"));
				ui.tableWidgetProperties->insertRow(++row);
				QTableWidgetItem* attrNameItem = new QTableWidgetItem(attrib.left(splitPos));
				attrNameItem->setFlags(attrNameItem->flags() &  ~Qt::ItemIsEditable);
				attrNameItem->setData(ParentAttrRole, "title");
				ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
				ui.tableWidgetProperties->setItem(row, 1, new QTableWidgetItem(attrib.mid(splitPos + 1)));
			}
		} else {
			ui.tableWidgetProperties->insertRow(++row);
			QTableWidgetItem* attrNameItem = new QTableWidgetItem(attribNode.nodeName());
			attrNameItem->setFlags(attrNameItem->flags() &  ~Qt::ItemIsEditable);
			ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
			ui.tableWidgetProperties->setItem(row, 1, new QTableWidgetItem(attribNode.nodeValue()));
		}
	}
	ui.tableWidgetProperties->blockSignals(false);
	QString str;
	QTextStream stream(&str);
	m_currentElement.save(stream, 1);
	ui.plainTextEditOutput->setPlainText(str);

	if(setCurrentSource(m_currentDocument.firstChildElement("div")) && s_bboxRx.indexIn(m_currentElement.attribute("title")) != -1) {
		int x1 = s_bboxRx.cap(1).toInt();
		int y1 = s_bboxRx.cap(2).toInt();
		int x2 = s_bboxRx.cap(3).toInt();
		int y2 = s_bboxRx.cap(4).toInt();
		m_tool->setSelection(QRect(x1, y1, x2-x1, y2-y1));
	}
}

bool OutputEditorHOCR::setCurrentSource(const QDomElement& pageElement, int* pageDpi) const
{
	if(s_pageTitleRx.indexIn(pageElement.attribute("title")) != -1)
	{
		QString filename = s_pageTitleRx.cap(1);
		int page = s_pageTitleRx.cap(2).toInt();
		double angle = s_pageTitleRx.cap(3).toDouble();
		int res = s_pageTitleRx.cap(4).toInt();
		if(pageDpi) {
			*pageDpi = res;
		}

		MAIN->getSourceManager()->addSource(filename);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		int dummy;
		// TODO: Handle this better
		if(MAIN->getDisplayer()->getCurrentImage(dummy) != filename) {
			return false;
		}
		if(MAIN->getDisplayer()->getCurrentPage() != page) {
			MAIN->getDisplayer()->setCurrentPage(page);
		}
		if(MAIN->getDisplayer()->getCurrentAngle() != angle) {
			MAIN->getDisplayer()->setAngle(angle);
		}
		if(MAIN->getDisplayer()->getCurrentResolution() != res) {
			MAIN->getDisplayer()->setResolution(res);
		}
		return true;
	}
	return false;
}

void OutputEditorHOCR::itemChanged(QTreeWidgetItem* item, int col)
{
	if(item != m_currentItem) {
		return;
	}
	ui.treeWidgetItems->blockSignals(true);
	bool isWord = item->data(0, ClassRole).toString() == "ocrx_word";
	if( isWord && item->checkState(col) == Qt::Checked) {
		// Update text
		updateCurrentItemText();
	}
	else if(item->checkState(col) == Qt::Checked) {
		item->setFlags(item->flags() | Qt::ItemIsSelectable);
		if(!isWord) {
			item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
		}
	} else {
		item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
		item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
		item->setExpanded(false);
	}
	ui.treeWidgetItems->blockSignals(false);
}

void OutputEditorHOCR::propertyCellChanged(int row, int /*col*/)
{
	QTableWidgetItem* keyItem = ui.tableWidgetProperties->item(row, 0);
	QTableWidgetItem* valueItem = ui.tableWidgetProperties->item(row, 1);
	QString parentAttr = keyItem->data(ParentAttrRole).toString();
	if(!parentAttr.isEmpty()) {
		updateCurrentItemAttribute(parentAttr, keyItem->text(), valueItem->text());
	} else {
		updateCurrentItemAttribute(keyItem->text(), "", valueItem->text());
	}
}

void OutputEditorHOCR::updateCurrentItemText()
{
	if(m_currentItem) {
		QString newText = m_currentItem->text(0);
		m_currentElement.replaceChild(m_currentDocument.createTextNode(newText), m_currentElement.firstChild());
		updateCurrentItem();
	}
}

void OutputEditorHOCR::updateCurrentItemAttribute(const QString& key, const QString& subkey, const QString& newvalue, bool update)
{
	if(m_currentItem) {
		if(subkey.isEmpty()) {
			m_currentElement.setAttribute(key, newvalue);
		} else {
			QString value = m_currentElement.attribute(key);
			QStringList subattrs = value.split(QRegExp("\\s*;\\s*"));
			for(int i = 0, n = subattrs.size(); i < n; ++i) {
				int splitPos = subattrs[i].indexOf(QRegExp("\\s+"));
				if(subattrs[i].left(splitPos) == subkey) {
					subattrs[i] = subkey + " " + newvalue;
					break;
				}
			}
			m_currentElement.setAttribute(key, subattrs.join("; "));
		}
		if(update)
			updateCurrentItem();
	}
}

void OutputEditorHOCR::updateCurrentItemBBox(QRect rect)
{
	if(m_currentItem) {
		ui.treeWidgetItems->blockSignals(true);
		m_currentItem->setData(0, BBoxRole, rect);
		ui.treeWidgetItems->blockSignals(false);
		QString bboxstr = QString("%1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.x() + rect.width()).arg(rect.y() + rect.height());
		for(int row = 0, n = ui.tableWidgetProperties->rowCount(); row < n; ++row) {
			QTableWidgetItem* cell = ui.tableWidgetProperties->item(row, 0);
			if(cell->text() == "bbox" && cell->data(ParentAttrRole).toString() == "title") {
				ui.tableWidgetProperties->blockSignals(true);
				ui.tableWidgetProperties->item(row, 1)->setText(bboxstr);
				ui.tableWidgetProperties->blockSignals(false);
				break;
			}
		}
		updateCurrentItemAttribute("title", "bbox", bboxstr, false);
		QString str;
		QTextStream stream(&str);
		m_currentDocument.save(stream, 1);
		m_currentPageItem->setData(0, SourceRole, str);

		QString elemstr;
		QTextStream elemstream(&elemstr);
		m_currentElement.save(elemstream, 1);
		ui.plainTextEditOutput->setPlainText(elemstr);
	}
}

void OutputEditorHOCR::updateCurrentItem()
{
	QString spellLang = Utils::getSpellingLanguage(m_currentElement.attribute("lang"));
	if(m_spell.getLanguage() != spellLang) {
		m_spell.setLanguage(spellLang);
	}
	ui.treeWidgetItems->blockSignals(true); // prevent item changed signal
	m_currentItem->setForeground(0, m_spell.checkWord(trimWord(m_currentItem->text(0))) ? m_currentItem->parent()->foreground(0) : QBrush(Qt::red));
	ui.treeWidgetItems->blockSignals(false);

	QString str;
	QTextStream stream(&str);
	m_currentDocument.save(stream, 1);
	m_currentPageItem->setData(0, SourceRole, str);

	QString elemstr;
	QTextStream elemstream(&elemstr);
	m_currentElement.save(elemstream, 1);
	ui.plainTextEditOutput->setPlainText(elemstr);

	if(setCurrentSource(m_currentDocument.firstChildElement("div")) && s_bboxRx.indexIn(m_currentElement.attribute("title")) != -1) {
		int x1 = s_bboxRx.cap(1).toInt();
		int y1 = s_bboxRx.cap(2).toInt();
		int x2 = s_bboxRx.cap(3).toInt();
		int y2 = s_bboxRx.cap(4).toInt();
		m_tool->setSelection(QRect(x1, y1, x2-x1, y2-y1));
	}

	m_modified = true;
}

void OutputEditorHOCR::removeCurrentItem()
{
	m_currentElement.parentNode().removeChild(m_currentElement);

	QString str;
	QTextStream stream(&str);
	m_currentDocument.save(stream, 1);
	m_currentPageItem->setData(0, SourceRole, str);

	delete m_currentItem;
	m_currentItem = nullptr;
}

void OutputEditorHOCR::addGraphicRegion(QRect rect)
{
	QDomElement pageDiv = m_currentDocument.firstChildElement("div");
	if(pageDiv.isNull()) {
		return;
	}
	// Determine a free block id
	int pageId = 0;
	int blockId = 0;
	QDomElement blockEl = pageDiv.firstChildElement("div");
	while(!blockEl.isNull()) {
		if(s_idRx.indexIn(blockEl.attribute("id")) != -1) {
			pageId = qMax(pageId, s_idRx.cap(1).toInt() + 1);
			blockId = qMax(blockId, s_idRx.cap(2).toInt() + 1);
		}
		blockEl = blockEl.nextSiblingElement();
	}

	// Add html element
	QDomElement graphicElement = m_currentDocument.createElement("div");
	graphicElement.setAttribute("title", QString("bbox %1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.x() + rect.width()).arg(rect.y() + rect.height()));
	graphicElement.setAttribute("class", "ocr_carea");
	graphicElement.setAttribute("id", QString("block_%1_%2").arg(pageId).arg(blockId));
	pageDiv.appendChild(graphicElement);
	QString str;
	QTextStream stream(&str);
	m_currentDocument.save(stream, 1);
	m_currentPageItem->setData(0, SourceRole, str);

	// Add tree item
	QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << _("Graphic"));
	item->setCheckState(0, Qt::Checked);
	item->setIcon(0, QIcon(":/icons/item_halftone"));
	item->setData(0, IdRole, graphicElement.attribute("id"));
	item->setData(0, ClassRole, "ocr_graphic");
	item->setData(0, BBoxRole, rect);
	m_currentPageItem->addChild(item);

	ui.treeWidgetItems->setCurrentItem(item);
}

QString OutputEditorHOCR::trimWord(const QString& word, QString* prefix, QString* suffix)
{
	QRegExp wordRe("^(\\W*)(.*\\w)(\\W*)$");
	if(wordRe.indexIn(word) != -1) {
		if(prefix)
			*prefix = wordRe.cap(1);
		if(suffix)
			*suffix = wordRe.cap(3);
		return wordRe.cap(2);
	}
	return word;
}

void OutputEditorHOCR::mergeItems(const QList<QTreeWidgetItem*>& items)
{
	ui.treeWidgetItems->setCurrentItem(items.front());

	QRect bbox = items.front()->data(0, BBoxRole).toRect();
	QString text = items.front()->text(0);

	for(int i = 1, n = items.size(); i < n; ++i) {
		bbox = bbox.united(items[i]->data(0, BBoxRole).toRect());
		text += items[i]->text(0);
		QDomElement element = elementById(m_currentDocument.firstChildElement(), items[i]->data(0, IdRole).toString());
		element.parentNode().removeChild(element);
		delete items[i];
	}

	items.front()->setText(0, text);
	items.front()->setData(0, BBoxRole, bbox);
	updateCurrentItemText();
	updateCurrentItemAttribute("title", "bbox", QString("%1 %2 %3 %4").arg(bbox.x()).arg(bbox.y()).arg(bbox.x() + bbox.width()).arg(bbox.y() + bbox.height()));
	showItemProperties(m_currentItem);
}

void OutputEditorHOCR::showTreeWidgetContextMenu(const QPoint &point){
	QList<QTreeWidgetItem*> items = ui.treeWidgetItems->selectedItems();
	bool wordsSelected = true;
	for(QTreeWidgetItem* item : items) {
		QString itemClass = item->data(0, ClassRole).toString();
		if(itemClass != "ocrx_word") {
			wordsSelected = false;
			break;
		}
	}
	if(items.size() > 1 && wordsSelected) {
		QMenu menu;
		QAction* actionMerge = menu.addAction(_("Merge"));
		if(menu.exec(ui.treeWidgetItems->mapToGlobal(point)) == actionMerge) {
			mergeItems(items);
		}
		return;
	} else if(items.size() > 1) {
		return;
	}

	QTreeWidgetItem* item = ui.treeWidgetItems->itemAt(point);
	QString itemClass = item->data(0, ClassRole).toString();
	if(itemClass.isEmpty()) {
		return;
	}
	if(itemClass == "ocr_page") {
		QMenu menu;
		QAction* actionAddGraphic = menu.addAction(_("Add graphic region"));
		menu.addSeparator();
		QAction* actionRemove = menu.addAction(_("Remove"));
		QAction* clickedAction = menu.exec(ui.treeWidgetItems->mapToGlobal(point));
		if(clickedAction == actionRemove) {
			delete item;
			ui.actionOutputSaveHOCR->setEnabled(ui.treeWidgetItems->topLevelItemCount() > 0);
			ui.actionOutputExportPDF->setEnabled(ui.treeWidgetItems->topLevelItemCount() > 0);
		} else if(clickedAction == actionAddGraphic) {
			m_tool->clearSelection();
			m_tool->activateDrawSelection();
		}
	} else {
		QMenu menu;
		QAction* addAction = nullptr;
		QAction* ignoreAction = nullptr;
		 if(itemClass == "ocrx_word") {
			QString prefix, suffix, trimmedWord = trimWord(item->text(0), &prefix, &suffix);
			for(const QString& suggestion : m_spell.getSpellingSuggestions(trimmedWord)) {
				menu.addAction(prefix + suggestion + suffix);
			}
			if(menu.actions().isEmpty()) {
				menu.addAction(_("No suggestions"))->setEnabled(false);
			}
			if(!m_spell.checkWord(trimWord(item->text(0)))) {
				menu.addSeparator();
				addAction = menu.addAction(_("Add to dictionary"));
				ignoreAction = menu.addAction(_("Ignore word"));
			}
			menu.addSeparator();
		}
		QAction* removeAction = menu.addAction(_("Remove"));
		QAction* clickedAction = menu.exec(ui.treeWidgetItems->mapToGlobal(point));
		if(clickedAction) {
			if(clickedAction == addAction) {
				m_spell.addWordToDictionary(item->text(0));
				item->setForeground(0, item->parent()->foreground(0));
			} else if(clickedAction == ignoreAction) {
				m_spell.ignoreWord(item->text(0));
				item->setForeground(0, item->parent()->foreground(0));
			} else if(clickedAction == removeAction) {
				removeCurrentItem();
			} else {
				item->setText(0, clickedAction->text());
			}
		}
	}
}

void OutputEditorHOCR::updateFontButton(const QFont& font)
{
	m_pdfExportDialogUi.buttonFont->setText(QString("%1 %2").arg(font.family()).arg(font.pointSize()));
	updatePreview();
}

void OutputEditorHOCR::open()
{
	if(!clear(false)) {
		return;
	}
	QString dir = MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->getValue();
	QString filename = QFileDialog::getOpenFileName(m_widget, _("Open hOCR File"), dir, QString("%1 (*.html)").arg(_("hOCR HTML Files")));
	if(filename.isEmpty()) {
		return;
	}
	QFile file(filename);
	if(!file.open(QIODevice::ReadOnly)) {
		QMessageBox::critical(MAIN, _("Failed to open file"), _("The file could not be opened: %1.").arg(filename));
		return;
	}
	QDomDocument doc;
	doc.setContent(&file);
	QDomElement div = doc.firstChildElement("html").firstChildElement("body").firstChildElement("div");
	if(div.isNull() || div.attribute("class") != "ocr_page") {
		QMessageBox::critical(MAIN, _("Invalid hOCR file"), _("The file does not appear to contain valid hOCR HTML: %1").arg(filename));
		return;
	}
	int page = 0;
	while(!div.isNull()) {
		++page;
		addPage(div, QFileInfo(filename).fileName(), page);
		div = div.nextSiblingElement("div");
	}
}

bool OutputEditorHOCR::save(const QString& filename)
{
	QString outname = filename;
	if(outname.isEmpty()){
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		QString base = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
		outname = QDir(MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->getValue()).absoluteFilePath(base + ".html");
		outname = QFileDialog::getSaveFileName(MAIN, _("Save hOCR Output..."), outname, QString("%1 (*.html)").arg(_("hOCR HTML Files")));
		if(outname.isEmpty()){
			return false;
		}
		MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->setValue(QFileInfo(outname).absolutePath());
	}
	QFile file(outname);
	if(!file.open(QIODevice::WriteOnly)){
		QMessageBox::critical(MAIN, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	tesseract::TessBaseAPI tess;
	QString header = QString(
			"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
			"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
			" <head>\n"
			"  <title></title>\n"
			"  <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" />\n"
			"    <meta name='ocr-system' content='tesseract %1' />\n"
			"    <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
			"  </head>\n"
			"<body>\n").arg(tess.Version());
	file.write(header.toUtf8());
	for(int i = 0, n = ui.treeWidgetItems->topLevelItemCount(); i < n; ++i) {
		file.write(ui.treeWidgetItems->topLevelItem(i)->data(0, SourceRole).toString().toUtf8());
	}
	file.write("</body>\n</html>\n");
	m_modified = false;
	return true;
}

void OutputEditorHOCR::savePDF()
{
	m_preview = new QGraphicsPixmapItem();
	m_preview->setTransformationMode(Qt::SmoothTransformation);
	updatePreview();
	MAIN->getDisplayer()->scene()->addItem(m_preview);

	bool accepted = false;
	PoDoFo::PdfStreamedDocument* document = nullptr;
	PoDoFo::PdfFont* font = nullptr;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
	const PoDoFo::PdfEncoding* pdfEncoding = PoDoFo::PdfEncodingFactory::GlobalIdentityEncodingInstance();
#else
	const PoDoFo::PdfEncoding* pdfEncoding = new PoDoFo::PdfIdentityEncoding;
#endif
	while(true) {
		accepted = (m_pdfExportDialog->exec() == QDialog::Accepted);
		if(!accepted) {
			break;
		}

		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		QString base = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
		QString outname = QDir(MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->getValue()).absoluteFilePath(base + ".pdf");
		outname = QFileDialog::getSaveFileName(MAIN, _("Save PDF Output..."), outname, QString("%1 (*.pdf)").arg(_("PDF Files")));
		if(outname.isEmpty()){
			accepted = false;
			break;
		}
		MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->setValue(QFileInfo(outname).absolutePath());

		try {
			document = new PoDoFo::PdfStreamedDocument(outname.toLocal8Bit().data());
		} catch(...) {
			QMessageBox::critical(MAIN, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
			continue;
		}
		try {
			QFontInfo info(m_pdfFontDialog.currentFont());
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,9,3)
			font = document->CreateFontSubset(info.family().toLocal8Bit().data(), info.bold(), info.italic(), false, pdfEncoding);
#else
			font = document->CreateFontSubset(info.family().toLocal8Bit().data(), info.bold(), info.italic(), pdfEncoding);
#endif
		} catch(...) {
			font = nullptr;
		}
		if(!font) {
			QMessageBox::critical(MAIN, _("Error"), _("The PDF library does not support the selected font."));
			document->Close();
			delete document;
			continue;
		}
		break;
	}

	MAIN->getDisplayer()->scene()->removeItem(m_preview);
	delete m_preview;
	m_preview = nullptr;
	if(!accepted) {
		return;
	}

	PoDoFo::PdfPainter painter;

	PDFSettings pdfSettings;
	pdfSettings.colorFormat = static_cast<QImage::Format>(m_pdfExportDialogUi.comboBoxImageFormat->itemData(m_pdfExportDialogUi.comboBoxImageFormat->currentIndex()).toInt());
	pdfSettings.compression = static_cast<PDFSettings::Compression>(m_pdfExportDialogUi.comboBoxImageCompression->itemData(m_pdfExportDialogUi.comboBoxImageCompression->currentIndex()).toInt());
	pdfSettings.compressionQuality = m_pdfExportDialogUi.spinBoxCompressionQuality->value();
	pdfSettings.useDetectedFontSizes = m_pdfExportDialogUi.checkBoxFontSize->isChecked();
	pdfSettings.uniformizeLineSpacing = m_pdfExportDialogUi.checkBoxUniformizeSpacing->isChecked();
	pdfSettings.preserveSpaceWidth = m_pdfExportDialogUi.spinBoxPreserve->value();
	pdfSettings.overlay = m_pdfExportDialogUi.comboBoxOutputMode->currentIndex() == 1;
	pdfSettings.detectedFontScaling = m_pdfExportDialogUi.spinFontScaling->value() / 100.;
	QStringList failed;
	for(int i = 0, n = ui.treeWidgetItems->topLevelItemCount(); i < n; ++i) {
		QTreeWidgetItem* item = ui.treeWidgetItems->topLevelItem(i);
		if(item->checkState(0) != Qt::Checked) {
			continue;
		}
		QRect bbox = item->data(0, BBoxRole).toRect();
		QDomDocument doc;
		doc.setContent(item->data(0, SourceRole).toString());
		int pageDpi = 72;
		if(setCurrentSource(doc.firstChildElement("div"), &pageDpi)) {
			double dpiScale = 72. / pageDpi;
			double imageScale = m_pdfExportDialogUi.spinBoxDpi->value() / double(pageDpi);
			PoDoFo::PdfPage* page = document->CreatePage(PoDoFo::PdfRect(0, 0, bbox.width() * dpiScale, bbox.height() * dpiScale));
			painter.SetPage(page);
			painter.SetFont(font);

			PoDoFoPDFPainter pdfprinter(document, &painter, dpiScale, imageScale);
			pdfprinter.setFontSize(m_pdfFontDialog.currentFont().pointSize());
			printChildren(pdfprinter, item, pdfSettings);
			if(pdfSettings.overlay) {
				pdfprinter.drawImage(bbox, m_tool->getSelection(bbox), pdfSettings);
			}
			painter.FinishPage();
		} else {
			failed.append(item->text(0));
		}
	}
	if(!failed.isEmpty()) {
		QMessageBox::warning(m_widget, _("Errors occurred"), _("The following pages could not be rendered:\n%1").arg(failed.join("\n")));
	}
	document->Close();
	delete document;
}

void OutputEditorHOCR::printChildren(PDFPainter& painter, QTreeWidgetItem* item, const PDFSettings& pdfSettings) const
{
	if(item->checkState(0) != Qt::Checked) {
		return;
	}
	QString itemClass = item->data(0, ClassRole).toString();
	QRect itemRect = item->data(0, BBoxRole).toRect();
	if(itemClass == "ocr_par" && pdfSettings.uniformizeLineSpacing) {
		double yInc = double(itemRect.height()) / item->childCount();
		double y = itemRect.top() + yInc;
		int baseline = item->childCount() > 0 ? item->child(0)->data(0, BaselineRole).toInt() : 0;
		for(int iLine = 0, nLines = item->childCount(); iLine < nLines; ++iLine, y += yInc) {
			QTreeWidgetItem* lineItem = item->child(iLine);
			int x = itemRect.x();
			int prevWordRight = itemRect.x();
			for(int iWord = 0, nWords = lineItem->childCount(); iWord < nWords; ++iWord) {
				QTreeWidgetItem* wordItem = lineItem->child(iWord);
				if(wordItem->checkState(0) == Qt::Checked) {
					QRect wordRect = wordItem->data(0, BBoxRole).toRect();
					if(pdfSettings.useDetectedFontSizes) {
						painter.setFontSize(wordItem->data(0, FontSizeRole).toDouble() * pdfSettings.detectedFontScaling);
					}
					// If distance from previous word is large, keep the space
					if(wordRect.x() - prevWordRight > pdfSettings.preserveSpaceWidth * painter.getAverageCharWidth()) {
						x = wordRect.x();
					}
					prevWordRight = wordRect.right();
					painter.drawText(x, y + baseline, wordItem->text(0));
					x += painter.getTextWidth(wordItem->text(0) + " ");
				}
			}
		}
	} else if(itemClass == "ocr_line" && !pdfSettings.uniformizeLineSpacing) {
		int baseline = item->data(0, BaselineRole).toInt();
		double y = itemRect.bottom() + baseline;
		for(int iWord = 0, nWords = item->childCount(); iWord < nWords; ++iWord) {
			QTreeWidgetItem* wordItem = item->child(iWord);
			QRect wordRect = wordItem->data(0, BBoxRole).toRect();
			if(pdfSettings.useDetectedFontSizes) {
				painter.setFontSize(wordItem->data(0, FontSizeRole).toDouble() * pdfSettings.detectedFontScaling);
			}
			painter.drawText(wordRect.x(), y, wordItem->text(0));
		}
	} else if(itemClass == "ocr_graphic" && !pdfSettings.overlay) {
		painter.drawImage(itemRect, m_tool->getSelection(itemRect), pdfSettings);
	} else {
		for(int i = 0, n = item->childCount(); i < n; ++i) {
			printChildren(painter, item->child(i), pdfSettings);
		}
	}
}

void OutputEditorHOCR::updatePreview()
{
	if(!m_preview) {
		return;
	}
	m_preview->setVisible(m_pdfExportDialogUi.checkBoxPreview->isChecked());
	if(ui.treeWidgetItems->topLevelItemCount() == 0 || !m_pdfExportDialogUi.checkBoxPreview->isChecked()) {
		return;
	}
	QTreeWidgetItem* item = ui.treeWidgetItems->currentItem();
	if(!item) {
		item = ui.treeWidgetItems->topLevelItem(0);
	} else {
		while(item->parent()) {
			item = item->parent();
		}
	}
	QRect bbox = item->data(0, BBoxRole).toRect();
	QDomDocument doc;
	int pageDpi = 72;
	doc.setContent(item->data(0, SourceRole).toString());
	setCurrentSource(doc.firstChildElement("div"), &pageDpi);

	PDFSettings pdfSettings;
	pdfSettings.colorFormat = static_cast<QImage::Format>(m_pdfExportDialogUi.comboBoxImageFormat->itemData(m_pdfExportDialogUi.comboBoxImageFormat->currentIndex()).toInt());
	pdfSettings.compression = static_cast<PDFSettings::Compression>(m_pdfExportDialogUi.comboBoxImageCompression->itemData(m_pdfExportDialogUi.comboBoxImageCompression->currentIndex()).toInt());
	pdfSettings.compressionQuality = m_pdfExportDialogUi.spinBoxCompressionQuality->value();
	pdfSettings.useDetectedFontSizes = m_pdfExportDialogUi.checkBoxFontSize->isChecked();
	pdfSettings.uniformizeLineSpacing = m_pdfExportDialogUi.checkBoxUniformizeSpacing->isChecked();
	pdfSettings.preserveSpaceWidth = m_pdfExportDialogUi.spinBoxPreserve->value();
	pdfSettings.overlay = m_pdfExportDialogUi.comboBoxOutputMode->currentIndex() == 1;
	pdfSettings.detectedFontScaling = m_pdfExportDialogUi.spinFontScaling->value() / 100.;

	QImage image(bbox.size(), QImage::Format_ARGB32);
	image.setDotsPerMeterX(pageDpi / 0.0254);
	image.setDotsPerMeterY(pageDpi / 0.0254);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setFont(m_pdfFontDialog.currentFont());
	QPainterPDFPainter pdfPrinter(&painter);

	if(pdfSettings.overlay) {
		pdfPrinter.drawImage(bbox, m_tool->getSelection(bbox), pdfSettings);
		painter.fillRect(0, 0, bbox.width(), bbox.height(), QColor(255, 255, 255, 127));
	} else {
		image.fill(Qt::white);
	}
	printChildren(pdfPrinter, item, pdfSettings);
	m_preview->setPixmap(QPixmap::fromImage(image));
	m_preview->setPos(-0.5 * bbox.width(), -0.5 * bbox.height());
}

bool OutputEditorHOCR::clear(bool hide)
{
	if(!m_widget->isVisible()){
		return true;
	}
	if(getModified()){
		int response = QMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), QMessageBox::Save, QMessageBox::Discard, QMessageBox::Cancel);
		if(response == QMessageBox::Save){
			if(!save()){
				return false;
			}
		}else if(response != QMessageBox::Discard){
			return false;
		}
	}
	m_idCounter = 0;
	ui.treeWidgetItems->clear();
	ui.tableWidgetProperties->setRowCount(0);
	ui.plainTextEditOutput->clear();
	m_tool->clearSelection();
	m_modified = false;
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

bool OutputEditorHOCR::getModified() const
{
	return m_modified;
}

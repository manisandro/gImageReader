/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCROdtExporter.cc
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

#include "DisplayerToolHOCR.hh"
#include "HOCRDocument.hh"
#include "HOCROdtExporter.hh"
#include "MainWindow.hh"
#include "FileDialogs.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <QFileInfo>
#include <QMessageBox>
#include <QUuid>
#include <QXmlStreamWriter>
#include <quazipfile.h>

static QString manifestNS("urn:oasis:names:tc:opendocument:xmlns:manifest:1.0");
static QString officeNS("urn:oasis:names:tc:opendocument:xmlns:office:1.0");
static QString textNS ("urn:oasis:names:tc:opendocument:xmlns:text:1.0");
static QString styleNS ("urn:oasis:names:tc:opendocument:xmlns:style:1.0");
static QString foNS ("urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0");
static QString tableNS ("urn:oasis:names:tc:opendocument:xmlns:table:1.0");
static QString drawNS ("urn:oasis:names:tc:opendocument:xmlns:drawing:1.0");
static QString xlinkNS ("http://www.w3.org/1999/xlink");
static QString svgNS ("urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0");

bool HOCROdtExporter::run(const HOCRDocument *hocrdocument, QString &filebasename)
{
	QString suggestion = filebasename;
	if(suggestion.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
	}

	QString outname = FileDialogs::saveDialog(_("Save ODT Output..."), suggestion + ".odt", "outputdir", QString("%1 (*.odt)").arg(_("OpenDocument Text Documents")));
	if(outname.isEmpty()) {
		return false;
	}
	filebasename = QFileInfo(outname).completeBaseName();

	QuaZip zip(outname);
	if(!zip.open( QuaZip::mdCreate )) {
		QMessageBox::warning(MAIN, _("Export failed"), _("The ODT export failed: unable to write output file."));
		return false;
	}
	int pageCount = hocrdocument->pageCount();
	MainWindow::ProgressMonitor monitor(2 * pageCount);
	MAIN->showProgress(&monitor);
	Utils::busyTask([&] {
		// Image files
		QMap<const HOCRItem*, QString> imageFiles;
		for(int i = 0; i < pageCount; ++i) {
			monitor.increaseProgress();
			const HOCRPage* page = hocrdocument->page(i);
			if(!page->isEnabled()) {
				continue;
			}
			bool success = false;
			QMetaObject::invokeMethod(this, "setSource", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, success), Q_ARG(QString, page->sourceFile()), Q_ARG(int, page->pageNr()), Q_ARG(int, page->resolution()), Q_ARG(double, page->angle()));
			if(!success) {
				continue;
			}
			for(const HOCRItem* item : page->children()) {
				writeImage(zip, imageFiles, item);
			}
		}

		// Mimetype
		QuaZipFile* mimetype = new QuaZipFile(&zip);
		mimetype->open(QIODevice::WriteOnly, QuaZipNewInfo("mimetype"));
		mimetype->write("application/vnd.oasis.opendocument.text");
		mimetype->close();

		// Manifest
		QuaZipFile* manifest = new QuaZipFile(&zip);
		manifest->open(QIODevice::WriteOnly, QuaZipNewInfo("META-INF/manifest.xml"));
		QXmlStreamWriter manifestWriter(manifest);
		manifestWriter.setAutoFormatting(true);
		manifestWriter.setAutoFormattingIndent(1);
		manifestWriter.writeNamespace(manifestNS, "manifest");
		manifestWriter.writeStartDocument();
		manifestWriter.writeStartElement(manifestNS, "manifest");
		manifestWriter.writeAttribute(manifestNS, "version", "1.2");

		manifestWriter.writeEmptyElement(manifestNS, "file-entry");
		manifestWriter.writeAttribute(manifestNS, "media-type", "application/vnd.oasis.opendocument.text");
		manifestWriter.writeAttribute(manifestNS, "full-path", "/");

		manifestWriter.writeEmptyElement(manifestNS, "file-entry");
		manifestWriter.writeAttribute(manifestNS, "media-type", "text/xml");
		manifestWriter.writeAttribute(manifestNS, "full-path", "content.xml");

		for(const QString& imageFile : imageFiles.values()) {
			manifestWriter.writeEmptyElement(manifestNS, "file-entry");
			manifestWriter.writeAttribute(manifestNS, "media-type", "image/png");
			manifestWriter.writeAttribute(manifestNS, "full-path", imageFile);
		}

		manifestWriter.writeEndElement(); // manifest
		manifestWriter.writeEndDocument();
		manifest->close();

		// Content
		QuaZipFile* content = new QuaZipFile(&zip);
		content->open(QIODevice::WriteOnly, QuaZipNewInfo("content.xml"));
		QXmlStreamWriter writer(content);
		writer.setAutoFormatting(true);
		writer.setAutoFormattingIndent(2);

		writer.writeNamespace(officeNS, "office");
		writer.writeNamespace(textNS, "text");
		writer.writeNamespace(styleNS, "style");
		writer.writeNamespace(foNS, "fo");
		writer.writeNamespace(drawNS, "draw");
		writer.writeNamespace(xlinkNS, "xlink");
		writer.writeNamespace(svgNS, "svg");
		writer.writeStartDocument();
		writer.writeStartElement(officeNS, "document-content");
		writer.writeAttribute(officeNS, "version", "1.2");

		// - Styles
		writer.writeStartElement(officeNS, "automatic-styles");

		// -- Standard paragraph
		writer.writeStartElement(styleNS, "style");
		writer.writeAttribute(styleNS, "name", "P");
		writer.writeAttribute(styleNS, "parent-style-name", "Standard");
		writer.writeEndElement();

		// -- Paragraph preceded by page break
		writer.writeStartElement(styleNS, "style");
		writer.writeAttribute(styleNS, "name", "PB");
		writer.writeAttribute(styleNS, "family", "paragraph");
		writer.writeAttribute(styleNS, "parent-style-name", "Standard");
		writer.writeEmptyElement(styleNS, "paragraph-properties");
		writer.writeAttribute(foNS, "break-before", "page");
		writer.writeEndElement();

		// -- Frame, absolutely positioned on page
		writer.writeStartElement(styleNS, "style");
		writer.writeAttribute(styleNS, "name", "F");
		writer.writeAttribute(styleNS, "family", "graphic");
		writer.writeStartElement(styleNS, "graphic-properties");
		writer.writeAttribute(drawNS, "stroke", "none");
		writer.writeAttribute(drawNS, "fill", "none");
		writer.writeAttribute(styleNS, "vertical-pos", "from-top");
		writer.writeAttribute(styleNS, "vertical-rel", "page");
		writer.writeAttribute(styleNS, "horizontal-pos", "from-left");
		writer.writeAttribute(styleNS, "horizontal-rel", "page");
		writer.writeAttribute(styleNS, "flow-with-text", "false");
		writer.writeEndElement();
		writer.writeEndElement();

		// -- Text styles
		QMap<QString,QMap<double,QString>> fontStyles;
		for(int i = 0; i < pageCount; ++i) {
			const HOCRPage* page = hocrdocument->page(i);
			if(page->isEnabled()) {
				for(const HOCRItem* item : page->children()) {
					collectFontStyles(fontStyles, item);
				}
			}
		}
		int counter = 0;
		for(auto ffit = fontStyles.begin(), ffitEnd = fontStyles.end(); ffit != ffitEnd; ++ffit) {
			QString fontFamily = ffit.key();
			for(auto fsit = ffit.value().begin(), fsitEnd = ffit.value().end(); fsit != fsitEnd; ++fsit) {
				double fontSize = fsit.key();
				QString styleName = QString("T%1").arg(++counter);
				fsit.value() = styleName;
				writer.writeStartElement(styleNS, "style");
				writer.writeAttribute(styleNS, "name", styleName);
				writer.writeAttribute(styleNS, "family", "text");
				writer.writeStartElement(styleNS, "text-properties");
				writer.writeAttribute(foNS, "font-name", fontFamily);
				writer.writeAttribute(foNS, "font-size", QString("%1pt").arg(fontSize));
				writer.writeEndElement(); // text-properties
				writer.writeEndElement(); // style
			}
		}

		writer.writeEndElement(); // automatic-styles

		// - Body
		writer.writeStartElement(officeNS, "body");
		writer.writeStartElement(officeNS, "text");

		bool firstPage = true;
		for(int i = 0; i < pageCount; ++i) {
			monitor.increaseProgress();
			const HOCRPage* page = hocrdocument->page(i);
			if(!page->isEnabled()) {
				continue;
			}
			if(!firstPage) {
				writer.writeEmptyElement(textNS, "p");
				writer.writeAttribute(textNS, "style-name", "PB");
			}
			firstPage = false;
			for(const HOCRItem* item : page->children()) {
				printItem(writer, item, page->resolution(), fontStyles, imageFiles);
			}
		}

		writer.writeEndElement(); // text
		writer.writeEndElement(); // body
		writer.writeEndElement(); // document-content
		writer.writeEndDocument();
		content->close();
		return true;
	}, _("Exporting to ODT..."));
	MAIN->hideProgress();

	zip.close();
	return true;
}

void HOCROdtExporter::writeImage(QuaZip& zip, QMap<const HOCRItem*,QString>& images, const HOCRItem* item)
{
	if(!item->isEnabled()) {
		return;
	}
	if(item->itemClass() == "ocr_graphic") {
		QImage image;
		QMetaObject::invokeMethod(this, "getSelection",  Qt::BlockingQueuedConnection, Q_RETURN_ARG(QImage, image), Q_ARG(QRect, item->bbox()));
		QString filename = QString("Pictures/%1.png").arg(QUuid::createUuid().toString());
		QuaZipFile* file = new QuaZipFile(&zip);
		if(file->open(QIODevice::WriteOnly, QuaZipNewInfo(filename))) {
			image.save(file, "png");
			images.insert(item, filename);
		}
	}
}

void HOCROdtExporter::collectFontStyles(QMap<QString,QMap<double,QString>>& styles, const HOCRItem* item)
{
	if(!item->isEnabled()) {
		return;
	}
	if(item->itemClass() == "ocrx_word") {
		styles[item->fontFamily()].insert(item->fontSize(), "");
	} else {
		for(const HOCRItem* item : item->children()) {
			collectFontStyles(styles, item);
		}
	}
}

void HOCROdtExporter::printItem(QXmlStreamWriter& writer, const HOCRItem* item, int dpi, const QMap<QString,QMap<double,QString>>& fontStyleNames, const QMap<const HOCRItem*,QString>& images)
{
	if(!item->isEnabled()) {
		return;
	}
	QString itemClass = item->itemClass();
	const QRect& bbox = item->bbox();
	if(itemClass == "ocr_graphic") {
		writer.writeStartElement(textNS, "p");
		writer.writeAttribute(textNS, "style-name", "P");

		writer.writeStartElement(drawNS, "frame");
		writer.writeAttribute(drawNS, "style-name", "F");
		writer.writeAttribute(textNS, "anchor-type", "page");
		writer.writeAttribute(textNS, "anchor-page-number", QString::number(item->page()->pageNr()));
		writer.writeAttribute(svgNS, "x", QString("%1in").arg(bbox.x() / double(dpi)));
		writer.writeAttribute(svgNS, "y", QString("%1in").arg(bbox.y() / double(dpi)));
		writer.writeAttribute(svgNS, "width", QString("%1in").arg(bbox.width() / double(dpi)));
		writer.writeAttribute(svgNS, "height", QString("%1in").arg(bbox.height() / double(dpi)));
		writer.writeAttribute(drawNS, "z-index", "0");

		writer.writeStartElement(drawNS, "image");
		writer.writeAttribute(xlinkNS, "href", images[item]);
		writer.writeAttribute(xlinkNS, "type", "simple");
		writer.writeAttribute(xlinkNS, "show", "embed");
		writer.writeEndElement(); // image

		writer.writeEndElement(); // frame
		writer.writeEndElement(); // p
	}
	else if(itemClass == "ocr_par") {
		writer.writeStartElement(textNS, "p");
		writer.writeAttribute(textNS, "style-name", "P");

		writer.writeStartElement(drawNS, "frame");
		writer.writeAttribute(drawNS, "style-name", "F");
		writer.writeAttribute(textNS, "anchor-type", "page");
		writer.writeAttribute(textNS, "anchor-page-number", QString::number(item->page()->pageNr()));
		writer.writeAttribute(svgNS, "x", QString("%1in").arg(bbox.x() / double(dpi)));
		writer.writeAttribute(svgNS, "y", QString("%1in").arg(bbox.y() / double(dpi)));
		writer.writeAttribute(svgNS, "width", QString("%1in").arg(bbox.width() / double(dpi)));
		writer.writeAttribute(svgNS, "height", QString("%1in").arg(bbox.height() / double(dpi)));
		writer.writeAttribute(drawNS, "z-index", "1");

		writer.writeStartElement(drawNS, "text-box");

		writer.writeStartElement(textNS, "p");
		writer.writeAttribute(textNS, "style-name", "P");
		for(const HOCRItem* child : item->children()) {
			printItem(writer, child, dpi, fontStyleNames, images);
		}
		writer.writeEndElement();

		writer.writeEndElement(); // text-box
		writer.writeEndElement(); // frame
		writer.writeEndElement(); // p
	} else if(itemClass == "ocr_line") {
		const HOCRItem* firstWord = nullptr;
		int iChild = 0, nChilds = item->children().size();
		for(; iChild < nChilds; ++iChild) {
			const HOCRItem* child = item->children()[iChild];
			if(child->isEnabled()) {
				firstWord = child;
				break;
			}
		}
		if(!firstWord) {
			return;
		}
		QString currentFontStyleName = fontStyleNames[firstWord->fontFamily()][firstWord->fontSize()];
		writer.writeStartElement(textNS, "span");
		writer.writeAttribute(textNS, "style-name", currentFontStyleName);
		writer.writeCharacters(firstWord->text());
		++iChild;
		for(; iChild < nChilds; ++iChild) {
			const HOCRItem* child = item->children()[iChild];
			QString fontStyleName = fontStyleNames[child->fontFamily()][child->fontSize()];
			if(fontStyleName != currentFontStyleName) {
				currentFontStyleName = fontStyleName;
				writer.writeEndElement();
				writer.writeStartElement(textNS, "span");
				writer.writeAttribute(textNS, "style-name", currentFontStyleName);
			}
			writer.writeCharacters(" ");
			writer.writeCharacters(child->text());
		}
		writer.writeEndElement();
		writer.writeEmptyElement(textNS, "line-break");
	} else {
		for(const HOCRItem* child : item->children()) {
			printItem(writer, child, dpi, fontStyleNames, images);
		}
	}
}

bool HOCROdtExporter::setSource(const QString& sourceFile, int page, int dpi, double angle) {
	if(MAIN->getSourceManager()->addSource(sourceFile)) {
		MAIN->getDisplayer()->setup(&page, &dpi, &angle);
		return true;
	} else {
		return false;
	}
}

QImage HOCROdtExporter::getSelection(const QRect& bbox) {
	return m_displayerTool->getSelection(bbox);
}

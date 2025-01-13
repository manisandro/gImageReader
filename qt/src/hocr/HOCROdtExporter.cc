/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCROdtExporter.cc
 * Copyright (C) 2013-2025 Sandro Mani <manisandro@gmail.com>
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

#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "HOCRDocument.hh"
#include "HOCROdtExporter.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <QDesktopServices>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#include <QUuid>
#include <QXmlStreamWriter>
#include <quazipfile.h>

static QString manifestNS("urn:oasis:names:tc:opendocument:xmlns:manifest:1.0");
static QString officeNS("urn:oasis:names:tc:opendocument:xmlns:office:1.0");
static QString textNS("urn:oasis:names:tc:opendocument:xmlns:text:1.0");
static QString styleNS("urn:oasis:names:tc:opendocument:xmlns:style:1.0");
static QString foNS("urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0");
static QString tableNS("urn:oasis:names:tc:opendocument:xmlns:table:1.0");
static QString drawNS("urn:oasis:names:tc:opendocument:xmlns:drawing:1.0");
static QString xlinkNS("http://www.w3.org/1999/xlink");
static QString svgNS("urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0");

bool HOCROdtExporter::run(const HOCRDocument* hocrdocument, const QString& outname, const ExporterSettings* /*settings*/) {
	QuaZip zip(outname);
	if (!zip.open(QuaZip::mdCreate)) {
		QMessageBox::warning(MAIN, _("Export failed"), _("The ODT export failed: unable to write output file."));
		return false;
	}
	int pageCount = hocrdocument->pageCount();
	MainWindow::ProgressMonitor monitor(2 * pageCount);
	MAIN->showProgress(&monitor);
	Utils::busyTask([&] {
		// Image files
		QMap<const HOCRItem*, QString> imageFiles;
		for (int i = 0; i < pageCount; ++i) {
			monitor.increaseProgress();
			const HOCRPage* page = hocrdocument->page(i);
			if (!page->isEnabled()) {
				continue;
			}
			bool success = false;
			QMetaObject::invokeMethod(this, "setSource", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, success), Q_ARG(QString, page->sourceFile()), Q_ARG(int, page->pageNr()), Q_ARG(int, page->resolution()), Q_ARG(double, page->angle()));
			if (!success) {
				continue;
			}
			for (const HOCRItem* item : page->children()) {
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

		for (const QString& imageFile : imageFiles.values()) {
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

		// - Font face decls
		writer.writeStartElement(officeNS, "font-face-decls");
		QSet<QString> families;
		for (int i = 0; i < pageCount; ++i) {
			const HOCRPage* page = hocrdocument->page(i);
			if (page->isEnabled()) {
				writeFontFaceDecls(families, page, writer);
			}
		}
		writer.writeEndElement();

		// - Page styles
		writer.writeStartElement(officeNS, "automatic-styles");
		for (int i = 0; i < pageCount; ++i) {
			const HOCRPage* page = hocrdocument->page(i);
			if (page->isEnabled()) {
				writer.writeStartElement(styleNS, "page-layout");
				writer.writeAttribute(styleNS, "name", QString("PL%1").arg(i));
				writer.writeEmptyElement(styleNS, "page-layout-properties");
				writer.writeAttribute(foNS, "page-width", QString("%1in").arg(page->bbox().width() / double (page->resolution())));
				writer.writeAttribute(foNS, "page-height", QString("%1in").arg(page->bbox().height() / double (page->resolution())));
				writer.writeAttribute(styleNS, "print-orientation", page->bbox().width() > page->bbox().height() ? "landscape" : "portrait");
				writer.writeAttribute(styleNS, "writing-mode", "lr-tb");
				writer.writeAttribute(foNS, "margin-top", "0in");
				writer.writeAttribute(foNS, "margin-bottom", "0in");
				writer.writeAttribute(foNS, "margin-left", "0in");
				writer.writeAttribute(foNS, "margin-right", "0in");
				writer.writeEndElement(); // page-layout
			}
		}
		writer.writeEndElement(); // automatic-styles
		writer.writeStartElement(officeNS, "master-styles");
		for (int i = 0; i < pageCount; ++i) {
			if (hocrdocument->page(i)->isEnabled()) {
				writer.writeEmptyElement(styleNS, "master-page");
				writer.writeAttribute(styleNS, "name", QString("MP%1").arg(i));
				writer.writeAttribute(styleNS, "page-layout-name", QString("PL%1").arg(i));
			}
		}
		writer.writeEndElement(); // master-styles

		// - Styles
		writer.writeStartElement(officeNS, "automatic-styles");

		// -- Standard paragraph
		writer.writeStartElement(styleNS, "style");
		writer.writeAttribute(styleNS, "name", "P");
		writer.writeAttribute(styleNS, "parent-style-name", "Standard");
		writer.writeEndElement();

		// -- Page paragraphs
		for (int i = 0; i < pageCount; ++i) {
			if (hocrdocument->page(i)->isEnabled()) {
				writer.writeStartElement(styleNS, "style");
				writer.writeAttribute(styleNS, "name", QString("PP%1").arg(i));
				writer.writeAttribute(styleNS, "family", "paragraph");
				writer.writeAttribute(styleNS, "parent-style-name", "Standard");
				writer.writeAttribute(styleNS, "master-page-name", QString("MP%1").arg(i));
				writer.writeEmptyElement(styleNS, "paragraph-properties");
				writer.writeAttribute(foNS, "break-before", "page");
				writer.writeEndElement();
			}
		}

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
		int counter = 0;
		QMap<QString, QMap<double, QString >> fontStyles;
		for (int i = 0; i < pageCount; ++i) {
			const HOCRPage* page = hocrdocument->page(i);
			if (page->isEnabled()) {
				for (const HOCRItem* item : page->children()) {
					writeFontStyles(fontStyles, item, writer, counter);
				}
			}
		}

		writer.writeEndElement(); // automatic-styles

		// - Body
		writer.writeStartElement(officeNS, "body");
		writer.writeStartElement(officeNS, "text");

		int pageCounter = 1;
		for (int i = 0; i < pageCount; ++i) {
			monitor.increaseProgress();
			const HOCRPage* page = hocrdocument->page(i);
			if (!page->isEnabled()) {
				continue;
			}
			writer.writeStartElement(textNS, "p");
			writer.writeAttribute(textNS, "style-name", QString("PP%1").arg(i));
			for (const HOCRItem* item : page->children()) {
				printItem(writer, item, pageCounter, page->resolution(), fontStyles, imageFiles);
			}
			writer.writeEndElement();
			++pageCounter;
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

	bool openAfterExport = ConfigSettings::get<SwitchSetting> ("openafterexport")->getValue();
	if (openAfterExport) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(outname));
	}

	return true;
}

void HOCROdtExporter::writeImage(QuaZip& zip, QMap<const HOCRItem*, QString>& images, const HOCRItem* item) {
	if (!item->isEnabled()) {
		return;
	}
	if (item->itemClass() == "ocr_graphic") {
		QImage image;
		QMetaObject::invokeMethod(this, "getSelection",  Qt::BlockingQueuedConnection, Q_RETURN_ARG(QImage, image), Q_ARG(QRect, item->bbox()));
		QString filename = QString("Pictures/%1.png").arg(QUuid::createUuid().toString());
		QuaZipFile* file = new QuaZipFile(&zip);
		if (file->open(QIODevice::WriteOnly, QuaZipNewInfo(filename))) {
			image.save(file, "png");
			images.insert(item, filename);
		}
	}
}

void HOCROdtExporter::writeFontFaceDecls(QSet<QString>& families, const HOCRItem* item, QXmlStreamWriter& writer) {
	if (!item->isEnabled()) {
		return;
	}
	if (item->itemClass() == "ocrx_word") {
		QString fontFamily = item->fontFamily();
		if (!families.contains(fontFamily)) {
			if (!fontFamily.isEmpty()) {
				writer.writeEmptyElement(styleNS, "font-face");
				writer.writeAttribute(styleNS, "name", fontFamily);
				writer.writeAttribute(svgNS, "font-family", fontFamily);
			}
			families.insert(fontFamily);
		}
	} else {
		for (const HOCRItem* child : item->children()) {
			writeFontFaceDecls(families, child, writer);
		}
	}
}

void HOCROdtExporter::writeFontStyles(QMap<QString, QMap<double, QString >> & styles, const HOCRItem* item, QXmlStreamWriter& writer, int& counter) {
	if (!item->isEnabled()) {
		return;
	}
	if (item->itemClass() == "ocrx_word") {
		QString fontKey = item->fontFamily() + (item->fontBold() ? "@bold" : "") + (item->fontItalic() ? "@italic" : "");
		if (!styles.contains(fontKey) || !styles[fontKey].contains(item->fontSize())) {
			QString styleName = QString("T%1").arg(++counter);
			writer.writeStartElement(styleNS, "style");
			writer.writeAttribute(styleNS, "name", styleName);
			writer.writeAttribute(styleNS, "family", "text");
			writer.writeStartElement(styleNS, "text-properties");
			writer.writeAttribute(styleNS, "font-name", item->fontFamily());
			writer.writeAttribute(foNS, "font-size", QString("%1pt").arg(item->fontSize()));
			if (item->fontBold()) {
				writer.writeAttribute(foNS, "font-weight", "bold");
			}
			if (item->fontItalic()) {
				writer.writeAttribute(foNS, "font-style", "italic");
			}
			writer.writeEndElement(); // text-properties
			writer.writeEndElement(); // style

			styles[fontKey].insert(item->fontSize(), styleName);
		}
	}
	else {
		for (const HOCRItem* child : item->children()) {
			writeFontStyles(styles, child, writer, counter);
		}
	}
}

void HOCROdtExporter::printItem(QXmlStreamWriter& writer, const HOCRItem* item, int pageNr, int dpi, const QMap<QString, QMap<double, QString >>& fontStyleNames, const QMap<const HOCRItem*, QString>& images) {
	if (!item->isEnabled()) {
		return;
	}
	QString itemClass = item->itemClass();
	const QRect& bbox = item->bbox();
	if (itemClass == "ocr_graphic") {
		writer.writeStartElement(drawNS, "frame");
		writer.writeAttribute(drawNS, "style-name", "F");
		writer.writeAttribute(textNS, "anchor-type", "page");
		writer.writeAttribute(textNS, "anchor-page-number", QString::number(pageNr));
		writer.writeAttribute(svgNS, "x", QString("%1in").arg(bbox.x() / double (dpi)));
		writer.writeAttribute(svgNS, "y", QString("%1in").arg(bbox.y() / double (dpi)));
		writer.writeAttribute(svgNS, "width", QString("%1in").arg(bbox.width() / double (dpi)));
		writer.writeAttribute(svgNS, "height", QString("%1in").arg(bbox.height() / double (dpi)));
		writer.writeAttribute(drawNS, "z-index", "0");

		writer.writeStartElement(drawNS, "image");
		writer.writeAttribute(xlinkNS, "href", images[item]);
		writer.writeAttribute(xlinkNS, "type", "simple");
		writer.writeAttribute(xlinkNS, "show", "embed");
		writer.writeEndElement(); // image

		writer.writeEndElement(); // frame
	} else if (itemClass == "ocr_par") {
		writer.writeStartElement(drawNS, "frame");
		writer.writeAttribute(drawNS, "style-name", "F");
		writer.writeAttribute(textNS, "anchor-type", "page");
		writer.writeAttribute(textNS, "anchor-page-number", QString::number(pageNr));
		writer.writeAttribute(svgNS, "x", QString("%1in").arg(bbox.x() / double (dpi)));
		writer.writeAttribute(svgNS, "y", QString("%1in").arg(bbox.y() / double (dpi)));
		writer.writeAttribute(svgNS, "width", QString("%1in").arg(bbox.width() / double (dpi)));
		writer.writeAttribute(svgNS, "height", QString("%1in").arg(bbox.height() / double (dpi)));
		writer.writeAttribute(drawNS, "z-index", "1");

		writer.writeStartElement(drawNS, "text-box");

		writer.writeStartElement(textNS, "p");
		writer.writeAttribute(textNS, "style-name", "P");
		for (const HOCRItem* child : item->children()) {
			printItem(writer, child, pageNr, dpi, fontStyleNames, images);
		}
		writer.writeEndElement();

		writer.writeEndElement(); // text-box
		writer.writeEndElement(); // frame
	} else if (itemClass == "ocr_line") {
		const HOCRItem* firstWord = nullptr;
		int iChild = 0, nChilds = item->children().size();
		for (; iChild < nChilds; ++iChild) {
			const HOCRItem* child = item->children() [iChild];
			if (child->isEnabled()) {
				firstWord = child;
				break;
			}
		}
		if (!firstWord) {
			return;
		}
		QString fontKey = firstWord->fontFamily() + (firstWord->fontBold() ? "@bold" : "") + (firstWord->fontItalic() ? "@italic" : "");
		QString currentFontStyleName = fontStyleNames[fontKey][firstWord->fontSize()];
		writer.writeStartElement(textNS, "span");
		writer.writeAttribute(textNS, "style-name", currentFontStyleName);
		writer.writeCharacters(firstWord->text());
		++iChild;
		for (; iChild < nChilds; ++iChild) {
			const HOCRItem* child = item->children() [iChild];
			if (!child->isEnabled()) {
				continue;
			}
			fontKey = child->fontFamily() + (child->fontBold() ? "@bold" : "") + (child->fontItalic() ? "@italic" : "");
			QString fontStyleName = fontStyleNames[fontKey][child->fontSize()];
			if (fontStyleName != currentFontStyleName) {
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
		for (const HOCRItem* child : item->children()) {
			printItem(writer, child, pageNr, dpi, fontStyleNames, images);
		}
	}
}

bool HOCROdtExporter::setSource(const QString& sourceFile, int page, int dpi, double angle) {
	if (MAIN->getSourceManager()->addSource(sourceFile, true)) {
		MAIN->getDisplayer()->setup(&page, &dpi, &angle);
		return true;
	} else {
		return false;
	}
}

QImage HOCROdtExporter::getSelection(const QRect& bbox) {
	Displayer* displayer = MAIN->getDisplayer();
	return displayer->getImage(bbox.translated(displayer->getSceneBoundingRect().toRect().topLeft()));
}

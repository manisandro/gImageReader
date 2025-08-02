/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRPdfExporter.cc
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

#include <cstring>
#include <podofo.h>
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
#include <podofo/base/PdfDictionary.h>
#include <podofo/base/PdfFilter.h>
#include <podofo/base/PdfStream.h>
#include <podofo/doc/PdfFont.h>
#include <podofo/doc/PdfIdentityEncoding.h>
#include <podofo/doc/PdfImage.h>
#include <podofo/doc/PdfPage.h>
#include <podofo/doc/PdfPainter.h>
#include <podofo/doc/PdfStreamedDocument.h>
#endif
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "CCITTFax4Encoder.hh"
#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "HOCRDocument.hh"
#include "HOCRPdfExporter.hh"
#include "HOCRPdfExportWidget.hh"
#include "MainWindow.hh"
#include "PaperSize.hh"
#include "SourceManager.hh"
#include "Utils.hh"


bool HOCRPdfExporter::run(const HOCRDocument* hocrdocument, const std::string& outname, const ExporterSettings* settings) {
	const PDFSettings* pdfSettings = static_cast<const PDFSettings*> (settings);

	int pageCount = hocrdocument->pageCount();
	Glib::ustring errMsg;
	MainWindow::ProgressMonitor monitor(pageCount);
	MAIN->showProgress(&monitor);

	double pageWidth, pageHeight;
	// Page dimensions are in points: 1 in = 72 pt
	if (pdfSettings->paperSize == "custom") {
		pageWidth = pdfSettings->paperWidthIn * 72.0;
		pageHeight = pdfSettings->paperHeightIn * 72.0;
	} else if (pdfSettings->paperSize != "source") {
		auto inchSize = PaperSize::getSize(PaperSize::inch, pdfSettings->paperSize, pdfSettings->paperSizeLandscape);
		pageWidth = inchSize.width * 72.0;
		pageHeight = inchSize.height * 72.0;
	}


	bool success = Utils::busyTask([&] {
		HOCRPdfPrinter* painter = HOCRPoDoFoPdfPrinter::create(outname, *pdfSettings, pdfSettings->fallbackFontFamily, pdfSettings->fallbackFontSize, errMsg);
		for (int i = 0; i < pageCount; ++i) {
			if (monitor.cancelled()) {
				errMsg = _("The operation was cancelled");
				return false;
			}
			const HOCRPage* page = hocrdocument->page(i);
			if (page->isEnabled()) {
				Geometry::Rectangle bbox = page->bbox();
				Glib::ustring sourceFile = page->sourceFile();
				// If the source file is an image, its "resolution" is actually just the scale factor that was used for recognizing.
				bool isImage = Glib::ustring(sourceFile.substr(sourceFile.length() - 4)).lowercase() != ".pdf" && Glib::ustring(sourceFile.substr(sourceFile.length() - 5)).lowercase() != ".djvu";
				int sourceDpi = page->resolution();
				int sourceScale = sourceDpi;
				if (isImage) {
					sourceDpi *= pdfSettings->assumedImageDpi / 100;
				}
				// [pt] = 72 * [in]
				// [in] = 1 / dpi * [px]
				// => [pt] = 72 / dpi * [px]
				double px2pt = (72.0 / sourceDpi);
				double imgScale = double (pdfSettings->outputDpi) / sourceDpi;

				bool success = false;
				if (isImage) {
					Utils::runInMainThreadBlocking([&] { success = setSource(page->sourceFile(), page->pageNr(), sourceScale * imgScale, page->angle()); });
				} else {
					Utils::runInMainThreadBlocking([&] { success = setSource(page->sourceFile(), page->pageNr(), pdfSettings->outputDpi, page->angle()); });
				}
				if (success) {
					if (pdfSettings->paperSize == "source") {
						pageWidth = bbox.width * px2pt;
						pageHeight = bbox.height * px2pt;
					}
					double offsetX = 0.5 * (pageWidth - bbox.width * px2pt);
					double offsetY = 0.5 * (pageHeight - bbox.height * px2pt);
					if (!painter->createPage(pageWidth, pageHeight, offsetX, offsetY, errMsg)) {
						return false;
					}
					try {
						painter->printChildren(page, *pdfSettings, px2pt, imgScale, double (sourceScale) / sourceDpi, true);
					} catch (const std::exception& e) {
						errMsg = e.what();
						return false;
					}
					if (pdfSettings->overlay) {
						Geometry::Rectangle scaledRect(imgScale * bbox.x, imgScale * bbox.y, imgScale * bbox.width, imgScale * bbox.height);
						Geometry::Rectangle printRect(bbox.x * px2pt, bbox.y * px2pt, bbox.width * px2pt, bbox.height * px2pt);
						Cairo::RefPtr<Cairo::ImageSurface> selection;
						Utils::runInMainThreadBlocking([&] { selection = painter->getSelection(scaledRect); });
						painter->drawImage(printRect, selection, *pdfSettings);
					}
					Utils::runInMainThreadBlocking([&] { setSource(page->sourceFile(), page->pageNr(), sourceScale, page->angle()); });
					painter->finishPage();
				} else {
					errMsg = Glib::ustring::compose(_("Failed to render page %1"), page->title());
					return false;
				}
			}
			monitor.increaseProgress();
		}
		bool success = painter->finishDocument(errMsg);
		delete painter;
		return success;
	}, _("Exporting to PDF..."));
	MAIN->hideProgress();

	if (!success) {
		Utils::messageBox(Gtk::MESSAGE_WARNING, _("Export failed"), Glib::ustring::compose(_("The PDF export failed: %1."), errMsg));
	} else {
		bool openAfterExport = ConfigSettings::get<SwitchSettingT<Gtk::CheckButton >> ("openafterexport")->getValue();
		if (openAfterExport) {
			Utils::openUri(Glib::filename_to_uri(outname));
		}
	}
	return success;
}

bool HOCRPdfExporter::setSource(const Glib::ustring& sourceFile, int page, int dpi, double angle) {
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(sourceFile);
	if (MAIN->getSourceManager()->addSource(file, true)) {
		MAIN->getDisplayer()->setup(&page, &dpi, &angle);
		return true;
	} else {
		return false;
	}
}


HOCRPdfPrinter::HOCRPdfPrinter() {
	Glib::RefPtr<Pango::FontMap> fontMap = Glib::wrap(pango_cairo_font_map_get_default(),  true);
	for (const Glib::RefPtr<Pango::FontFamily>& family : fontMap->list_families()) {
		m_fontFamilies.push_back(family->get_name());
	}
}

void HOCRPdfPrinter::printChildren(const HOCRItem* item, const HOCRPdfExporter::PDFSettings& pdfSettings, double px2pu/*pixels to printer units*/, double imgScale, double fontScale, bool inThread) {
	if (!item->isEnabled()) {
		return;
	}
	if (pdfSettings.fontSize != -1) {
		setFontSize(pdfSettings.fontSize * fontScale);
	}
	Glib::ustring itemClass = item->itemClass();
	Geometry::Rectangle itemRect = item->bbox();
	int childCount = item->children().size();
	if (itemClass == "ocr_par" && pdfSettings.uniformizeLineSpacing) {
		double yInc = double (itemRect.height) / childCount;
		double y = itemRect.y + yInc;
		std::pair<double, double> baseline = childCount > 0 ? item->children() [0]->baseLine() : std::make_pair(0.0, 0.0);
		for (int iLine = 0; iLine < childCount; ++iLine, y += yInc) {
			HOCRItem* lineItem = item->children() [iLine];
			int x = itemRect.x;
			int prevWordRight = itemRect.x;
			for (int iWord = 0, nWords = lineItem->children().size(); iWord < nWords; ++iWord) {
				HOCRItem* wordItem = lineItem->children() [iWord];
				if (!wordItem->isEnabled()) {
					continue;
				}
				Geometry::Rectangle wordRect = wordItem->bbox();
				setFontFamily(pdfSettings.fontFamily.empty() ? wordItem->fontFamily() : pdfSettings.fontFamily, wordItem->fontBold(), wordItem->fontItalic());
				if (pdfSettings.fontSize == -1) {
					setFontSize(wordItem->fontSize() * pdfSettings.detectedFontScaling * fontScale);
				}
				// If distance from previous word is large, keep the space
				if (wordRect.x - prevWordRight > pdfSettings.preserveSpaceWidth * getAverageCharWidth() / px2pu) {
					x = wordRect.x;
				}
				prevWordRight = wordRect.x + wordRect.width;
				Glib::ustring text = wordItem->text();
				if (iWord == nWords - 1 && pdfSettings.sanitizeHyphens) {
					text = Glib::Regex::create("[-\u2014]\\s*$")->replace(text, 0, "-", static_cast<Glib::RegexMatchFlags> (0));
				}
				double wordBaseline = (x - itemRect.x) * baseline.first + baseline.second;
				drawText(x * px2pu, (y + wordBaseline) * px2pu, text);
				x += getTextWidth(text + " ") / px2pu;
			}
		}
	} else if (itemClass == "ocr_line" && !pdfSettings.uniformizeLineSpacing) {
		std::pair<double, double> baseline = item->baseLine();
		for (int iWord = 0, nWords = item->children().size(); iWord < nWords; ++iWord) {
			HOCRItem* wordItem = item->children() [iWord];
			if (!wordItem->isEnabled()) {
				continue;
			}
			Geometry::Rectangle wordRect = wordItem->bbox();
			setFontFamily(pdfSettings.fontFamily.empty() ? wordItem->fontFamily() : pdfSettings.fontFamily, wordItem->fontBold(), wordItem->fontItalic());
			if (pdfSettings.fontSize == -1) {
				setFontSize(wordItem->fontSize() * pdfSettings.detectedFontScaling * fontScale);
			}
			double y = itemRect.y + itemRect.height + (wordRect.x + 0.5 * wordRect.width - itemRect.x) * baseline.first + baseline.second;
			Glib::ustring text = wordItem->text();
			if (iWord == nWords - 1 && pdfSettings.sanitizeHyphens) {
				text = Glib::Regex::create("[-\u2014]\\s*$")->replace(text, 0, "-", static_cast<Glib::RegexMatchFlags> (0));
			}
			drawText(wordRect.x * px2pu, y * px2pu, text);
		}
	} else if (itemClass == "ocr_graphic" && !pdfSettings.overlay) {
		Geometry::Rectangle scaledItemRect(imgScale * itemRect.x, imgScale * itemRect.y, imgScale * itemRect.width, imgScale * itemRect.height);
		Geometry::Rectangle printRect(itemRect.x * px2pu, itemRect.y * px2pu, itemRect.width * px2pu, itemRect.height * px2pu);
		Cairo::RefPtr<Cairo::ImageSurface> selection;
		if (inThread) {
			Utils::runInMainThreadBlocking([&] { selection = getSelection(scaledItemRect); });
		} else {
			selection = getSelection(scaledItemRect);
		}
		drawImage(printRect, selection, pdfSettings);
	} else {
		for (int i = 0, n = item->children().size(); i < n; ++i) {
			printChildren(item->children() [i], pdfSettings, px2pu, imgScale, fontScale, inThread);
		}
	}
}

Cairo::RefPtr<Cairo::ImageSurface> HOCRPdfPrinter::getSelection(const Geometry::Rectangle& bbox) {
	Displayer* displayer = MAIN->getDisplayer();
	Geometry::Rectangle sceneRect = displayer->getSceneBoundingRect();
	return displayer->getImage(bbox.translate(sceneRect.x, sceneRect.y));
}


HOCRCairoPdfPrinter::HOCRCairoPdfPrinter(Cairo::RefPtr<Cairo::Context> context, const Glib::ustring& defaultFont)
	: m_context(context), m_defaultFont(defaultFont) {
	m_curFont = m_defaultFont;
	m_context->select_font_face(m_curFont, Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
}

void HOCRCairoPdfPrinter::setFontFamily(const Glib::ustring& family, bool bold, bool italic) {
	if (family != m_curFont) {
		if (std::find(m_fontFamilies.begin(), m_fontFamilies.end(), family) != m_fontFamilies.end()) {
			m_curFont = family;
		}  else {
			m_curFont = m_defaultFont;
		}
	}
	m_context->select_font_face(m_curFont, italic ? Cairo::FONT_SLANT_ITALIC : Cairo::FONT_SLANT_NORMAL, bold ? Cairo::FONT_WEIGHT_BOLD : Cairo::FONT_WEIGHT_NORMAL);
}

void HOCRCairoPdfPrinter::setFontSize(double pointSize) {
	m_context->set_font_size(pointSize);
}

void HOCRCairoPdfPrinter::drawText(double x, double y, const Glib::ustring& text) {
	m_context->move_to(x, y/* - ext.y_bearing*/);
	m_context->show_text(text);
}

void HOCRCairoPdfPrinter::drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const HOCRPdfExporter::PDFSettings& settings) {
	m_context->save();
	m_context->move_to(bbox.x, bbox.y);
	if (settings.compression == HOCRPdfExporter::PDFSettings::CompressJpeg) {
		Image img(image, settings.colorFormat, settings.conversionFlags);
		uint8_t* buf = nullptr;
		unsigned long bufLen = 0;
		img.writeJpeg(settings.compressionQuality, buf, bufLen);
		Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create("jpeg");
		loader->write(buf, bufLen);
		loader->close();
		std::free(buf);
		Gdk::Cairo::set_source_pixbuf(m_context, loader->get_pixbuf(), bbox.x, bbox.y);
	} else {
		Cairo::RefPtr<Cairo::ImageSurface> img = Image::simulateFormat(image, settings.colorFormat, settings.conversionFlags);
		m_context->set_source(img, bbox.x, bbox.y);
	}
	m_context->paint();
	m_context->restore();
}

double HOCRCairoPdfPrinter::getAverageCharWidth() const {
	Cairo::TextExtents ext;
	m_context->get_text_extents("x ", ext);
	return ext.x_advance - ext.width; // spaces are ignored in width but counted in advance
}

double HOCRCairoPdfPrinter::getTextWidth(const Glib::ustring& text) const {
	Cairo::TextExtents ext;
	m_context->get_text_extents(text, ext);
	return ext.x_advance;
}


HOCRPoDoFoPdfPrinter* HOCRPoDoFoPdfPrinter::create(const std::string& filename, const HOCRPdfExporter::PDFSettings& settings, const Glib::ustring& defaultFont, int defaultFontSize, Glib::ustring& errMsg) {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
	PoDoFo::PdfMemDocument* document = nullptr;
#else
	PoDoFo::PdfStreamedDocument* document = nullptr;
#endif
	PoDoFo::PdfFont* defaultPdfFont = nullptr;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
	PoDoFo::PdfEncoding* pdfFontEncoding = nullptr;
#else
	const PoDoFo::PdfEncoding* pdfFontEncoding = PoDoFo::PdfEncodingFactory::GlobalIdentityEncodingInstance();
#endif

	try {
		Glib::ustring password = settings.password;
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
		PoDoFo::PdfEncrypt* encrypt = nullptr;
#else
		std::unique_ptr<PoDoFo::PdfEncrypt> encrypt;
#endif
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
		if (!password.empty()) {
			encrypt = PoDoFo::PdfEncrypt::CreatePdfEncrypt(password, password,
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

		PoDoFo::EPdfVersion pdfVersion = PoDoFo::ePdfVersion_Default;
		switch (settings.version) {
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_0:
			pdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_0;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_1:
			pdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_1;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_2:
			pdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_2;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_3:
			pdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_3;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_4:
			pdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_4;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_5:
			pdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_5;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_6:
			pdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_6;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_7:
			pdfVersion = PoDoFo::EPdfVersion::ePdfVersion_1_7;
			break;
		}
		document = new PoDoFo::PdfStreamedDocument(filename.c_str(), pdfVersion, encrypt);

		// Set PDF info
		PoDoFo::PdfInfo* pdfInfo = document->GetInfo();
		pdfInfo->SetProducer(settings.producer.c_str());
		pdfInfo->SetCreator(settings.creator.c_str());
		pdfInfo->SetTitle(settings.title.c_str());
		pdfInfo->SetSubject(settings.subject.c_str());
		pdfInfo->SetKeywords(settings.keywords.c_str());
		pdfInfo->SetAuthor(settings.author.c_str());
#else
		if (!password.empty()) {
			encrypt = PoDoFo::PdfEncrypt::Create(password.raw(),
			                                     password.raw(),
			                                     PoDoFo::PdfPermissions::Print |
			                                     PoDoFo::PdfPermissions::Edit |
			                                     PoDoFo::PdfPermissions::Copy |
			                                     PoDoFo::PdfPermissions::EditNotes |
			                                     PoDoFo::PdfPermissions::FillAndSign |
			                                     PoDoFo::PdfPermissions::Accessible |
			                                     PoDoFo::PdfPermissions::DocAssembly |
			                                     PoDoFo::PdfPermissions::HighPrint,
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(1, 0, 0)
			                                     PoDoFo::PdfEncryptAlgorithm::RC4V2);
#else
			                                     PoDoFo::PdfEncryptionAlgorithm::RC4V2);
#endif
		}
		PoDoFo::PdfVersion pdfVersion = PoDoFo::PdfVersionDefault;
		switch (settings.version) {
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_0:
			pdfVersion = PoDoFo::PdfVersion::V1_0;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_1:
			pdfVersion = PoDoFo::PdfVersion::V1_1;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_2:
			pdfVersion = PoDoFo::PdfVersion::V1_2;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_3:
			pdfVersion = PoDoFo::PdfVersion::V1_3;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_4:
			pdfVersion = PoDoFo::PdfVersion::V1_4;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_5:
			pdfVersion = PoDoFo::PdfVersion::V1_5;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_6:
			pdfVersion = PoDoFo::PdfVersion::V1_6;
			break;
		case HOCRPdfExporter::PDFSettings::Version::PdfVersion_1_7:
			pdfVersion = PoDoFo::PdfVersion::V1_7;
			break;
		}
		document = new PoDoFo::PdfMemDocument();
		document->SetEncrypt(std::move(encrypt));

		// Set PDF info
		document->GetMetadata().SetProducer(PoDoFo::PdfString(settings.producer.raw()));
		document->GetMetadata().SetCreator(PoDoFo::PdfString(settings.creator.raw()));
		document->GetMetadata().SetTitle(PoDoFo::PdfString(settings.title.raw()));
		document->GetMetadata().SetSubject(PoDoFo::PdfString(settings.subject.raw()));
		document->GetMetadata().SetKeywords(std::vector<std::string> {settings.keywords.raw() });
		document->GetMetadata().SetAuthor(PoDoFo::PdfString(settings.author.raw()));
		document->GetMetadata().SetPdfVersion(pdfVersion);
#endif
	} catch (PoDoFo::PdfError& err) {
		errMsg = err.what();
		return nullptr;
	}

	// Attempt to load the default/fallback font to ensure it is valid
	try {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
		defaultPdfFont = document->GetFonts().SearchFont(defaultFont.raw());
#else
		defaultPdfFont = document->CreateFontSubset(defaultFont.raw(), false, false, false, pdfFontEncoding);
#endif
	} catch (PoDoFo::PdfError&) {
	}
	if (!defaultPdfFont) {
		errMsg = Glib::ustring::compose(_("The PDF library could not load the font '%1'."), defaultFont);
		return nullptr;
	}

	return new HOCRPoDoFoPdfPrinter(document, filename, pdfFontEncoding, defaultPdfFont, defaultFont, defaultFontSize);
}

HOCRPoDoFoPdfPrinter::HOCRPoDoFoPdfPrinter(PoDoFo::PdfDocument* document, const std::string& filename, const PoDoFo::PdfEncoding* fontEncoding, PoDoFo::PdfFont* defaultFont, const Glib::ustring& defaultFontFamily, double defaultFontSize)
	: m_document(document), m_filename(filename), m_pdfFontEncoding(fontEncoding), m_defaultFont(defaultFont), m_defaultFontFamily(defaultFontFamily), m_defaultFontSize(defaultFontSize) {
	m_painter = new PoDoFo::PdfPainter();
}

HOCRPoDoFoPdfPrinter::~HOCRPoDoFoPdfPrinter() {
	delete m_document;
	delete m_painter;
	// Fonts are deleted by the internal PoDoFo font cache of the document
}

bool HOCRPoDoFoPdfPrinter::createPage(double width, double height, double offsetX, double offsetY, Glib::ustring& /*errMsg*/) {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
	PoDoFo::PdfPage& pdfpage = m_document->GetPages().CreatePage(PoDoFo::Rect(0, 0, width, height));
	m_painter->SetCanvas(pdfpage);
	m_pageHeight = height;
	m_painter->TextState.SetFont(*m_defaultFont, m_defaultFontSize);
#else
	PoDoFo::PdfPage* pdfpage = m_document->CreatePage(PoDoFo::PdfRect(0, 0, width, height));
	m_painter->SetPage(pdfpage);
	m_pageHeight = m_painter->GetPage()->GetPageSize().GetHeight();
	m_painter->SetFont(m_defaultFont);
	if (m_defaultFontSize > 0) {
		m_painter->GetFont()->SetFontSize(m_defaultFontSize);
	}
#endif
	m_offsetX = offsetX;
	m_offsetY = offsetY;
	return true;
}

void HOCRPoDoFoPdfPrinter::finishPage() {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
	m_painter->FinishDrawing();
#else
	m_painter->FinishPage();
#endif
}

bool HOCRPoDoFoPdfPrinter::finishDocument(Glib::ustring& errMsg) {
	try {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
		static_cast<PoDoFo::PdfMemDocument*> (m_document)->Save(m_filename);
#else
		static_cast<PoDoFo::PdfStreamedDocument*> (m_document)->Close();
#endif
	} catch (PoDoFo::PdfError& e) {
		errMsg = e.what();
		return false;
	}
	return true;
}

void HOCRPoDoFoPdfPrinter::setFontFamily(const Glib::ustring& family, bool bold, bool italic) {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
	float curSize = m_painter->TextState.GetFontSize();
	m_painter->TextState.SetFont(*getFont(family, bold, italic), curSize);
#else
	float curSize = m_painter->GetFont()->GetFontSize();
	m_painter->SetFont(getFont(family, bold, italic));
	m_painter->GetFont()->SetFontSize(curSize);
#endif
}

void HOCRPoDoFoPdfPrinter::setFontSize(double pointSize) {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
	m_painter->TextState.SetFont(*m_painter->TextState.GetFont(), pointSize);
#else
	m_painter->GetFont()->SetFontSize(pointSize);
#endif
}

void HOCRPoDoFoPdfPrinter::drawText(double x, double y, const Glib::ustring& text) {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
	m_painter->DrawText(text.raw(), m_offsetX + x, m_pageHeight - m_offsetY - y);
#else
	PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*> (text.c_str()));
	m_painter->DrawText(m_offsetX + x, m_pageHeight - m_offsetY - y, pdfString);
#endif
}

void HOCRPoDoFoPdfPrinter::drawImage(const Geometry::Rectangle& bbox, const Cairo::RefPtr<Cairo::ImageSurface>& image, const HOCRPdfExporter::PDFSettings& settings) {
	Image img(image, settings.colorFormat, settings.conversionFlags);
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
	std::unique_ptr<PoDoFo::PdfImage> pdfImage = m_document->CreateImage();
#else
	PoDoFo::PdfImage pdfImage(m_document);
#endif
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
	pdfImage.SetImageColorSpace(settings.colorFormat == Image::Format_RGB24 ? PoDoFo::ePdfColorSpace_DeviceRGB : PoDoFo::ePdfColorSpace_DeviceGray);
#endif
	if (settings.compression == HOCRPdfExporter::PDFSettings::CompressZip) {
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
		PoDoFo::PdfMemoryInputStream is(reinterpret_cast<const char*> (img.data), PoDoFo::pdf_long(img.bytesPerLine) * img.height);
		pdfImage.SetImageData(img.width, img.height, img.sampleSize, &is, {PoDoFo::ePdfFilter_FlateDecode});
#else
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(1, 0, 0)
		PoDoFo::PdfColorSpace colorSpace = settings.colorFormat == Image::Format_RGB24 ? PoDoFo::PdfColorSpace::DeviceRGB : PoDoFo::PdfColorSpace::DeviceGray;
#else
		PoDoFo::PdfColorSpaceType colorSpace = settings.colorFormat == Image::Format_RGB24 ? PoDoFo::PdfColorSpaceType::DeviceRGB : PoDoFo::PdfColorSpaceType::DeviceGray;
#endif
		PoDoFo::PdfImageInfo info;
		info.Width = img.width;
		info.Height = img.height;
		info.ColorSpace = colorSpace;
		info.BitsPerComponent = img.sampleSize;
		pdfImage->SetDataRaw(PoDoFo::bufferview(reinterpret_cast<const char*> (img.data), img.bytesPerLine * img.height), info);
#endif
	} else if (settings.compression == HOCRPdfExporter::PDFSettings::CompressJpeg) {
		uint8_t* buf = nullptr;
		unsigned long bufLen = 0;
		img.writeJpeg(settings.compressionQuality, buf, bufLen);
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
		PoDoFo::PdfName dctFilterName(PoDoFo::PdfFilterFactory::FilterTypeToName(PoDoFo::ePdfFilter_DCTDecode));
		pdfImage.GetObject()->GetDictionary().AddKey(PoDoFo::PdfName::KeyFilter, dctFilterName);
		PoDoFo::PdfMemoryInputStream is(reinterpret_cast<const char*> (buf), bufLen);
		pdfImage.SetImageDataRaw(img.width, img.height, img.sampleSize, &is);
#else
		pdfImage->LoadFromBuffer(PoDoFo::bufferview(reinterpret_cast<const char*> (buf), bufLen));
#endif
		std::free(buf);
	} else if (settings.compression == HOCRPdfExporter::PDFSettings::CompressFax4) {
		CCITTFax4Encoder encoder;
		uint32_t encodedLen = 0;
		uint8_t* encoded = encoder.encode(img.data, img.width, img.height, img.bytesPerLine, encodedLen);
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
		PoDoFo::PdfName faxFilterName(PoDoFo::PdfFilterFactory::FilterTypeToName(PoDoFo::ePdfFilter_CCITTFaxDecode));
		pdfImage.GetObject()->GetDictionary().AddKey(PoDoFo::PdfName::KeyFilter, faxFilterName);
		PoDoFo::PdfDictionary decodeParams;
		decodeParams.AddKey("Columns", PoDoFo::PdfObject(PoDoFo::pdf_int64(img.width)));
		decodeParams.AddKey("Rows", PoDoFo::PdfObject(PoDoFo::pdf_int64(img.height)));
		decodeParams.AddKey("K", PoDoFo::PdfObject(PoDoFo::pdf_int64(-1)));      // K < 0 --- Pure two-dimensional encoding (Group 4)
		pdfImage.GetObject()->GetDictionary().AddKey("DecodeParms", PoDoFo::PdfObject(decodeParams));
		PoDoFo::PdfMemoryInputStream is(reinterpret_cast<char*> (encoded), encodedLen);
		pdfImage.SetImageDataRaw(img.width, img.height, img.sampleSize, &is);
#else
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(1, 0, 0)
		PoDoFo::PdfColorSpace colorSpace = PoDoFo::PdfColorSpace::DeviceGray;
#else
		PoDoFo::PdfColorSpaceType colorSpace = PoDoFo::PdfColorSpaceType::DeviceGray;
#endif
		PoDoFo::PdfImageInfo info;
		info.Width = img.width;
		info.Height = img.height;
		info.ColorSpace = colorSpace;
		info.BitsPerComponent = img.sampleSize;
		info.Filters = {PoDoFo::PdfFilterType::CCITTFaxDecode};
		PoDoFo::PdfDictionary decodeParams;
		decodeParams.AddKey("Columns", PoDoFo::PdfObject(int64_t (img.width)));
		decodeParams.AddKey("Rows", PoDoFo::PdfObject(int64_t (img.height)));
		decodeParams.AddKey("K", PoDoFo::PdfObject(int64_t (-1)));     // K < 0 --- Pure two-dimensional encoding (Group 4)
		pdfImage->GetDictionary().AddKey("DecodeParms", PoDoFo::PdfObject(decodeParams));
		pdfImage->SetDataRaw(PoDoFo::bufferview(reinterpret_cast<const char*> (encoded), encodedLen), info);
#endif
	}
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
	m_painter->DrawImage(m_offsetX + bbox.x, m_pageHeight - m_offsetY - (bbox.y + bbox.height),
	                     &pdfImage, bbox.width / double (image->get_width()), bbox.height / double (image->get_height()));
#else
	m_painter->DrawImage(*pdfImage, m_offsetX + bbox.x, m_pageHeight - m_offsetY - (bbox.y + bbox.height),
	                     bbox.width / double (image->get_width()), bbox.height / double (image->get_height()));
#endif
}

double HOCRPoDoFoPdfPrinter::getAverageCharWidth() const {
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
	return m_painter->GetFont()->GetFontMetrics()->CharWidth(static_cast<unsigned char> ('x'));
#else
	return m_painter->TextState.GetFont()->GetStringLength("x", m_painter->TextState);
#endif
}

double HOCRPoDoFoPdfPrinter::getTextWidth(const Glib::ustring& text) const {
#if PODOFO_VERSION < PODOFO_MAKE_VERSION(0, 10, 0)
	PoDoFo::PdfString pdfString(reinterpret_cast<const PoDoFo::pdf_utf8*> (text.c_str()));
	return m_painter->GetFont()->GetFontMetrics()->StringWidth(pdfString);
#else
	return m_painter->TextState.GetFont()->GetStringLength(text.raw(), m_painter->TextState);
#endif
}

PoDoFo::PdfFont* HOCRPoDoFoPdfPrinter::getFont(Glib::ustring family, bool bold, bool italic) {
	Glib::ustring key = family + (bold ? "@bold" : "") + (italic ? "@italic" : "");
	auto it = m_fontCache.find(key);
	if (it == m_fontCache.end()) {
		if (family.empty() || std::find(m_fontFamilies.begin(), m_fontFamilies.end(), family) == m_fontFamilies.end()) {
			family = m_defaultFontFamily;
		}
		PoDoFo::PdfFont* font = nullptr;
		try {
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
			PoDoFo::PdfFontSearchParams params;
			PoDoFo::PdfFontStyle style = PoDoFo::PdfFontStyle::Regular;
			if (bold) {
				style |= PoDoFo::PdfFontStyle::Bold;
			}
			if (italic) {
				style |= PoDoFo::PdfFontStyle::Italic;
			}
			params.Style = style;
			font = m_document->GetFonts().SearchFont(family.raw(), params);
#else
			font = m_document->CreateFontSubset(family.raw(), bold, italic, false, m_pdfFontEncoding);
#endif
			it = m_fontCache.insert(std::make_pair(key, font)).first;
		} catch (PoDoFo::PdfError& /*err*/) {
			it = m_fontCache.insert(std::make_pair(key, m_defaultFont)).first;
		}
	}
	return it->second;
}


HOCRPdfExportDialog::HOCRPdfExportDialog(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument, const HOCRPage* hocrpage, Gtk::Window* parent) {
	if (!parent) {
		parent = MAIN->getWindow();
	}
	set_transient_for(*parent);
	m_widget = new HOCRPdfExportWidget(displayerTool, hocrdocument, hocrpage);
	get_content_area()->pack_start(*m_widget->getWidget(), true, true);

	add_button(_("OK"), Gtk::RESPONSE_OK);
	add_button(_("Cancel"), Gtk::RESPONSE_CANCEL);
	show_all();
}

HOCRPdfExportDialog::~HOCRPdfExportDialog() {
	delete m_widget;
}
HOCRPdfExporter::PDFSettings HOCRPdfExportDialog::getPdfSettings() const {
	return m_widget->getPdfSettings();
}

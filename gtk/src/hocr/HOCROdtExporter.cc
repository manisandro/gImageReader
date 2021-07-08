/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCROdtExporter.cc
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

#include "ConfigSettings.hh"
#include "DisplayerToolHOCR.hh"
#include "HOCRDocument.hh"
#include "HOCROdtExporter.hh"
#include "MainWindow.hh"
#include "FileDialogs.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <cstring>
#include <iomanip>
#include <libxml++/libxml++.h>
#include <glib/guuid.h>
#include <zip.h>

static Glib::ustring manifestNS_URI("urn:oasis:names:tc:opendocument:xmlns:manifest:1.0");
static Glib::ustring manifestNS("manifest");
static Glib::ustring officeNS_URI("urn:oasis:names:tc:opendocument:xmlns:office:1.0");
static Glib::ustring officeNS("office");
static Glib::ustring textNS_URI ("urn:oasis:names:tc:opendocument:xmlns:text:1.0");
static Glib::ustring textNS("text");
static Glib::ustring styleNS_URI ("urn:oasis:names:tc:opendocument:xmlns:style:1.0");
static Glib::ustring styleNS("style");
static Glib::ustring foNS_URI ("urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0");
static Glib::ustring foNS("fo");
static Glib::ustring tableNS_URI ("urn:oasis:names:tc:opendocument:xmlns:table:1.0");
static Glib::ustring tableNS("table");
static Glib::ustring drawNS_URI ("urn:oasis:names:tc:opendocument:xmlns:drawing:1.0");
static Glib::ustring drawNS("draw");
static Glib::ustring xlinkNS_URI ("http://www.w3.org/1999/xlink");
static Glib::ustring xlinkNS("xlink");
static Glib::ustring svgNS_URI ("urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0");
static Glib::ustring svgNS("svg");

template<typename char_t>
static zip_source* zip_source_buffer_from_data(zip* fzip, const char_t* data, std::size_t len) {
	void* copy = std::malloc(len);
	std::memcpy(copy, data, len);
	return zip_source_buffer(fzip, copy, len, 1);
}

bool HOCROdtExporter::run(const Glib::RefPtr<HOCRDocument>& hocrdocument, const std::string& filebasename) {
	Glib::ustring suggestion = filebasename;
	if(suggestion.empty()) {
		std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.empty() ? Utils::split_filename(sources.front()->displayname).first : _("output");
	}

	FileDialogs::FileFilter filter = {_("OpenDocument Text Documents"), {"application/vnd.oasis.opendocument.text"}, {"*.odt"}};
	std::string outname = FileDialogs::save_dialog(_("Save ODT Output..."), suggestion + ".odt", "outputdir", filter);
	if(outname.empty()) {
		return false;
	}

	zip* fzip = zip_open(outname.c_str(), ZIP_CREATE | ZIP_TRUNCATE, nullptr);
	if(!fzip) {
		Utils::messageBox(Gtk::MESSAGE_WARNING, _("Export failed"), _("The ODT export failed: unable to write output file."));
		return false;
	}
	int pageCount = hocrdocument->pageCount();
	MainWindow::ProgressMonitor monitor(2 * pageCount);
	MAIN->showProgress(&monitor);
	bool success = Utils::busyTask([&] {
		// Image files
		if(zip_dir_add(fzip, "Pictures", ZIP_FL_ENC_UTF_8) < 0) {
			return false;
		}
		std::map<const HOCRItem*, Glib::ustring> imageFiles;
		for(int i = 0; i < pageCount; ++i) {
			monitor.increaseProgress();
			const HOCRPage* page = hocrdocument->page(i);
			if(!page->isEnabled()) {
				continue;
			}
			bool ok = false;
			Utils::runInMainThreadBlocking([&] { ok = setSource(page->sourceFile(), page->pageNr(), page->resolution(), page->angle()); });
			if(!ok) {
				continue;
			}
			for(const HOCRItem* item : page->children()) {
				writeImage(fzip, imageFiles, item);
			}
		}

		// Mimetype
		{
			Glib::ustring mimetype = "application/vnd.oasis.opendocument.text";
			zip_source* source = zip_source_buffer_from_data(fzip, mimetype.c_str(), mimetype.bytes());
			if(zip_file_add(fzip, "mimetype", source, ZIP_FL_ENC_UTF_8) < 0) {
				zip_source_free(source);
				return false;
			}
		}

		// Manifest
		{
			if(zip_dir_add(fzip, "META-INF", ZIP_FL_ENC_UTF_8) < 0) {
				return false;
			}
			xmlpp::Document doc;
			xmlpp::Element* manifestEl = doc.create_root_node("manifest", manifestNS_URI, manifestNS);
			manifestEl->set_attribute("version", "1.2", manifestNS);

#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* fileEntryEl = manifestEl->add_child("file-entry", manifestNS);
#else
			xmlpp::Element* fileEntryEl = manifestEl->add_child_element("file-entry", manifestNS);
#endif
			fileEntryEl->set_attribute("media-type", "application/vnd.oasis.opendocument.text", manifestNS);
			fileEntryEl->set_attribute("full-path", "/", manifestNS);

#ifdef HAVE_LIBXMLPP_2
			fileEntryEl = manifestEl->add_child("file-entry", manifestNS);
#else
			fileEntryEl = manifestEl->add_child_element("file-entry", manifestNS);
#endif
			fileEntryEl->set_attribute("media-type", "text/xml", manifestNS);
			fileEntryEl->set_attribute("full-path", "content.xml", manifestNS);

			for(auto it = imageFiles.begin(), itEnd = imageFiles.end(); it != itEnd; ++it) {
#ifdef HAVE_LIBXMLPP_2
				fileEntryEl = manifestEl->add_child("file-entry", manifestNS);
#else
				fileEntryEl = manifestEl->add_child_element("file-entry", manifestNS);
#endif
				fileEntryEl->set_attribute("media-type", "image/png", manifestNS);
				fileEntryEl->set_attribute("full-path", it->second, manifestNS);
			}
			Glib::ustring data = doc.write_to_string();
			zip_source* source = zip_source_buffer_from_data(fzip, data.c_str(), data.bytes());
			if(zip_file_add(fzip, "META-INF/manifest.xml", source, ZIP_FL_ENC_UTF_8) < 0) {
				zip_source_free(source);
				return false;
			}
		}

		// Content
		{
			xmlpp::Document doc;
			xmlpp::Element* documentContentEl = doc.create_root_node("document-content", officeNS_URI, officeNS);
			documentContentEl->set_namespace_declaration(textNS_URI, textNS);
			documentContentEl->set_namespace_declaration(styleNS_URI, styleNS);
			documentContentEl->set_namespace_declaration(foNS_URI, foNS);
			documentContentEl->set_namespace_declaration(drawNS_URI, drawNS);
			documentContentEl->set_namespace_declaration(xlinkNS_URI, xlinkNS);
			documentContentEl->set_namespace_declaration(svgNS_URI, svgNS);
			documentContentEl->set_attribute("version", "1.2", officeNS);

			// - Font face decls
#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* fontFaceDeclsEl = documentContentEl->add_child("font-face-decls", officeNS);
#else
			xmlpp::Element* fontFaceDeclsEl = documentContentEl->add_child_element("font-face-decls", officeNS);
#endif
			std::set<Glib::ustring> families;
			for(int i = 0; i < pageCount; ++i) {
				const HOCRPage* page = hocrdocument->page(i);
				if(page->isEnabled()) {
					writeFontFaceDecls(families, page, fontFaceDeclsEl);
				}
			}

			// - Page styles
#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* automaticStylesEl = documentContentEl->add_child("automatic-styles", officeNS);
#else
			xmlpp::Element* automaticStylesEl = documentContentEl->add_child_element("automatic-styles", officeNS);
#endif
			for(int i = 0; i < pageCount; ++i) {
				const HOCRPage* page = hocrdocument->page(i);
				if(page->isEnabled()) {
#ifdef HAVE_LIBXMLPP_2
					xmlpp::Element* pageLayoutEl = automaticStylesEl->add_child("page-layout", styleNS);
#else
					xmlpp::Element* pageLayoutEl = automaticStylesEl->add_child_element("page-layout", styleNS);
#endif
					pageLayoutEl->set_attribute("name", Glib::ustring::compose("PL%1", i), styleNS);
#ifdef HAVE_LIBXMLPP_2
					xmlpp::Element* pageLayoutPropertiesEl = pageLayoutEl->add_child("page-layout-properties", styleNS);
#else
					xmlpp::Element* pageLayoutPropertiesEl = pageLayoutEl->add_child_element("page-layout-properties", styleNS);
#endif
					pageLayoutPropertiesEl->set_attribute("page-width", Glib::ustring::compose("%1in", page->bbox().width / double(page->resolution())), foNS);
					pageLayoutPropertiesEl->set_attribute("page-height", Glib::ustring::compose("%1in", page->bbox().height / double(page->resolution())), foNS);
					pageLayoutPropertiesEl->set_attribute("print-orientation", page->bbox().width > page->bbox().height ? "landscape" : "portrait", styleNS);
					pageLayoutPropertiesEl->set_attribute("writing-mode", "lr-tb", styleNS);
					pageLayoutPropertiesEl->set_attribute("margin-top", "0in", foNS);
					pageLayoutPropertiesEl->set_attribute("margin-bottom", "0in", foNS);
					pageLayoutPropertiesEl->set_attribute("margin-left", "0in", foNS);
					pageLayoutPropertiesEl->set_attribute("margin-right", "0in", foNS);
				}
			}
#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* masterStylesEl = documentContentEl->add_child("master-styles", officeNS);
#else
			xmlpp::Element* masterStylesEl = documentContentEl->add_child_element("master-styles", officeNS);
#endif
			for(int i = 0; i < pageCount; ++i) {
				if(hocrdocument->page(i)->isEnabled()) {
#ifdef HAVE_LIBXMLPP_2
					xmlpp::Element* masterPageEl = masterStylesEl->add_child("master-page", styleNS);
#else
					xmlpp::Element* masterPageEl = masterStylesEl->add_child_element("master-page", styleNS);
#endif
					masterPageEl->set_attribute("name", Glib::ustring::compose("MP%1", i), styleNS);
					masterPageEl->set_attribute("page-layout-name", Glib::ustring::compose("PL%1", i), styleNS);
				}
			}

			// - Styles
#ifdef HAVE_LIBXMLPP_2
			automaticStylesEl = documentContentEl->add_child("automatic-styles", officeNS);
#else
			automaticStylesEl = documentContentEl->add_child_element("automatic-styles", officeNS);
#endif

			// -- Standard paragraph
#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* styleEl = automaticStylesEl->add_child("style", styleNS);
#else
			xmlpp::Element* styleEl = automaticStylesEl->add_child_element("style", styleNS);
#endif
			styleEl->set_attribute("name", "P", styleNS);
			styleEl->set_attribute("parent-style-name", "Standard", styleNS);

			// -- Page paragraphs
			for(int i = 0; i < pageCount; ++i) {
				if(hocrdocument->page(i)->isEnabled()) {
#ifdef HAVE_LIBXMLPP_2
					xmlpp::Element* styleEl = automaticStylesEl->add_child("style", styleNS);
#else
					xmlpp::Element* styleEl = automaticStylesEl->add_child_element("style", styleNS);
#endif
					styleEl->set_attribute("name", Glib::ustring::compose("PP%1", i), styleNS);
					styleEl->set_attribute("family", "paragraph", styleNS);
					styleEl->set_attribute("parent-style-name", "Standard", styleNS);
					styleEl->set_attribute("master-page-name", Glib::ustring::compose("MP%1", i), styleNS);
#ifdef HAVE_LIBXMLPP_2
					xmlpp::Element* paragraphPropertiesEl = styleEl->add_child("paragraph-properties", styleNS);
#else
					xmlpp::Element* paragraphPropertiesEl = styleEl->add_child_element("paragraph-properties", styleNS);
#endif
					paragraphPropertiesEl->set_attribute("break-before", "page", foNS);
				}
			}

			// -- Frame, absolutely positioned on page
#ifdef HAVE_LIBXMLPP_2
			styleEl = automaticStylesEl->add_child("style", styleNS);
#else
			styleEl = automaticStylesEl->add_child_element("style", styleNS);
#endif
			styleEl->set_attribute("name", "F", styleNS);
			styleEl->set_attribute("family", "graphic", styleNS);
#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* graphicPropertiesEl = styleEl->add_child("graphic-properties", styleNS);
#else
			xmlpp::Element* graphicPropertiesEl = styleEl->add_child_element("graphic-properties", styleNS);
#endif
			graphicPropertiesEl->set_attribute("stroke", "none", drawNS);
			graphicPropertiesEl->set_attribute("fill", "none", drawNS);
			graphicPropertiesEl->set_attribute("vertical-pos", "from-top", styleNS);
			graphicPropertiesEl->set_attribute("vertical-rel", "page", styleNS);
			graphicPropertiesEl->set_attribute("horizontal-pos", "from-left", styleNS);
			graphicPropertiesEl->set_attribute("horizontal-rel", "page", styleNS);
			graphicPropertiesEl->set_attribute("flow-with-text", "false", styleNS);

			// -- Text styles
			int counter = 0;
			std::map<Glib::ustring, std::map<double, Glib::ustring>> fontStyles;
			for(int i = 0; i < pageCount; ++i) {
				const HOCRPage* page = hocrdocument->page(i);
				if(page->isEnabled()) {
					for(const HOCRItem* item : page->children()) {
						writeFontStyles(fontStyles, item, automaticStylesEl, counter);
					}
				}
			}

			// - Body
#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* bodyEl = documentContentEl->add_child("body", officeNS);
			xmlpp::Element* textEl = bodyEl->add_child("text", officeNS);
#else
			xmlpp::Element* bodyEl = documentContentEl->add_child_element("body", officeNS);
			xmlpp::Element* textEl = bodyEl->add_child_element("text", officeNS);
#endif

			int pageCounter = 1;
			for(int i = 0; i < pageCount; ++i) {
				monitor.increaseProgress();
				const HOCRPage* page = hocrdocument->page(i);
				if(!page->isEnabled()) {
					continue;
				}
#ifdef HAVE_LIBXMLPP_2
				xmlpp::Element* pEl = textEl->add_child("p", textNS);
#else
				xmlpp::Element* pEl = textEl->add_child_element("p", textNS);
#endif
				pEl->set_attribute("style-name", Glib::ustring::compose("PP%1", i), textNS);
				for(const HOCRItem* item : page->children()) {
					printItem(pEl, item, pageCounter, page->resolution(), fontStyles, imageFiles);
				}
				++pageCounter;
			}

			Glib::ustring data = doc.write_to_string();
			zip_source* source = zip_source_buffer_from_data(fzip, data.c_str(), data.bytes());
			if(zip_file_add(fzip, "content.xml", source, ZIP_FL_ENC_UTF_8) < 0) {
				zip_source_free(source);
				return false;
			}
		}
		return true;
	}, _("Exporting to ODT..."));
	MAIN->hideProgress();

	zip_close(fzip);
	bool openAfterExport = ConfigSettings::get<SwitchSettingT<Gtk::CheckButton>>("openafterexport")->getValue();
	if(!success) {
		Utils::messageBox(Gtk::MESSAGE_WARNING, _("Export failed"), _("The ODT export failed: unable to write output file."));
	} else if(openAfterExport) {
		Utils::openUri(Glib::filename_to_uri(outname));
	}
	return success;
}

void HOCROdtExporter::writeImage(zip* fzip, std::map<const HOCRItem*, Glib::ustring>& images, const HOCRItem* item) {
	if(!item->isEnabled()) {
		return;
	}
	if(item->itemClass() == "ocr_graphic") {
		Cairo::RefPtr<Cairo::ImageSurface> selection;
		Utils::runInMainThreadBlocking([&] { selection = getSelection(item->bbox()); });

		gchar* guuid = g_uuid_string_random();
		Glib::ustring uuid(guuid);
		g_free(guuid);
		uuid = uuid.substr(1, uuid.length() - 2); // Remove {}

		Glib::ustring filename = Glib::ustring::compose("Pictures/%1.png", uuid);

		Glib::RefPtr<Glib::ByteArray> pngbytes = Glib::ByteArray::create();
		selection->write_to_png_stream([&](const unsigned char* data, unsigned int length)->Cairo::ErrorStatus {
			pngbytes->append(data, length);
			return CAIRO_STATUS_SUCCESS;
		});

		zip_source* source = zip_source_buffer_from_data(fzip, pngbytes->get_data(), pngbytes->size());
		if(zip_file_add(fzip, filename.c_str(), source, ZIP_FL_ENC_UTF_8) < 0) {
			zip_source_free(source);
			return;
		}
		images.insert(std::make_pair(item, filename));
	}
}

void HOCROdtExporter::writeFontFaceDecls(std::set<Glib::ustring>& families, const HOCRItem* item, xmlpp::Element* parentEl) {
	if(!item->isEnabled()) {
		return;
	}
	if(item->itemClass() == "ocrx_word") {
		Glib::ustring fontFamily = item->fontFamily();
		if(families.find(fontFamily) == families.end()) {
			if(!fontFamily.empty()) {
#ifdef HAVE_LIBXMLPP_2
				xmlpp::Element* fontFaceEl = parentEl->add_child("font-face", styleNS);
#else
				xmlpp::Element* fontFaceEl = parentEl->add_child_element("font-face", styleNS);
#endif
				fontFaceEl->set_attribute("name", fontFamily, styleNS);
				fontFaceEl->set_attribute("font-family", fontFamily, svgNS);
			}
			families.insert(fontFamily);
		}
	} else {
		for(const HOCRItem* child : item->children()) {
			writeFontFaceDecls(families, child, parentEl);
		}
	}
}

void HOCROdtExporter::writeFontStyles(std::map<Glib::ustring, std::map<double, Glib::ustring>>& styles, const HOCRItem* item, xmlpp::Element* parentEl, int& counter) {
	if(!item->isEnabled()) {
		return;
	}
	if(item->itemClass() == "ocrx_word") {
		Glib::ustring fontKey = item->fontFamily() + (item->fontBold() ? "@bold" : "") + (item->fontItalic() ? "@italic" : "");
		if(styles.find(fontKey) == styles.end() || styles[fontKey].find(item->fontSize()) == styles[fontKey].end()) {
			Glib::ustring styleName = Glib::ustring::compose("T%1", ++counter);
#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* styleEl = parentEl->add_child("style", styleNS);
#else
			xmlpp::Element* styleEl = parentEl->add_child_element("style", styleNS);
#endif
			styleEl->set_attribute("name", styleName, styleNS);
			styleEl->set_attribute("family", "text", styleNS);
#ifdef HAVE_LIBXMLPP_2
			xmlpp::Element* textPropertiesEl = styleEl->add_child("text-properties", styleNS);
#else
			xmlpp::Element* textPropertiesEl = styleEl->add_child_element("text-properties", styleNS);
#endif
			textPropertiesEl->set_attribute("font-name", item->fontFamily(), styleNS);
			textPropertiesEl->set_attribute("font-size", Glib::ustring::compose("%1pt", item->fontSize()), foNS);
			if(item->fontBold()) {
				textPropertiesEl->set_attribute("font-weight", "bold", foNS);
			}
			if(item->fontItalic()) {
				textPropertiesEl->set_attribute("font-style", "italic", foNS);
			}

			styles[fontKey].insert(std::make_pair(item->fontSize(), styleName));
		}
	} else {
		for(const HOCRItem* child : item->children()) {
			writeFontStyles(styles, child, parentEl, counter);
		}
	}
}

void HOCROdtExporter::printItem(xmlpp::Element* parentEl, const HOCRItem* item, int pageNr, int dpi, std::map<Glib::ustring, std::map<double, Glib::ustring>>& fontStyleNames, std::map<const HOCRItem*, Glib::ustring>& images) {
	if(!item->isEnabled()) {
		return;
	}
	Glib::ustring itemClass = item->itemClass();
	const Geometry::Rectangle& bbox = item->bbox();
	if(itemClass == "ocr_graphic") {
#ifdef HAVE_LIBXMLPP_2
		xmlpp::Element* frameEl = parentEl->add_child("frame", drawNS);
#else
		xmlpp::Element* frameEl = parentEl->add_child_element("frame", drawNS);
#endif
		frameEl->set_attribute("style-name", "F", drawNS);
		frameEl->set_attribute("anchor-type", "page", textNS);
		frameEl->set_attribute("anchor-page-number", Glib::ustring::format(std::fixed, std::setprecision(0), pageNr), textNS);
		frameEl->set_attribute("x", Glib::ustring::compose("%1in", bbox.x / double(dpi)), svgNS);
		frameEl->set_attribute("y", Glib::ustring::compose("%1in", bbox.y / double(dpi)), svgNS);
		frameEl->set_attribute("width", Glib::ustring::compose("%1in", bbox.width / double(dpi)), svgNS);
		frameEl->set_attribute("height", Glib::ustring::compose("%1in", bbox.height / double(dpi)), svgNS);
		frameEl->set_attribute("z-index", "0", drawNS);

#ifdef HAVE_LIBXMLPP_2
		xmlpp::Element* imageEl = frameEl->add_child("image", drawNS);
#else
		xmlpp::Element* imageEl = frameEl->add_child_element("image", drawNS);
#endif
		imageEl->set_attribute("href", images[item], xlinkNS);
		imageEl->set_attribute("type", "simple", xlinkNS);
		imageEl->set_attribute("show", "embed", xlinkNS);
	} else if(itemClass == "ocr_par") {
#ifdef HAVE_LIBXMLPP_2
		xmlpp::Element* frameEl = parentEl->add_child("frame", drawNS);
#else
		xmlpp::Element* frameEl = parentEl->add_child_element("frame", drawNS);
#endif
		frameEl->set_attribute("style-name", "F", drawNS);
		frameEl->set_attribute("anchor-type", "page", textNS);
		frameEl->set_attribute("anchor-page-number", Glib::ustring::format(std::fixed, std::setprecision(0), pageNr), textNS);
		frameEl->set_attribute("x", Glib::ustring::compose("%1in", bbox.x / double(dpi)), svgNS);
		frameEl->set_attribute("y", Glib::ustring::compose("%1in", bbox.y / double(dpi)), svgNS);
		frameEl->set_attribute("width", Glib::ustring::compose("%1in", bbox.width / double(dpi)), svgNS);
		frameEl->set_attribute("height", Glib::ustring::compose("%1in", bbox.height / double(dpi)), svgNS);
		frameEl->set_attribute("z-index", "1", drawNS);

#ifdef HAVE_LIBXMLPP_2
		xmlpp::Element* textBoxEl = frameEl->add_child("text-box", drawNS);
		xmlpp::Element* ppEl = textBoxEl->add_child("p", textNS);
#else
		xmlpp::Element* textBoxEl = frameEl->add_child_element("text-box", drawNS);
		xmlpp::Element* ppEl = textBoxEl->add_child_element("p", textNS);
#endif

		ppEl->set_attribute("style-name", "P", textNS);
		for(const HOCRItem* child : item->children()) {
			printItem(ppEl, child, pageNr, dpi, fontStyleNames, images);
		}
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
		Glib::ustring fontKey = firstWord->fontFamily() + (firstWord->fontBold() ? "@bold" : "") + (firstWord->fontItalic() ? "@italic" : "");
		Glib::ustring currentFontStyleName = fontStyleNames[fontKey][firstWord->fontSize()];
#ifdef HAVE_LIBXMLPP_2
		xmlpp::Element* spanEl = parentEl->add_child("span", textNS);
#else
		xmlpp::Element* spanEl = parentEl->add_child_element("span", textNS);
#endif
		spanEl->set_attribute("style-name", currentFontStyleName, textNS);
		spanEl->add_child_text(firstWord->text());
		++iChild;
		for(; iChild < nChilds; ++iChild) {
			const HOCRItem* child = item->children()[iChild];
			if(!child->isEnabled()) {
				continue;
			}
			fontKey = child->fontFamily() + (child->fontBold() ? "@bold" : "") + (child->fontItalic() ? "@italic" : "");
			Glib::ustring fontStyleName = fontStyleNames[fontKey][child->fontSize()];
			if(fontStyleName != currentFontStyleName) {
				currentFontStyleName = fontStyleName;
#ifdef HAVE_LIBXMLPP_2
				spanEl = parentEl->add_child("span", textNS);
#else
				spanEl = parentEl->add_child_element("span", textNS);
#endif
				spanEl->set_attribute("style-name", currentFontStyleName, textNS);
			}
			spanEl->add_child_text(" ");
			spanEl->add_child_text(child->text());
		}
#ifdef HAVE_LIBXMLPP_2
		parentEl->add_child("line-break", textNS);
#else
		parentEl->add_child_element("line-break", textNS);
#endif
	} else {
		for(const HOCRItem* child : item->children()) {
			printItem(parentEl, child, pageNr, dpi, fontStyleNames, images);
		}
	}
}

bool HOCROdtExporter::setSource(const Glib::ustring& sourceFile, int page, int dpi, double angle) {
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(sourceFile);
	if(MAIN->getSourceManager()->addSource(file, true)) {
		MAIN->getDisplayer()->setup(&page, &dpi, &angle);
		return true;
	} else {
		return false;
	}
}

Cairo::RefPtr<Cairo::ImageSurface> HOCROdtExporter::getSelection(const Geometry::Rectangle& bbox) {
	return m_displayerTool->getSelection(bbox);
}

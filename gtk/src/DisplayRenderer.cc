/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayRenderer.cc
 * Copyright (C) (\d+)-2018 Sandro Mani <manisandro@gmail.com>
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

#include "DjVuDocument.hh"
#include "DisplayRenderer.hh"
#include "Utils.hh"

#include <poppler-document.h>
#include <poppler-page.h>

void DisplayRenderer::adjustImage(const Cairo::RefPtr<Cairo::ImageSurface>& surf, int brightness, int contrast, bool invert) const {
	if(brightness == 0 && contrast == 0 && !invert) {
		return;
	}

	float kBr = 1.f - std::abs(brightness / 200.f);
	float dBr = brightness > 0 ? 255.f : 0.f;

	float kCn = contrast * 2.55f;
	// http://thecryptmag.com/Online/56/imgproc_5.html
	float FCn = (259.f * (kCn + 255.f)) / (255.f * (259.f - kCn));

	int n = surf->get_height() * surf->get_width();
	uint8_t* data = surf->get_data();
	#pragma omp parallel for schedule(static)
	for(int i = 0; i < n; ++i) {
		uint8_t& r = data[4 * i + 2];
		uint8_t& g = data[4 * i + 1];
		uint8_t& b = data[4 * i + 0];
		// Brightness
		r = dBr * (1.f - kBr) + r * kBr;
		g = dBr * (1.f - kBr) + g * kBr;
		b = dBr * (1.f - kBr) + b * kBr;
		// Contrast
		r = std::max(0.f, std::min(FCn * (r - 128.f) + 128.f, 255.f));
		g = std::max(0.f, std::min(FCn * (g - 128.f) + 128.f, 255.f));
		b = std::max(0.f, std::min(FCn * (b - 128.f) + 128.f, 255.f));
		// Invert
		if(invert) {
			r = 255 - r;
			g = 255 - g;
			b = 255 - b;
		}
	}
}

Cairo::RefPtr<Cairo::ImageSurface> ImageRenderer::render(int /*page*/, double resolution) const {
	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
	try {
		pixbuf = Gdk::Pixbuf::create_from_file(m_filename);
	} catch(const Glib::Error&) {
		return Cairo::RefPtr<Cairo::ImageSurface>();
	}

	double scale = resolution / 100.;
	int w = Utils::round(pixbuf->get_width() * scale);
	int h = Utils::round(pixbuf->get_height() * scale);
	Cairo::RefPtr<Cairo::ImageSurface> surf;
	try {
		surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w, h);
	} catch(const std::exception&) {
		return Cairo::RefPtr<Cairo::ImageSurface>();
	}
	Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surf);
	if(pixbuf->get_has_alpha()) {
		ctx->set_source_rgba(1., 1., 1., 1.);
		ctx->paint();
	}
	ctx->scale(scale, scale);
	try {
		Gdk::Cairo::set_source_pixbuf(ctx, pixbuf);
	} catch(const std::exception&) {
		return Cairo::RefPtr<Cairo::ImageSurface>();
	}

	ctx->paint();
	return surf;
}

PDFRenderer::PDFRenderer(const std::string& filename, const Glib::ustring& password) : DisplayRenderer(filename) {
	m_document = poppler_document_new_from_file(Glib::filename_to_uri(m_filename).c_str(), password.c_str(), 0);
}

PDFRenderer::~PDFRenderer() {
	if(m_document) {
		g_object_unref(m_document);
	}
}

Cairo::RefPtr<Cairo::ImageSurface> PDFRenderer::render(int page, double resolution) const {
	if(!m_document) {
		return Cairo::RefPtr<Cairo::ImageSurface>();
	}
	m_mutex.lock();
	double scale = resolution / 72;
	PopplerPage* poppage = poppler_document_get_page(m_document, page - 1);
	double width, height;
	poppler_page_get_size(poppage, &width, &height);
	int w = Utils::round(width * scale);
	int h = Utils::round(height * scale);
	Cairo::RefPtr<Cairo::ImageSurface> surf;
	try {
		surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w, h);
	} catch(const std::exception&) {
		m_mutex.unlock();
		return Cairo::RefPtr<Cairo::ImageSurface>();
	}
	Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surf);
	ctx->set_source_rgba(1., 1., 1., 1.);
	ctx->paint();
	ctx->scale(scale, scale);
	poppler_page_render(poppage, ctx->cobj());
	g_object_unref(poppage);
	m_mutex.unlock();
	return surf;
}

int PDFRenderer::getNPages() const {
	return m_document ? poppler_document_get_n_pages(m_document) : 1;
}


DJVURenderer::DJVURenderer(const std::string& filename) : DisplayRenderer(filename) {
	m_djvu = new DjVuDocument();
	m_djvu->openFile(filename);
}

DJVURenderer::~DJVURenderer() {
	delete m_djvu;
}

Cairo::RefPtr<Cairo::ImageSurface> DJVURenderer::render(int page, double resolution) const {
	return m_djvu->image(page, resolution);
}

int DJVURenderer::getNPages() const {
	return m_djvu->pageCount();
}

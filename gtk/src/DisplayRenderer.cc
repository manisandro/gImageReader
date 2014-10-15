/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayRenderer.cc
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#include "DisplayRenderer.hh"
#include "Utils.hh"

#include <poppler-document.h>
#include <poppler-page.h>

Cairo::RefPtr<Cairo::ImageSurface> ImageRenderer::render(int page, double resolution) const
{
	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
	try{
		pixbuf = Gdk::Pixbuf::create_from_file(m_filename);
	}catch(const Glib::Error&){
		return Cairo::RefPtr<Cairo::ImageSurface>();
	}

	double scale = resolution / 100.;
	int w = Utils::round(pixbuf->get_width() * scale);
	int h = Utils::round(pixbuf->get_height() * scale);
	Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w, h);
	Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surf);
	if(pixbuf->get_has_alpha()){
		ctx->set_source_rgba(1., 1., 1., 1.);
		ctx->paint();
	}
	ctx->scale(scale, scale);
	Gdk::Cairo::set_source_pixbuf(ctx, pixbuf);
	ctx->paint();
	return surf;
}

PDFRenderer::PDFRenderer(const std::string& filename) : DisplayRenderer(filename)
{
	m_document = poppler_document_new_from_file(Glib::filename_to_uri(m_filename).c_str(), 0, 0);
}

PDFRenderer::~PDFRenderer()
{
	g_object_unref(m_document);
}

Cairo::RefPtr<Cairo::ImageSurface> PDFRenderer::render(int page, double resolution) const
{
	if(!m_document){
		return Cairo::RefPtr<Cairo::ImageSurface>();
	}
	m_mutex.lock();
	double scale = resolution / 72;
	PopplerPage* poppage = poppler_document_get_page(m_document, page - 1);
	double width, height;
	poppler_page_get_size(poppage, &width, &height);
	int w = Utils::round(width * scale);
	int h = Utils::round(height * scale);
	Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w, h);
	Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surf);
	ctx->set_source_rgba(1., 1., 1., 1.);
	ctx->paint();
	ctx->scale(scale, scale);
	poppler_page_render(poppage, ctx->cobj());
	g_object_unref(poppage);
	m_mutex.unlock();
	return surf;
}

int PDFRenderer::getNPages() const
{
	return poppler_document_get_n_pages(m_document);
}

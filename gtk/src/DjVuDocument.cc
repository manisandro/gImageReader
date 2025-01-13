/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DjVuDocument.cc
 * Based on code from Okular, which is:
 *   Copyright (C) 2006 by Pino Toscano <toscano.pino@tiscali.it>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "DjVuDocument.hh"

#include <cairomm/context.h>
#include <libdjvu/ddjvuapi.h>
#include <libdjvu/miniexp.h>

/**
 * Explore the message queue until there are no message left in it.
 */
static void handle_ddjvu_messages(ddjvu_context_t* ctx, int wait) {
	const ddjvu_message_t* msg;
	if (wait) {
		ddjvu_message_wait(ctx);
	}
	while ((msg = ddjvu_message_peek(ctx))) {
		ddjvu_message_pop(ctx);
	}
}

/**
 * Explore the message queue until the message \p mid is found.
 */
static void wait_for_ddjvu_message(ddjvu_context_t* ctx, ddjvu_message_tag_t mid) {
	ddjvu_message_wait(ctx);
	const ddjvu_message_t* msg;
	while ((msg = ddjvu_message_peek(ctx)) && msg && (msg->m_any.tag != mid)) {
		ddjvu_message_pop(ctx);
	}
}


DjVuDocument::DjVuDocument() {
	// creating the djvu context
	m_djvu_cxt = ddjvu_context_create("DjVuDocument");
	// creating the rendering format
	unsigned int formatmask[4] = { 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 }; // R, G, B, A masks
	m_format = ddjvu_format_create(DDJVU_FORMAT_RGBMASK32, 4, formatmask);
	ddjvu_format_set_row_order(m_format, 1);
	ddjvu_format_set_y_direction(m_format, 1);
}

DjVuDocument::~DjVuDocument() {
	closeFile();
	ddjvu_format_release(m_format);
	ddjvu_context_release(m_djvu_cxt);
}

bool DjVuDocument::openFile(const std::string& fileName) {
	// first, close the old file
	if (m_djvu_document) {
		closeFile();
	}

	// load the document...
	m_djvu_document = ddjvu_document_create_by_filename(m_djvu_cxt, fileName.c_str(), true);
	if (!m_djvu_document) { return false; }
	// ...and wait for its loading
	wait_for_ddjvu_message(m_djvu_cxt, DDJVU_DOCINFO);
	if (ddjvu_document_decoding_error(m_djvu_document)) {
		ddjvu_document_release(m_djvu_document);
		m_djvu_document = nullptr;
		return false;
	}

	int numofpages = ddjvu_document_get_pagenum(m_djvu_document);
	m_pages.clear();
	m_pages.resize(numofpages);

	// read the pages
	for (int i = 0; i < numofpages; ++i) {
		ddjvu_status_t sts;
		ddjvu_pageinfo_t info;
		while ((sts = ddjvu_document_get_pageinfo(m_djvu_document, i, &info)) < DDJVU_JOB_OK) {
			handle_ddjvu_messages(m_djvu_cxt, true);
		}
		if (sts >= DDJVU_JOB_FAILED) {
			closeFile();
			return false;
		}
		m_pages[i] = {info.width, info.height, info.dpi};
	}

	return true;
}

void DjVuDocument::closeFile() {
	m_pages.clear();
	// releasing the old document
	if (m_djvu_document) {
		ddjvu_document_release(m_djvu_document);
	}
	m_djvu_document = nullptr;
}

Cairo::RefPtr<Cairo::ImageSurface> DjVuDocument::image(int pageno, int resolution) {
	if (pageno < 0 || pageno >= pageCount()) {
		return Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, 0, 0);
	}

	const DjVuDocument::Page& page = m_pages[pageno];

	ddjvu_page_t* djvupage = ddjvu_page_create_by_pageno(m_djvu_document, pageno);
	// wait for the new page to be loaded
	ddjvu_status_t sts;
	while ((sts = ddjvu_page_decoding_status(djvupage)) < DDJVU_JOB_OK) {
		handle_ddjvu_messages(m_djvu_cxt, true);
	}

	double scaleFactor = double (resolution) / double (page.dpi);
	ddjvu_rect_t pagerect;
	pagerect.x = 0;
	pagerect.y = 0;
	pagerect.w = page.width * scaleFactor;
	pagerect.h = page.height * scaleFactor;
	ddjvu_rect_t renderrect = pagerect;
	Cairo::RefPtr<Cairo::ImageSurface> res_img = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, renderrect.w, renderrect.h);
	int res = ddjvu_page_render(djvupage, DDJVU_RENDER_COLOR, &pagerect, &renderrect, m_format, res_img->get_stride(), (char*) res_img->get_data());
	if (!res) {
		Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(res_img);
		ctx->set_source_rgba(1., 1., 1., 0.);
		ctx->paint();
	}
	res_img->mark_dirty();

	ddjvu_page_release(djvupage);
	return res_img;
}

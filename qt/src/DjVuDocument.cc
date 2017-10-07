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

#include <QFile>
#include <QPainter>

#include <libdjvu/ddjvuapi.h>
#include <libdjvu/miniexp.h>

/**
 * Explore the message queue until there are message left in it.
 */
static void handle_ddjvu_messages( ddjvu_context_t *ctx, int wait )
{
	const ddjvu_message_t *msg;
	if ( wait )
		ddjvu_message_wait( ctx );
	while ( ( msg = ddjvu_message_peek( ctx ) ) )
	{
		ddjvu_message_pop( ctx );
	}
}

/**
 * Explore the message queue until the message \p mid is found.
 */
static void wait_for_ddjvu_message( ddjvu_context_t *ctx, ddjvu_message_tag_t mid )
{
	ddjvu_message_wait( ctx );
	const ddjvu_message_t *msg;
	while ( ( msg = ddjvu_message_peek( ctx ) ) && msg && ( msg->m_any.tag != mid ) )
	{
		ddjvu_message_pop( ctx );
	}
}

class DjVuDocument::Private
{
	public:
		QImage generateImageTile( ddjvu_page_t *djvupage, int& res,
			int width, int row, int xdelta, int height, int col, int ydelta );

		ddjvu_context_t *m_djvu_cxt = nullptr;
		ddjvu_document_t *m_djvu_document = nullptr;
		ddjvu_format_t *m_format = nullptr;

		QVector<DjVuDocument::Page*> m_pages;
		QVector<ddjvu_page_t *> m_pages_cache;

		static unsigned int s_formatmask[4];
};

unsigned int DjVuDocument::Private::s_formatmask[4] = { 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

QImage DjVuDocument::Private::generateImageTile( ddjvu_page_t *djvupage, int& res,
	int width, int row, int xdelta, int height, int col, int ydelta )
{
	ddjvu_rect_t renderrect;
	renderrect.x = row * xdelta;
	renderrect.y = col * ydelta;
	int realwidth = std::min( width - renderrect.x, xdelta );
	int realheight = std::min( height - renderrect.y, ydelta );
	renderrect.w = realwidth;
	renderrect.h = realheight;
	ddjvu_rect_t pagerect;
	pagerect.x = 0;
	pagerect.y = 0;
	pagerect.w = width;
	pagerect.h = height;
	handle_ddjvu_messages( m_djvu_cxt, false );
	QImage res_img( realwidth, realheight, QImage::Format_RGB32 );
	// the following line workarounds a rare crash in djvulibre;
	// it should be fixed with >= 3.5.21
	ddjvu_page_get_width( djvupage );
	res = ddjvu_page_render( djvupage, DDJVU_RENDER_COLOR,
				  &pagerect, &renderrect, m_format, res_img.bytesPerLine(), (char *)res_img.bits() );
	if (!res)
	{
		res_img.fill(Qt::white);
	}
	handle_ddjvu_messages( m_djvu_cxt, false );

	return res_img;
}

DjVuDocument::DjVuDocument() : d( new Private )
{
	// creating the djvu context
	d->m_djvu_cxt = ddjvu_context_create( "DjVuDocument" );
	// creating the rendering format
#if DDJVUAPI_VERSION >= 18
	d->m_format = ddjvu_format_create( DDJVU_FORMAT_RGBMASK32, 4, Private::s_formatmask );
#else
	d->m_format = ddjvu_format_create( DDJVU_FORMAT_RGBMASK32, 3, Private::s_formatmask );
#endif
	ddjvu_format_set_row_order( d->m_format, 1 );
	ddjvu_format_set_y_direction( d->m_format, 1 );
}

DjVuDocument::~DjVuDocument()
{
	closeFile();

	ddjvu_format_release( d->m_format );
	ddjvu_context_release( d->m_djvu_cxt );

	delete d;
}

bool DjVuDocument::openFile( const QString & fileName )
{
	// first, close the old file
	if ( d->m_djvu_document )
		closeFile();

	// load the document...
	d->m_djvu_document = ddjvu_document_create_by_filename( d->m_djvu_cxt, QFile::encodeName( fileName ).constData(), true );
	if ( !d->m_djvu_document ) return false;
	// ...and wait for its loading
	wait_for_ddjvu_message( d->m_djvu_cxt, DDJVU_DOCINFO );
	if ( ddjvu_document_decoding_error( d->m_djvu_document ) )
	{
		ddjvu_document_release( d->m_djvu_document );
		d->m_djvu_document = nullptr;
		return false;
	}

	int numofpages = ddjvu_document_get_pagenum( d->m_djvu_document );
	d->m_pages.clear();
	d->m_pages.resize( numofpages );
	d->m_pages_cache.clear();
	d->m_pages_cache.resize( numofpages );

	// read the pages
	for ( int i = 0; i < numofpages; ++i )
	{
		ddjvu_status_t sts;
		ddjvu_pageinfo_t info;
		while ( ( sts = ddjvu_document_get_pageinfo( d->m_djvu_document, i, &info ) ) < DDJVU_JOB_OK )
			handle_ddjvu_messages( d->m_djvu_cxt, true );
		if ( sts >= DDJVU_JOB_FAILED )
		{
			return false;
		}

		DjVuDocument::Page *p = new DjVuDocument::Page();
		p->m_width = info.width;
		p->m_height = info.height;
		p->m_dpi = info.dpi;
		d->m_pages[i] = p;
	}

	return true;
}

void DjVuDocument::closeFile()
{
	// deleting the pages
	qDeleteAll( d->m_pages );
	d->m_pages.clear();
	// releasing the djvu pages
	for (ddjvu_page_t *page : d->m_pages_cache )
		ddjvu_page_release( page );
	d->m_pages_cache.clear();
	// releasing the old document
	if ( d->m_djvu_document )
		ddjvu_document_release( d->m_djvu_document );
	d->m_djvu_document = nullptr;
}

const QVector<DjVuDocument::Page*> &DjVuDocument::pages() const
{
	return d->m_pages;
}

QImage DjVuDocument::image( int page, int width, int height )
{
	if ( !d->m_pages_cache.at( page ) )
	{
		ddjvu_page_t *newpage = ddjvu_page_create_by_pageno( d->m_djvu_document, page );
		// wait for the new page to be loaded
		ddjvu_status_t sts;
		while ( ( sts = ddjvu_page_decoding_status( newpage ) ) < DDJVU_JOB_OK )
			handle_ddjvu_messages( d->m_djvu_cxt, true );
		d->m_pages_cache[page] = newpage;
	}
	ddjvu_page_t *djvupage = d->m_pages_cache[page];

	static const int xdelta = 1500;
	static const int ydelta = 1500;

	int xparts = width / xdelta + 1;
	int yparts = height / ydelta + 1;

	QImage newimg;

	int res = 10000;
	if ( ( xparts == 1 ) && ( yparts == 1 ) )
	{
		 // only one part -- render at once with no need to auxiliary image
		 newimg = d->generateImageTile( djvupage, res, width, 0, xdelta, height, 0, ydelta );
	}
	else
	{
		// more than one part -- need to render piece-by-piece and to compose
		// the results
		newimg = QImage( width, height, QImage::Format_RGB32 );
		QPainter p;
		p.begin( &newimg );
		int parts = xparts * yparts;
		for ( int i = 0; i < parts; ++i )
		{
			int row = i % xparts;
			int col = i / xparts;
			int tmpres = 0;
			QImage tempp = d->generateImageTile( djvupage, tmpres, width, row, xdelta, height, col, ydelta );
			if ( tmpres )
			{
				p.drawImage( row * xdelta, col * ydelta, tempp );
			}
			res = std::min( tmpres, res );
		}
		p.end();
	}

	return newimg;
}

int DjVuDocument::pageCount() const
{
	if ( !d->m_djvu_document )
	{
		return 1;
	}
	return ddjvu_document_get_pagenum(d->m_djvu_document);
}

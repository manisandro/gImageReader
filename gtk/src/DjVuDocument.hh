/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DjVuDocument.hh
 * Based on code from Okular, which is:
 *   Copyright (C) 2006 by Pino Toscano <toscano.pino@tiscali.it>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#ifndef DJVUDOCUMENT_HH
#define DJVUDOCUMENT_HH

#include <cairomm/surface.h>
#include <vector>

typedef struct ddjvu_context_s    ddjvu_context_t;
typedef struct ddjvu_document_s   ddjvu_document_t;
typedef struct ddjvu_format_s     ddjvu_format_t;

class DjVuDocument {
public:
	DjVuDocument();
	~DjVuDocument();

	struct Page {
		int width;
		int height;
		int dpi;
	};

	bool openFile( const std::string& fileName );
	void closeFile();
	Cairo::RefPtr<Cairo::ImageSurface> image(int pageno, int resolution);
	int pageCount() const {
		return m_pages.size();
	}
	const Page& page(int pageno) const { return m_pages[pageno]; }

private:
	ddjvu_context_t* m_djvu_cxt = nullptr;
	ddjvu_document_t* m_djvu_document = nullptr;
	ddjvu_format_t* m_format = nullptr;
	std::vector<Page> m_pages;
};

#endif

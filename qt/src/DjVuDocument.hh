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

#include <QImage>
#include <QVector>

class DjVuDocument
{
public:
	DjVuDocument();
	~DjVuDocument();

	class Page
	{
		friend class DjVuDocument;

	public:
		int width() const{ return m_width; }
		int height() const{ return m_height; }
		int dpi() const{ return m_dpi; }

	private:
		Page() = default;

		int m_width;
		int m_height;
		int m_dpi;
	};

	bool openFile( const QString & fileName );
	void closeFile();
	const QVector<DjVuDocument::Page*> &pages() const;
	QImage image( int page, int width, int height );
	int pageCount() const;

private:
	class Private;
	Private * const d;
};

#endif

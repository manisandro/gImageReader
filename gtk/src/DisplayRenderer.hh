/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayRenderer.hh
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

#ifndef DISPLAYRENDERER_HH
#define DISPLAYRENDERER_HH

#include "common.hh"

typedef struct _PopplerDocument PopplerDocument;

class DisplayRenderer {
public:
	DisplayRenderer(const std::string& filename) : m_filename(filename) {}
	virtual ~DisplayRenderer(){}
	virtual Cairo::RefPtr<Cairo::ImageSurface> render(int page, double resolution) const = 0;
	virtual int getNPages() const = 0;

	void adjustImage(const Cairo::RefPtr<Cairo::ImageSurface>& surf, int brightness, int contrast, bool invert) const;

protected:
	std::string m_filename;
};

class ImageRenderer : public DisplayRenderer {
public:
	ImageRenderer(const std::string& filename) : DisplayRenderer(filename) {}
	Cairo::RefPtr<Cairo::ImageSurface> render(int page, double resolution) const;
	int getNPages() const{ return 1; }
};

class PDFRenderer : public DisplayRenderer {
public:
	PDFRenderer(const std::string& filename);
	~PDFRenderer();
	Cairo::RefPtr<Cairo::ImageSurface> render(int page, double resolution) const;
	int getNPages() const;

private:
	PopplerDocument* m_document;
	mutable Glib::Threads::Mutex m_mutex;
};

#endif // IMAGERENDERER_HH

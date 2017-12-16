/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayRenderer.hh
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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

#include <QByteArray>
#include <QString>
#include <QMutex>

class DjVuDocument;

class QImage;
namespace Poppler {
class Document;
}

class DisplayRenderer {
public:
	DisplayRenderer(const QString& filename) : m_filename(filename) {}
	virtual ~DisplayRenderer() {}
	virtual QImage render(int page, double resolution) const = 0;
	virtual int getNPages() const = 0;

	void adjustImage(QImage& image, int brightness, int contrast, bool invert) const;

protected:
	QString m_filename;
};

class ImageRenderer : public DisplayRenderer {
public:
	ImageRenderer(const QString& filename) ;
	QImage render(int page, double resolution) const override;
	int getNPages() const override {
		return m_pageCount;
	}
private:
	int m_pageCount;
};

class PDFRenderer : public DisplayRenderer {
public:
	PDFRenderer(const QString& filename, const QByteArray& password);
	~PDFRenderer();
	QImage render(int page, double resolution) const override;
	int getNPages() const override;

private:
	Poppler::Document* m_document;
	mutable QMutex m_mutex;
};

class DJVURenderer : public DisplayRenderer {
public:
	DJVURenderer(const QString& filename);
	~DJVURenderer();
	QImage render(int page, double resolution) const override;
	int getNPages() const override;

private:
	DjVuDocument* m_djvu;

	mutable QMutex m_mutex;
};

#endif // IMAGERENDERER_HH

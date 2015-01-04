/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayRenderer.cc
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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

#include <QImageReader>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <poppler-qt4.h>
#else
#include <poppler-qt5.h>
#endif

#include "DisplayRenderer.hh"
#include "Utils.hh"

void DisplayRenderer::adjustImage(QImage &image, int brightness, int contrast, bool invert) const
{
	if(brightness == 0 && contrast == 0 && !invert){
		return;
	}

	double kBr = 1.f - qAbs(brightness / 200.f);
	double dBr = brightness > 0 ? 255. : 0.;

	double kCn = contrast * 2.55;
	double FCn = (259. * (kCn + 255.)) / (255. * (259. - kCn));

	int nLinePixels = image.bytesPerLine() / 4;
	int nLines = image.height();
#pragma omp parallel for
	for(int line = 0; line < nLines; ++line){
		QRgb* rgb = reinterpret_cast<QRgb*>(image.scanLine(line));
		for(int i = 0; i < nLinePixels; ++i){
			int red = qRed(rgb[i]);
			int green = qGreen(rgb[i]);
			int blue = qBlue(rgb[i]);
			// Brighntess
			red = dBr * (1.f - kBr) + red * kBr;
			green = dBr * (1.f - kBr) + green * kBr;
			blue = dBr * (1.f - kBr) + blue * kBr;
			// Contrast
			red = qMax(0., qMin(FCn * (red - 128.) + 128., 255.));
			green = qMax(0., qMin(FCn * (green - 128.) + 128., 255.));
			blue = qMax(0., qMin(FCn * (blue - 128.) + 128., 255.));
			// Invert
			if(invert){
				red = 255 - red;
				green = 255 - green;
				blue = 255 - blue;
			}

			rgb[i] = qRgb(red, green, blue);
		}
	}
}

QImage ImageRenderer::render(int /*page*/, double resolution) const
{
	QImageReader reader(m_filename);
	reader.setBackgroundColor(Qt::white);
	reader.setScaledSize(reader.size() * resolution / 100.);
	return reader.read().convertToFormat(QImage::Format_RGB32);
}

PDFRenderer::PDFRenderer(const QString& filename) : DisplayRenderer(filename)
{
	m_document = Poppler::Document::load(filename);
	m_document->setRenderHint(Poppler::Document::TextAntialiasing);
}

PDFRenderer::~PDFRenderer()
{
	delete m_document;
}

QImage PDFRenderer::render(int page, double resolution) const
{
	if(!m_document){
		return QImage();
	}
	m_mutex.lock();
	Poppler::Page* poppage = m_document->page(page - 1);
	m_mutex.unlock();
	QImage image = poppage->renderToImage(resolution, resolution);
	delete poppage;
	return image.convertToFormat(QImage::Format_RGB32);
}

int PDFRenderer::getNPages() const
{
	return m_document->numPages();
}

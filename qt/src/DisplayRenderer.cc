/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * DisplayRenderer.cc
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

#include <QImageReader>
#include <poppler-qt5.h>

#include <cmath>
#include "DjVuDocument.hh"
#include "DisplayRenderer.hh"
#include "Utils.hh"

void DisplayRenderer::adjustImage(QImage& image, int brightness, int contrast, bool invert) const {
	if(brightness == 0 && contrast == 0 && !invert) {
		return;
	}

	double kBr = 1.0 - std::abs(brightness / 200.0);
	double dBr = brightness > 0 ? 255.0 : 0.0;

	double kCn = contrast * 2.55;
	double FCn = (259.0 * (kCn + 255.0)) / (255.0 * (259.0 - kCn));

	int nLinePixels = image.bytesPerLine() / 4;
	int nLines = image.height();
	#pragma omp parallel for
	for(int line = 0; line < nLines; ++line) {
		QRgb* rgb = reinterpret_cast<QRgb*>(image.scanLine(line));
		for(int i = 0; i < nLinePixels; ++i) {
			int red = qRed(rgb[i]);
			int green = qGreen(rgb[i]);
			int blue = qBlue(rgb[i]);
			// Brighntess
			red = dBr * (1.0 - kBr) + red * kBr;
			green = dBr * (1.0 - kBr) + green * kBr;
			blue = dBr * (1.0 - kBr) + blue * kBr;
			// Contrast
			red = std::max(0.0, std::min(FCn * (red - 128.0) + 128.0, 255.0));
			green = std::max(0.0, std::min(FCn * (green - 128.0) + 128.0, 255.0));
			blue = std::max(0.0, std::min(FCn * (blue - 128.0) + 128.0, 255.0));
			// Invert
			if(invert) {
				red = 255 - red;
				green = 255 - green;
				blue = 255 - blue;
			}

			rgb[i] = qRgb(red, green, blue);
		}
	}
}

ImageRenderer::ImageRenderer(const QString& filename) : DisplayRenderer(filename) {
	m_pageCount = QImageReader(m_filename).imageCount();
}

QImage ImageRenderer::render(int page, double resolution) const {
	QImageReader reader(m_filename);
	reader.jumpToImage(page - 1);
	reader.setBackgroundColor(Qt::white);
	reader.setScaledSize(reader.size() * resolution / 100.0);
	return reader.read().convertToFormat(QImage::Format_RGB32);
}

QImage ImageRenderer::renderThumbnail(int page) const {
	QImageReader reader(m_filename);
	reader.jumpToImage(page - 1);
	reader.setBackgroundColor(Qt::white);
	QSize size = reader.size();
	double scale = size.width() > size.height() ? (64. / size.width()) : (64. / size.height());
	reader.setScaledSize(size * scale);
	return reader.read().convertToFormat(QImage::Format_RGB32);
}

PDFRenderer::PDFRenderer(const QString& filename, const QByteArray& password) : DisplayRenderer(filename) {
	m_document = Poppler::Document::load(filename);
	if(m_document) {
		if(m_document->isLocked()) {
			m_document->unlock(password, password);
		}

		m_document->setRenderHint(Poppler::Document::Antialiasing);
		m_document->setRenderHint(Poppler::Document::TextAntialiasing);
	}
}

PDFRenderer::~PDFRenderer() {
	delete m_document;
}

QImage PDFRenderer::render(int page, double resolution) const {
	if(!m_document) {
		return QImage();
	}
	m_mutex.lock();
	Poppler::Page* poppage = m_document->page(page - 1);
	m_mutex.unlock();
	QImage image = poppage->renderToImage(resolution, resolution);
	delete poppage;
	return image.convertToFormat(QImage::Format_RGB32);
}

QImage PDFRenderer::renderThumbnail(int page) const {
	if(!m_document) {
		return QImage();
	}
	m_mutex.lock();
	Poppler::Page* poppage = m_document->page(page - 1);
	m_mutex.unlock();
	// Resolution such that largest dimension is 64px
	// [points] / 72 * resolution = 64 => resolution = 64 * 72 / points
	QSizeF size = poppage->pageSizeF();
	double resolution = 64. * 72. / qMax(size.width(), size.height());
	QImage image = poppage->renderToImage(resolution, resolution);
	delete poppage;
	return image.convertToFormat(QImage::Format_RGB32);
}

int PDFRenderer::getNPages() const {
	return m_document ? m_document->numPages() : 1;
}

DJVURenderer::DJVURenderer(const QString& filename) : DisplayRenderer(filename) {
	m_djvu = new DjVuDocument();
	m_djvu->openFile(filename);
}

DJVURenderer::~DJVURenderer() {
	delete m_djvu;
}

QImage DJVURenderer::render(int page, double resolution) const {
	return m_djvu->image(page, resolution);
}

QImage DJVURenderer::renderThumbnail(int pageno) const {
	const DjVuDocument::Page& page = m_djvu->page(pageno);
	double resolution = 64. / qMax(page.width, page.height) * page.dpi;
	return m_djvu->image(pageno, resolution);
}

int DJVURenderer::getNPages() const {
	return m_djvu->pageCount();
}

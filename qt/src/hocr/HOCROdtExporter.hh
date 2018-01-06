/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCROdtExporter.hh
 * Copyright (C) 2013-2018 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRODTEXPORTER_HH
#define HOCRODTEXPORTER_HH

#include "common.hh"

#include <QObject>
#include <QString>

class DisplayerToolHOCR;
class HOCRDocument;
class HOCRItem;
class QImage;
class QRect;
class QuaZip;
class QXmlStreamWriter;


class HOCROdtExporter : public QObject {
	Q_OBJECT
public:
	HOCROdtExporter(DisplayerToolHOCR* displayerTool) : m_displayerTool(displayerTool) {}
	bool run(const HOCRDocument* hocrdocument, QString& filebasename);

private:
	DisplayerToolHOCR* m_displayerTool;

	void writeImage(QuaZip& zip, QMap<const HOCRItem*, QString>& images, const HOCRItem* item);
	void collectFontStyles(QMap<QString, QMap<double, QString> >& styles, const HOCRItem* item);
	void printItem(QXmlStreamWriter& writer, const HOCRItem* item, int pageNr, int dpi, const QMap<QString, QMap<double, QString> >& fontStyleNames, const QMap<const HOCRItem*, QString>& images);

private slots:
	bool setSource(const QString& sourceFile, int page, int dpi, double angle);
	QImage getSelection(const QRect& bbox);
};

#endif // HOCRODTEXPORTER_HH

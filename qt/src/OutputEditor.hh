/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditor.hh
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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

#ifndef OUTPUTEDITOR_HH
#define OUTPUTEDITOR_HH

#include <QObject>
#include "Config.hh"

namespace tesseract {
class TessBaseAPI;
}

class OutputEditor : public QObject {
	Q_OBJECT
public:
	struct ReadSessionData {
		virtual ~ReadSessionData() = default;
		int page;
		QString file;
		double angle;
		int resolution;
	};

	OutputEditor(QObject* parent = 0);

	virtual QWidget* getUI() = 0;
	virtual ReadSessionData* initRead(tesseract::TessBaseAPI &tess) = 0;
	virtual void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) = 0;
	virtual void readError(const QString& errorMsg, ReadSessionData* data) = 0;
	virtual void finalizeRead(ReadSessionData* data) {
		delete data;
	}

	virtual bool getModified() const = 0;

public slots:
	virtual void onVisibilityChanged(bool /*visible*/) {}
	virtual bool clear(bool hide = true) = 0;
	virtual bool save(const QString& filename = "") = 0;
	virtual void setLanguage(const Config::Lang &lang) {}
};

#endif // OUTPUTEDITOR_HH

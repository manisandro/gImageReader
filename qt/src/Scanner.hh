/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Scanner.hh
 *   Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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

#ifndef SCANNER_HH
#define SCANNER_HH

#include "common.hh"
#include <QMetaType>
#include <QObject>

class Scanner : public QObject {
	Q_OBJECT
public:
	enum class State { IDLE = 0, OPEN, SET_OPTIONS, START, GET_PARAMETERS, READ };
	enum class ScanMode { DEFAULT = 0, COLOR, GRAY, LINEART };
	enum class ScanType { SINGLE = 0, ADF_FRONT, ADF_BACK, ADF_BOTH };

	struct Device {
		QString name;
		QString label;
	};

	struct Params {
		QString device;
		QString filename;
		double dpi;
		ScanMode scan_mode;
		int depth;
		ScanType type;
		int page_width;
		int page_height;
	};

	virtual ~Scanner() {}

	virtual void init() = 0;
	virtual void redetect() = 0;
	virtual void scan(const Params& params) = 0;
	virtual void cancel() = 0;
	virtual void close() = 0;

signals:
	void initFailed();
	void devicesDetected(QList<Scanner::Device> devices);
	void scanFailed(const QString& message);
	void scanStateChanged(Scanner::State state);
	void pageAvailable(const QString& filename);
};

Q_DECLARE_METATYPE(Scanner::State)

#endif // SCANNER_HH

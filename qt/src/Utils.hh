/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.hh
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

#ifndef UTILS_HH
#define UTILS_HH

#include <functional>
#include <QString>

class QSpinBox;
class QDoubleSpinBox;

namespace Utils {
	QString documentsFolder();
	QString makeOutputFilename(const QString& filename);

	bool busyTask(const std::function<bool()>& f, const QString& msg);

	void setSpinBlocked(QSpinBox* spin, int value);
	void setSpinBlocked(QDoubleSpinBox* spin, double value);
}

#endif

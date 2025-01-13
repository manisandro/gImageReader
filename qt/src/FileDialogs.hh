/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FileDialogs.hh
 * Copyright (C) 2013-2025 Sandro Mani <manisandro@gmail.com>
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

#ifndef FILEDIALOGS_HH
#define FILEDIALOGS_HH

#include <QString>

class QWidget;

namespace FileDialogs {

QStringList openDialog(const QString& title, const QString& initialDirectory, const QString& initialDirSetting, const QString& filter, bool multiple, QWidget* parent = nullptr);
QString saveDialog(const QString& title, const QString& initialFilename, const QString& initialDirSetting, const QString& filter, bool generateUniqueName = false, QWidget* parent = nullptr);

} // FileDialogs

#endif // FILEDIALOGS_HH

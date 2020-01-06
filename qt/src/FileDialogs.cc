/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FileDialogs.cc
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

#include <QFileDialog>
#include <QFileInfo>
#include <QStringList>

#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "Utils.hh"

namespace FileDialogs {

QStringList openDialog(const QString& title, const QString& initialDirectory, const QString& initialDirSetting, const QString& filter, bool multiple, QWidget* parent) {
	parent = parent == nullptr ? MAIN : parent;
	QString initialDir = initialDirectory;
	if(initialDir.isEmpty()) {
		initialDir = ConfigSettings::get<VarSetting<QString>>(initialDirSetting)->getValue();
		if(initialDir.isEmpty()) {
			initialDir = Utils::documentsFolder();
		}
	}
	QStringList filenames;
	if(multiple) {
		filenames.append(QFileDialog::getOpenFileNames(parent, title, initialDir, filter));
	} else {
		QString filename = QFileDialog::getOpenFileName(parent, title, initialDir, filter);
		if(!filename.isEmpty()) {
			filenames.append(filename);
		}
	}
	if(!filenames.isEmpty()) {
		ConfigSettings::get<VarSetting<QString>>(initialDirSetting)->setValue(QFileInfo(filenames.first()).absolutePath());
	}
	return filenames;
}

QString saveDialog(const QString& title, const QString& initialFilename, const QString& initialDirSetting, const QString& filter, bool generateUniqueName, QWidget* parent) {
	parent = parent == nullptr ? MAIN : parent;
	QString suggestedFile;
	if(!initialFilename.isEmpty() && QFileInfo(initialFilename).isAbsolute()) {
		suggestedFile = initialFilename;
	} else {
		QString initialDir = ConfigSettings::get<VarSetting<QString>>(initialDirSetting)->getValue();
		if(initialDir.isEmpty()) {
			initialDir = Utils::documentsFolder();
		}
		suggestedFile = QDir(initialDir).absoluteFilePath(initialFilename);
		if(generateUniqueName) {
			suggestedFile = Utils::makeOutputFilename(suggestedFile);
		}
	}
	QString filename = QFileDialog::getSaveFileName(parent, title, suggestedFile, filter);
	if(!filename.isEmpty()) {
		QString sext = QFileInfo(suggestedFile).completeSuffix();
		QString ext = QFileInfo(filename).completeSuffix();
		if(ext.isEmpty()) {
			filename += "." + sext;
		}
		ConfigSettings::get<VarSetting<QString>>(initialDirSetting)->setValue(QFileInfo(filename).absolutePath());
	}
	return filename;
}

} // FileDialogs

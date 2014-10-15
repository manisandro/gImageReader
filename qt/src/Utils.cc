/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Utils.cc
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

#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include "Utils.hh"
#include "MainWindow.hh"

QString Utils::documentsFolder()
{
	QString documentsFolder;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	documentsFolder = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
	documentsFolder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
	return documentsFolder.isEmpty() ? QDir::homePath() : documentsFolder;
}

QString Utils::makeOutputFilename(const QString& filename)
{
	// Ensure directory exists
	QFileInfo finfo(filename);
	QDir dir = finfo.absoluteDir();
	if(!dir.exists()){
		dir = QDir(Utils::documentsFolder());
	}
	// Generate non-existing file
	QString base = finfo.baseName().replace(QRegExp("_[0-9]+$"), "");
	QString ext = finfo.completeSuffix();
	QString newfilename = dir.absoluteFilePath(base + "." + ext);
	for(int i = 1; QFile(newfilename).exists(); ++i){
		newfilename = dir.absoluteFilePath(QString("%1_%2.%3").arg(base).arg(i).arg(ext));
	}
	return newfilename;
}

bool Utils::busyTask(const std::function<bool()> &f, const QString& msg)
{
	MAIN->pushState(MainWindow::State::Busy, msg);
	QEventLoop evLoop;
	QFutureWatcher<bool> watcher;

	QObject::connect(&watcher, SIGNAL(finished()), &evLoop, SLOT(quit()));

	watcher.setFuture(QtConcurrent::run(f));
	evLoop.exec(QEventLoop::ExcludeUserInputEvents);

	MAIN->popState();
	return watcher.future().result();
}

void Utils::setSpinBlocked(QSpinBox *spin, int value)
{
	spin->blockSignals(true);
	spin->setValue(value);
	spin->blockSignals(false);
}

void Utils::setSpinBlocked(QDoubleSpinBox *spin, double value)
{
	spin->blockSignals(true);
	spin->setValue(value);
	spin->blockSignals(false);
}

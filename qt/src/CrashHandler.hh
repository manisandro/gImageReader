/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * CrashHandler.hh
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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

#ifndef CRASHHANDLER_HH
#define CRASHHANDLER_HH

#include "ui_CrashHandler.h"
#include <QProcess>

class CrashHandler : public QDialog {
	Q_OBJECT

public:
	explicit CrashHandler(int pid, int tesseractCrash, const QString& savefile, QWidget* parent = 0);

private:
	Ui::CrashHandler ui;
	int m_pid;
	QProcess m_gdbProcess;
	QPushButton* m_refreshButton = nullptr;

	void closeEvent(QCloseEvent*) override;

private slots:
	void appendGdbOutput();
	void handleGdbFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void regenerateBacktrace();
};

#endif // CRASHHANDLER_HH

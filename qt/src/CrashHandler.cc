/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * CrashHandler.cc
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#include "common.hh"
#include "CrashHandler.hh"
#include <QPushButton>

CrashHandler::CrashHandler(int pid, int tesseractCrash, const QString& savefile, QWidget* parent):
	QDialog(parent), m_pid(pid) {
	ui.setupUi(this);
	ui.labelIntroTesseract->setVisible(tesseractCrash);
	ui.labelIntro->setVisible(!tesseractCrash);

	if(!savefile.isEmpty()) {
		ui.labelAutosave->setText(_("Your work has been saved under <b>%1</b>.").arg(savefile));
	} else {
		ui.labelAutosave->setText(_("There was no unsaved work."));
	}

	setWindowTitle(QString("%1 - %2").arg(PACKAGE_NAME).arg( _("Crash Handler")));

	m_refreshButton = new QPushButton(QIcon::fromTheme("view-refresh"), _("Regenerate backtrace"));
	m_refreshButton->setEnabled(false);
	ui.buttonBox->addButton(m_refreshButton, QDialogButtonBox::ActionRole);
	connect(m_refreshButton, &QPushButton::clicked, this, &CrashHandler::regenerateBacktrace);

	m_gdbProcess.setProcessChannelMode(QProcess::SeparateChannels);
	connect(&m_gdbProcess, &QProcess::readyReadStandardOutput, this, &CrashHandler::appendGdbOutput);
	connect(&m_gdbProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &CrashHandler::handleGdbFinished);

	regenerateBacktrace();
}

void CrashHandler::closeEvent(QCloseEvent*) {
	m_gdbProcess.kill();
}

void CrashHandler::appendGdbOutput() {
	ui.plainTextEditBacktrace->appendPlainText(m_gdbProcess.readAllStandardOutput());
}

void CrashHandler::handleGdbFinished(int exitCode, QProcess::ExitStatus exitStatus) {
	ui.progressBarBacktrace->setVisible(false);
	m_refreshButton->setEnabled(true);
	if(exitCode != 0 || exitStatus != QProcess::NormalExit) {
		ui.plainTextEditBacktrace->appendPlainText(_("Failed to obtain backtrace. Is GDB installed?"));
	} else {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		QStringList lines = ui.plainTextEditBacktrace->toPlainText().split("\n", QString::SkipEmptyParts);
#else
		QStringList lines = ui.plainTextEditBacktrace->toPlainText().split("\n", Qt::SkipEmptyParts);
#endif
		ui.plainTextEditBacktrace->setPlainText(lines[0]);
		ui.plainTextEditBacktrace->appendPlainText("\n");
		for(int i = 1, n = lines.length(); i < n; ++i) {
			if(lines[i].startsWith("Thread")) {
				ui.plainTextEditBacktrace->appendPlainText("\n");
				ui.plainTextEditBacktrace->appendPlainText(lines[i]);
			} else if(lines[i].startsWith('#')) {
				ui.plainTextEditBacktrace->appendPlainText(lines[i]);
			}
		}
	}
	QTextCursor c = ui.plainTextEditBacktrace->textCursor();
	c.movePosition(QTextCursor::Start);
	ui.plainTextEditBacktrace->setTextCursor(c);
	ui.plainTextEditBacktrace->ensureCursorVisible();
}

void CrashHandler::regenerateBacktrace() {
	ui.plainTextEditBacktrace->setPlainText(QString("%1 %2 (%3)\n\n").arg(PACKAGE_NAME).arg(PACKAGE_VERSION).arg(PACKAGE_REVISION));
	m_refreshButton->setEnabled(false);
	ui.progressBarBacktrace->setVisible(true);

	m_gdbProcess.start("gdb", QStringList() << "-p" << QString::number(m_pid));
	m_gdbProcess.write("set pagination off\n");
	m_gdbProcess.write("bt\n");
	m_gdbProcess.write("thread apply all bt full\n");
	m_gdbProcess.write("quit\n");
}

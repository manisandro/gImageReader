/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Acquirer.hh
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

#ifndef ACQUIRER_HH
#define ACQUIRER_HH

#include "Scanner.hh"

class UI_MainWindow;

class Acquirer : public QObject {
	Q_OBJECT
public:
	Acquirer(const UI_MainWindow& _ui);
	~Acquirer();

signals:
	void scanPageAvailable(QString);

private:
	const UI_MainWindow& ui;
	QString m_outputPath;

	Scanner* m_scanner;

	void genOutputPath();

private slots:
	void cancelScan();
	void doneDetectDevices(QList<Scanner::Device> devices);
	void doneScan();
	void scanInitFailed();
	void scanFailed(const QString& msg);
	void selectOutputPath();
	void setDeviceComboTooltip();
	void setScanState(Scanner::State state);
	void startDetectDevices();
	void startScan();
};

Q_DECLARE_METATYPE(QList<Scanner::Device>)

#endif // ACQUIRER_HH

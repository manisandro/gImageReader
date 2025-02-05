/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Acquirer.cc
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

#include <QDir>
#include <QImageReader>
#include <QThread>

#include "Acquirer.hh"
#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "Utils.hh"

#ifdef Q_OS_WIN32
#include "scanner/ScannerTwain.hh"
#else
#include "scanner/ScannerSane.hh"
#endif

Acquirer::Acquirer(const UI_MainWindow& _ui)
	: ui(_ui) {
	ui.pushButtonScanCancel->setVisible(false);
	ui.toolButtonScanDevicesRefresh->setEnabled(false);
	ui.pushButtonScan->setEnabled(false);
	ui.comboBoxScanDevice->setCursor(Qt::WaitCursor);
	// TODO: Elide combobox

	m_scanner = new ScannerImpl;

	qRegisterMetaType<QList<Scanner::Device >> ();
	qRegisterMetaType<Scanner::State>();

	connect(ui.toolButtonScanDevicesRefresh, &QToolButton::clicked, this, &Acquirer::startDetectDevices);
	connect(ui.pushButtonScan, &QPushButton::clicked, this, &Acquirer::startScan);
	connect(ui.pushButtonScanCancel, &QPushButton::clicked, this, &Acquirer::cancelScan);
	connect(ui.toolButtonScanOutput, &QToolButton::clicked, this, &Acquirer::selectOutputPath);
	connect(ui.comboBoxScanDevice, qOverload<int> (&QComboBox::currentIndexChanged), this, &Acquirer::setDeviceComboTooltip);
	connect(m_scanner, &Scanner::initFailed, this, &Acquirer::scanInitFailed);
	connect(m_scanner, &Scanner::devicesDetected, this, &Acquirer::doneDetectDevices);
	connect(m_scanner, &Scanner::scanFailed, this, &Acquirer::scanFailed);
	connect(m_scanner, &Scanner::scanStateChanged, this, &Acquirer::setScanState);
	connect(m_scanner, &Scanner::pageAvailable, this, &Acquirer::scanPageAvailable);

	ADD_SETTING(ComboSetting("scanres", ui.comboBoxScanResolution, 2));
	ADD_SETTING(ComboSetting("scanmode", ui.comboBoxScanMode, 0));
	ADD_SETTING(ComboSetting("scandev", ui.comboBoxScanDevice, 0));
	ADD_SETTING(ComboSetting("scansource", ui.comboBoxScanSource, 0));
#ifdef Q_OS_WIN32
	ui.labelScanSource->setVisible(false);
	ui.comboBoxScanSource->setVisible(false);
#endif

	QString sourcedir = ConfigSettings::get<VarSetting<QString >> ("sourcedir")->getValue();
	m_outputPath = QDir(sourcedir.isEmpty() ? Utils::documentsFolder() : sourcedir).absoluteFilePath(_("scan.png"));
	genOutputPath();
	m_scanner->init();
}

Acquirer::~Acquirer() {
	m_scanner->close();
	delete m_scanner;
}

void Acquirer::selectOutputPath() {
	QSet<QString> formats;
	for (const QByteArray& format : QImageReader::supportedImageFormats()) {
		formats.insert(QString("*.%1").arg(QString(format).toLower()));
	}
	QString filter = QString("%1 (%2)").arg(_("Images")).arg(QStringList(formats.values()).join(" "));
	QString filename = FileDialogs::saveDialog(_("Choose Output Filename..."), m_outputPath, "sourcedir", filter);
	if (!filename.isEmpty()) {
		m_outputPath = filename;
		genOutputPath();
	}
}

void Acquirer::genOutputPath() {
	m_outputPath = Utils::makeOutputFilename(m_outputPath);
	ui.lineEditScanOutput->setText(m_outputPath);
	ui.lineEditScanOutput->setToolTip(m_outputPath);
}

void Acquirer::scanInitFailed() {
	ui.labelScanMessage->setText(QString("<span style=\"color:#FF0000;\">%1</span>").arg(_("Failed to initialize the scanning backend.")));
	ui.pushButtonScan->setEnabled(false);
	ui.toolButtonScanDevicesRefresh->setEnabled(false);
}

void Acquirer::scanFailed(const QString& msg) {
	ui.labelScanMessage->setText(QString("<span style=\"color:#FF0000;\">%1: %2.</span>").arg(_("Scan failed")).arg(msg));
}

void Acquirer::startDetectDevices() {
	ui.toolButtonScanDevicesRefresh->setEnabled(false);
	ui.pushButtonScan->setEnabled(false);
	ui.labelScanMessage->setText("");
	ui.comboBoxScanDevice->clear();
	ui.comboBoxScanDevice->setCursor(Qt::WaitCursor);
	m_scanner->redetect();
}

void Acquirer::doneDetectDevices(QList<Scanner::Device> devices) {
	ui.comboBoxScanDevice->unsetCursor();
	ui.toolButtonScanDevicesRefresh->setEnabled(true);
	if (devices.isEmpty()) {
		ui.labelScanMessage->setText(QString("<span style=\"color:#FF0000;\">%1</span>").arg(_("No scanners were detected.")));
	} else {
		for (const Scanner::Device& device : devices) {
			ui.comboBoxScanDevice->addItem(device.label, device.name);
		}
		ui.comboBoxScanDevice->setCurrentIndex(0);
		ui.pushButtonScan->setEnabled(true);
	}
}

void Acquirer::startScan() {
	ui.pushButtonScan->setVisible(false);
	ui.pushButtonScanCancel->setEnabled(true);
	ui.pushButtonScanCancel->setVisible(true);
	ui.labelScanMessage->setText(_("Starting scan..."));

	const double dpi[] = {75.0, 100.0, 200.0, 300.0, 600.0, 1200.0};
	const Scanner::ScanMode modes[] = {Scanner::ScanMode::GRAY, Scanner::ScanMode::COLOR};
	const Scanner::ScanType types[] = {Scanner::ScanType::SINGLE, Scanner::ScanType::ADF_FRONT, Scanner::ScanType::ADF_BACK, Scanner::ScanType::ADF_BOTH};
	genOutputPath();
	QString device = ui.comboBoxScanDevice->itemData(ui.comboBoxScanDevice->currentIndex()).toString();
	Scanner::Params params = {device, m_outputPath, dpi[ui.comboBoxScanResolution->currentIndex()], modes[ui.comboBoxScanMode->currentIndex()], 8, types[ui.comboBoxScanSource->currentIndex()], 0, 0};
	m_scanner->scan(params);
}

void Acquirer::setScanState(Scanner::State state) {
	if (state == Scanner::State::OPEN) {
		ui.labelScanMessage->setText(_("Opening device..."));
	} else if (state == Scanner::State::SET_OPTIONS) {
		ui.labelScanMessage->setText(_("Setting options..."));
	} else if (state == Scanner::State::START) {
		ui.labelScanMessage->setText(_("Starting scan..."));
	} else if (state == Scanner::State::GET_PARAMETERS) {
		ui.labelScanMessage->setText(_("Getting parameters..."));
	} else if (state == Scanner::State::READ) {
		ui.labelScanMessage->setText(_("Transferring data..."));
	} else if (state == Scanner::State::IDLE) {
		doneScan();
	}
}

void Acquirer::cancelScan() {
	ui.labelScanMessage->setText(_("Canceling scan..."));
	m_scanner->cancel();
	ui.pushButtonScanCancel->setEnabled(false);
}

void Acquirer::doneScan() {
	ui.pushButtonScanCancel->setVisible(false);
	ui.pushButtonScan->setVisible(true);
	ui.labelScanMessage->setText("");
	genOutputPath();
}

void Acquirer::setDeviceComboTooltip() {
	ui.comboBoxScanDevice->setToolTip(ui.comboBoxScanDevice->currentText());
}

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Acquirer.cc
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

#include <QDir>
#include <QFileDialog>
#include <QThread>

#include "Acquirer.hh"
#include "Config.hh"
#include "MainWindow.hh"
#include "Utils.hh"

Acquirer::Acquirer(const UI_MainWindow& _ui)
	: ui(_ui)
{
	ui.pushButtonScanCancel->setVisible(false);
	ui.toolButtonScanDevicesRefresh->setEnabled(false);
	ui.pushButtonScan->setEnabled(false);
	ui.comboBoxScanDevice->setCursor(Qt::WaitCursor);
	// TODO: Elide combobox

	m_thread = new QThread(this);
	m_scanThread = new ScanThread();
	m_scanThread->moveToThread(m_thread);

	qRegisterMetaType<QList<ScanBackend::Device>>();
	qRegisterMetaType<ScanThread::State>();

	connect(ui.toolButtonScanDevicesRefresh, SIGNAL(clicked()), this, SLOT(startDetectDevices()));
	connect(ui.pushButtonScan, SIGNAL(clicked()), this, SLOT(startScan()));
	connect(ui.pushButtonScanCancel, SIGNAL(clicked()), this, SLOT(cancelScan()));
	connect(ui.toolButtonScanOutput, SIGNAL(clicked()), this, SLOT(selectOutputPath()));
	connect(ui.comboBoxScanDevice, SIGNAL(currentIndexChanged(int)), this, SLOT(setDeviceComboTooltip()));
	connect(m_scanThread, SIGNAL(initFailed()), this, SLOT(scanInitFailed()));
	connect(m_scanThread, SIGNAL(devicesDetected(QList<ScanBackend::Device>)), this, SLOT(doneDetectDevices(QList<ScanBackend::Device>)));
	connect(m_scanThread, SIGNAL(scanFailed(QString)), this, SLOT(scanFailed(QString)));
	connect(m_scanThread, SIGNAL(scanStateChanged(ScanThread::State)), this, SLOT(setScanState(ScanThread::State)));
	connect(m_scanThread, SIGNAL(pageAvailable(QString)), this, SIGNAL(scanPageAvailable(QString)));
	connect(m_scanThread, SIGNAL(stopped()), m_thread, SLOT(quit()));
	connect(m_scanThread, SIGNAL(stopped()), m_scanThread, SLOT(deleteLater()));
	connect(m_thread, SIGNAL(started()), m_scanThread, SLOT(run()));

	MAIN->getConfig()->addSetting(new ComboSetting("scanres", ui.comboBoxScanResolution, 2));
	MAIN->getConfig()->addSetting(new ComboSetting("scanmode", ui.comboBoxScanMode, 0));
	MAIN->getConfig()->addSetting(new ComboSetting("scandev", ui.comboBoxScanDevice, 0));
	MAIN->getConfig()->addSetting(new VarSetting<QString>("scanoutput", QDir(Utils::documentsFolder()).absoluteFilePath(_("scan.png"))));

	m_outputPath = MAIN->getConfig()->getSetting<VarSetting<QString>>("scanoutput")->getValue();
	genOutputPath();
	m_thread->start();
}

Acquirer::~Acquirer()
{
	m_scanThread->stop();
	while(!m_thread->wait(100)){
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}
}

void Acquirer::selectOutputPath()
{
	QString filename = QFileDialog::getSaveFileName(MAIN, _("Choose Output Filename..."), m_outputPath, QString("%1 (*.png)").arg(_("PNG Images")));
	if(!filename.isEmpty()){
		m_outputPath = filename;
		genOutputPath();
	}
}

void Acquirer::genOutputPath()
{
	m_outputPath = Utils::makeOutputFilename(m_outputPath);
	ui.lineEditScanOutput->setText(m_outputPath);
	ui.lineEditScanOutput->setToolTip(m_outputPath);;
	MAIN->getConfig()->getSetting<VarSetting<QString>>("scanoutput")->setValue(m_outputPath);
}

void Acquirer::scanInitFailed()
{
	ui.labelScanMessage->setText(QString("<span style=\"color:#FF0000;\">%1</span>").arg(_("Failed to initialize the scanning backend.")));
	ui.pushButtonScan->setEnabled(false);
	ui.toolButtonScanDevicesRefresh->setEnabled(false);
	m_scanThread->stop();
}

void Acquirer::scanFailed(const QString &msg)
{
	ui.labelScanMessage->setText(QString("<span style=\"color:#FF0000;\">%1: %2.</span>").arg(_("Scan failed")).arg(msg));
}

void Acquirer::startDetectDevices()
{
	ui.toolButtonScanDevicesRefresh->setEnabled(false);
	ui.pushButtonScan->setEnabled(false);
	ui.labelScanMessage->setText("");
	ui.comboBoxScanDevice->clear();
	ui.comboBoxScanDevice->setCursor(Qt::WaitCursor);
	m_scanThread->redetect();
}

void Acquirer::doneDetectDevices(QList<ScanBackend::Device> devices)
{
	ui.comboBoxScanDevice->unsetCursor();
	ui.toolButtonScanDevicesRefresh->setEnabled(true);
	if(devices.isEmpty()){
		ui.labelScanMessage->setText(QString("<span style=\"color:#FF0000;\">%1</span>").arg(_("No scanners were detected.")));
	}else{
		for(const ScanBackend::Device& device : devices){
			ui.comboBoxScanDevice->addItem(device.label, device.name);
		}
		ui.comboBoxScanDevice->setCurrentIndex(0);
		ui.pushButtonScan->setEnabled(true);
	}
}

void Acquirer::startScan()
{
	ui.pushButtonScan->setVisible(false);
	ui.pushButtonScanCancel->setEnabled(true);
	ui.pushButtonScanCancel->setVisible(true);
	ui.labelScanMessage->setText(_("Starting scan..."));

	int res[] = {75, 100, 200, 300, 600, 1200};
	ScanBackend::ScanMode modes[] = {ScanBackend::ScanMode::GRAY, ScanBackend::ScanMode::COLOR};
	genOutputPath();
	ScanBackend::Options opts = {m_outputPath, res[ui.comboBoxScanResolution->currentIndex()], modes[ui.comboBoxScanMode->currentIndex()], 8, ScanBackend::ScanType::SINGLE, 0, 0};
	genOutputPath(); // Prepare for next
	QString scanner = ui.comboBoxScanDevice->itemData(ui.comboBoxScanDevice->currentIndex()).toString();
	m_scanThread->scan(scanner, opts);
}

void Acquirer::setScanState(ScanThread::State state)
{
	if(state == ScanThread::State::OPEN){
		ui.labelScanMessage->setText(_("Opening device..."));
	}else if(state == ScanThread::State::SET_OPTIONS){
		ui.labelScanMessage->setText(_("Setting options..."));
	}else if(state == ScanThread::State::START){
		ui.labelScanMessage->setText(_("Starting scan..."));
	}else if(state == ScanThread::State::GET_PARAMETERS){
		ui.labelScanMessage->setText(_("Getting parameters..."));
	}else if(state == ScanThread::State::READ){
		ui.labelScanMessage->setText(_("Transferring data..."));
	}else if(state == ScanThread::State::IDLE){
		doneScan();
	}
}

void Acquirer::cancelScan()
{
	m_scanThread->cancel();
	ui.pushButtonScanCancel->setEnabled(false);
	ui.labelScanMessage->setText(_("Canceling scan..."));
}

void Acquirer::doneScan()
{
	ui.pushButtonScanCancel->setVisible(false);
	ui.pushButtonScan->setVisible(true);
	ui.labelScanMessage->setText("");
}

void Acquirer::setDeviceComboTooltip()
{
	ui.comboBoxScanDevice->setToolTip(ui.comboBoxScanDevice->currentText());
}

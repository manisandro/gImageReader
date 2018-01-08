/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Acquirer.cc
 * Copyright (C) (\d+)-2018 Sandro Mani <manisandro@gmail.com>
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

#include "Acquirer.hh"
#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "Utils.hh"

#ifdef G_OS_WIN32
#include "scanner/ScannerTwain.hh"
#else
#include "scanner/ScannerSane.hh"
#endif

Acquirer::Acquirer(const Ui::MainWindow& _ui)
	: ui(_ui) {
	m_cancelButton = Gtk::manage(new Gtk::Button(_("Cancel")));
	m_cancelButton->set_image_from_icon_name("dialog-cancel", Gtk::ICON_SIZE_BUTTON);
	m_cancelButton->show();

	ui.comboAcquireDevice->set_model(Gtk::ListStore::create(m_devComboCols));
	Gtk::CellRendererText* cell = Gtk::manage(new Gtk::CellRendererText);
	cell->property_ellipsize() = Pango::ELLIPSIZE_END;
	ui.comboAcquireDevice->pack_start(*cell, true);
	ui.comboAcquireDevice->add_attribute(*cell, "text", 0);

	if(ui.comboAcquireResolution->get_active_row_number() == -1) ui.comboAcquireResolution->set_active(2);
	if(ui.comboAcquireMode->get_active_row_number() == -1) ui.comboAcquireMode->set_active(0);

	m_scanner = new ScannerImpl;

	CONNECT(ui.buttonAcquireRefresh, clicked, [this] { startDetectDevices(); });
	CONNECT(ui.buttonAcquireScan, clicked, [this] { startScan(); });
	CONNECT(m_cancelButton, clicked, [this] { cancelScan(); });
	CONNECT(ui.buttonAcquireOutput, clicked, [this] { selectOutputPath(); });
	CONNECT(ui.comboAcquireDevice, changed, [this] { setDeviceComboTooltip(); });
	CONNECT(m_scanner, initFailed, [this] { scanInitFailed(); });
	CONNECT(m_scanner, devicesDetected, [this](const std::vector<Scanner::Device>& devices) {
		doneDetectDevices(devices);
	});
	CONNECT(m_scanner, scanFailed, [this](const std::string& msg) {
		scanFailed(msg);
	});
	CONNECT(m_scanner, scanStateChanged, [this](Scanner::State state) {
		setScanState(state);
	});
	CONNECT(m_scanner, pageAvailable, [this](const std::string& file) {
		m_signal_scanPageAvailable.emit(file);
	});

	ADD_SETTING(ComboSetting("scanres", ui.comboAcquireResolution));
	ADD_SETTING(ComboSetting("scanmode", ui.comboAcquireMode));
	ADD_SETTING(ComboSetting("scandev", ui.comboAcquireDevice));
	ADD_SETTING(ComboSetting("scansource", ui.comboAcquireSource));

#ifdef G_OS_WIN32
	ui.labelAcquireSource->set_visible(false);
	ui.comboAcquireSource->set_visible(false);
#endif

	std::string sourcedir = ConfigSettings::get<VarSetting<Glib::ustring>>("sourcedir")->getValue();
	m_outputPath = Glib::build_filename(sourcedir.empty() ? Utils::get_documents_dir() : sourcedir, _("scan.png"));
	genOutputPath();
	m_scanner->init();
}

Acquirer::~Acquirer() {
	m_scanner->close();
	delete m_scanner;
}

void Acquirer::selectOutputPath() {
	FileDialogs::FileFilter filter = FileDialogs::FileFilter::pixbuf_formats();
	filter.name = _("Images");
	std::string filename = FileDialogs::save_dialog(_("Choose Output Filename..."), m_outputPath, "sourcedir", filter);
	if(!filename.empty()) {
		m_outputPath = filename;
		genOutputPath();
	}
}

void Acquirer::genOutputPath() {
	m_outputPath = Utils::make_output_filename(m_outputPath);
	ui.labelAcquirePath->set_text(m_outputPath);
	ui.labelAcquirePath->set_tooltip_text(m_outputPath);
}

void Acquirer::scanInitFailed() {
	ui.labelAcquireMessage->set_markup(Glib::ustring::compose("<span color='red'>%1</span>", _("Failed to initialize the scanning backend.")));
	ui.buttonAcquireScan->set_sensitive(false);
	ui.buttonAcquireRefresh->set_sensitive(false);
	ui.buttonAcquireRefresh->show();
	ui.spinnerAcquire->hide();
	ui.spinnerAcquire->stop();
}

void Acquirer::scanFailed(const Glib::ustring &msg) {
	ui.labelAcquireMessage->set_markup(Glib::ustring::compose("<span color='red'>%1: %2</span>", _("Scan failed"), msg));
}

void Acquirer::startDetectDevices() {
	ui.buttonAcquireRefresh->hide();
	ui.spinnerAcquire->show();
	ui.spinnerAcquire->start();
	ui.labelAcquireMessage->set_text("");
	ui.comboAcquireDevice->set_model(Gtk::ListStore::create(m_devComboCols));
	ui.buttonAcquireScan->set_sensitive(false);
	m_scanner->redetect();
}

void Acquirer::doneDetectDevices(const std::vector<Scanner::Device>& devices) {
	ui.buttonAcquireRefresh->show();
	ui.spinnerAcquire->hide();
	ui.spinnerAcquire->stop();
	if(devices.empty()) {
		ui.labelAcquireMessage->set_markup(Glib::ustring::compose("<span color='red'>%1</span>", _("No scanners were detected.")));
	} else {
		Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.comboAcquireDevice->get_model());
		for(const Scanner::Device& device : devices) {
			Gtk::TreeIter it = store->append();
			it->set_value(m_devComboCols.label, device.label);
			it->set_value(m_devComboCols.name, device.name);
		}
		ui.comboAcquireDevice->set_active(0);
		ui.buttonAcquireScan->set_sensitive(true);
	}
}

void Acquirer::startScan() {
	ui.buttonboxAcquire->remove(*ui.buttonAcquireScan);
	m_cancelButton->set_sensitive(true);
	ui.buttonboxAcquire->pack_start(*m_cancelButton, false, true);
	ui.labelAcquireMessage->set_text(_("Starting scan..."));

	double res[] = {75., 100., 200., 300., 600., 1200.};
	Scanner::ScanMode modes[] = {Scanner::ScanMode::COLOR, Scanner::ScanMode::GRAY};
	Scanner::ScanType types[] = {Scanner::ScanType::SINGLE, Scanner::ScanType::ADF_FRONT, Scanner::ScanType::ADF_BACK, Scanner::ScanType::ADF_BOTH};
	genOutputPath();
	std::string device = (*ui.comboAcquireDevice->get_active())[m_devComboCols.name];
	Scanner::Params params = {device, m_outputPath, res[ui.comboAcquireResolution->get_active_row_number()], modes[ui.comboAcquireMode->get_active_row_number()], 8, types[ui.comboAcquireSource->get_active_row_number()], 0, 0};
	m_scanner->scan(params);
	genOutputPath();
}

void Acquirer::setScanState(Scanner::State state) {
	if(state == Scanner::State::OPEN) {
		ui.labelAcquireMessage->set_text(_("Opening device..."));
	} else if(state == Scanner::State::SET_OPTIONS) {
		ui.labelAcquireMessage->set_text(_("Setting options..."));
	} else if(state == Scanner::State::START) {
		ui.labelAcquireMessage->set_text(_("Starting scan..."));
	} else if(state == Scanner::State::GET_PARAMETERS) {
		ui.labelAcquireMessage->set_text(_("Getting parameters..."));
	} else if(state == Scanner::State::READ) {
		ui.labelAcquireMessage->set_text(_("Transferring data..."));
	} else if(state == Scanner::State::IDLE) {
		doneScan();
	}
}

void Acquirer::cancelScan() {
	ui.labelAcquireMessage->set_text(_("Canceling scan..."));
	m_scanner->cancel();
	m_cancelButton->set_sensitive(false);
}

void Acquirer::doneScan() {
	ui.buttonboxAcquire->remove(*m_cancelButton);
	ui.buttonboxAcquire->pack_start(*ui.buttonAcquireScan, false, true);
	ui.labelAcquireMessage->set_text("");
}

void Acquirer::setDeviceComboTooltip() {
	auto it = ui.comboAcquireDevice->get_active();
	ui.comboAcquireDevice->set_tooltip_text(it ? static_cast<std::string>((*it)[m_devComboCols.label]) : "");
}

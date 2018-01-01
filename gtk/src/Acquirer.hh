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

#include "common.hh"
#include "Scanner.hh"

namespace Ui { class MainWindow; }

class Acquirer {
public:
	Acquirer(const Ui::MainWindow& _ui);
	~Acquirer();

	sigc::signal<void,std::string> signal_scanPageAvailable() const {
		return m_signal_scanPageAvailable;
	}

private:
	class DevicesComboColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		Gtk::TreeModelColumn<std::string> label;
		Gtk::TreeModelColumn<std::string> name;
		DevicesComboColumns() {
			add(label);
			add(name);
		}
	} m_devComboCols;

	const Ui::MainWindow& ui;
	ClassData m_classdata;

	Gtk::Button* m_cancelButton;
	std::string m_outputPath;
	sigc::signal<void,std::string> m_signal_scanPageAvailable;
	Scanner* m_scanner = nullptr;

	void genOutputPath();
	void cancelScan();
	void doneDetectDevices(const std::vector<Scanner::Device>& devices);
	void doneScan();
	void scanInitFailed();
	void scanFailed(const Glib::ustring& msg);
	void selectOutputPath();
	void setDeviceComboTooltip();
	void setScanState(Scanner::State state);
	void startDetectDevices();
	void startScan();
};

#endif // ACQUIRER_HH

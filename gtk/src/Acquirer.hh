/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Acquirer.hh
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

#ifndef ACQUIRER_HH
#define ACQUIRER_HH

#include "common.hh"
#include "ScanThread.hh"

class Scanner;

class Acquirer {
public:
	Acquirer();
	~Acquirer();

private:
	class DevicesComboColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		Gtk::TreeModelColumn<std::string> label;
		Gtk::TreeModelColumn<std::string> name;
		DevicesComboColumns() { add(label); add(name); }
	};

	Gtk::Button* m_refreshButton;
	Gtk::Button* m_scanButton;
	Gtk::Button* m_cancelButton;
	Gtk::ButtonBox* m_buttonBox;
	Gtk::ComboBox* m_devCombo;
	Gtk::ComboBox* m_modeCombo;
	Gtk::ComboBox* m_resCombo;
	Gtk::Label* m_msgLabel;
	Gtk::Label* m_outputLabel;
	Gtk::Spinner* m_refreshSpinner;
	DevicesComboColumns m_devComboCols;
	std::string m_outputPath;

	Glib::Threads::Thread* m_thread;
	ScanThread* m_scanThread;

	void genOutputPath();
	void cancelScan();
	void doneDetectDevices(const std::vector<ScanBackend::Device>& devices);
	void doneScan();
	void scanInitFailed();
	void scanFailed(const Glib::ustring& msg);
	void selectOutputPath();
	void setDeviceComboTooltip();
	void setScanState(ScanThread::State state);
	void startDetectDevices();
	void startScan();
};

#endif // ACQUIRER_HH

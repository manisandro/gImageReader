/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Acquirer.hh
 * Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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
#include "SaneScanner.hh"

class Acquirer : public Scanner::PageHandler {
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

	Scanner* m_scanner;

	int m_height, m_rowstride;
	std::vector<uint8_t> m_buf;

	void selectOutputPath();
	void genOutputPath();
	void scanInitFailed();
	void authorizeScanner(const std::string &res);
	void startDetectDevices();
	void doneDetectDevices(const std::vector<Scanner::ScanDevice>& devices);
	void startScan();
	void cancelScan();
	void doneScan();
	void setupPage(const Scanner::ScanPageInfo &info);
	void handleData(const Scanner::ScanLine &line);
	void finalizePage();
};

#endif // ACQUIRER_HH

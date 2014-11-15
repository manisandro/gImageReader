/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Scanner.hh
 * Based on code from Simple Scan, which is:
 *   Copyright (C) 2009-2013 Canonical Ltd.
 *   Author: Robert Ancell <robert.ancell@canonical.com>
 * Modifications are:
 *   Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "common.hh"
#include "Utils.hh"

class Scanner {
public:
	enum class ScanState { IDLE = 0, OPEN, SET_OPTIONS, START, GET_PARAMETERS, READ };
	enum class ScanMode { DEFAULT = 0, COLOR, GRAY, LINEART };
	enum class ScanType { SINGLE = 0, ADF_FRONT, ADF_BACK, ADF_BOTH };

	struct ScanDevice {
		std::string name;
		std::string label;
	};

	struct ScanOptions {
		std::string filename;
		int dpi;
		ScanMode scan_mode;
		int depth;
		ScanType type;
		int paper_width;
		int paper_height;
	};

	sigc::signal<void> signal_init_failed(){ return m_signal_init_failed; }
	sigc::signal<void,const std::vector<ScanDevice>&> signal_update_devices(){ return m_signal_update_devices; }
	sigc::signal<void,const std::string&> signal_scan_failed(){ return m_signal_scan_failed; }
	sigc::signal<void,ScanState> signal_scanning_changed(){ return m_signal_scanning_changed; }
	sigc::signal<void,const std::string&> signal_page_available(){ return m_signal_page_available; }

	void start();
	void redetect();
	void scan(const std::string& device, ScanOptions m_options);
	void cancel();
	void stop();

	static Scanner* get_instance();

protected:
	enum class StartStatus { Fail = 0, Success, NoDocs };
	enum class ReadStatus { Fail = 0, Done, Reading };
	enum class PageStatus { Done = 0, AnotherPass };

	struct ScanJob {
		std::string device;
		double dpi;
		Scanner::ScanMode scan_mode;
		int depth;
		Scanner::ScanType type;
		int page_width;
		int page_height;
		std::string filename;

		int pass_number = 0;
		int page_number = 0;
	};

	Scanner() = default;

private:
	struct Request {
		enum class Type { Redetect, StartScan, Cancel, Quit } type;
		void* data;
	};

	ScanState m_state = ScanState::IDLE;
	Glib::Threads::Thread* m_thread = nullptr;
	Utils::AsyncQueue<Request> m_requestQueue;
	std::queue<ScanJob*> m_jobQueue;

	sigc::signal<void> m_signal_init_failed;
	sigc::signal<void,const std::vector<ScanDevice>&> m_signal_update_devices;
	sigc::signal<void,const std::string&> m_signal_scan_failed;
	sigc::signal<void,ScanState> m_signal_scanning_changed;
	sigc::signal<void,const std::string&> m_signal_page_available;

	// Scan thread functions
	void do_complete_document();
	void do_complete_page();
	void do_set_options();
	void do_get_parameters();
	void do_open();
	void do_read();
	void do_redetect();
	void do_start();
	void do_close();
	void set_state(ScanState state);
	void fail_scan(const Glib::ustring& error_string);
	void scan_thread();

	// Backend interface
	virtual bool initBackend() = 0;
	virtual void closeBackend() = 0;
	virtual std::vector<ScanDevice> detectDevices() = 0;
	virtual bool openDevice(const std::string& device) = 0;
	virtual void closeDevice() = 0;
	virtual void setOptions(ScanJob* job) = 0;
	virtual StartStatus startDevice() = 0;
	virtual bool getParameters() = 0;
	virtual ReadStatus read() = 0;
	virtual PageStatus completePage(ScanJob* job) = 0;

	static Glib::ustring get_scan_mode_string(ScanMode mode);
	static Glib::ustring get_scan_type_string(ScanType type);
};

#endif // SCANNER_HPP

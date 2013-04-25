/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SaneScanner.hh
 * Based on code from Simple Scan, which is:
 *   Copyright (C) 2009-2013 Canonical Ltd.
 *   Author: Robert Ancell <robert.ancell@canonical.com>
 * Modifications are:
 *   Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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

#ifndef SANESCANNER_HPP
#define SANESCANNER_HPP

#include "common.hh"
#include "AsyncQueue.hh"
#include <sane/sane.h>
#include <sane/saneopts.h>

class Scanner {
public:
	enum class ScanMode { DEFAULT = 0, COLOR, GRAY, LINEART };
	enum class ScanType { SINGLE = 0, ADF_FRONT, ADF_BACK, ADF_BOTH };
	enum class Channel { GRAY = 0, RGB, RED, GREEN, BLUE };

	struct ScanDevice {
		std::string name;
		std::string label;
	};

	struct ScanPageInfo {
		int width;			// Page width in pixels
		int height;			// Page heigth in pixels
		int depth;			// Bit depth
		int n_channels;		// Number of color channels
		double dpi;			// Resolution
		std::string device;	// The device this page came from
	};

	struct ScanLine {
		int number;							// Line number
		int n_lines;						// Number of lines in this packet
		int width;							// Width in pixels
		int depth;							// Bit depth
		Channel channel;					// Channel for this line
		std::vector<unsigned char> data;	// Raw line data
	};

	struct ScanOptions {
		int dpi;
		ScanMode scan_mode;
		int depth;
		ScanType type;
		int paper_width;
		int paper_height;
	};

	struct PageHandler {
		virtual ~PageHandler() {}
		virtual void setupPage(const ScanPageInfo& info) = 0;
		virtual void handleData(const ScanLine& line) = 0;
		virtual void finalizePage() = 0;
	};

	sigc::signal<void> signal_init_failed(){ return m_signal_init_failed; }
	sigc::signal<void,const std::vector<ScanDevice>&> signal_update_devices(){ return m_signal_update_devices; }
	sigc::signal<void,const std::string&> signal_request_authorization(){ return m_signal_request_authorization; }
	sigc::signal<void,int,const std::string&> signal_scan_failed(){ return m_signal_scan_failed; }
	sigc::signal<void,bool> signal_scanning_changed(){ return m_signal_scanning_changed; }

	void start();
	void redetect();
	void scan(const std::string& device, PageHandler* handler, ScanOptions m_options);
	void cancel();
	void stop();
	void authorize(const Glib::ustring& username, const Glib::ustring& password){ m_authorizeQueue.push({username, password}); }

	static Scanner* get_instance();

private:
	enum class ScanState { IDLE = 0, OPEN, GET_OPTION, START, GET_PARAMETERS, READ };

	struct Credentials {
		Glib::ustring username;
		Glib::ustring password;
	};

	struct Request {
		enum class Type { Redetect, StartScan, Cancel, Quit } type;
		void* data;
	};

	struct ScanJob;

	ScanState m_state;
	SANE_Handle m_handle = nullptr;
	std::map<std::string, int> m_options;
	Glib::Threads::Thread* m_thread = nullptr;
	AsyncQueue<Request> m_requestQueue;
	AsyncQueue<Credentials> m_authorizeQueue;
	std::queue<ScanJob*> m_jobQueue;

	sigc::signal<void> m_signal_init_failed;
	sigc::signal<void,const std::vector<ScanDevice>&> m_signal_update_devices;
	sigc::signal<void,const std::string&> m_signal_request_authorization;
	sigc::signal<void,int,const std::string&> m_signal_scan_failed;
	sigc::signal<void,bool> m_signal_scanning_changed;

	static Scanner* s_instance;
	Scanner(){}

	const SANE_Option_Descriptor *get_option_by_name(SANE_Handle, const std::string& name, int& index);
	bool set_default_option(SANE_Handle m_handle, const SANE_Option_Descriptor *option, SANE_Int option_index);
	void set_bool_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, bool value, bool*result);
	void set_int_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, int value, int* result);
	void set_fixed_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, double value, double* result);
	bool set_string_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, const std::string& value, std::string* result);
	bool set_constrained_string_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, const std::vector<std::string>& values, std::string *result);
	void log_option(SANE_Int index, const SANE_Option_Descriptor* option);

	void close_device();
	void fail_scan(int error_code, const Glib::ustring& error_string);

	void do_complete_document();
	void do_complete_page();
	void do_get_option();
	void do_get_parameters();
	void do_open();
	void do_read();
	void do_redetect();
	void do_start();
	void scan_thread();

	static void authorization_cb(const char* resource, char* username, char* password);
	static int get_device_weight(const std::string& device);
	static int compare_devices(const ScanDevice& device1, const ScanDevice& device2);
	static Glib::ustring get_frame_mode_string(SANE_Frame frame);
	static Glib::ustring get_scan_mode_string(ScanMode mode);
	static Glib::ustring get_scan_type_string(ScanType type);
};

#endif // SANESCANNER_HPP

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Scanner.hh
 *   Copyright (C) 2013-2018 Sandro Mani <manisandro@gmail.com>
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

#ifndef SCANNER_HH
#define SCANNER_HH

#include "common.hh"

class Scanner {
public:
	enum class State { IDLE = 0, OPEN, SET_OPTIONS, START, GET_PARAMETERS, READ };
	enum class ScanMode { DEFAULT = 0, COLOR, GRAY, LINEART };
	enum class ScanType { SINGLE = 0, ADF_FRONT, ADF_BACK, ADF_BOTH };

	struct Device {
		std::string name;
		std::string label;
	};

	struct Params {
		std::string device;
		std::string filename;
		double dpi;
		ScanMode scan_mode;
		int depth;
		ScanType type;
		int page_width;
		int page_height;
	};

	virtual ~Scanner() { }

	virtual void init() = 0;
	virtual void redetect() = 0;
	virtual void scan(const Params& params) = 0;
	virtual void cancel() = 0;
	virtual void close() = 0;

	sigc::signal<void> signal_initFailed() const {
		return m_signal_initFailed;
	}
	sigc::signal<void, std::vector<Scanner::Device>> signal_devicesDetected() const {
		return m_signal_devicesDetected;
	}
	sigc::signal<void, Glib::ustring> signal_scanFailed() const {
		return m_signal_scanFailed;
	}
	sigc::signal<void, Scanner::State> signal_scanStateChanged() const {
		return m_signal_scanStateChanged;
	}
	sigc::signal<void, std::string> signal_pageAvailable() const {
		return m_signal_pageAvailable;
	}

protected:
	sigc::signal<void> m_signal_initFailed;
	sigc::signal<void, std::vector<Scanner::Device>> m_signal_devicesDetected;
	sigc::signal<void, Glib::ustring> m_signal_scanFailed;
	sigc::signal<void, Scanner::State> m_signal_scanStateChanged;
	sigc::signal<void, std::string> m_signal_pageAvailable;
};


#endif // SCANNER_HH

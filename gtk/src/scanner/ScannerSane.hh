/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScannerSane.hh
 * Based on code from Simple Scan, which is:
 *   Copyright (C) 2009-2013 Canonical Ltd.
 *   Author: Robert Ancell <robert.ancell@canonical.com>
 * Modifications are:
 *   Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#ifndef SCANNER_SANE_HH
#define SCANNER_SANE_HH

#include "../Scanner.hh"
#include "../Utils.hh"
#include <sane/sane.h>

namespace std { class thread; }

class ScannerSane : public Scanner {
public:
	void init() override;
	void redetect() override;
	void scan(const Params& params) override;
	void cancel() override;
	void close() override;

private:
	struct ScanJob {
		ScanJob(const Params& _params) : params(_params) {}
		Params params;
		SANE_Handle handle = nullptr;
		SANE_Parameters parameters;
		std::vector<uint8_t> lineBuffer;
		std::vector<uint8_t> imgbuf;
		int nUsed = 0;		// Number of used bytes in the line buffer
		int lineCount = 0;	// Number of read lines
		int height = 0;
		int rowstride = 0;
		int pageNumber = 0;
	};

	struct Request {
		enum class Type { Redetect, StartScan, Cancel, Quit } type;
		ScanJob* job;
	};

	std::thread* m_thread = nullptr;
	State m_state = State::IDLE;
	Utils::AsyncQueue<Request> m_requestQueue;
	ScanJob* m_job = nullptr;
	bool m_finished = false;

	void run();
	void doRedetect();
	void doOpen();
	void doSetOptions();
	void doStart();
	void doGetParameters();
	void doRead();
	void doCompletePage();
	void doStop();
	void setState(State state);
	void failScan(const Glib::ustring& errorString);

	const SANE_Option_Descriptor* getOptionByName(const std::map<std::string, int>& options, SANE_Handle, const std::string& name, int& index);
	bool setDefaultOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index);
	void setBoolOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, bool value, bool* result);
	void setIntOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, int value, int* result);
	void setFixedOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, double value, double* result);
	bool setStringOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, const std::string& value, std::string* result);
	bool setConstrainedStringOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, const std::vector<std::string>& values, std::string* result);
	void logOption(SANE_Int index, const SANE_Option_Descriptor* option);

	static Glib::ustring getFrameModeString(SANE_Frame frame);
	static Glib::ustring getErrorMessage(SANE_Status status, const Glib::ustring& defaultMessage);
};

typedef ScannerSane ScannerImpl;

#endif // SCANNER_SANE_HH

 /* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScanBackend.hh
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

#ifndef SCANBACKEND_HPP
#define SCANBACKEND_HPP

#include <vector>
#include <glibmm/ustring.h>

class ScanBackend {
public:
	enum class ScanMode { DEFAULT = 0, COLOR, GRAY, LINEART };
	enum class ScanType { SINGLE = 0, ADF_FRONT, ADF_BACK, ADF_BOTH };
	enum class StartStatus { Fail = 0, Success, NoDocs };
	enum class ReadStatus { Fail = 0, Done, Reading };
	enum class PageStatus { Done = 0, AnotherPass };

	struct Device {
		std::string name;
		std::string label;
	};

	struct Options {
		std::string filename;
		int dpi;
		ScanMode scan_mode;
		int depth;
		ScanType type;
		int paper_width;
		int paper_height;
	};

	struct Job {
		std::string device;
		double dpi;
		ScanMode scan_mode;
		int depth;
		ScanType type;
		int page_width;
		int page_height;
		std::string filename;

		int pass_number = 0;
		int page_number = 0;
	};

	virtual ~ScanBackend(){}
	virtual bool init() = 0;
	virtual void close() = 0;
	virtual std::vector<Device> detectDevices() = 0;
	virtual bool openDevice(const std::string& device) = 0;
	virtual void closeDevice() = 0;
	virtual void setOptions(Job* job) = 0;
	virtual StartStatus startDevice() = 0;
	virtual bool getParameters() = 0;
	virtual ReadStatus read() = 0;
	virtual PageStatus completePage(Job* job) = 0;

	static Glib::ustring getScanModeString(ScanMode mode)
	{
		Glib::ustring modestr[] = {"ScanMode::DEFAULT", "ScanMode::COLOR", "ScanMode::GRAY", "ScanMode::LINEART"};
		if(mode <= ScanMode::LINEART){
			return modestr[static_cast<int>(mode)];
		}else{
			return Glib::ustring::compose("ScanMode(%1)", int(mode));
		}
	}

	static Glib::ustring getScanTypeString(ScanType type)
	{
		Glib::ustring typestr[] = {"ScanType::SINGLE", "ScanType::ADF_FRONT", "ScanType::ADF_BACK", "ScanType::ADF_BOTH"};
		if(type <= ScanType::ADF_BOTH){
			return typestr[static_cast<int>(type)];
		}else{
			return Glib::ustring::compose("ScanType(%1)", int(type));
		}
	}
};

#endif // SCANBACKEND_HPP

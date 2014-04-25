/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScannerSane.hh
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

#ifndef SCANNERSANE_HH
#define SCANNERSANE_HH

#include "Scanner.hh"
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <sane/sane.h>
#include <sane/saneopts.h>

typedef class ScannerSane ScannerImpl;

class ScannerSane : public Scanner{
private:
	SANE_Handle m_handle = nullptr;
	std::vector<unsigned char> m_buffer;	// Buffer for received line
	int m_nUsed = 0;						// Number of used bytes in the buffer
	int m_lineCount = 0;					// Number of read lines
	SANE_Parameters m_parameters;

	int m_height, m_rowstride;
	std::vector<uint8_t> m_buf;

	const SANE_Option_Descriptor* get_option_by_name(const std::map<std::string, int>& options, SANE_Handle, const std::string& name, int& index);
	bool set_default_option(SANE_Handle m_handle, const SANE_Option_Descriptor *option, SANE_Int option_index);
	void set_bool_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, bool value, bool*result);
	void set_int_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, int value, int* result);
	void set_fixed_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, double value, double* result);
	bool set_string_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, const std::string& value, std::string* result);
	bool set_constrained_string_option(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, const std::vector<std::string>& values, std::string *result);
	void log_option(SANE_Int index, const SANE_Option_Descriptor* option);

	static Glib::ustring get_frame_mode_string(SANE_Frame frame);

	bool initBackend();
	void closeBackend();
	std::vector<ScanDevice> detectDevices();
	bool openDevice(const std::string& device);
	void closeDevice();
	void setOptions(ScanJob* job);
	StartStatus startDevice();
	bool getParameters();
	ReadStatus read();
	PageStatus completePage(ScanJob* job);
};

/********** ScannerSane Public Methods **********/

bool ScannerSane::initBackend()
{
	SANE_Int version_code;
	SANE_Status status = sane_init(&version_code, nullptr);
	g_debug("sane_init() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		g_warning("Unable to initialize SANE backend: %s", sane_strstatus(status));
		return false;
	}
	g_debug("SANE version %d.%d.%d", SANE_VERSION_MAJOR(version_code), SANE_VERSION_MINOR(version_code), SANE_VERSION_BUILD(version_code));
	return true;
}

void ScannerSane::closeBackend()
{
	g_debug("sane_exit()");
	sane_exit();
}

std::vector<Scanner::ScanDevice> ScannerSane::detectDevices()
{
	std::vector<ScanDevice> devices;

	const SANE_Device** device_list = nullptr;
	SANE_Status status = sane_get_devices(&device_list, false);
	g_debug("sane_get_devices() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		g_warning("Unable to get SANE devices: %s", sane_strstatus(status));
		return devices;
	}

	for(int i = 0; device_list[i] != nullptr; ++i){
		g_debug("Device: name=\"%s\" vendor=\"%s\" model=\"%s\" type=\"%s\"",
			   device_list[i]->name, device_list[i]->vendor, device_list[i]->model, device_list[i]->type);

		ScanDevice scan_device;
		scan_device.name = device_list[i]->name;

		/* Abbreviate HP as it is a long string and does not match what is on the physical scanner */
		Glib::ustring vendor = device_list[i]->vendor;
		if(vendor == "Hewlett-Packard")
			vendor = "HP";

		scan_device.label = Glib::ustring::compose("%1 %2", vendor, device_list[i]->model);
		std::replace(scan_device.label.begin(), scan_device.label.end(), '_', ' ');

		devices.push_back(scan_device);
	}
	return devices;
}

bool ScannerSane::openDevice(const std::string &device)
{
	SANE_Status status = sane_open(device.c_str(), &m_handle);
	g_debug("sane_open(\"%s\") -> %s", device.c_str(), sane_strstatus(status));

	if(status != SANE_STATUS_GOOD){
		g_warning("Unable to get open device: %s", sane_strstatus(status));
		return false;
	}
	return true;
}

void ScannerSane::closeDevice()
{
	if(m_handle != nullptr){
		sane_close(m_handle);
		g_debug("sane_close()");
		m_handle = nullptr;
	}
}

void ScannerSane::setOptions(ScanJob* job)
{
	int index = 0;
	const SANE_Option_Descriptor* option;
	std::map<std::string, int> options;

	/* Build the option table */
	while((option = sane_get_option_descriptor(m_handle, index)) != nullptr){
		log_option(index, option);
		if(
			/* Ignore groups */
			option->type != SANE_TYPE_GROUP &&
			/* Option disabled */
			(option->cap & SANE_CAP_INACTIVE) == 0 &&
			/* Some options are unnamed (e.g. Option 0) */
			option->name != nullptr && strlen(option->name) > 0
		){
			options.insert(std::make_pair(std::string(option->name), index));
		}
		++index;
	}

	/* Apply settings */

	/* Pick source */
	option = get_option_by_name(options, m_handle, SANE_NAME_SCAN_SOURCE, index);
	if(option != nullptr){
		std::vector<std::string> flatbed_sources = {
			"Auto",
			SANE_I18N("Auto"),
			"Flatbed",
			SANE_I18N("Flatbed"),
			"FlatBed",
			"Normal",
			SANE_I18N("Normal")
		};
		std::vector<std::string> adf_sources = {
			"Automatic Document Feeder",
			SANE_I18N("Automatic Document Feeder"),
			"ADF",
			"Automatic Document Feeder(left aligned)", /* Seen in the proprietary brother3 driver */
			"Automatic Document Feeder(centrally aligned)" /* Seen in the proprietary brother3 driver */
		};
		std::vector<std::string> adf_front_sources = {
			"ADF Front",
			SANE_I18N("ADF Front")
		};
		std::vector<std::string> adf_back_sources = {
			"ADF Back",
			SANE_I18N("ADF Back")
		};
		std::vector<std::string> adf_duplex_sources = {
			"ADF Duplex",
			SANE_I18N("ADF Duplex")
		};

		switch(job->type) {
		case ScanType::SINGLE:
			if(!set_default_option(m_handle, option, index))
				if(!set_constrained_string_option(m_handle, option, index, flatbed_sources, nullptr))
					g_warning("Unable to set single page source, please file a bug");
			break;
		case ScanType::ADF_FRONT:
			if(!set_constrained_string_option(m_handle, option, index, adf_front_sources, nullptr))
				if(!set_constrained_string_option(m_handle, option, index, adf_sources, nullptr))
					g_warning("Unable to set front ADF source, please file a bug");
			break;
		case ScanType::ADF_BACK:
			if(!set_constrained_string_option(m_handle, option, index, adf_back_sources, nullptr))
				if(!set_constrained_string_option(m_handle, option, index, adf_sources, nullptr))
					g_warning("Unable to set back ADF source, please file a bug");
			break;
		case ScanType::ADF_BOTH:
			if(!set_constrained_string_option(m_handle, option, index, adf_duplex_sources, nullptr))
				if(!set_constrained_string_option(m_handle, option, index, adf_sources, nullptr))
					g_warning("Unable to set duplex ADF source, please file a bug");
			break;
		}
	}

	/* Scan mode (before resolution as it tends to affect that */
	option = get_option_by_name(options, m_handle, SANE_NAME_SCAN_MODE, index);
	if(option != nullptr){
		/* The names of scan modes often used in drivers, as taken from the sane-backends source */
		std::vector<std::string> color_scan_modes = {
			SANE_VALUE_SCAN_MODE_COLOR,
			"Color",
			"24bit Color" /* Seen in the proprietary brother3 driver */
		};
		std::vector<std::string> gray_scan_modes = {
			SANE_VALUE_SCAN_MODE_GRAY,
			"Gray",
			"Grayscale",
			SANE_I18N("Grayscale"),
			"True Gray" /* Seen in the proprietary brother3 driver */
		};
		std::vector<std::string> lineart_scan_modes = {
			SANE_VALUE_SCAN_MODE_LINEART,
			"Lineart",
			"LineArt",
			SANE_I18N("LineArt"),
			"Black & White",
			SANE_I18N("Black & White"),
			"Binary",
			SANE_I18N("Binary"),
			"Thresholded",
			SANE_VALUE_SCAN_MODE_GRAY,
			"Gray",
			"Grayscale",
			SANE_I18N("Grayscale"),
			"True Gray" /* Seen in the proprietary brother3 driver */
		};

		switch(job->scan_mode) {
		case ScanMode::COLOR:
			if(!set_constrained_string_option(m_handle, option, index, color_scan_modes, nullptr))
				g_warning("Unable to set Color mode, please file a bug");
			break;
		case ScanMode::GRAY:
			if(!set_constrained_string_option(m_handle, option, index, gray_scan_modes, nullptr))
				g_warning("Unable to set Gray mode, please file a bug");
			break;
		case ScanMode::LINEART:
			if(!set_constrained_string_option(m_handle, option, index, lineart_scan_modes, nullptr))
				g_warning("Unable to set Lineart mode, please file a bug");
			break;
		default:
			break;
		}
	}

	/* Duplex */
	option = get_option_by_name(options, m_handle, "duplex", index);
	if(option != nullptr && option->type == SANE_TYPE_BOOL)
		set_bool_option(m_handle, option, index, job->type == ScanType::ADF_BOTH, nullptr);

	/* Multi-page options */
	option = get_option_by_name(options, m_handle, "batch-scan", index);
	if(option != nullptr && option->type == SANE_TYPE_BOOL)
		set_bool_option(m_handle, option, index, job->type != ScanType::SINGLE, nullptr);

	/* Disable compression, we will compress after scanning */
	option = get_option_by_name(options, m_handle, "compression", index);
	if(option != nullptr){
		std::vector<std::string> disable_compression_names = {
			SANE_I18N("None"),
			SANE_I18N("none"),
			"None",
			"none"
		};

		if(!set_constrained_string_option(m_handle, option, index, disable_compression_names, nullptr))
			g_warning("Unable to disable compression, please file a bug");
	}

	/* Set resolution and bit depth */
	option = get_option_by_name(options, m_handle, SANE_NAME_SCAN_RESOLUTION, index);
	if(option != nullptr){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, job->dpi, &job->dpi);
		else{
			int dpi;
			set_int_option(m_handle, option, index, job->dpi, &dpi);
			job->dpi = dpi;
		}
		option = get_option_by_name(options, m_handle, SANE_NAME_BIT_DEPTH, index);
		if(option != nullptr && job->depth > 0)
			set_int_option(m_handle, option, index, job->depth, nullptr);
	}

	/* Always use maximum scan area - some scanners default to using partial areas.  This should be patched in sane-backends */
	option = get_option_by_name(options, m_handle, SANE_NAME_SCAN_BR_X, index);
	if(option != nullptr && option->constraint_type == SANE_CONSTRAINT_RANGE){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, SANE_UNFIX(option->constraint.range->max), nullptr);
		else
			set_int_option(m_handle, option, index, option->constraint.range->max, nullptr);
	}
	option = get_option_by_name(options, m_handle, SANE_NAME_SCAN_BR_Y, index);
	if(option != nullptr && option->constraint_type == SANE_CONSTRAINT_RANGE){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, SANE_UNFIX(option->constraint.range->max), nullptr);
		else
			set_int_option(m_handle, option, index, option->constraint.range->max, nullptr);
	}

	/* Set page dimensions */
	option = get_option_by_name(options, m_handle, SANE_NAME_PAGE_WIDTH, index);
	if(option != nullptr && job->page_width > 0.0){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, job->page_width / 10.0, nullptr);
		else
			set_int_option(m_handle, option, index, job->page_width / 10, nullptr);
	}
	option = get_option_by_name(options, m_handle, SANE_NAME_PAGE_HEIGHT, index);
	if(option != nullptr && job->page_height > 0.0){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, job->page_height / 10.0, nullptr);
		else
			set_int_option(m_handle, option, index, job->page_height / 10, nullptr);
	}
}

Scanner::StartStatus ScannerSane::startDevice()
{
	SANE_Status status = sane_start(m_handle);
	g_debug("sane_start -> %s", sane_strstatus(status));
	if(status == SANE_STATUS_GOOD)
		return StartStatus::Success;
	else if(status == SANE_STATUS_NO_DOCS)
		return StartStatus::NoDocs;
	g_warning("Unable to start device: %s", sane_strstatus(status));
	return StartStatus::Fail;
}

bool ScannerSane::getParameters()
{
	SANE_Status status = sane_get_parameters(m_handle, &m_parameters);
	g_debug("sane_get_parameters() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		g_warning("Unable to get device parameters: %s", sane_strstatus(status));
		return false;
	}

	g_debug("Parameters: format=%s last_frame=%s bytes_per_line=%d pixels_per_line=%d lines=%d depth=%d",
			get_frame_mode_string(m_parameters.format).c_str(),
			m_parameters.last_frame ? "SANE_TRUE" : "SANE_FALSE",
			m_parameters.bytes_per_line,
			m_parameters.pixels_per_line,
			m_parameters.lines,
			m_parameters.depth);

	m_height = std::max(1, m_parameters.lines);
	m_rowstride = m_parameters.pixels_per_line * 3; // Buffer is for a 24 bit RGB image
	m_buf.resize(m_rowstride * m_parameters.lines);

	/* Prepare for read */
	m_buffer.resize(m_parameters.bytes_per_line);
	m_nUsed = 0;
	m_lineCount = 0;
	return true;
}

Scanner::ReadStatus ScannerSane::read()
{
	/* Read as many bytes as we need to complete a line */
	int n_to_read = m_buffer.size() - m_nUsed;

	SANE_Int n_read;
	unsigned char* b = &m_buffer[m_nUsed];
	SANE_Status status = sane_read(m_handle, b, n_to_read, &n_read);
	g_debug("sane_read(%d) -> (%s, %d)", n_to_read, sane_strstatus(status), n_read);

	/* Completed read */
	if(status == SANE_STATUS_EOF){
		if(m_parameters.lines > 0 && m_lineCount != m_parameters.lines)
			g_warning("Scan completed with %d lines, expected %d lines", m_lineCount, m_parameters.lines);
		if(m_nUsed > 0)
			g_warning("Scan complete with %d bytes of unused data", m_nUsed);
		return ReadStatus::Done;
	}else if(status != SANE_STATUS_GOOD){
		g_warning("Unable to read frame from device: %s", sane_strstatus(status));
		return ReadStatus::Fail;
	}

	m_nUsed += n_read;

	/* If we completed a line, feed it out */
	if(m_nUsed >= int(m_buffer.size())){
		assert(m_nUsed == m_parameters.bytes_per_line);
		++m_lineCount;

		// Resize image if necessary
		if(m_lineCount > m_height){
			m_height = m_lineCount;
			m_buf.resize(m_rowstride * m_height);
		}

		// Write data
		int offset = (m_lineCount - 1) * m_rowstride;
		if(m_parameters.format == SANE_FRAME_GRAY){
			for(std::size_t i = 0; i < m_buffer.size(); ++i){
				memset(&m_buf[offset + 3*i], m_buffer[i], 3);
			}
		}else if(m_parameters.format == SANE_FRAME_RGB){
			std::memcpy(&m_buf[offset], &m_buffer[0], m_buffer.size());
		}else if(m_parameters.format == SANE_FRAME_RED){
			for(std::size_t i = 0; i < m_buffer.size(); ++i){
				m_buf[offset + 3*i + 0] = m_buffer[i];
			}
		}else if(m_parameters.format == SANE_FRAME_GREEN){
			for(std::size_t i = 0; i < m_buffer.size(); ++i){
				m_buf[offset + 3*i + 1] = m_buffer[i];
			}
		}else if(m_parameters.format == SANE_FRAME_BLUE){
			for(std::size_t i = 0; i < m_buffer.size(); ++i){
				m_buf[offset + 3*i + 2] = m_buffer[i];
			}
		}

		/* Reset buffer */
		m_nUsed = 0;
		m_buffer.resize(m_parameters.bytes_per_line);
	}

	return ReadStatus::Reading;
}

Scanner::PageStatus ScannerSane::completePage(ScanJob* job)
{
	m_buf.clear();
	if(!m_parameters.last_frame){
		return PageStatus::AnotherPass;
	}
	Gdk::Pixbuf::create_from_data(m_buf.data(), Gdk::COLORSPACE_RGB, false, 8, m_rowstride/3, m_height, m_rowstride)->save(job->filename, "png");
	return PageStatus::Done;
}

/*********** ScannerSane Option stuff ***********/
const SANE_Option_Descriptor* ScannerSane::get_option_by_name(const std::map<std::string,int>& options, SANE_Handle, const std::string& name, int &index)
{
	std::map<std::string, int>::const_iterator it = options.find(name);
	if(it != options.end()){
		index = it->second;
		return sane_get_option_descriptor(m_handle, index);
	}
	index = 0;
	return nullptr;
}

bool ScannerSane::set_default_option(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index)
{
	/* Check if supports automatic option */
	if((option->cap & SANE_CAP_AUTOMATIC) == 0)
		return false;

	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_AUTO, nullptr, nullptr);
	g_debug("sane_control_option(%d, SANE_ACTION_SET_AUTO) -> %s", option_index, sane_strstatus(status));
	if(status != SANE_STATUS_GOOD)
		g_warning("Error setting default option %s: %s", option->name, sane_strstatus(status));
	return status == SANE_STATUS_GOOD;
}

void ScannerSane::set_bool_option(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index, bool value, bool* result)
{
	g_return_if_fail(option->type == SANE_TYPE_BOOL);

	SANE_Bool v = static_cast<SANE_Bool>(value);
	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, &v, nullptr);
	if(result) *result = static_cast<bool>(v);
	g_debug("sane_control_option(%d, SANE_ACTION_SET_VALUE, %s) -> (%s, %s)", option_index, value ? "SANE_TRUE" : "SANE_FALSE", sane_strstatus(status), result ? "SANE_TRUE" : "SANE_FALSE");
}

void ScannerSane::set_int_option(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index, int value, int* result)
{
	g_return_if_fail(option->type == SANE_TYPE_INT);

	SANE_Word v = static_cast<SANE_Word>(value);
	if(option->constraint_type == SANE_CONSTRAINT_RANGE){
		if(option->constraint.range->quant != 0)
			v *= option->constraint.range->quant;
		v = std::max(option->constraint.range->min, std::min(option->constraint.range->max, v));
	}else if(option->constraint_type == SANE_CONSTRAINT_WORD_LIST){
		int distance = std::numeric_limits<int>::max();
		int nearest = 0;

		/* Find nearest value to requested */
		for(int i = 0; i < option->constraint.word_list[0]; ++i){
			int x = option->constraint.word_list[i+1];
			int d = std::abs(x - v);
			if(d < distance){
				distance = d;
				nearest = x;
			}
		}
		v = nearest;
	}

	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, &v, nullptr);
	g_debug("sane_control_option(%d, SANE_ACTION_SET_VALUE, %d) -> (%s, %d)", option_index, value, sane_strstatus(status), v);
	if(result) *result = v;
}

void ScannerSane::set_fixed_option(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, double value, double* result)
{
	g_return_if_fail(option->type == SANE_TYPE_FIXED);

	double v = value;
	if(option->constraint_type == SANE_CONSTRAINT_RANGE){
		double min = SANE_UNFIX(option->constraint.range->min);
		double max = SANE_UNFIX(option->constraint.range->max);
		v = std::max(min, std::min(max, v));
	}else if(option->constraint_type == SANE_CONSTRAINT_WORD_LIST){
		double distance = std::numeric_limits<double>::max();
		double nearest = 0.0;

		/* Find nearest value to requested */
		for(int i = 0; i < option->constraint.word_list[0]; ++i){
			double x = SANE_UNFIX(option->constraint.word_list[i+1]);
			double d = std::abs(x - v);
			if(d < distance){
				distance = d;
				nearest = x;
			}
		}
		v = nearest;
	}

	SANE_Fixed v_fixed = SANE_FIX(v);
	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, &v_fixed, nullptr);
	g_debug("sane_control_option(%d, SANE_ACTION_SET_VALUE, %f) -> (%s, %f)", option_index, value, sane_strstatus(status), SANE_UNFIX(v_fixed));

	if(result) *result = SANE_UNFIX(v_fixed);
}

bool ScannerSane::set_string_option(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, const std::string &value, std::string* result)
{
	g_return_val_if_fail(option->type == SANE_TYPE_STRING, false);
	char* v = new char[value.size() + 1]; // +1: \0
	strncpy(v, value.c_str(), value.size()+1);
	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, v, nullptr);
	g_debug("sane_control_option(%d, SANE_ACTION_SET_VALUE, \"%s\") -> (%s, \"%s\")", option_index, value.c_str(), sane_strstatus(status), v);
	if(result) *result = v;
	delete[] v;
	return status == SANE_STATUS_GOOD;
}

bool ScannerSane::set_constrained_string_option(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, const std::vector<std::string> &values, std::string* result)
{
	g_return_val_if_fail(option->type == SANE_TYPE_STRING, false);
	g_return_val_if_fail(option->constraint_type == SANE_CONSTRAINT_STRING_LIST, false);

	for(const std::string& value : values){
		for(int j = 0; option->constraint.string_list[j] != nullptr; ++j){
			if(value == option->constraint.string_list[j]){
				return set_string_option(handle, option, option_index, value, result);
			}
		}
	}
	if(result) *result = "";
	return false;
}

void ScannerSane::log_option(SANE_Int index, const SANE_Option_Descriptor *option)
{
	Glib::ustring s = Glib::ustring::compose("Option %1:", index);

	if(option->name && strlen(option->name) > 0)
		s += Glib::ustring::compose(" name='%1'", option->name);

	if(option->title && strlen(option->title) > 0)
		s += Glib::ustring::compose(" title='%1'", option->title);

	Glib::ustring typestr[] = {"bool", "int", "fixed", "string", "button", "group"};
	if(option->type <= SANE_TYPE_GROUP){
		s += Glib::ustring::compose(" type=%1", typestr[option->type]);
	}else{
		s += Glib::ustring::compose(" type=%1", option->type);
	}

	s += Glib::ustring::compose(" size=%1", option->size);

	Glib::ustring unitstr[] = {"none", "pixel", "bit", "mm", "dpi", "percent", "microseconds"};
	if(option->unit <= SANE_UNIT_MICROSECOND){
		s += Glib::ustring::compose(" unit=%1", unitstr[option->unit]);
	}else{
		s += Glib::ustring::compose(" unit=%1", option->unit);
	}

	switch(option->constraint_type){
	case SANE_CONSTRAINT_RANGE:
		if(option->type == SANE_TYPE_FIXED)
			s += Glib::ustring::compose(" min=%1, max=%2, quant=%3", SANE_UNFIX(option->constraint.range->min), SANE_UNFIX(option->constraint.range->max), option->constraint.range->quant);
		else
			s += Glib::ustring::compose(" min=%1, max=%2, quant=%3", option->constraint.range->min, option->constraint.range->max, option->constraint.range->quant);
		break;
	case SANE_CONSTRAINT_WORD_LIST:
		s += " values=[";
		for(int i = 0; i < option->constraint.word_list[0]; ++i){
			if(i > 0)
				s += ", ";
			if(option->type == SANE_TYPE_INT)
				s += Glib::ustring::compose("%1", option->constraint.word_list[i+1]);
			else
				s += Glib::ustring::compose("%1", SANE_UNFIX(option->constraint.word_list[i+1]));
		}
		s += "]";
		break;
	case SANE_CONSTRAINT_STRING_LIST:
		s += " values=[";
		for(int i = 0; option->constraint.string_list[i] != nullptr; ++i){
			if(i > 0)
				s += ", ";
			s += Glib::ustring::compose("\"%1\"", option->constraint.string_list[i]);
		}
		s += "]";
		break;
	default:
		break;
	}

	int cap = option->cap;
	if(cap != 0){
		s += " cap=";
		if((cap & SANE_CAP_SOFT_SELECT) != 0){
			s += "soft-select,";
			cap &= ~SANE_CAP_SOFT_SELECT;
		}
		if((cap & SANE_CAP_HARD_SELECT) != 0){
			s += "hard-select,";
			cap &= ~SANE_CAP_HARD_SELECT;
		}
		if((cap & SANE_CAP_SOFT_DETECT) != 0){
			s += "soft-detect,";
			cap &= ~SANE_CAP_SOFT_DETECT;
		}
		if((cap & SANE_CAP_EMULATED) != 0){
			s += "emulated,";
			cap &= ~SANE_CAP_EMULATED;
		}
		if((cap & SANE_CAP_AUTOMATIC) != 0){
			s += "automatic,";
			cap &= ~SANE_CAP_AUTOMATIC;
		}
		if((cap & SANE_CAP_INACTIVE) != 0){
			s += "inactive,";
			cap &= ~SANE_CAP_INACTIVE;
		}
		if((cap & SANE_CAP_ADVANCED) != 0){
			s += "advanced,";
			cap &= ~SANE_CAP_ADVANCED;
		}
		/* Unknown capabilities */
		if(cap != 0){
			s += Glib::ustring::compose("unknown (%1)", cap);
		}
	}

	g_debug("%s", s.c_str());
	if(option->desc != nullptr)
		g_debug("  Description: %s", option->desc);
}


/****************** Enum to string ******************/

Glib::ustring ScannerSane::get_frame_mode_string(SANE_Frame frame)
{
	Glib::ustring framestr[] = {"SANE_FRAME_GRAY", "SANE_FRAME_RGB", "SANE_FRAME_RED", "SANE_FRAME_GREEN", "SANE_FRAME_BLUE"};
	if(frame <= SANE_FRAME_BLUE){
		return framestr[frame];
	}else{
		return Glib::ustring::compose("SANE_FRAME(%1)", frame);
	}
}

#endif // SCANNERSANE_HH

/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SaneScanner.cc
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

#include "SaneScanner.hh"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

#define SYNC_INIT Glib::Threads::Cond __cond; Glib::Threads::Mutex __mutex; bool __finished = false;
#define SYNC_NOTIFY __finished = true; __cond.signal();
#define SYNC_WAIT __mutex.lock(); while(!__finished){ __cond.wait(__mutex); }

//#undef g_debug
//#define g_debug(...) printf(__VA_ARGS__); printf("\n"); fflush(stdout);


struct Scanner::ScanJob {
	std::string device;
	double dpi;
	Scanner::ScanMode scan_mode;
	int depth;
	Scanner::ScanType type;
	int page_width;
	int page_height;
	PageHandler* handler;

	int line_count = 0;
	int pass_number = 0;
	int page_number = 0;
	std::vector<unsigned char> buffer;	// Buffer for received line
	int n_used = 0;						// Number of used bytes in the buffer
	SANE_Parameters parameters;
};

Scanner* Scanner::s_instance = nullptr;

Scanner* Scanner::get_instance(){
	static Scanner instance;
	s_instance = &instance;
	return s_instance;
}

/****************** Option stuff ******************/

const SANE_Option_Descriptor* Scanner::get_option_by_name(SANE_Handle, const std::string& name, int &index)
{
	std::map<std::string, int>::const_iterator it = m_options.find(name);
	if(it != m_options.end()){
		index = it->second;
		return sane_get_option_descriptor(m_handle, index);
	}
	index = 0;
	return nullptr;
}

bool Scanner::set_default_option(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index)
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

void Scanner::set_bool_option(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index, bool value, bool* result)
{
	g_return_if_fail(option->type == SANE_TYPE_BOOL);

	SANE_Bool v = static_cast<SANE_Bool>(value);
	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, &v, nullptr);
	if(result) *result = static_cast<bool>(v);
	g_debug("sane_control_option(%d, SANE_ACTION_SET_VALUE, %s) -> (%s, %s)", option_index, value ? "SANE_TRUE" : "SANE_FALSE", sane_strstatus(status), result ? "SANE_TRUE" : "SANE_FALSE");
}

void Scanner::set_int_option(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index, int value, int* result)
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

void Scanner::set_fixed_option(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, double value, double* result)
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

bool Scanner::set_string_option(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, const std::string &value, std::string* result)
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

bool Scanner::set_constrained_string_option(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, const std::vector<std::string> &values, std::string* result)
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

void Scanner::log_option(SANE_Int index, const SANE_Option_Descriptor *option)
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

/****************** Misc scanner thread functions ******************/

int Scanner::get_device_weight(const std::string& device)
{
	/* NOTE: This is using trends in the naming of SANE devices, SANE should be able to provide this information better */
	/* Use webcams as a last resort */
	if(device.substr(0, 3) == "vfl:")
		return 2;

	/* Use locally connected devices first */
	if(device.find("usb") != std::string::npos)
		return 0;

	return 1;
}

int Scanner::compare_devices(const ScanDevice &device1, const ScanDevice &device2)
{
	/* TODO: Should do some fuzzy matching on the last selected device and set that to the default */
	int weight1 = get_device_weight(device1.name);
	int weight2 = get_device_weight(device2.name);
	return weight1 != weight2 ? weight1 - weight2 : device1.label.compare(device2.label);
}

void Scanner::authorization_cb(const char *resource, char *username, char *password)
{
	Glib::signal_idle().connect_once([resource]{ s_instance->m_signal_request_authorization.emit(resource); });

	/* Pop waits until there is something in the queue */
	Scanner::Credentials credentials = s_instance->m_authorizeQueue.pop();
	strncpy(username, credentials.username.c_str(), SANE_MAX_USERNAME_LEN);
	strncpy(password, credentials.password.c_str(), SANE_MAX_PASSWORD_LEN);
}

void Scanner::close_device()
{
	if(m_handle != nullptr){
		sane_close(m_handle);
		g_debug("sane_close()");
		m_handle = nullptr;
	}

	m_options.clear();
	m_state = ScanState::IDLE;
	Glib::signal_idle().connect_once([this]{ m_signal_scanning_changed.emit(false); });
}

void Scanner::fail_scan(int error_code, const Glib::ustring &error_string)
{
	close_device();
	while(!m_jobQueue.empty()){
		delete m_jobQueue.front();
		m_jobQueue.pop();
	}
	Glib::signal_idle().connect_once([this,error_code,error_string]{ m_signal_scan_failed.emit(error_code, error_string); });
}

/****************** do_*** scanner thread functions ******************/

void Scanner::do_redetect()
{
	m_state = ScanState::IDLE;

	const SANE_Device** device_list = nullptr;
	SANE_Status status = sane_get_devices(&device_list, false);
	g_debug("sane_get_devices() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		g_warning("Unable to get SANE devices: %s", sane_strstatus(status));
		return;
	}

	std::vector<ScanDevice> devices;
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

	/* Sort devices by priority */
	std::sort(devices.begin(), devices.end(), compare_devices);

	Glib::signal_idle().connect_once([this,devices]{ m_signal_update_devices.emit(devices); });
}

void Scanner::do_open()
{
	Glib::signal_idle().connect_once([this]{ m_signal_scanning_changed.emit(true); });

	ScanJob* job = m_jobQueue.front();

	if(job->device.empty()){
		g_warning("No scan device specified");
		fail_scan(0, _("No scanner specified."));
		return;
	}

	SANE_Status status = sane_open(job->device.c_str(), &m_handle);
	g_debug("sane_open(\"%s\") -> %s", job->device.c_str(), sane_strstatus(status));

	if(status != SANE_STATUS_GOOD){
		g_warning("Unable to get open device: %s", sane_strstatus(status));
		fail_scan(status, _("Unable to connect to scanner"));
		return;
	}

	m_state = ScanState::GET_OPTION;
}

void Scanner::do_get_option()
{
	ScanJob* job = m_jobQueue.front();
	int index = 0;
	const SANE_Option_Descriptor* option;

	/* Build the option table if it was not yet done */
	if(m_options.empty()){
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
				m_options.insert(std::make_pair(std::string(option->name), index));
			}
			++index;
		}
	}

	/* Apply settings */

	/* Pick source */
	option = get_option_by_name(m_handle, SANE_NAME_SCAN_SOURCE, index);
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
	option = get_option_by_name(m_handle, SANE_NAME_SCAN_MODE, index);
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
	option = get_option_by_name(m_handle, "duplex", index);
	if(option != nullptr && option->type == SANE_TYPE_BOOL)
		set_bool_option(m_handle, option, index, job->type == ScanType::ADF_BOTH, nullptr);

	/* Multi-page options */
	option = get_option_by_name(m_handle, "batch-scan", index);
	if(option != nullptr && option->type == SANE_TYPE_BOOL)
		set_bool_option(m_handle, option, index, job->type != ScanType::SINGLE, nullptr);

	/* Disable compression, we will compress after scanning */
	option = get_option_by_name(m_handle, "compression", index);
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
	option = get_option_by_name(m_handle, SANE_NAME_SCAN_RESOLUTION, index);
	if(option != nullptr){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, job->dpi, &job->dpi);
		else{
			int dpi;
			set_int_option(m_handle, option, index, job->dpi, &dpi);
			job->dpi = dpi;
		}
		option = get_option_by_name(m_handle, SANE_NAME_BIT_DEPTH, index);
		if(option != nullptr && job->depth > 0)
			set_int_option(m_handle, option, index, job->depth, nullptr);
	}

	/* Always use maximum scan area - some scanners default to using partial areas.  This should be patched in sane-backends */
	option = get_option_by_name(m_handle, SANE_NAME_SCAN_BR_X, index);
	if(option != nullptr && option->constraint_type == SANE_CONSTRAINT_RANGE){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, SANE_UNFIX(option->constraint.range->max), nullptr);
		else
			set_int_option(m_handle, option, index, option->constraint.range->max, nullptr);
	}
	option = get_option_by_name(m_handle, SANE_NAME_SCAN_BR_Y, index);
	if(option != nullptr && option->constraint_type == SANE_CONSTRAINT_RANGE){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, SANE_UNFIX(option->constraint.range->max), nullptr);
		else
			set_int_option(m_handle, option, index, option->constraint.range->max, nullptr);
	}

	/* Set page dimensions */
	option = get_option_by_name(m_handle, SANE_NAME_PAGE_WIDTH, index);
	if(option != nullptr && job->page_width > 0.0){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, job->page_width / 10.0, nullptr);
		else
			set_int_option(m_handle, option, index, job->page_width / 10, nullptr);
	}
	option = get_option_by_name(m_handle, SANE_NAME_PAGE_HEIGHT, index);
	if(option != nullptr && job->page_height > 0.0){
		if(option->type == SANE_TYPE_FIXED)
			set_fixed_option(m_handle, option, index, job->page_height / 10.0, nullptr);
		else
			set_int_option(m_handle, option, index, job->page_height / 10, nullptr);
	}

	m_state = ScanState::START;
}

void Scanner::do_start()
{
	ScanJob* job = m_jobQueue.front();

	SANE_Status status = sane_start(m_handle);
	g_debug("sane_start(page=%d, pass=%d) -> %s", job->page_number, job->pass_number, sane_strstatus(status));
	if(status == SANE_STATUS_GOOD)
		m_state = ScanState::GET_PARAMETERS;
	else if(status == SANE_STATUS_NO_DOCS)
		do_complete_document();
	else{
		g_warning("Unable to start device: %s", sane_strstatus(status));
		fail_scan(status, _("Unable to start scan"));
	}
}

void Scanner::do_get_parameters()
{
	ScanJob* job = m_jobQueue.front();

	SANE_Status status = sane_get_parameters(m_handle, &job->parameters);
	g_debug("sane_get_parameters() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		g_warning("Unable to get device parameters: %s", sane_strstatus(status));
		fail_scan(status, _("Error communicating with scanner"));
		return;
	}

	g_debug("Parameters: format=%s last_frame=%s bytes_per_line=%d pixels_per_line=%d lines=%d depth=%d",
			get_frame_mode_string(job->parameters.format).c_str(),
			job->parameters.last_frame ? "SANE_TRUE" : "SANE_FALSE",
			job->parameters.bytes_per_line,
			job->parameters.pixels_per_line,
			job->parameters.lines,
			job->parameters.depth);

	ScanPageInfo info;
	info.width = job->parameters.pixels_per_line;
	info.height = job->parameters.lines;
	info.depth = job->parameters.depth;
	info.n_channels = job->parameters.format == SANE_FRAME_GRAY ? 1 : 3;
	info.dpi = job->dpi;
	info.device = job->device;

	SYNC_INIT;
	Glib::signal_idle().connect_once([&]{ job->handler->setupPage(info); SYNC_NOTIFY; });
	SYNC_WAIT;

	/* Prepare for read */
	job->buffer.resize(job->parameters.bytes_per_line);
	m_state = ScanState::READ;
}

void Scanner::do_read()
{
	ScanJob* job = m_jobQueue.front();

	/* Read as many bytes as we need to complete a line */
	int n_to_read = job->buffer.size() - job->n_used;

	SANE_Int n_read;
	unsigned char* b = &job->buffer[job->n_used];
	SANE_Status status = sane_read(m_handle, b, n_to_read, &n_read);
	g_debug("sane_read(%d) -> (%s, %d)", n_to_read, sane_strstatus(status), n_read);

	/* Completed read */
	if(status == SANE_STATUS_EOF){
		if(job->parameters.lines > 0 && job->line_count != job->parameters.lines)
			g_warning("Scan completed with %d lines, expected %d lines", job->line_count, job->parameters.lines);
		if(job->n_used > 0)
			g_warning("Scan complete with %d bytes of unused data", job->n_used);
		do_complete_page();
		return;
	}else if(status != SANE_STATUS_GOOD){
		g_warning("Unable to read frame from device: %s", sane_strstatus(status));
		fail_scan(status, _("Error communicating with scanner"));
		return;
	}

	job->n_used += n_read;

	/* If we completed a line, feed it out */
	if(job->n_used >= int(job->buffer.size())){
		assert(job->n_used == job->parameters.bytes_per_line);
		ScanLine line;
		line.channel = Channel(job->parameters.format); /* Channel enum is 1:1 copy of SANE_Frame, but without requiring inclusion of sane header */
		line.width = job->parameters.pixels_per_line;
		line.depth = job->parameters.depth;
		std::swap(job->buffer, line.data);
		line.number = job->line_count;
		line.n_lines = 1;

		job->line_count += line.n_lines;

		/* Reset buffer */
		job->n_used = 0;
		job->buffer.resize(job->parameters.bytes_per_line);

		SYNC_INIT;
		Glib::signal_idle().connect_once([&]{ job->handler->handleData(line); SYNC_NOTIFY; });
		SYNC_WAIT;
	}
}

void Scanner::do_complete_page()
{
	ScanJob* job = m_jobQueue.front();

	SYNC_INIT;
	Glib::signal_idle().connect_once([&]{ job->handler->finalizePage(); SYNC_NOTIFY; });
	SYNC_WAIT;

	/* If multi-pass then scan another page */
	if(!job->parameters.last_frame){
		++job->pass_number;
		m_state = ScanState::START;
		return;
	}

	/* Go back for another page */
	if(job->type != ScanType::SINGLE){
		++job->page_number;
		job->pass_number = 0;
		m_state = ScanState::START;
		return;
	}

	do_complete_document();
}

void Scanner::do_complete_document()
{
	sane_cancel(m_handle);
	g_debug("sane_cancel()");

	delete m_jobQueue.front();
	m_jobQueue.pop();

	/* Continue onto the next job */
	if(!m_jobQueue.empty()){
		m_state = ScanState::OPEN;
		return;
	}

	close_device();
}

/*********************** Main scanner thread ***********************/

void Scanner::scan_thread()
{
	m_state = ScanState::IDLE;

	SANE_Int version_code;
	SANE_Status status = sane_init(&version_code, authorization_cb);
	g_debug("sane_init() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		Glib::signal_idle().connect_once([this]{ m_signal_init_failed.emit(); });
		g_warning("Unable to initialize SANE backend: %s", sane_strstatus(status));
		return;
	}
	g_debug("SANE version %d.%d.%d",
			SANE_VERSION_MAJOR(version_code),
			SANE_VERSION_MINOR(version_code),
			SANE_VERSION_BUILD(version_code));

	/* Scan for devices on first start */
	redetect();

	bool need_redetect = false;
	while(true){
		// Fetch request if requests are pending or wait for request if idle
		if(!m_requestQueue.empty() || m_state == ScanState::IDLE){
			Request request = m_requestQueue.pop();
			if(request.type == Request::Type::Redetect){
				need_redetect = true;
			}else if(request.type == Request::Type::StartScan){
				m_jobQueue.push(static_cast<ScanJob*>(request.data));
			}else if(request.type == Request::Type::Cancel){
				fail_scan(SANE_STATUS_CANCELLED, _("Scan cancelled"));
			}else if(request.type == Request::Type::Quit){
				close_device();
				break;
			}
		}
		// Perform operations according to state
		if(m_state == ScanState::IDLE){
			if(need_redetect){
				do_redetect();
				need_redetect = false;
			}
			if(!m_jobQueue.empty()){
				m_state = ScanState::OPEN;
			}
		}else if(m_state == ScanState::OPEN){
			do_open();
		}else if(m_state == ScanState::GET_OPTION){
			do_get_option();
		}else if(m_state == ScanState::START){
			do_start();
		}else if(m_state == ScanState::GET_PARAMETERS){
			do_get_parameters();
		}else if(m_state == ScanState::READ){
			do_read();
		}
	}
}

/****************** Public, main thread functions ******************/

void Scanner::start(){
	if(m_thread == nullptr){
		try{
			m_thread = Glib::Threads::Thread::create(sigc::mem_fun(this, &Scanner::scan_thread));
		}catch(const Glib::Error& e){
			g_critical("Unable to create thread: %s", e.what().c_str());
		}
	}
}

void Scanner::redetect(){
	g_debug("Requesting redetection of scan devices");
	m_requestQueue.push({Request::Type::Redetect});
}

void Scanner::scan(const std::string& device, PageHandler* handler, ScanOptions options)
{
	g_debug("Scanner.scan(\"%s\", dpi=%d, scan_mode=%s, depth=%d, type=%s, paper_width=%d, paper_height=%d)",
			!device.empty() ? device.c_str() : "(null)", options.dpi, get_scan_mode_string(options.scan_mode).c_str(),
			options.depth, get_scan_type_string(options.type).c_str(), options.paper_width, options.paper_height);
	assert(options.depth == 8);
	ScanJob* job = new ScanJob;
	job->device = device;
	job->dpi = options.dpi;
	job->scan_mode = options.scan_mode;
	job->depth = options.depth;
	job->type = options.type;
	job->page_width = options.paper_width;
	job->page_height = options.paper_height;
	job->handler = handler;
	m_requestQueue.push({Request::Type::StartScan, job});
}

void Scanner::cancel()
{
	m_requestQueue.push({Request::Type::Cancel});
}

void Scanner::stop()
{
	g_debug("Stopping scan thread");

	if(m_thread != nullptr){
		m_requestQueue.push({Request::Type::Quit});
		m_thread->join();
		sane_exit();
		m_thread = nullptr;
	}

	g_debug("sane_exit()");
}

/****************** Enum to string ******************/

Glib::ustring Scanner::get_frame_mode_string(SANE_Frame frame)
{
	Glib::ustring framestr[] = {"SANE_FRAME_GRAY", "SANE_FRAME_RGB", "SANE_FRAME_RED", "SANE_FRAME_GREEN", "SANE_FRAME_BLUE"};
	if(frame <= SANE_FRAME_BLUE){
		return framestr[frame];
	}else{
		return Glib::ustring::compose("SANE_FRAME(%1)", frame);
	}
}

Glib::ustring Scanner::get_scan_mode_string(ScanMode mode)
{
	Glib::ustring modestr[] = {"ScanMode::DEFAULT", "ScanMode::COLOR", "ScanMode::GRAY", "ScanMode::LINEART"};
	if(mode <= ScanMode::LINEART){
		return modestr[static_cast<int>(mode)];
	}else{
		return Glib::ustring::compose("ScanMode(%1)", int(mode));
	}
}

Glib::ustring Scanner::get_scan_type_string(ScanType type)
{
	Glib::ustring typestr[] = {"ScanType::SINGLE", "ScanType::ADF_FRONT", "ScanType::ADF_BACK", "ScanType::ADF_BOTH"};
	if(type <= ScanType::ADF_BOTH){
		return typestr[static_cast<int>(type)];
	}else{
		return Glib::ustring::compose("ScanType(%1)", int(type));
	}
}

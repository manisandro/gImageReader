/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Scanner.cc
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

#include "Scanner.hh"
#include <cassert>
#include <cstring>

//#undef g_debug
//#define g_debug(...) printf(__VA_ARGS__); printf("\n"); fflush(stdout);

#if defined(G_OS_UNIX)
#include "ScannerSane.hh"
#elif defined(G_OS_WIN32)
#include "ScannerTwain.hh"
#endif

Scanner* Scanner::get_instance(){
	static ScannerImpl instance;
	return &instance;
}

Glib::ustring Scanner::get_scan_mode_string(ScanMode mode)
{
	Glib::ustring modestr[] = {"ScanMode::DEFAULT", "ScanMode::COLOR", "ScanMode::GRAY", "ScanMode::LINEART"};
	if(mode <= ScanMode::LINEART)
		return modestr[static_cast<int>(mode)];
	else
		return Glib::ustring::compose("ScanMode(%1)", int(mode));
}

Glib::ustring Scanner::get_scan_type_string(ScanType type)
{
	Glib::ustring typestr[] = {"ScanType::SINGLE", "ScanType::ADF_FRONT", "ScanType::ADF_BACK", "ScanType::ADF_BOTH"};
	if(type <= ScanType::ADF_BOTH)
		return typestr[static_cast<int>(type)];
	else
		return Glib::ustring::compose("ScanType(%1)", int(type));
}

void Scanner::set_state(ScanState state)
{
	m_state = state;
	Glib::signal_idle().connect_once([=]{ m_signal_scanning_changed.emit(m_state); });
}

void Scanner::fail_scan(const Glib::ustring &error_string)
{
	do_close();
	while(!m_jobQueue.empty()){
		delete m_jobQueue.front();
		m_jobQueue.pop();
	}
	Glib::signal_idle().connect_once([=]{ m_signal_scan_failed.emit(error_string); });
}

void Scanner::do_redetect()
{
	g_debug("do_redetect");
	std::vector<ScanDevice> devices = detectDevices();
	Glib::signal_idle().connect_once([=]{ m_signal_update_devices.emit(devices); });
}

void Scanner::do_open()
{
	g_debug("do_open");
	ScanJob* job = m_jobQueue.front();
	if(job->device.empty()){
		fail_scan(_("No scanner specified."));
		return;
	}else if(!openDevice(job->device)){
		fail_scan(_("Unable to connect to scanner"));
		return;
	}
	set_state(ScanState::SET_OPTIONS);
}

void Scanner::do_close()
{
	g_debug("do_close");
	closeDevice();
	set_state(ScanState::IDLE);
}

void Scanner::do_set_options()
{
	g_debug("do_set_options");
	setOptions(m_jobQueue.front());
	set_state(ScanState::START);
}

void Scanner::do_start()
{
	g_debug("do_start");
	StartStatus status = startDevice();
	if(status == StartStatus::Success)
		set_state(ScanState::GET_PARAMETERS);
	else if(status == StartStatus::Fail)
		do_complete_document();
	else
		fail_scan(_("Unable to start scan"));
}

void Scanner::do_get_parameters()
{
	g_debug("do_get_parameters");
	if(!getParameters()){
		fail_scan(_("Error communicating with scanner"));
	}else{
		set_state(ScanState::READ);
	}
}

void Scanner::do_read()
{
	g_debug("do_read");
	ReadStatus status = read();
	if(status == ReadStatus::Done){
		do_complete_page();
		return;
	}else if(status == ReadStatus::Fail){
		fail_scan(_("Error communicating with scanner"));
		return;
	}
}

void Scanner::do_complete_page()
{
	g_debug("do_complete_page");
	ScanJob* job = m_jobQueue.front();
	PageStatus status = completePage(job);
	if(status == PageStatus::AnotherPass){
		++job->pass_number;
		set_state(ScanState::START);
		return;
	}

	m_signal_page_available.emit(job->filename);

	if(job->type != ScanType::SINGLE){
		++job->page_number;
		job->pass_number = 0;
		set_state(ScanState::START);
		return;
	}
	do_complete_document();
}

void Scanner::do_complete_document()
{
	g_debug("do_complete_document");
	do_close();
	delete m_jobQueue.front();
	m_jobQueue.pop();
}

/*********************** Main scanner thread ***********************/

void Scanner::scan_thread()
{
	if(!initBackend()){
		Glib::signal_idle().connect_once([=]{ m_signal_init_failed.emit(); });
		return;
	}

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
				fail_scan(_("Scan cancelled"));
			}else if(request.type == Request::Type::Quit){
				do_close();
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
				set_state(ScanState::OPEN);
			}
		}else if(m_state == ScanState::OPEN){
			do_open();
		}else if(m_state == ScanState::SET_OPTIONS){
			do_set_options();
		}else if(m_state == ScanState::START){
			do_start();
		}else if(m_state == ScanState::GET_PARAMETERS){
			do_get_parameters();
		}else if(m_state == ScanState::READ){
			do_read();
		}
	}
	closeBackend();
}

/****************** Public, main thread functions ******************/

void Scanner::start(){
	if(m_thread == nullptr){
		m_thread = Glib::Threads::Thread::create(sigc::mem_fun(this, &Scanner::scan_thread));
	}
}

void Scanner::redetect(){
	m_requestQueue.push({Request::Type::Redetect});
}

void Scanner::scan(const std::string& device, ScanOptions options)
{
	assert(options.depth == 8);
	ScanJob* job = new ScanJob;
	job->filename = options.filename;
	job->device = device;
	job->dpi = options.dpi;
	job->scan_mode = options.scan_mode;
	job->depth = options.depth;
	job->type = options.type;
	job->page_width = options.paper_width;
	job->page_height = options.paper_height;
	m_requestQueue.push({Request::Type::StartScan, job});
}

void Scanner::cancel()
{
	m_requestQueue.push({Request::Type::Cancel});
}

void Scanner::stop()
{
	if(m_thread != nullptr){
		m_requestQueue.push({Request::Type::Quit});
		m_thread->join();
		m_thread = nullptr;
	}
}

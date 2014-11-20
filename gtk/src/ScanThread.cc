/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScanThread.cc
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

#include "common.hh"
#include "ScanThread.hh"

#ifdef G_OS_WIN32
#include "ScanBackendTwain.hh"
#else
#include "ScanBackendSane.hh"
#endif


void ScanThread::setState(State state)
{
	m_state = state;
	Glib::signal_idle().connect_once([this, state]{ m_signal_scanStateChanged.emit(state); });
}

void ScanThread::failScan(const Glib::ustring& errorString)
{
	doClose();
	while(!m_jobQueue.empty()){
		delete m_jobQueue.front();
		m_jobQueue.pop();
	}
	Glib::signal_idle().connect_once([this, errorString]{ m_signal_scanFailed.emit(errorString); });
}

void ScanThread::doRedetect()
{
	g_debug("do_redetect");
	std::vector<ScanBackend::Device> devices = m_backend->detectDevices();
	Glib::signal_idle().connect_once([this, devices]{ m_signal_devicesDetected.emit(devices); });
}

void ScanThread::doOpen()
{
	g_debug("do_open");
	ScanBackend::Job* job = m_jobQueue.front();
	if(job->device.empty()){
		failScan(_("No scanner specified"));
		return;
	}else if(!m_backend->openDevice(job->device)){
		failScan(_("Unable to connect to scanner"));
		return;
	}
	setState(State::SET_OPTIONS);
}

void ScanThread::doClose()
{
	g_debug("do_close");
	m_backend->closeDevice();
	setState(State::IDLE);
}

void ScanThread::doSetOptions()
{
	g_debug("do_set_options");
	m_backend->setOptions(m_jobQueue.front());
	setState(State::START);
}

void ScanThread::doStart()
{
	g_debug("do_start");
	ScanBackend::StartStatus status = m_backend->startDevice();
	if(status == ScanBackend::StartStatus::Success){
		setState(State::GET_PARAMETERS);
	}else{
		doCompleteDocument();
		failScan(_("Unable to start scan"));
	}
}

void ScanThread::doGetParameters()
{
	g_debug("do_get_parameters");
	if(!m_backend->getParameters()){
		failScan(_("Error communicating with scanner"));
	}else{
		setState(State::READ);
	}
}

void ScanThread::doRead()
{
	g_debug("do_read");
	ScanBackend::ReadStatus status = m_backend->read();
	if(status == ScanBackend::ReadStatus::Done){
		doCompletePage();
		return;
	}else if(status == ScanBackend::ReadStatus::Fail){
		failScan(_("Error communicating with scanner"));
		return;
	}
}

void ScanThread::doCompletePage()
{
	g_debug("do_complete_page");
	ScanBackend::Job* job = m_jobQueue.front();
	ScanBackend::PageStatus status = m_backend->completePage(job);
	if(status == ScanBackend::PageStatus::AnotherPass){
		++job->pass_number;
		setState(State::START);
		return;
	}

	const std::string& filename = job->filename;
	Glib::signal_idle().connect_once([this, filename]{ m_signal_pageAvailable.emit(filename); });

	if(job->type != ScanBackend::ScanType::SINGLE){
		++job->page_number;
		job->pass_number = 0;
		setState(State::START);
		return;
	}
	doCompleteDocument();
}

void ScanThread::doCompleteDocument()
{
	g_debug("do_complete_document");
	doClose();
	delete m_jobQueue.front();
	m_jobQueue.pop();
}

/*********************** Main scanner thread ***********************/

void ScanThread::run()
{
#ifdef G_OS_WIN32
	m_backend = new ScanBackendTwain();
#else
	m_backend = new ScanBackendSane();
#endif

	if(!m_backend->init()){
		Glib::signal_idle().connect_once([this]{ m_signal_initFailed.emit(); });
		Glib::signal_idle().connect_once([this]{ m_signal_stopped.emit(); });
		return;
	}

	/* Scan for devices on first start */
	redetect();

	bool need_redetect = false;
	while(true){
		// Fetch request if requests are pending or wait for request if idle
		if(!m_requestQueue.empty() || m_state == State::IDLE){
			Request request = m_requestQueue.pop();
			if(request.type == Request::Type::Redetect){
				need_redetect = true;
			}else if(request.type == Request::Type::StartScan){
				m_jobQueue.push(static_cast<ScanBackend::Job*>(request.data));
			}else if(request.type == Request::Type::Cancel){
				failScan(_("Scan cancelled"));
			}else if(request.type == Request::Type::Quit){
				doClose();
				break;
			}
		}
		// Perform operations according to state
		if(m_state == State::IDLE){
			if(need_redetect){
				doRedetect();
				need_redetect = false;
			}
			if(!m_jobQueue.empty()){
				setState(State::OPEN);
			}
		}else if(m_state == State::OPEN){
			doOpen();
		}else if(m_state == State::SET_OPTIONS){
			doSetOptions();
		}else if(m_state == State::START){
			doStart();
		}else if(m_state == State::GET_PARAMETERS){
			doGetParameters();
		}else if(m_state == State::READ){
			doRead();
		}
	}
	m_backend->close();
	Glib::signal_idle().connect_once([this]{ m_signal_stopped.emit(); });
}

void ScanThread::redetect(){
	m_requestQueue.push({Request::Type::Redetect, 0});
}

void ScanThread::scan(const std::string& device, ScanBackend::Options options)
{
	g_assert(options.depth == 8);
	ScanBackend::Job* job = new ScanBackend::Job;
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

void ScanThread::cancel()
{
	m_requestQueue.push({Request::Type::Cancel, 0});
}

void ScanThread::stop()
{
	m_requestQueue.push({Request::Type::Quit, 0});
}

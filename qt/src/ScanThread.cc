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

#include <QDebug>
#include <QThread>

#include "common.hh"
#include "ScanThread.hh"

#ifdef Q_OS_WIN32
#include "ScanBackendTwain.hh"
#else
#include "ScanBackendSane.hh"
#endif


void ScanThread::setState(State state)
{
	m_state = state;
	emit scanStateChanged(m_state);
}

void ScanThread::failScan(const QString& errorString)
{
	doClose();
	qDeleteAll(m_jobQueue);
	m_jobQueue.clear();
	emit scanFailed(errorString);
}

void ScanThread::doRedetect()
{
	qDebug() << "do_redetect";
	QList<ScanBackend::Device> devices = m_backend->detectDevices();
	emit devicesDetected(devices);
}

void ScanThread::doOpen()
{
	qDebug() << "do_open";
	ScanBackend::Job* job = m_jobQueue.front();
	if(job->device.isEmpty()){
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
	qDebug() << "do_close";
	m_backend->closeDevice();
	setState(State::IDLE);
}

void ScanThread::doSetOptions()
{
	qDebug() << "do_set_options";
	m_backend->setOptions(m_jobQueue.front());
	setState(State::START);
}

void ScanThread::doStart()
{
	qDebug() << "do_start";
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
	qDebug() << "do_get_parameters";
	if(!m_backend->getParameters()){
		failScan(_("Error communicating with scanner"));
	}else{
		setState(State::READ);
	}
}

void ScanThread::doRead()
{
	qDebug() << "do_read";
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
	qDebug() << "do_complete_page";
	ScanBackend::Job* job = m_jobQueue.front();
	ScanBackend::PageStatus status = m_backend->completePage(job);
	if(status == ScanBackend::PageStatus::AnotherPass){
		++job->pass_number;
		setState(State::START);
		return;
	}

	emit pageAvailable(job->filename);

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
	qDebug() << "do_complete_document";
	doClose();
	delete m_jobQueue.dequeue();
}

/*********************** Main scanner thread ***********************/

void ScanThread::run()
{
#ifdef Q_OS_WIN32
	m_backend = new ScanBackendTwain();
#else
	m_backend = new ScanBackendSane();
#endif

	if(!m_backend->init()){
		emit initFailed();
		return;
	}

	/* Scan for devices on first start */
	redetect();

	bool need_redetect = false;
	while(true){
		// Fetch request if requests are pending or wait for request if idle
		if(!m_requestQueue.isEmpty() || m_state == State::IDLE){
			Request request = m_requestQueue.dequeue();
			if(request.type == Request::Type::Redetect){
				need_redetect = true;
			}else if(request.type == Request::Type::StartScan){
				m_jobQueue.enqueue(static_cast<ScanBackend::Job*>(request.data));
			}else if(request.type == Request::Type::Cancel){
				failScan(_("Scan canceled"));
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
			if(!m_jobQueue.isEmpty()){
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
}

void ScanThread::redetect(){
	m_requestQueue.enqueue({Request::Type::Redetect, 0});
}

void ScanThread::scan(const QString& device, ScanBackend::Options options)
{
	Q_ASSERT(options.depth == 8);
	ScanBackend::Job* job = new ScanBackend::Job;
	job->filename = options.filename;
	job->device = device;
	job->dpi = options.dpi;
	job->scan_mode = options.scan_mode;
	job->depth = options.depth;
	job->type = options.type;
	job->page_width = options.paper_width;
	job->page_height = options.paper_height;
	m_requestQueue.enqueue({Request::Type::StartScan, job});
}

void ScanThread::cancel()
{
	m_requestQueue.enqueue({Request::Type::Cancel, 0});
}

void ScanThread::stop()
{
	m_requestQueue.enqueue({Request::Type::Quit, 0});
}

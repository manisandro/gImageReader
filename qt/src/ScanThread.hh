/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScanThread.hh
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

#ifndef SCANTHREAD_HPP
#define SCANTHREAD_HPP

#include <QMetaType>
#include <QObject>

#include "AsyncQueue.hh"
#include "ScanBackend.hh"

class ScanThread : public QObject {
	Q_OBJECT
public:
	enum class State { IDLE = 0, OPEN, SET_OPTIONS, START, GET_PARAMETERS, READ };

	~ScanThread(){ delete m_backend; }
	void redetect();
	void scan(const QString& device, ScanBackend::Options options);
	void cancel();
	void stop();

public slots:
	void run();

signals:
	void initFailed();
	void devicesDetected(QList<ScanBackend::Device> devices);
	void scanFailed(const QString& message);
	void scanStateChanged(ScanThread::State state);
	void pageAvailable(const QString& filename);
	void stopped();

private:
	struct Request {
		enum class Type { Redetect, StartScan, Cancel, Quit } type;
		void* data;
	};

	ScanBackend* m_backend = nullptr;
	State m_state = State::IDLE;
	AsyncQueue<Request> m_requestQueue;
	QQueue<ScanBackend::Job*> m_jobQueue;

	void doCompleteDocument();
	void doCompletePage();
	void doSetOptions();
	void doGetParameters();
	void doOpen();
	void doRead();
	void doRedetect();
	void doStart();
	void doClose();
	void setState(State state);
	void failScan(const QString& errorString);
};

Q_DECLARE_METATYPE(ScanThread::State)

#endif // SCANTHREAD_HPP

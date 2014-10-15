/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * AsyncQueue.hh
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

#ifndef ASYNCQUEUE_HH
#define ASYNCQUEUE_HH

#include <QWaitCondition>
#include <QMutex>
#include <QQueue>

template<typename T>
class AsyncQueue {
public:
	AsyncQueue();
	bool isEmpty();
	void enqueue(const T& item);
	void enqueue_unlocked(const T& item);
	T dequeue();
	T dequeue_unlocked();
	void lock();
	void unlock();
	void interrupt();

private:
	QQueue<T> m_queue;
	QMutex m_mutex;
	QWaitCondition m_cond;
	bool m_interrupted;
};

template<typename T>
AsyncQueue<T>::AsyncQueue()
	: m_interrupted(false)
{
}

template<typename T>
bool AsyncQueue<T>::isEmpty()
{
	QMutexLocker locker(&m_mutex);
	return m_queue.isEmpty();
}

template<typename T>
void AsyncQueue<T>::enqueue(const T& item)
{
	QMutexLocker locker(&m_mutex);
	enqueue_unlocked(item);
}

template<typename T>
void AsyncQueue<T>::enqueue_unlocked(const T& item)
{
	m_queue.enqueue(item);
	m_cond.wakeOne();
}

template<typename T>
T AsyncQueue<T>::dequeue()
{
	QMutexLocker locker(&m_mutex);
	return dequeue_unlocked();
}

template<typename T>
T AsyncQueue<T>::dequeue_unlocked()
{
	if(m_queue.isEmpty()){
		while(!m_interrupted && m_queue.isEmpty()){
			m_cond.wait(&m_mutex);
		}
	}
	return m_queue.dequeue();
}

template<typename T>
void AsyncQueue<T>::lock(){
	m_mutex.lock();;
}

template<typename T>
void AsyncQueue<T>::unlock(){
	m_mutex.unlock();
}

template<typename T>
void AsyncQueue<T>::interrupt(){
	QMutexLocker locker(&m_mutex);
	m_interrupted = true;
	m_cond.wakeAll();
}

#endif

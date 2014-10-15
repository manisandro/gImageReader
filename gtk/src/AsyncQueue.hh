/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * AsyncQueue.hh
 * Adapted from https://bugzilla.gnome.org/show_bug.cgi?id=511972#c11
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

#include <queue>

/*
 * implementation details
 *  - pop is not strong-exception-safe but:
 *     - it's friendlier to use it this way
 *     - everybody seems to implement it this way too
 *     - is strong-exception-safety mandatory at the price of a more complex interface?
 *     - we mean by not strong-exception-safe the fact that it can happen (very unlikely)
 *       that the call to std::queue::front is saved to result, but std::queue::pop() raises an exception that makes this result unsaved.
 *  - try_pop raise an exception if the queue is empty even if the fact that the queue is empty is expected, because:
 *     - the result is returned by value, so there is no room for another result
 *     - every other pop methods already raise an exception when the logic flow of execution is not sastified
 *     - indeed, the logic of the pop method is to return a value, if it's not the case, an exception has to be raised
 */

/**
 * \brief AsyncQueue allow inter-thread communication in a thread-safe and exception safe way
 * it tries to map the fonctionnalities of GAsyncQueue that can be found in Glib, but uses the std::queue as a backend
 * it's by default a FIFO queue, but you can specify your own container to custom this beaviour :
 * \code
 *  AsyncQueue<int,LIFO<int>> aqueue;
 *  aqueue.push(1); // push 1 at the top of the queue
 *  aqueue.push(2); // push 2 at the top of the queue
 *  int result = aqueue.pop(); // pop 1 from the queue
 * \endcode
 */
template<typename T, typename S = std::deque<T>>
class AsyncQueue {
	std::queue<T,S>   queue_;
	Glib::Mutex       mutex_;
	Glib::Cond        cond_;
	bool              interrupted_;
public:
	AsyncQueue();
	/**
	 * \brief Returns whether the queue is empty
	 * \note this method is thread-safe and exception-safe
	 */
	bool empty();
	/**
	 * \brief push an item into the queue
	 * \param item to push
	 * \note this method is thread-safe and exception-safe
	 */
	void push(const T& item);
	/**
	 * \brief push an item into the queue
	 * \param item to push
	 * \note the mutex must be hold while using this method
	 * \see @lock, @unlock
	 */
	void push_unlocked(const T& item);
	/**
	 * \brief retrieve one item from the queue, this method waits until there is at least something to retrieve
	 * \return the item to be retrieved
	 * \throw std::exception raised if this operation have been interrupted
	 * \see @interrupt
	 */
	T pop();
	/**
	 * \brief retrieve one item from the queue, this method waits until there is at least something to retrieve
	 * \return the item to be retrieved
	 * \throw std::exception raised if this operation have been interrupted
	 * \note the mutex must be hold while using this method
	 * \see @lock, @unlock, @interrupt
	 */
	T pop_unlocked();
	/**
	 * \brief tries to retrieve one item from the queue
	 * \return the item to be retrieved
	 * \throw std::exception raised if there is no item to retrieve or if this operation have been interrupted
	 */
	T try_pop();
	/**
	 * \brief tries to retrieve one item from the queue
	 * \return the item to be retrieved
	 * \throw std::exception raised if there is no item to retrieve or if this operation have been interrupted
	 * \note the mutex must be hold while using this method
	 * \see @lock, @unlock
	 */
	T try_pop_unlocked();
	/**
	 * \brief retrieve one item from the queue, this method waits until there is at least something to retrieve or if endtime is reached
	 * \param endtime allow this method to wait until specified time is reached
	 * \throw std::exception raised a timeout occured or if the operation have been interrupted
	 */
	T timed_pop(const Glib::TimeVal& endtime);
	/**
	 * \brief retrieve one item from the queue, this method waits until there is at least something to retrieve or if endtime is reached
	 * \param endtime allow this method to wait until specified time is reached
	 * \throw std::exception raised a timeout occured or if the operation have been interrupted
	 * \note the mutex must be hold while using this method
	 * \see @lock, @unlock
	 */
	T timed_pop_unlocked(const Glib::TimeVal& endtime);
	/**
	 * \brief hold the mutex on this queue
	 */
	void lock();
	/**
	 * \brief release the mutex from this queue
	 */
	void unlock();
	/**
	 * \brief interrupt all the threads waiting on pop
	 */
	void interrupt();
};

template<typename T, typename S>
AsyncQueue<T,S>::AsyncQueue():
interrupted_(false)
{
}

template<typename T, typename S>
bool AsyncQueue<T,S>::empty()
{
	Glib::Mutex::Lock queue_guard(mutex_);
	return queue_.empty();
}

template<typename T, typename S>
void AsyncQueue<T,S>::push(const T& item)
{
	Glib::Mutex::Lock queue_guard(mutex_);
	push_unlocked(item);
}

template<typename T, typename S>
void AsyncQueue<T,S>::push_unlocked(const T& item)
{
	queue_.push(item);
	cond_.signal(); //only awake one thread at a time
}

template<typename T, typename S>
T AsyncQueue<T,S>::pop()
{
	Glib::Mutex::Lock queue_guard(mutex_);
	return pop_unlocked();
}

template<typename T, typename S>
T AsyncQueue<T,S>::pop_unlocked()
{
	if(queue_.empty())
	{
		while ( !interrupted_ && queue_.empty() )
			cond_.wait(mutex_);
		if(queue_.empty())
			throw std::logic_error("pop interrupted");
	}
	T result(queue_.front());
	queue_.pop();
	return result;
}

template<typename T, typename S>
T AsyncQueue<T,S>::try_pop()
{
	Glib::Mutex::Lock queue_guard(mutex_);
	return try_pop_unlocked();
}

template<typename T, typename S>
T AsyncQueue<T,S>::try_pop_unlocked()
{
	if(queue_.empty())
		throw std::logic_error("queue empty");
	T result(queue_.front());
	queue_.pop();
	return result;
}

template<typename T, typename S>
T AsyncQueue<T,S>::timed_pop(const Glib::TimeVal& endtime)
{
	Glib::Mutex::Lock queue_guard(mutex_);
	return timed_pop_unlocked(endtime);
}

template<typename T, typename S>
T AsyncQueue<T,S>::timed_pop_unlocked(const Glib::TimeVal& endtime)
{
	if(queue_.empty())
	{
		while ( !interrupted_ && queue_.empty() )
			if(!cond_.timed_wait(mutex_,endtime))
			break;
		if( interrupted_ && queue_.empty() )
			throw std::logic_error("pop interrupted");
		if(queue_.empty())
			throw std::logic_error("pop timeout");
	}
	T result(queue_.front());
	queue_.pop();
	return result;
}

template<typename T, typename S>
void AsyncQueue<T,S>::lock(){
	mutex_.lock();
}

template<typename T, typename S>
void AsyncQueue<T,S>::unlock(){
	mutex_.unlock();
}

template<typename T, typename S>
void AsyncQueue<T,S>::interrupt(){
	Glib::Mutex::Lock queue_guard(mutex_);
	interrupted_ = true;
	cond_.broadcast(); //signal every thread waiting on pop
}

#endif

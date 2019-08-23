#pragma once

#include "../include/source_config.h"

#ifdef _WIN32
#else
#include "util_thread_unix.h"
#endif

namespace util {

#ifdef _WIN32

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

typedef std::thread							IMThread;
typedef std::mutex							IMMutex;					// �ǵݹ���
typedef std::condition_variable				IMCondition;
typedef std::recursive_mutex				IMRecursiveMutex;			// �ݹ���
typedef std::lock_guard<IMMutex>			IMMutexLockGuard;
typedef std::lock_guard<IMRecursiveMutex>	IMRecursiveMutexLockGuard;  
typedef std::unique_lock<IMMutex>			IMMutexUniqueLock;


class IMRWMutex
{
public:
	IMRWMutex():read_cnt_(0),wait_read_cnt(0),write_cnt_(0),wait_write_cnt(0),swap_wait_(false){}
	~IMRWMutex(){}

	void lockRead()
	{
		IMMutexUniqueLock lock(mtx_);
		++wait_read_cnt;

		while(write_cnt_ > 0 || wait_write_cnt > 0 )
		{
			read_cond_.wait(lock);
			if(0 == write_cnt_)  // ǿ�ƶ�����
			{
				break;
			}
		}
		++read_cnt_;
		--wait_read_cnt;
	}
	void unlockRead()
	{
		IMMutexUniqueLock lock(mtx_);
		--read_cnt_;
		if(0 == read_cnt_ && wait_write_cnt > 0)
		{
			write_cond_.notify_one();
		}
		
	}
	void lockWrite()
	{
		IMMutexUniqueLock lock(mtx_);
		++wait_write_cnt;
		while(read_cnt_ != 0 || write_cnt_ !=0)
		{ 
			write_cond_.wait(lock);
		}
		++ write_cnt_;
		-- wait_write_cnt;
	}
	void unlockWrite()
	{
		IMMutexUniqueLock lock(mtx_);
		-- write_cnt_;
		if(swap_wait_)   // ��ֹ�����д�����
		{
			if(wait_write_cnt > 0)
			{
				write_cond_.notify_one();
			}
			else
			{
				read_cond_.notify_all();
			}
		}
		else
		{
			if(wait_read_cnt > 0)
			{
				read_cond_.notify_all();	
			}
			else
			{
				write_cond_.notify_one();
			}
		}
		swap_wait_ = !swap_wait_;
	}

private:
	IMMutex						mtx_;
	//volatile 
			Int32				read_cnt_;
	//volatile  
			Int32				wait_read_cnt;
	//volatile 
			Int32				write_cnt_;
	//volatile 
			Int32				wait_write_cnt;
	bool						swap_wait_;
	std::condition_variable		read_cond_;
	std::condition_variable		write_cond_;

};

class IMReadLock
{
public:
	IMReadLock(IMRWMutex& rw_mutex):rw_mutex_(rw_mutex)
	{
		rw_mutex_.lockRead();
	}
	~IMReadLock()
	{
		rw_mutex_.unlockRead();
	}
private:
	IMRWMutex&  rw_mutex_;
};

class IMWriteLock
{
public:
	IMWriteLock(IMRWMutex& rw_mutex):rw_mutex_(rw_mutex)
	{
		rw_mutex_.lockWrite();
	}
	~IMWriteLock()
	{
		rw_mutex_.unlockWrite();
	}
private:
	IMRWMutex&  rw_mutex_;
};

class IMConditionWait
{
public:
	bool	wait(IMMutexUniqueLock& lock, int milliseconds)
	{
		return (std::cv_status::timeout==condi_.wait_for(lock, std::chrono::milliseconds(milliseconds)))?false:true;
	}
	void	notifyOne()
	{
		condi_.notify_one();
	}
	void	notifyAll()
	{
		condi_.notify_all();
	}
	IMMutex& mutex() {return mutex_;}
private:
	IMMutex					mutex_;
	IMCondition				condi_;
};

#else

template <typename T_MUTEX>
class LockGuard
{
public:
	LockGuard(T_MUTEX& mutex)
	:mutex_(mutex)
	{
		mutex_.lock();
	}
	~LockGuard()
	{
		mutex_.unlock();
	}
private:
	T_MUTEX&	mutex_;
};


class IMReadLock
{
public:
	IMReadLock(RWMutex& rw_mutex):rw_mutex_(rw_mutex)
	{
		rw_mutex_.rdlock();
	}
	~IMReadLock()
	{
		rw_mutex_.unlock();
	}
private:
	RWMutex&  rw_mutex_;
};

class IMWriteLock
{
public:
	IMWriteLock(RWMutex& rw_mutex):rw_mutex_(rw_mutex)
	{
		rw_mutex_.wrlock();
	}
	~IMWriteLock()
	{
		rw_mutex_.unlock();
	}
private:
	RWMutex&  rw_mutex_;
};

typedef Mutex 						IMMutex;
typedef RecursiveMutex 				IMRecursiveMutex;
typedef RWMutex   					IMRWMutex;
typedef LockGuard<IMMutex>			IMMutexLockGuard;
typedef LockGuard<IMRecursiveMutex>	IMRecursiveMutexLockGuard;

#endif

template <typename T_MUTEX>
class IMLockIfNeed
{
public:
	IMLockIfNeed(T_MUTEX& mutex, bool if_need):mutex_(mutex),if_need_(if_need)
	{
		if(if_need_)
		{
			mutex_.lock();
		}
	}
	~IMLockIfNeed()
	{
		if(if_need_)
		{
			mutex_.unlock();
		}
	}
private:
	T_MUTEX&	mutex_;
	bool		if_need_;
};

}

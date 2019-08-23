#pragma once

#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <list>

#include "source_config.h"

namespace util{

typedef enum _ThreadQuitMode
{
	kNormal,			    // 正常退出
	kSignal,			    // 发送中断信号
	kSignalAndKill	    // 发送中断信息并调用pthead_exit()
}ThreadQuitMode;



#define THREAD_STACK_MIN		2*1024*1024 //2Mb


typedef void  (*FP_waitReady)(void* arg);
class Semaphore
{
public:
	Semaphore();
	~Semaphore();
	void 	post();
	bool	wait(int second, FP_waitReady fp_wait_ready = NULL, void* arg = NULL);
private:
	pthread_condattr_t  attr_;
	pthread_mutex_t     mutex_;
	pthread_cond_t      cond_;
};


typedef void* (*FP_EXEC)(void* arg);

class ThreadMQ;
class Thread
{
public:
	Thread();
	~Thread();
	bool start(FP_EXEC fun, void* arg, size_t stack_size = 0);
	void stop(ThreadQuitMode mode = kNormal);
	bool waitStop(int second); // 线程体while循环使用，检测是否已调用stop函数。 秒
	inline bool isStopCalled() const {return is_stop_called_;}
	void 		setInterrupt(bool is);
	bool        isInterrupt() const;

	//void	useSignalExit();
public:
	pthread_t 		thread_t_;
	pthread_attr_t  *attr_;
private:
	Semaphore 	    cond_;
	bool	  	    is_stop_called_;
	bool            is_interrupt_;
};

class Mutex
{
public:
	Mutex();
	~Mutex();
	bool lock();
	bool trylock();
	void unlock();
private:
	pthread_mutex_t  mx_;
};

class RecursiveMutex
{
public:
	RecursiveMutex();
	~RecursiveMutex();
	bool lock();
	bool trylock();
	void unlock();
private:
	pthread_mutex_t  	mx_;
	pthread_mutexattr_t attr_;
};

class RWMutex
{
public:
	RWMutex();
	~RWMutex();
	bool rdlock();
	bool wrlock();
	void unlock();
private:
	pthread_rwlock_t  mx_;
};


class MutexLockGuard
{
public:
	MutexLockGuard(Mutex& mutex):mutex_(mutex)
	{
		mutex_.lock();
	}
	~MutexLockGuard()
	{
		mutex_.unlock();
	}
private:
	Mutex&  mutex_;
};

class RecursiveMutexLockGuard
{
public:
	RecursiveMutexLockGuard(RecursiveMutex& mutex):mutex_(mutex)
	{
		mutex_.lock();
	}
	~RecursiveMutexLockGuard()
	{
		mutex_.unlock();
	}
private:
	RecursiveMutex&  mutex_;
};

class MutexLockIfNeed
{
public:
	MutexLockIfNeed(Mutex& mutex, bool if_need):mutex_(mutex),if_need_(if_need)
	{
		if(if_need_)
		{
			mutex_.lock();
		}
	}
	~MutexLockIfNeed()
	{
		if(if_need_)
		{
			mutex_.unlock();
		}
	}
private:
	Mutex&		mutex_;
	bool		if_need_;
};

class RDLockGuard
{
public:
	RDLockGuard(RWMutex& mutex):mutex_(mutex)
	{
		mutex_.rdlock();
	}
	~RDLockGuard()
	{
		mutex_.unlock();
	}
private:
	RWMutex&  mutex_;
};

class WRLockGuard
{
public:
	WRLockGuard(RWMutex& mutex):mutex_(mutex)
	{
		mutex_.wrlock();
	}
	~WRLockGuard()
	{
		mutex_.unlock();
	}
private:
	RWMutex&  mutex_;
};


class TMsg
{
public:
	typedef enum _MsgType
	{
		kReqRsp,
		kNotice
	}MsgType;
protected:
	TMsg(MsgType type)
	:type_(type)
	{

	}
public:
	virtual ~TMsg(){}
	inline MsgType   type() const {return type_;}
protected:
	MsgType				type_;
};


class TReqRspMsg : public TMsg
{
public:
	TReqRspMsg(const char* data);
	~TReqRspMsg();
	const char* dataReq() const;
	bool        waitRsp(int wait_s, FP_waitReady fp_wait_ready, void* arg, im_string& resp);
	void 		rsp(const char* data);
	bool        waitFinish() const  {return is_wait_finish_;}
private:
	char* 			data_req_;
	char*			data_rsp_;
	Semaphore 	    cond_rsp_;
	bool			is_wait_finish_;
};
class TNoticeMsg : public TMsg
{
public:
	TNoticeMsg(const char* data);
	~TNoticeMsg();
	im_string      data() const;
private:
	char* 			data_;
};

class ThreadMQ
{
public:
	ThreadMQ();
	~ThreadMQ();

	bool 	        req(Thread* rsp_thread, const char* data, int wait_s, im_string& resp);
	void			push(Thread* thread, const char* data);
	TMsg* 			pop(Thread* thread);

	static void     msgWaitReady(void* arg);
private:
	void*			    list_mq_;
	void*               list_mq_recovery_;
	RecursiveMutex		mx_list_;
};
}

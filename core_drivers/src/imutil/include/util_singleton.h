#pragma once

#include "source_config.h"
#include "util_thread.h"

namespace util { 

template<class T>
class Singleton
{
protected:
	Singleton(){};
public:
	static T* instance()
	{
		if(nullptr == g_instance_)
		{
			g_mutex_.lock();
			if(nullptr == g_instance_)
			{
				g_instance_ = new T();
			}
			g_mutex_.unlock();
		}
		return g_instance_;
	}		
	static void release()
	{
		g_mutex_.lock();
		if(nullptr != g_instance_)
		{
			delete g_instance_;
			g_instance_ = nullptr;
		}
		g_mutex_.unlock();
	}
protected:
	static IMMutex			g_mutex_;
	static T*				g_instance_;
};

template<typename T>
T* Singleton<T>::g_instance_ = nullptr;

template<typename T>
IMMutex Singleton<T>::g_mutex_;

}  // end namespace

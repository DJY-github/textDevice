#pragma once
#include "source_config.h"

#include "util_imdir.h"
#include "util_thread.h"
class Log
{
public:
	Log(void);
	~Log(void);
	void setLogFile(const im_string& file_name);
	void write(const char *fmt, ...);
private:
	im_string	file_name_;

	util::IMRecursiveMutex	mutex_;
	util::IMFile file_;
};


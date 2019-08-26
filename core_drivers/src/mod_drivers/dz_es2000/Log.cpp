#include "Log.h"
#include <stdarg.h>

#include "util_datetime.h"
#include "util_textcode.h"


Log::Log(void)
{
}


Log::~Log(void)
{
}

void Log::setLogFile(const im_string& file_name)
{
	file_name_ = file_name;
}
void Log::write(const char *fmt, ...)
{
	using namespace util;

	char msg[4096] = {0};
	va_list ap;
	va_start(ap, fmt);
	im_vsnprintf_s(msg, 4096, 4096, fmt, ap);
	va_end(ap);

	strcat_s(msg, "\r\n");

	static unsigned char utf8_header[] = {0xEF,0xBB,0xBF};

	IMRecursiveMutexLockGuard locker(mutex_);

	static const unsigned long kLogFileMaxSize = 1024*1024*20;
	static unsigned long	current_log_file_size = 0;
	im_string log_file_name = im_string(IMDir::applicationDir().path()) + "/log/" + file_name_;

	if(current_log_file_size > kLogFileMaxSize)
	{
		file_.close();
		file_.open(log_file_name.c_str(), IMFile::kRead|IMFile::kWrite, IMFile::kTruncateExisting);
		file_.write("\n",1);
		file_.close();
	}
	if(!file_.isOpen())
	{
		file_.open(log_file_name.c_str(), IMFile::kRead|IMFile::kWrite, IMFile::kOpenAlways);
	}

	if(file_.isOpen())
	{
		file_.setFilePointer(0, FILE_END);
		current_log_file_size  = file_.getFileSize();
		if(0 == current_log_file_size)
		{

			file_.write((char*)utf8_header, 3);
		}

		im_string str_time = IMDateTime::currentDateTime().toString() + " ";
		file_.write(str_time.c_str(), str_time.size());
		file_.write(msg, strlen(msg));
	}

}

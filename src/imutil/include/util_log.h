/*
用法： LogDebug("软件模块名称").format("格式化字符串")
	如： logDebug("rtdb").format("client: %d XX", clientID)
*/


#ifndef _UTIL_LOG_H_
#define _UTIL_LOG_H_

#include "../include/source_config.h"

#ifdef IM_UTIL_EXPORTS
#define UTILLOG DLL_EXPORT
#else
#define UTILLOG DLL_IMPORT
#endif



namespace util {


#define IM_STRINGIZE(x) #x
#define IM_TO_STRING(x)  IM_STRINGIZE(x)
#define IM_FILE_LINE __FILE__ "[" IM_TO_STRING(__LINE__) "] "

#define LOG_WRITE(module, verbo, level)  util::IMLog(module, verbo, level, IM_FILE_LINE, __FUNCTION__)
#define LogLable(module)		LOG_WRITE(module, util::IMLog::kText | util::IMLog::kModuleName, util::IMLog::kLable)
#define LogDebug(module)		LOG_WRITE(module, util::IMLog::kDefault, util::IMLog::kDebug)
#define LogInfo(module)			LOG_WRITE(module, util::IMLog::kDefault, util::IMLog::kInformation)
#define LogWarning(module)		LOG_WRITE(module, util::IMLog::kDefault, util::IMLog::kWarning)
#define LogError(module)		LOG_WRITE(module, util::IMLog::kDefault, util::IMLog::kError)
#define LogCritical(module)		LOG_WRITE(module, util::IMLog::kAll, util::IMLog::kCritical)

#define MODULE_NULL	0
#define MODULE_DB  "DB"

class UTILLOG IMLog
{
public:
	enum Level
	{
			kLable,
			kDebug,
			kInformation,
			kWarning,
			kError,
			kCritical
	};

	enum Verbosity
	{
		kText			= 0,				// 只包含基本内容
		KLevel			= (1 << 0),			// 包含日志级别
		kFileLine		= (1 << 1),			// 包含代码行
		kFunction		= (1 << 2),			// 包含代码函数名称
		kDateTime		= (1 << 3),			// 包含日期时间
		kPid			= (1 << 4),			// 包含进程ID
		kThreadId		= (1 << 5),			// 包含线程ID
		kModuleName		= (1 << 6),			// 包含模块名称

		kDefault		= KLevel | kDateTime | kModuleName,
		kAll			= KLevel | kFileLine | kFunction | kDateTime | kPid | kThreadId | kModuleName
	};
	IMLog(const im_char* module_name, int verbo, int level, const char* file_line, const char* funcation);
	~IMLog();
	im_string				format(const im_char *fmt, ...);

	static void setLogFile(const char* log_file);

private:
	void				writeToFile(const char* strdate, const char* content);
	void                writeToDb(const char* content);
private:
	static const Int32 kMaxLogBufSize = 4096;
	char	log_msg_[kMaxLogBufSize];
	static char log_file_[256];
    char    module_name_[256];
};

}

#endif

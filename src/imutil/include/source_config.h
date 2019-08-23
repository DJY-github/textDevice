#ifndef _SOURCE_CONFIG_H_
#define _SOURCE_CONFIG_H_

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdint.h>
#include <string.h>


#ifdef _WIN32
	#ifndef _UNICODE
	#define _UNICODE
	#endif
#else
#endif

// 编码要求:
// 1.整个系统代码中采用im_char,im_string等形式的窄字符。
// 2.源代码文件采用utf8代码页
// 3.源代码中不能硬编码非ascii字符， 注释除外
// 4.尽量不要 对非ascii字符 进行字符串的比较，查找。
// 5.在windows系统中，涉及到的windows api 中 带有wchar_t的参数， 需用util_textcode.h中的unicodeToUtf8或utf8ToUnicode函数转换im_string

typedef	 char					im_char;
typedef  std::string			im_string;
typedef  std::ostringstream		im_ostringstream;
typedef  std::ostream			im_ostream;
#define	 im_atoi				atoi
#define  im_atof				atof

#ifdef _WIN32
	#define  im_sprintf				_snprintf_s    // 禁止使用sprintf 或 sprintf_s
#else
	#define  im_sprintf				snprintf
	#define	  nullptr  NULL
#endif // _WIN32

#define	 im_printf				printf
#define  im_vsnprintf_s			vsnprintf_s


typedef int16_t			Int16;
typedef int32_t			Int32;
typedef int64_t			Int64;
typedef uint16_t		UInt16;
typedef uint32_t		UInt32;
typedef uint64_t		UInt64;
typedef float			Float;
typedef double			Double;
typedef	time_t			Time;

// CIID类型选项
#define CIID_IS_STRING
#ifdef  CIID_IS_STRING
	typedef	im_string		CIID;
#else
	typedef	Int32			CIID;
#endif

typedef std::string		 libistring;


// new 操作
#ifdef	USE_MEM_POOL
	//#define IM_NEW			mynew
#else
	#define IM_NEW			new
	#define IM_DELETE		delete
#endif

// new失败后的处理,在main() 函数中调用即可
#define  SET_NEW_FAILD_CALL(x) std::set_new_handler(x)


// new失败后的处理,在main() 函数中调用即可
#ifdef _WIN32
	#define DLL_EXPORT  __declspec(dllexport)
	#define DLL_IMPORT  __declspec(dllimport)
#else
	#define DLL_EXPORT
	#define DLL_IMPORT
#endif

#define LOG_MODULE_ZFMS			"zfms"
#define LOG_MODULE_CMDB			"cmdb"
#define LOG_MODULE_RTDB			"rtdb"
#define LOG_MODULE_HDB			"hdb"
#define LOG_MODULE_DM			"dm"
#define LOG_MODULE_LINKAGE		"linkage"
#define LOG_MODULE_DRIVER		"driver"
#define LOG_MODULE_UTIL			"util"
#define LOG_MODULE_RESTFUL		"restful"
#define LOG_MODULE_AE			"ae"
#define LOG_MODULE_RBAC			"rbac"
#define LOG_MODULE_STAT			"stat"
#define LOG_MODULE_UPLINK		"uplink"
#define LOG_MODULE_DOG			"dog"


#endif // _UTIL_SOURCE_H_

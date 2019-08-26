
#pragma once

#include "source_config.h"
#ifdef _WIN32
	#include <windows.h>
	#include <tchar.h>
#endif

#ifdef IM_UTIL_EXPORTS
#define UTILTEXTCODE DLL_EXPORT
#else
#define UTILTEXTCODE DLL_IMPORT
#endif

namespace util {

UTILTEXTCODE std::string	tr(const char* text);			// 翻译


UTILTEXTCODE std::wstring utf8ToUnicode(const char *utf8_str);
#ifdef _WIN32
UTILTEXTCODE std::string unicodeToUtf8(const wchar_t *unicode_str);
UTILTEXTCODE std::string  utf8ToGBK(const char* utf8_str);
#endif

}   // end namespace

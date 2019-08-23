#pragma once

#ifdef IM_UTIL_EXPORTS
#define UTILCOMMFUN DLL_EXPORT
#else
#define UTILCOMMFUN DLL_IMPORT
#endif

#include "source_config.h"
#include <vector>

namespace util{


im_string UTILCOMMFUN replaceString(const im_string& str,  const char* src, const char* dest);

im_string UTILCOMMFUN replaceChar(im_string& str, const char src, const char dest);

std::vector<im_string> UTILCOMMFUN spliteString(const im_string& src, const char spliter);
im_string UTILCOMMFUN strupr(const im_string& str);
im_string UTILCOMMFUN urlencode(const im_string& str);
im_string UTILCOMMFUN urldecode(const im_string& str);

im_string UTILCOMMFUN shellExe(const char* cmd, bool trim_return_char = true, bool* is_ok = NULL);
int UTILCOMMFUN ifMacAddr(unsigned char mac[6]);

im_string UTILCOMMFUN trim(im_string& str);

int UTILCOMMFUN spliteString(const im_string& src, const im_string& spliter, im_string &first, im_string &second, bool from_tail= false);

im_string UTILCOMMFUN escapeString(const std::string& str);

int UTILCOMMFUN diskAilableMB(const char* path);

int UTILCOMMFUN createDir(const char* dir);
int UTILCOMMFUN writeFile(const char* filename,   const char* data,  int len);

}


#pragma once


#include "source_config.h"
#include "json/value.h"
#include "util_imdata.h"


#ifdef IM_UTIL_EXPORTS
#define UTILCONF DLL_EXPORT
#else
#define UTILCONF DLL_IMPORT
#endif


namespace util {

UTILCONF bool readJConf(const im_string& file, Json::Value& val);
UTILCONF bool writeJConf(const im_string& file, const Json::Value& val);

UTILCONF bool readJConf(const im_string& file, IMData& val);
UTILCONF bool writeJConf(const im_string& file, const IMData& val);

}

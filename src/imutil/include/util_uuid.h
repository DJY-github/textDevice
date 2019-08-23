#pragma once

#ifdef IM_UTIL_EXPORTS
#define UTILUUID DLL_EXPORT 
#else
#define UTILUUID DLL_IMPORT
#endif

#include "source_config.h"

namespace util {

class UTILUUID IMUuid
{
public:
	static im_string createUuid(); 
};

}   // end namespace

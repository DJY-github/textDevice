#pragma once
#include "source_config.h"
#include "util_imdata.h"


#ifdef IM_DM_EXPORTS
#define DMAPI DLL_EXPORT
#else
#define DMAPI DLL_IMPORT
#endif

namespace dm {

DMAPI	Int32   ctl(const CIID& devid, const util::IMData& req, util::IMData& resp);
DMAPI	Int32   notice(const CIID& devid, const util::IMData& msg);

}

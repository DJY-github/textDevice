/*
util_env_conf.h
ϵͳ��ȫ������
*/

#pragma once

#include "source_config.h"


#ifdef IM_UTIL_EXPORTS
#define UTILENVCONF DLL_EXPORT
#else
#define UTILENVCONF DLL_IMPORT
#endif


namespace util {

UTILENVCONF	im_string	getAppPath();
UTILENVCONF	im_string	getEnv(const im_string& module, const im_string& var_name, const im_string& default_var_value="");
UTILENVCONF	void		setEnv(const im_string& module, const im_string& var_name, const im_string& var_value);


UTILENVCONF int         loadDebugSwitch();
UTILENVCONF bool        serialLogSwitch();


}   // end namespace

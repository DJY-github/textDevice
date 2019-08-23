#pragma once
#include "source_config.h"

#ifdef IM_UTIL_EXPORTS
#define BASE64 DLL_EXPORT 
#else
#define BASE64 DLL_IMPORT
#endif


namespace util{

class BASE64 IMBase64
{
public:
	IMBase64();
	~IMBase64();

	static im_string	encode(const unsigned char *in_data, unsigned long in_len);
	static bool			decode(const im_string& str, unsigned char *out_data, unsigned long *out_len);	
};



}

#pragma once

#include "source_config.h"

#ifdef IM_UTIL_EXPORTS
#define UTILMD5 DLL_EXPORT 
#else
#define UTILMD5 DLL_IMPORT
#endif

namespace util {  namespace security {

class UTILMD5 CMD5
{
public:
	CMD5();
	std::string generateMD5(const char* buffer,int buffer_len);

private:
	#define uint8  unsigned char
	#define uint32 unsigned long int

	unsigned long m_data[4];
	struct md5_context
	{
		uint32 total[2];
		uint32 state[4];
		uint8 buffer[64];
	};

private:
	void md5_starts( struct md5_context *ctx );
	void md5_process( struct md5_context *ctx, uint8 data[64] );
	void md5_update( struct md5_context *ctx, uint8 *input, uint32 length );
	void md5_finish( struct md5_context *ctx, uint8 digest[16] );

	std::string toString();
};
                                        

}}

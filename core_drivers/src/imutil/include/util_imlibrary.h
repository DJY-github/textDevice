#pragma  once

#include "source_config.h"


#ifdef IM_UTIL_EXPORTS
#define UTILLIBRARY DLL_EXPORT 
#else
#define UTILLIBRARY DLL_IMPORT
#endif


namespace util {

class UTILLIBRARY IMLibrary
{
public:
	IMLibrary();
	~IMLibrary();
	bool		dlOpen(const im_char* libname);
	void*		dlSymbol(const char *symbol);
	void		dlClose();
private:
	void*		handle_;
};

}

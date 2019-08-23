#pragma once

#include <list>
#include "source_config.h"
#include "util_thread.h"

#ifdef IM_UTIL_EXPORTS
#define UTILPROCESSMGR DLL_EXPORT 
#else
#define UTILPROCESSMGR DLL_IMPORT
#endif


namespace util{


class UTILPROCESSMGR ProcessMgr
{
public:
	struct Process
	{
		im_string	exec_name;
		im_string	file_full_name;
		im_string   cmd_params;
		im_string	work_path;
		int			show_cmd;
	};

	static	void addProcess(const Process& process);
	static	void removeProcess(const im_string& exec_name);
	static	void clear();

	static  void daemon();  
	
};

}

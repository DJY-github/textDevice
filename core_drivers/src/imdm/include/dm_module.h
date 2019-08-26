#pragma once
#include "source_config.h"

#ifdef IM_DM_EXPORTS
#define DMMODULE DLL_EXPORT 
#else
#define DMMODULE DLL_IMPORT
#endif

namespace dm {

/*************************************************
No.1
Name:       init
Desc:       ��ʼ��
Calls:      dispatcher::Dispatcher::instance()->load()
Input:      ��
Output:     ��
Return:     0:�ɹ���<0:ʧ��
Others:     
*************************************************/
DMMODULE	Int32		init();

/*************************************************
No.2
Name:       uninit
Desc:       ����ʼ��
Calls:      dispatcher::Dispatcher::instance()->unload()
Input:      ��
Output:     ��
Return:     0:�ɹ���<0:ʧ��
Others:     
*************************************************/
DMMODULE	Int32		uninit();



}

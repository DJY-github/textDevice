#pragma once

#include "source_config.h"

#ifdef IM_UTIL_EXPORTS
#define UTILSERIAL DLL_EXPORT
#else
#define UTILSERIAL DLL_IMPORT
#endif

#define SERIAL_LOG
#ifdef SERIAL_LOG
    #include "util_imdir.h"
#endif

class  PortUDP
{
private:
	PortUDP();
public:
	~PortUDP();
public:

	static PortUDP* getSerial();

	/*
	serial_name:  like "COMxx:" in windows  or like 192.168.1.100:8001(kSocketSerial)
	serial_params: must be like "9600:N:8:1"
	*/
	bool  open(const char* serial_name, const char* serial_params, float timeout_second = 3);			// 如果已经打开，直接返回true
	bool  isOpen();
	bool  reOpen();
	void  close();
	int	  read(char* data, int len, int timeout = 1000);
	int   write(char* data, int len, int timeout = 1000);
    void  flush();

    #ifdef SERIAL_LOG
    void writeLog(const char* type, char* data, int len);
    #endif

    const char* name() {return serial_name_;}


private:

    void*  sock_;

	bool   is_open_;
	static const int kSzie = 32;
	char   serial_name_[kSzie];
	char   serial_params_[kSzie];
    float timeout_second_;

    #ifdef SERIAL_LOG
    util::IMFile log_;
    unsigned long current_log_size_;
    #endif
};





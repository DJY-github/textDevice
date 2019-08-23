#pragma once

#ifdef _WIN32
#include <winsock.h>
typedef SOCKET Socket;
#else
#include "sys/socket.h"
#include "util_thread.h"

typedef int Socket;
#define SOCKET_ERROR  (-1)
#endif

typedef void  (*FP_RECV_CALLBACK)(const im_string& data,  void* arg);
typedef void  (*FP_RECONN_CALLBACK)(void* arg);
class Connector
{
public:
	enum SockState
	{
		kOK				= 0,
		kDisconnected	= -1,
		kError			= -2,
		kTimeOut		= -3
	};

	Connector(void);
	~Connector(void);

	bool		init(const char* ips, unsigned int port, unsigned int tm_second=3);
	void		uninit();
	void       switchConnection();
    void       closeSocket();
    bool	    isConnected() const;

	bool       send(const im_string& data);
    void       setCallback(FP_RECONN_CALLBACK fp_reconn_callback, FP_RECV_CALLBACK fp_recv_callback, void* arg);

private:
	bool		reconnect();
	int			write(const char* data, int len);
	int			read(char* data, int len, unsigned int time_out);
    int			getSockError() const;

private:
    static void* recvThread(void* arg);

private:
    util::Thread                            thread_;
    FP_RECONN_CALLBACK  fp_reconn_callback_;
    FP_RECV_CALLBACK         fp_recv_callback_;
    void*                                       callback_arg_;

private:
	Socket			                                sock_;
	std::vector<im_string>			vec_ips_;                // 主备IP
	unsigned int                              vec_ips_index_;
	unsigned int	                            port_;
	bool			                                    connected_;
	unsigned        int                       tm_second_;

	util::IMRecursiveMutex   mutex_;

};


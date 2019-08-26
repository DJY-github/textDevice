#pragma once


#ifdef _WIN32
#include <winsock.h>
typedef SOCKET Socket;
#else
#include "sys/socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int Socket;
#define SOCKET_ERROR  (-1)
#endif


#define IP_CHAR_LEN	16

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

	bool		connectHost(const char* ip, unsigned int port, unsigned int tm_second=3);
	void		disconnect();
	bool		connect();
	int			write(const char* data, int len, unsigned int time_out);
	int			read(char* data, int len, unsigned int time_out);
    void        flush();

	inline const bool	isConnected() const
	{
		return connected_ && (SOCKET_ERROR == getSockError()?false:true);
	}

private:
	int			getSockError() const;

private:
	Socket			sock_;
	char			ip_[IP_CHAR_LEN];
	unsigned int	port_;
	bool			connected_;
	unsigned        int tm_second_;
    sockaddr_in     host_addr_;


};


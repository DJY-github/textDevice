#include "serial_socket_udp.h"

#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>


#ifdef _WIN32
#include <windows.h>
#define INVALIDSOCKHANDLE   INVALID_SOCKET
#define ISSOCKHANDLE(x)		(x!=INVALID_SOCKET)
#define BLOCKREADWRITE      0
#define NONBLOCKREADWRITE   0
#define SENDNOSIGNAL        0
#define ETRYAGAIN(x)		(x==WSAEWOULDBLOCK||x==WSAETIMEDOUT)

#else

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#define ISSOCKHANDLE(x)    (x>0)
#define BLOCKREADWRITE      MSG_WAITALL
#define NONBLOCKREADWRITE   MSG_DONTWAIT
#define SENDNOSIGNAL        MSG_NOSIGNAL
#define ETRYAGAIN(x)        (x==EAGAIN || x==EWOULDBLOCK)
#define INVALID_SOCKET  0

#endif



Connector::Connector(void):connected_(false)
{
	memset(ip_, 0, IP_CHAR_LEN);
#ifdef _WIN32
	WSADATA  ws;
	WSAStartup(MAKEWORD(2,2), &ws);
#endif
    memset(&host_addr_, 0, sizeof(host_addr_));
}


Connector::~Connector(void)
{
#ifdef _WIN32
	WSACleanup();
#endif
	disconnect();
}

bool Connector::connectHost(const char* ip, unsigned int port, unsigned int tm_second)
{
#ifdef _WIN32
	strcpy_s(ip_, IP_CHAR_LEN, ip);
#else
	strncpy(ip_, ip, IP_CHAR_LEN);
#endif
	port_ = port;
	tm_second_ = tm_second;
    if(tm_second_ <= 0)
    {
        tm_second_ = 1;
    }

    return connect();
}

void Connector::disconnect()
{
	if(sock_ != INVALID_SOCKET)
	{
#ifdef _WIN32
		closesocket(sock_);
#else
		close(sock_);
#endif
		sock_ = INVALID_SOCKET;
	}

	connected_ = false;
	close(sock_);
}

int Connector::getSockError() const
{
#ifdef _WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

int Connector::write(const char* data, int len, unsigned int time_out)
{
	//SockState sock_state = kOK;
	int  data_pos		= 0;
	int  one_time_send	= 0;
	int  need_send		= len;
	int  timeout_cnt    = 0;

	while(true)
	{
        if(timeout_cnt > 1)
        {
            return -1;
        }

        one_time_send = sendto(sock_, data+data_pos, need_send, BLOCKREADWRITE|SENDNOSIGNAL, (struct sockaddr *)&host_addr_, sizeof(host_addr_));

		if(one_time_send > 0)
		{
			data_pos	+= one_time_send;
			need_send	-= one_time_send;
			if(data_pos >= len)
			{
				break;
			}
		}
		else if(0 == one_time_send)
		{
            errno = ECONNRESET;
			return -1;
		}
		else
		{
			if(ETRYAGAIN(getSockError()))  // socket busy
			{
                timeout_cnt++;
				usleep(20000);
			}
			else
			{
				return -1;
			}
		}

	}

	return data_pos;
}

int Connector::read(char* data, int len, unsigned int time_out)
{
	int  data_pos		= 0;
	int  one_time_recv	= 0;
	int  need_recv		= len;
	int  timeout_cnt    = 0;

	//while(true)
	//{
        //if(timeout_cnt > 1)
        //{
         //   return -1;
       // }

        fd_set fdRead;
        FD_ZERO(&fdRead);
        FD_SET(sock_, &fdRead);

        struct timeval timeout;

        if(0 == timeout_cnt)
        {
            timeout.tv_sec = time_out/1000;
            if(0 == timeout.tv_sec )
            {
                timeout.tv_usec = time_out*1000;
            }
            else
            {
                timeout.tv_usec = 0;
            }
        }
        else
        {
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
        }

		int iRet = select(sock_+1, &fdRead, NULL, NULL, &timeout);
		if (iRet && FD_ISSET(sock_, &fdRead))
		{
            socklen_t host_len=sizeof(host_addr_);
            one_time_recv = recvfrom(sock_, data+data_pos, need_recv, NONBLOCKREADWRITE, (struct sockaddr *)&host_addr_, &host_len);

			if(one_time_recv > 0)
			{
				data_pos	+= one_time_recv;
				/*need_recv	-= one_time_recv;
				if(data_pos >= len)
				{
					break;
				}*/
				return data_pos;
			}
			else if(0 == one_time_recv)
			{
                errno = ECONNRESET;
				return -1; //kDisconnected;
			}
			else
			{
				if(!ETRYAGAIN(getSockError()))  // socket busy
				{
					return -1;
				}
				else
				{
                   //timeout_cnt++;
                    usleep(200000);        //200毫秒
				}

			}

		}
		else if(0 == iRet)    // TimeOut
		{
           // timeout_cnt++;
            //usleep(20000);        //20毫秒
            errno = ETIMEDOUT;
           // return -1;
		}
		else
		{
            printf("!!!!!!!!!!select -1\n");
            return -1;
		}

	//}

	return data_pos;

}

void Connector::flush()
{
    int try_cnt = 0;
    while(true)
    {
        if(try_cnt++ > 100)
        {
            return;
        }

        fd_set fdRead;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        FD_ZERO(&fdRead);
        FD_SET(sock_, &fdRead);
        int iRet = select(sock_+1, &fdRead, NULL, NULL, &timeout);
        if (iRet && FD_ISSET(sock_, &fdRead))
        {
            char tmp[2] = {0};
            if(recv(sock_, tmp, 1, BLOCKREADWRITE) > 0)
            {
                 usleep(10000);
            }
            else
            {
                break;
            }

        }
        else
        {
            break;
        }

    }
}


bool Connector::connect()
{
    disconnect();

    sock_=socket(AF_INET,SOCK_DGRAM,0);
    if(-1 != sock_)
	{
		int flags = fcntl(sock_, F_GETFL, NULL);
		flags |= O_NONBLOCK;
        fcntl(sock_, F_SETFL, flags);
        fcntl(sock_, F_SETFD, FD_CLOEXEC);

	    int set =1;
	    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &set,  sizeof(int));
        host_addr_.sin_family = AF_INET;
        host_addr_.sin_port = htons(port_);
        host_addr_.sin_addr.s_addr = htonl(INADDR_ANY);

        int ret =bind(sock_,(struct sockaddr*)&host_addr_,sizeof(host_addr_));
        if(ret < 0)
        {
            printf("bind err %s\n",strerror(errno));
            return false;
        }
        printf("sock:%d,bind:%d,port:%d,err:%s\n",sock_,ret,port_,strerror(errno));
	}
	else
    {
        printf("INVALID_SOCKET??sock_=%d\n",sock_);
    }

	connected_ = true;
	return connected_;

}



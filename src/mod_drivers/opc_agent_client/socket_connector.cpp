#include "socket_connector.h"

#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>
#include "util_commonfun.h"

#include <stdlib.h>
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


Connector::Connector(void):connected_(false)
{
	fp_reconn_callback_ = NULL;
	fp_recv_callback_ = NULL;
	callback_arg_ = NULL;
	sock_ = INVALID_SOCKET;
	vec_ips_index_ = 0;
}


Connector::~Connector(void)
{

}

bool Connector::init(const char* ips, unsigned int port, unsigned int tm_second)
{
    vec_ips_ = util::spliteString(ips, ';');

	port_ = port;
	tm_second_ = tm_second;
    if(tm_second_ <= 0)
    {
        tm_second_ = 1;
    }

    if( !thread_.start(recvThread,  this) )
    {
        return false;
    }
	return reconnect();
}

 void Connector::setCallback(FP_RECONN_CALLBACK fp_reconn_callback,
                             FP_RECV_CALLBACK fp_recv_callback, void* arg)
 {
    fp_reconn_callback_  = fp_reconn_callback;
    fp_recv_callback_       = fp_recv_callback;
    callback_arg_               = arg;
 }

bool Connector::reconnect()
{
     util::IMRecursiveMutexLockGuard locker(mutex_);

    const char * ip = vec_ips_[vec_ips_index_++].c_str();
    if(vec_ips_index_ >=  vec_ips_.size())
    {
        vec_ips_index_ = 0;
    }
    printf("----------opc_agent reconnect:%s\n", ip);

	closeSocket();

	int protocol = 0;
	sock_ = socket(AF_INET, SOCK_STREAM, protocol);
	if(INVALID_SOCKET != sock_)
	{
        // Set non-blocking
		int flags = fcntl(sock_, F_GETFL, NULL);
		flags |= O_NONBLOCK;
        fcntl(sock_, F_SETFL, flags);
        fcntl(sock_, F_SETFD, FD_CLOEXEC);

        // Trying to connect with timeout
		sockaddr_in host_addr;
		host_addr.sin_family = AF_INET;
		host_addr.sin_port = htons(port_);
		host_addr.sin_addr.s_addr = inet_addr(ip);
		int re = ::connect(sock_, (sockaddr*)&host_addr, sizeof(sockaddr));

		if(re < 0)
        {
            if(EINPROGRESS == errno)
            {
                fd_set myset;
                struct timeval tv;
                tv.tv_sec = tm_second_;
                tv.tv_usec = 0;
                FD_ZERO(&myset);
                FD_SET(sock_, &myset);
                if(select(sock_+1, NULL, &myset, NULL, &tv) > 0)
                {
                    socklen_t lon = sizeof(int);
                    int valopt;
                    getsockopt(sock_, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
                    if(valopt)
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }

            }
            else
            {
                return false;
            }
        }
	}

    connected_ = true;
	return true;

}

bool Connector::isConnected() const
{
		return connected_;
}

void Connector::uninit()
{
    thread_.stop();
    closeSocket();
}

void  Connector::switchConnection()
{
    if(vec_ips_.size() > 1)
    {
            closeSocket();
    }
}

void Connector::closeSocket()
{
    if(sock_ != INVALID_SOCKET)
	{
		close(sock_);
		sock_ = INVALID_SOCKET;
	}

	connected_ = false;
}

int Connector::getSockError() const
{
	return errno;
}

bool Connector::send(const im_string& data)
{
    util::IMRecursiveMutexLockGuard locker(mutex_);

    if(!connected_)
    {
        return false;
    }
    char head[9] = {0};
    snprintf(head, 9, "%.8d", data.size());

    im_string msg = im_string(head) + data;

    int len = msg.size();
    int re = write(msg.c_str(), len);
    if( re != len)
    {
        if(-1 == re)
        {
            closeSocket();
        }
        return false;
    }
    return true;
}

int Connector::write(const char* data, int len)
{
	int  data_pos		= 0;
	int  one_time_send	= 0;
	int  need_send		= len;
	int  timeout_cnt    = 0;

	while(true)
	{
        if(timeout_cnt > 1)
        {
            return 0;
        }

		one_time_send = ::send(sock_, data+data_pos, need_send, BLOCKREADWRITE|SENDNOSIGNAL);
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

void* Connector::recvThread(void* arg)
{

    Connector* conn = (Connector*)arg;

    char data_head[9] = {0};
    int body_len = 0;
    while(!conn->thread_.isStopCalled())
    {
        if(!conn->isConnected())
        {
             if(!conn->reconnect())
             {
                 sleep(3);
                 continue;
             }

             if(NULL != conn->fp_reconn_callback_)
             {
                 conn->fp_reconn_callback_(conn->callback_arg_);
             }
        }


         int re = conn->read(data_head, 8, 2000);
         if(8 == re )
         {
            body_len = atoi(data_head);

            char *body_data = new char[body_len+1];
            memset(body_data, 0, body_len+1);
             re = conn->read(body_data, body_len, 6000);
             if(body_len == re)
             {
                 if(NULL != conn->fp_recv_callback_)
                 {
                     conn->fp_recv_callback_(body_data, conn->callback_arg_);
                 }
             }

             delete[] body_data;
         }

         if(-1 == re)
         {
            conn->closeSocket();
         }
    }

    return 0;
}
int Connector::read(char* data, int len, unsigned int time_out)
{
	int  data_pos		= 0;
	int  one_time_recv	= 0;
	int  need_recv		= len;
	int  timeout_cnt    = 0;

	while(true)
	{
        if(timeout_cnt > 1)
        {
            return 0;
        }

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
            mutex_.lock();
			one_time_recv = recv(sock_, data+data_pos, need_recv, BLOCKREADWRITE);
            mutex_.unlock();

			if(one_time_recv > 0)
			{
				data_pos	+= one_time_recv;
				need_recv	-= one_time_recv;
				if(data_pos >= len)
				{
					break;
				}
			}
			else if(0 == one_time_recv)
			{
                errno = ECONNRESET;

				return -1; //kDisconnected;
			}
			else
			{
				if(!ETRYAGAIN(getSockError()))  // ! socket busy
				{
					return -1;
				}
				else
				{
                    timeout_cnt++;
                    usleep(200000);        //200毫秒
				}
			}
		}
		else if(0 == iRet)    // TimeOut
		{
            timeout_cnt++;
            usleep(20000);        //20毫秒
            errno = ETIMEDOUT;
		}
		else
		{
            return -1;
		}

	}

	return data_pos;

}


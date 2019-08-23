#include "serial_port.h"
#ifdef _WIN32
#include <windows.h>
#else
#include "serial_socket_udp.h"
#endif

#include "util_imvariant.h"
#include "util_log.h"
#include "util_thread.h"
#include "stdlib.h"
#include <errno.h>

#include "util_imdir.h"


#ifdef SERIAL_LOG
    #include "util_datetime.h"
    #include "util_imbufer.h"
    #include "util_env_conf.h"
#endif


static PortUDP*	g_serial_port;

PortUDP* PortUDP::getSerial()
{
	if(nullptr == g_serial_port)
	{
		g_serial_port = new PortUDP();
	}

	return g_serial_port;
}

PortUDP::PortUDP()
{
	sock_ = NULL;
	is_open_	= false;
	memset(serial_name_, 0 , kSzie);
	memset(serial_params_, 0, kSzie);

	#ifdef SERIAL_LOG
    current_log_size_ = 0;
    #endif

}

PortUDP::~PortUDP()
{
	close();
}

bool PortUDP::open(const char* serial_name, const char* serial_params, float timeout_second)
{
	if(isOpen())
	{
		return true;
	}

	int  pos = 0;
    if(-1 != (pos=im_string(serial_name).find(":")))
	{
        strncpy(serial_name_, serial_name, strlen(serial_name));
        strncpy(serial_params_, serial_params, strlen(serial_params));
        timeout_second_ = timeout_second;

        im_string ip = im_string(serial_name).substr(0, pos);
        im_string port = im_string(serial_name).substr(pos+1);
        sock_ = new Connector();

        is_open_ = static_cast<Connector*>(sock_)->connectHost(ip.c_str(), atoi(port.c_str()), timeout_second_);
        if (!is_open_)
        {
            delete static_cast<Connector*>(sock_);
            sock_ = NULL;
        }

        return is_open_;
	}
    return false;
}
void PortUDP::close()
{
    if(NULL != sock_)
    {
        static_cast<Connector*>(sock_)->disconnect();
        delete static_cast<Connector*>(sock_);
        sock_	= NULL;
    }
    is_open_ = false;
}

bool PortUDP::isOpen()
{
	return is_open_;
}

bool PortUDP::reOpen()
{
	close();
	return open(serial_name_, serial_params_, timeout_second_);
}

int PortUDP::read(char* data, int len, int timeout)
{
    int re = 0;

    if(NULL == sock_)
    {
        return -1;
    }

    re = static_cast<Connector*>(sock_)->read(data, len, timeout);

	if(-1 == re)
    {
        if(errno == ETIMEDOUT)
        {
           // flush();
            re = 0;
        }
        else if((errno == ECONNRESET || errno == ECONNREFUSED || errno == EBADF))
        {
            close();
        }

        #ifdef SERIAL_LOG
        if(len > 1)
        {
            writeLog("recv timeout.", data, 0);
        }

        #endif

	}
    #ifdef SERIAL_LOG
	else
	{
        writeLog("recv:", data, re);
	}
     #endif

	return re;

}

int PortUDP::write(char* data, int len, int timeout)
{
    int re = 0;
    if(NULL == sock_)
    {
        return -1;
    }
    re =  static_cast<Connector*>(sock_)->write(data, len, timeout);

	if(-1 == re)
	{
        if(errno == ETIMEDOUT)
        {
            flush();
        }
        else if((errno == EBADF || errno == ECONNRESET || errno == EPIPE))
        {
            close();
        }

    #ifdef SERIAL_LOG
    writeLog("send error.", data, 0);
    #endif

	}
    #ifdef SERIAL_LOG
	else
	{
        writeLog("send:", data, re);
	}
	#endif


	return re;

}

void  PortUDP::flush()
{
    if(NULL != sock_)
    {
        static_cast<Connector*>(sock_)->flush();
    }
}


#ifdef SERIAL_LOG
void PortUDP::writeLog(const char* type, char* data, int len)
{
    if(!util::serialLogSwitch())
    {
        return;
    }

    util::IMBuffer<1024> buf;

    if(len > 0)
    {
        buf.ToAsc((unsigned char*)data, len);
    }

    im_string serilname = serial_name_;
    size_t pos = serilname.find("/dev/");
    if(im_string::npos != pos)
    {
        serilname = serilname.substr(pos+5);
    }

    static const unsigned long kLogFileMaxSize = 1024*1024*1;
	im_string log_file_name = im_string(util::IMDir::applicationDir().path()) + "/log/chan_" + serilname + ".log";
	if(current_log_size_ > kLogFileMaxSize)
	{
		log_.close();
		log_.open(log_file_name.c_str(), util::IMFile::kRead|util::IMFile::kWrite, util::IMFile::kTruncateExisting);
		log_.write("\n",1);
		log_.close();
	}
	if(!log_.isOpen())
	{
		log_.open(log_file_name.c_str(), util::IMFile::kRead|util::IMFile::kWrite, util::IMFile::kOpenAlways);
	}

	if(log_.isOpen())
	{
		log_.setFilePointer(0, SEEK_END);
		current_log_size_  = log_.getFileSize();

		im_string msg;
		if(len > 0)
		{
            msg = util::IMDateTime::currentDateTime(util::kLocal).toString() + " " + im_string(type) + ":" + im_string(buf.ptr()) + "\n";
		}
		else
		{
            msg = util::IMDateTime::currentDateTime(util::kLocal).toString() + " " + im_string(type) + "\n";
		}

		log_.write(msg.c_str(), msg.size());
	}
}
#endif



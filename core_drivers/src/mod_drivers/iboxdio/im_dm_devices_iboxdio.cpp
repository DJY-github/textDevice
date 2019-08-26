#include "im_dm_devices_iboxdio.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "util_imvariant.h"
#include "util_datetime.h"
#include "util_datastruct.h"


#define IBOXGPIO_SET_OUTPUT_LOW 0
#define IBOXGPIO_SET_OUTPUT_HIGH 1

#include <iostream>
using namespace std;

Device::Device(void)
{
    fd_ = 0;
    do_status = 15;
}


Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{
    if(0 == fd_)
    {
        fd_ = ::open("/tmp/dostatus", O_CREAT|O_RDWR, 0644);
        if(0 >= fd_)
        {
            return util::RTData::kCommDisconnected;
        }
    }

	return util::RTData::kOk;
}

int Device::close()
{
    if(0 != fd_)
    {
        ::close(fd_);
    }
	return util::RTData::kOk;
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    if(0 == fd_)
    {
        return util::RTData::kCommDisconnected;
    }

    unsigned char level[1]={0};
    unsigned char di_status;
    di_status = ibox_dio.GetIO(&level[0],0);

    lseek(fd_, 0, SEEK_SET);
    if(-1 == ::read(fd_,&do_status,sizeof(do_status)))
    {
        ::close(fd_);
        fd_ = 0;
        return util::RTData::kCommDisconnected;
    }

    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {

        if (strcmp((*it)->conf.get_param2.c_str(),"do") == 0)
        {
            int     index  = atoi((*it)->conf.get_param1.c_str());
            int     val = (do_status>>(index-1))&0x1;
            (*it)->value.pv			= util::IMVariant((1==val)?true:false);
        }
        else if (strcmp((*it)->conf.get_param2.c_str(),"di") == 0)
        {
            int     index  = atoi((*it)->conf.get_param1.c_str());
            int     val = (di_status>>(index-1))&0x1;
            (*it)->value.pv			= util::IMVariant((1==val)?true:false);
        }

        (*it)->value.quality	= util::RTData::kOk;
        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
    }
    return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
    if(0 == fd_)
    {
        return util::RTData::kCommDisconnected;
    }

    int cmd = IBOXGPIO_SET_OUTPUT_LOW;
    int val = util::IMVariant(tag_value_pv.c_str()).toInt();
    if(val > 0)
    {
        cmd = IBOXGPIO_SET_OUTPUT_HIGH;
    }
    int do_no = util::IMVariant(tag_conf.get_param1.c_str()).toInt() -1 ;

    if(true != ibox_dio.SetIO(cmd, do_no))
    {
        return util::RTData::kCmdRespError;
    }
    if(true != update_dofile(cmd, do_no))
    {
        return util::RTData::kCmdRespError;
    }
	return util::RTData::kOk;
}

bool Device::update_dofile(int cmd, int do_no)
{
    if(0 == fd_)
    {
        return util::RTData::kCommDisconnected;
    }
    if(IBOXGPIO_SET_OUTPUT_LOW == cmd)
    {
        do_status &= (~((!cmd)<<do_no));
    }
    else
    {
        do_status |= (cmd << do_no);
    }

    lseek(fd_, 0, SEEK_SET);
    int w = write(fd_, &do_status, sizeof(do_status));

    if(w < 0)
    {
        return false;
    }
    return true;
}
DRV_EXPORT const im_char*  getDrvVer()
{
	return "V.01";
}
DRV_EXPORT int initDrv(const im_char* dm_ver)
{
	return util::RTData::kOk;
}
DRV_EXPORT int uninitDrv()
{
	return util::RTData::kOk;
}
DRV_EXPORT IDevice* createDevice()
{
	return new Device;
}
DRV_EXPORT int releaseDevice(IDevice* device)
{
	delete device;
	return util::RTData::kOk;
}

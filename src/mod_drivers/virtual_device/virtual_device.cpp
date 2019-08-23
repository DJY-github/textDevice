#include "virtual_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "util_imvariant.h"
#include "util_datetime.h"
#include "util_datastruct.h"
#include "util_commonfun.h"

#include <iostream>
using namespace std;

#define RESP_BUFF_LEN 256

Device::Device(void)
{
    fd_ = 0;
    cycle = -1;
    status = -1;
    seconds = -1;
    last_second_ = -1;
}


Device::~Device(void)
{
}

int Device::open(const DeviceConf& device_info)
{
	fd_ = 1;

	return util::RTData::kOk;
}

bool Device::reopen()
{
    fd_ = 1;
    return true;
}
int Device::close()
{
    fd_ = -1;
	return util::RTData::kOk;
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    //printf("--------%s\n", cmd_feature.auxiliary.c_str());
    if(1 != fd_)
	{
        if(!reopen())
        {
            return util::RTData::kCommDisconnected;
        }
	}

    time_t now ;
    struct tm *tm_now ;
    time(&now);
    tm_now = localtime(&now);
    seconds = tm_now->tm_sec;
    minutes = tm_now->tm_min;

    // get函数被调用周期小于1秒，这里确保数据1秒变一次
    if(seconds == last_second_)
    {
        return util::RTData::kNeedWait;
    }
    last_second_ = seconds;

    status = seconds % 2;
    status_min = minutes % 2;
    cycle %= 1000000;
    cycle++;
    analog = util::IMDateTime::currentDateTime().toUTC();

//    printf("---Device::get time:%lld", analog);

    //A:模拟量 analog
    //D:开关量 status
    //E:枚举量 seconds
    //I:递增量 cycle
    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {
        if(strcmp((*it)->conf.get_param1.c_str(),"A") == 0)
        {
            (*it)->value.pv			= util::IMVariant(analog);
        }
        else if(strcmp((*it)->conf.get_param1.c_str(),"D") == 0)
        {
            (*it)->value.pv			= util::IMVariant((bool)status);
        }
        else if(strcmp((*it)->conf.get_param1.c_str(),"E") == 0)
        {
            (*it)->value.pv			= util::IMVariant(seconds);
        }
        else if(strcmp((*it)->conf.get_param1.c_str(),"I") == 0)
        {
            (*it)->value.pv			= util::IMVariant(cycle);
        }
        else if(strcmp((*it)->conf.get_param1.c_str(),"M") == 0)
        {
            (*it)->value.pv			= util::IMVariant(minutes);
        }
        else if(strcmp((*it)->conf.get_param1.c_str(),"DM") == 0)
        {
            (*it)->value.pv			= util::IMVariant((bool)status_min);
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

	return util::RTData::kOk;
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

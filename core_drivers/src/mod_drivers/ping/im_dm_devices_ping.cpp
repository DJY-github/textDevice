#include "im_dm_devices_ping.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <getopt.h>
#include <linux/i2c-dev.h>
#include <dirent.h>

#include "util_imvariant.h"
#include "util_datetime.h"
#include "util_datastruct.h"

#include <iostream>

using namespace std;

//获取ping信息保存到buffer
void Device::get_comm_to_buff(string &cmd, char* buffer)
{
    FILE* pf;
    pf =  popen(cmd.c_str(),"r");
    memset(buffer,'0',100);\
    fgets(buffer, sizeof(buffer)-1 , pf);
    pclose(pf);
}

//将char类型的转换成string类型的
void Device::buff_to_string(char *buffer, string &str_buff)
{
    stringstream stream;
    stream.str("");
    stream << buffer;
    str_buff = stream.str();
}

//string 转 float
void Device::string_to_float(string &str,float &ftime)
{
    istringstream istream(str);
    istream >> ftime;
}

//判断数据是否在60秒内
void Device::within60(list_date &date)
{
    int value;
    while(1){
        value = ( *(date.rbegin()) ).timestamp - ( *(date.begin()) ).timestamp;
        if (value > 60)
        {
            date.pop_front();
        }else{
            break;
        }
    }
}

float Device::handle_pack_lost(list_date &date)
{
    int fail_times = 0;
    float value;
    list_date::iterator it;
    for (it = date.begin(); it != date.end(); it++)
    {
        if ( (*it).net_status > 0)
        {
            ++fail_times;
        }
    }

    value = fail_times / (float)date.size();
    return value;
}

float Device::handle_delay(list_date &date)
{
    int succ_times = 0;
    float value;
    float sum_times = 0.0f;
    list_date::iterator it;
    for (it = date.begin(); it != date.end(); it++)
    {
        if ( (*it).net_status == 0 )
        {
            ++succ_times;
        }
        sum_times += (*it).delay_time;
    }

    value = sum_times / succ_times;

    if (isnan(value))
    {
        value = 0.0;
    }

    return value;
}

Device::Device(void)
{

}

Device::~Device(void)
{
}

int Device::open(const DeviceConf& device_info)
{
    int pos = device_info.channel_identity.find(":");
    ip_ = device_info.channel_identity.substr(0, pos);
	return util::RTData::kOk;
}

int Device::close()
{
	return util::RTData::kOk;
}


int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    char           buffer[100];
    string         cmd;
    string         pfbuff;

    string         stime;
    int            fpos=0;
    ip_date         date;

    cmd = "ping -c 1 " + ip_ + " | grep seq";

    get_comm_to_buff(cmd, buffer);

    buff_to_string(buffer, pfbuff);

    fpos = pfbuff.find("time=");
    if ( fpos<0 )
    {
        stime = "0.0";
        date.net_status = 1;
    }
    else
    {
        stime = pfbuff.substr(fpos+5, 5);
        date.net_status = 0;
    }

    string_to_float(stime, date.delay_time);
    date.timestamp = util::IMDateTime::currentDateTime().toUTC();

    list_ip_status_.push_back(date);

    within60(list_ip_status_);
    pack_lost_ = handle_pack_lost(list_ip_status_) * 100.0;
    ave_delay_ = handle_delay(list_ip_status_);

    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it )
    {
        if(strcmp((*it)->conf.get_param1.c_str(),"net_ste") == 0)
        {
           (*it)->value.pv			= util::IMVariant( date.net_status );
           (*it)->value.quality	    = util::RTData::kOk;
        }
        else if(strcmp((*it)->conf.get_param1.c_str(),"ave_delay") == 0)
        {
           (*it)->value.pv			= util::IMVariant(ave_delay_);
           (*it)->value.quality	    = util::RTData::kOk;
        }
        else if(strcmp((*it)->conf.get_param1.c_str(),"pack_lost") == 0)
        {
           (*it)->value.pv			= util::IMVariant(pack_lost_);
           (*it)->value.quality	    = util::RTData::kOk;
        }
        else if(strcmp((*it)->conf.get_param1.c_str(),"ip") == 0)
        {
           (*it)->value.pv			= util::IMVariant(ip_.c_str());
           (*it)->value.quality	    = util::RTData::kOk;
        }
        (*it)->value.timestamp	    = util::IMDateTime::currentDateTime().toUTC();
    }
    return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{

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

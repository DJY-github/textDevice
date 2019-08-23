#include "im_dm_devices_ct_ip2000.h"

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
#include "util_commonfun.h"

#include <iostream>
using namespace std;

static struct Device::Arm_attr g_arm_attr;
// key arm_num
static std::map<int, Device::Arm_attr> g_map_arm_attr;
static int     g_normal_status_cnt;

Device::Device(void)
{
    fd_ = 0;
    subsys_ = 0;
    arm_num_ = 0;
    recv_len_ = 0;
   // normal_status_cnt = 0;
    memset(recv_buff_,0,sizeof(recv_buff_));
}

Device::~Device(void)
{
}

int Device::open(const DeviceConf& device_info)
{
    printf("open udp\n");
	serial = PortUDP::getSerial();

	if(!serial->isOpen())
	{
		if(!serial->open(device_info.channel_identity.c_str(),im_string("udp").c_str() ))
		{
			return util::RTData::kCommDisconnected;
		}
	}
    printf("open udp ok\n");

    init_all_arm_zone();
	return util::RTData::kOk;
}

int Device::close()
{
	if(nullptr != serial)
	{
		serial->close();
	}

	return util::RTData::kOk;
}

// init all arm zone to normal
void Device::init_all_arm_zone()
{

}

bool Device::parase_recv_len()
{
    if(0 >= recv_len_)
    {
        printf("recv_len <= 0,recv_len=%d\n",recv_len_);
        return false;
    }

    if (recv_len_ > 10 || recv_len_ != 5)
    {
        printf("filter data,len:%d\n",recv_len_);
        for(int i=0;i<recv_len_;i++)
        {
            printf("0x%02x ",recv_buff_[i]);
        }
        printf("\n");
        return false;
    }
    return true;
}

bool Device::parase_recv_status()
{
    if(recv_buff_[0] != 0xF7 ||  recv_buff_[0] != 0xFC)
    {
        return false;
    }
    return true;
}

void Device::parase_subsys()
{
    subsys_ = recv_buff_[1];
}

bool Device::parase_arm_map()
{
    arm_num_ = (recv_buff_[2]>>4)*10 + (recv_buff_[2]&0x0F);
    if(arm_num_ >= 100)
    {
        return false;
    }

    std::map<int, Arm_attr>::iterator it = g_map_arm_attr.find(arm_num_);
    if (it != g_map_arm_attr.end())
    {
        it->second.ae_status = g_arm_attr.ae_status;
        it->second.ae_bypass_status = g_arm_attr.ae_bypass_status;
        it->second.arm_loss = g_arm_attr.arm_loss;
    }
    else
    {
        g_map_arm_attr.insert(std::make_pair(arm_num_,g_arm_attr));
    }
    return true;
}

void Device::parase_ae_status()
{
    bool last_is_ae = is_ae_;
    is_ae_ = true;

    if(((recv_buff_[3]&1) == 1 )||(((recv_buff_[3]>>5) &1) == 1))
    {
        g_arm_attr.ae_status = 1;
    }
    else if (((recv_buff_[3]>>3)&1) == 1)
    {
        g_arm_attr.ae_bypass_status = 1;
    }
    else if ((((recv_buff_[3]>>1) &1) == 0) && (((recv_buff_[3]>>1) &1) == 0) && (((recv_buff_[3]>>1) &1) == 0))
    {
        g_arm_attr.arm_loss = 1;
    }
    else
    {
        g_arm_attr.ae_status = 0;
        g_arm_attr.ae_bypass_status = 0;
        g_arm_attr.arm_loss = 0;
        is_ae_ = false;
    }

    // if this resp is ae,clean the all caculate status
    if (is_ae_)
    {
        g_normal_status_cnt = 0;
    }
    else
    {
        if(!last_is_ae)
        {
            g_normal_status_cnt++;
            if(g_normal_status_cnt > 10)
            {
                g_normal_status_cnt = 10;
            }
        }

    }
}

void Device::try_recovery_normal()
{
    if(g_normal_status_cnt > 2)
    {
        printf("set all normal\n");
        std::map<int, Arm_attr>::iterator it;
        for(it=g_map_arm_attr.begin();it != g_map_arm_attr.end(); ++it)
        {
            it->second.ae_status = 0;
            it->second.ae_bypass_status = 0;
            it->second.arm_loss = 0;
        }
    }
}


int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			return util::RTData::kCommDisconnected;
		}
	}

    memset(recv_buff_,0,sizeof(recv_buff_));
    recv_len_ = serial->read(recv_buff_, MAX_BUFF_LEN, 1000);
    printf("recv[0]= %x",recv_buff_[0]);
    if (!parase_recv_len())
    {
        return util::RTData::kOk;
    }
    for(int i=0;i<recv_len_;i++)
    {
        printf("0x%02x ",recv_buff_[i]);
    }
    printf("\n");

    if (!parase_recv_status())
    {
        printf("recv first status wrong\n");
        return util::RTData::kCmdRespError;
    }

    parase_ae_status();

    if (!parase_arm_map())
    {
        printf("parase arm number err:0x%02x to %d\n", recv_buff_[2], arm_num_);
        return util::RTData::kCmdRespError;
    }

    try_recovery_normal();

    Tags::iterator it;
    std::map<int, Arm_attr>::iterator arm_it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {
        int  subsys  = atoi((*it)->conf.get_param1.c_str());
        int  arm_num = atoi((*it)->conf.get_param2.c_str());
        im_string desc = (*it)->conf.get_param3;
        Arm_attr one_attr;

        if(subsys<0 || ((subsys_>>(subsys-1)&1) == 0))
        {
            (*it)->value.quality	= util::RTData::kConfigError;
            (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
            continue;
        }

        arm_it = g_map_arm_attr.find(arm_num);
        if(arm_it == g_map_arm_attr.end())
        {
            one_attr.arm_num = arm_num;
            one_attr.ae_status = 0;
            one_attr.ae_bypass_status = 0;
            one_attr.arm_loss = 0;
            g_map_arm_attr.insert(std::make_pair(arm_num,one_attr));
        }

        arm_it = g_map_arm_attr.find(arm_num);
        Arm_attr value;
        value = arm_it->second;
        if(desc == "ae")
        {
            (*it)->value.pv	= util::IMVariant(arm_it->second.ae_status);
        }
        else if (desc == "by")
        {
            (*it)->value.pv	= util::IMVariant(arm_it->second.ae_bypass_status);
        }
        else if (desc == "loss")
        {
            (*it)->value.pv	= util::IMVariant(arm_it->second.arm_loss);
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

#include "im_dm_devices_ups_santak.h"

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

#define RESP_BUFF_SIZE 256

Device::Device(void)
{
    fd_ = 0;
    end_str_ = 0x0D;
    check_start_ = true;
    start_str_ = '(';
}

Device::~Device(void)
{
}

int Device::open(const DeviceConf& device_info)
{
	serial = IMSerialPort::getSerial(device_info.channel_identity.c_str());

	if(!serial->isOpen())
	{
		if(!serial->open(device_info.channel_identity.c_str(), device_info.channel_params.c_str()))
		{
			return util::RTData::kCommDisconnected;
		}
	}
    im_string start_str;
    start_str = util::strupr(device_info.ver);
    if(im_string::npos != start_str.find("NO_CHECK"))
    {
        check_start_ = false;
        printf("no_check\n");
        return util::RTData::kOk;
    }

    if (start_str.length()!=0)
    {
        start_str_ = start_str[0];
    }
    else
    {
        start_str_ = '(';
    }
    //printf("start-str:%c\n",start_str_);
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

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			return util::RTData::kCommDisconnected;
		}
	}

	util::IMBuffer<16> cmd;
	cmd.append(cmd_feature.cmd.c_str(), cmd_feature.cmd.size());
	*cmd.append_ptr(1) = end_str_;

    char resp_buff[RESP_BUFF_SIZE]={0};
    int cycle = 0;
    while(1)
    {
        if(3 == cycle)
        {
            close();
            return util::RTData::kCommDisconnected;
        }
        char temp[3]= {0};
        // 收取可能出现的异常数据
        int try_recv_abnormal_cnt = 100;
        while(serial->read(temp,1, 300) > 0)
        {
            if(try_recv_abnormal_cnt-- <= 0)
            {
                serial->close();
                break;
            }
        }

        serial->write((char *)cmd.ptr(), cmd.size());
       // int len = cmd.size();
        //printf("sennd: ");
       // for (int k=0;k<len;k++)
       // {
        //   printf("%02x ",cmd[k]);
       // }
        //printf("\n");



        serial->read(resp_buff, RESP_BUFF_SIZE, 2000);
        if (strlen(resp_buff) < 3)
        {
            cycle++;
            continue;
        }
        else if (strlen(resp_buff) >= 3)
        {
            break;
        }
        else
        {
            return util::RTData::kCommDisconnected;
        }
    }
   // printf("resp:%s\n",resp_buff);
    if(strncmp((resp_buff), "(NAK", 4) == 0)
    {
        return util::RTData::kConfigError;
    }

    char convert_buff[RESP_BUFF_SIZE] = {0};
    if (check_start_)
    {
        if ( start_str_ == resp_buff[0])
        {
            memcpy(convert_buff, &resp_buff[1], strlen(resp_buff)-2);
        }
        else
        {
            return util::RTData::kCmdRespError;
        }
    }
    else
    {
        memcpy(convert_buff, &resp_buff[1], strlen(resp_buff)-2);
    }



    std::vector<im_string> split_buff = util::spliteString(convert_buff, ' ');
    //Computer： Q1<CR>
	//UPS：  (MMM.M NNN.N PPP.P QQQ RR.R S.SS TT.T b7b6b5b4b3b2b1b0<CR>
    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {
        int     index_param1  = atoi((*it)->conf.get_param1.c_str());

        if((unsigned int)index_param1 >= split_buff.size())
        {
            return util::RTData::kConfigError;
        }

        if (strcmp((*it)->conf.get_param2.c_str(),"") == 0)
        {
            float     val = atof(split_buff[index_param1].c_str());
            (*it)->value.pv			= util::IMVariant(val);
        }
        else
        {
            int index_param2  = atoi((*it)->conf.get_param2.c_str());
            //状态量
            if(TagValue::kDigit == (*it)->conf.data_type)
            {
                int len = strlen(split_buff[index_param1].c_str());
                if(index_param2 > len)
                {
                    return util::RTData::kConfigError;
                }
                if (strcmp((*it)->conf.get_param3.c_str(),"") == 0)
                {
                    if('0' == split_buff[index_param1][index_param2])
                    {
                        (*it)->value.pv			= util::IMVariant(0);
                    }
                    else
                    {
                        (*it)->value.pv			= util::IMVariant(1);
                    }
                }
                else if(strcmp((*it)->conf.get_param3.c_str(),"r") == 0)
                {
                    if('0' == split_buff[index_param1][len-index_param2-1])
                    {
                        (*it)->value.pv			= util::IMVariant(0);
                    }
                    else
                    {
                        (*it)->value.pv			= util::IMVariant(1);
                    }
                }
            }
            else if (TagValue::kAnalog == (*it)->conf.data_type)//模拟量
            {
                std::vector<im_string> split_analog_buff = util::spliteString(split_buff[index_param1].c_str(), '/');
                // 若访问的游标超过了大小 报错
                if(index_param2 > split_analog_buff.size()-1)
                {
                    return util::RTData::kConfigError;
                }
                float     val = atof(split_analog_buff[index_param2].c_str());
                (*it)->value.pv			= util::IMVariant(val);
            }
            else if (TagValue::kEnumStatus == (*it)->conf.data_type)
            {
                if(std::string::npos != (*it)->conf.get_param2.find("-"))  // 取2字节	第x位到第y位的值，如0-2
                {
                    int pos = (*it)->conf.get_param2.find("-");
                    int b1 = atoi((*it)->conf.get_param2.substr(0, pos).c_str());
                    int b2 = atoi((*it)->conf.get_param2.substr(pos+1).c_str());

                    if (b1 <0 || b2>15)
                    {
                        return util::RTData::kConfigError;
                    }
                    im_string str;
                   // printf("all b:%s\n",split_buff[index_param1].c_str());
                    //printf("b1-b2:%s\n",split_buff[index_param1].substr(b1, b2).c_str());
                    int val = atoi(split_buff[index_param1].substr(b1, b2).c_str());
                    (*it)->value.pv = util::IMVariant(val);
                }
                else if(std::string::npos != (*it)->conf.get_param2.find("str_int"))
                {
                    if (0 == strcmp("OK",split_buff[index_param1].c_str()))
                    {
                        int val = 0;
                        (*it)->value.pv = util::IMVariant(val);
                    }
                    else
                    {
                        int val = atoi((const char *)split_buff[index_param1].c_str());
                        (*it)->value.pv = util::IMVariant(val);
                    }
                }
                else if(std::string::npos != (*it)->conf.get_param2.find("str_one"))
                {
                    if(1 == split_buff[index_param1].size())
                    {
                        int val = split_buff[index_param1][0];
                        (*it)->value.pv = util::IMVariant(val);
                    }
                }
                else // 没有填写任何的参数，表示取到字符串.
                {
                   // if(1 == split_buff[index_param1].size())
                    {
                        int val = atoi((const char *)split_buff[index_param1].c_str());
                        (*it)->value.pv = util::IMVariant(val);
                    }
                }
            }
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

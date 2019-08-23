#include "im_dm_devices_dac_zchw.h"
#include "util_imvariant.h"
#include "util_imbufer.h"
#include "util_datetime.h"
#include "util_commonfun.h"

#include <unistd.h>
#include <math.h>

#define RESP_BUFF_SIZE 256

Device::Device(void)
{
	serial      = nullptr;
}


Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{
    printf("1111");
	serial = IMSerialPort::getSerial(device_info.channel_identity.c_str());

	if(!serial->isOpen())
	{
		if(!serial->open(device_info.channel_identity.c_str(), device_info.channel_params.c_str()))
		{
			return util::RTData::kCommDisconnected;
		}
	}
    printf("2222");
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

void Device::flush(unsigned int timeout_ms)
{
       // 收取可能出现的异常数据
       char temp[2] = {0};
        int try_recv_abnormal_cnt = 256;
        while(serial->read(temp,1, timeout_ms) > 0)
        {
            if(try_recv_abnormal_cnt-- <= 0)
            {
                serial->close();
                break;
            }
        }
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    printf("3333\n");
	if(!serial->isOpen())
	{
	    printf("4444");
		if(!serial->reOpen())
		{
		    printf("55555");
			return util::RTData::kCommDisconnected;
		}
	}

 	util::IMBuffer<32> cmd;
	if(!cmd.fromHex(cmd_feature.cmd.c_str()))
	{
		return util::RTData::kConfigError;
	}
	printf("666666\n");
	printf("cmd len:%d\n",cmd.size());
	for (int i=0;i<cmd.size();i++)
    {
        //printf("cmd_size:%d\n",cmd.size());
        printf("pos:%d content:%02x\n",i,*(cmd.ptr()+i));
    }
    printf("\n");

	ZchwPacketSend packet_send;
	//      01 00 00 00 02 00 16
	// cmd   0  1  2  3  4  5  6
	char szcmd[1024]={0};
	memcpy(szcmd, cmd.ptr(),cmd.size());

    printf("cmd_size:%d,cmd_data_send_len:%d\n",cmd.size(),(szcmd[3]<<8)+szcmd[4]);
    int cmd_size = cmd.size();
	if(((cmd[3]<<8)+cmd[4]) > 2 && cmd.size()>7)
    {
        printf("---777\n");
        packet_send.buildPacket(atoi(cmd_feature.addr.c_str()), cmd[0],cmd[1],cmd[2],(cmd[3]<<8) + cmd[4],&cmd[5],&cmd[6],(cmd[3]<<8)+cmd[4]-2);
    }
    else if(cmd_size == 7)
    {
        printf("77----\n");
         if ( 2 == ((cmd[3]<<8) + cmd[4]))
        //else if((2 == (*(cmd.ptr()+3) << 8 ) + *(cmd.ptr()+3) && cmd.size() == 7 )
    {
        printf("777777-----\n");
        packet_send.buildPacket(atoi(cmd_feature.addr.c_str()), cmd[0],cmd[1],cmd[2],(cmd[3]<<8) + cmd[4],&cmd[5],NULL,0);
    }
    }

    printf("packet_send len:%d",packet_send.sendByte());
    for (int i=0;i<packet_send.sendByte();i++)
    {
        printf("%02x ",packet_send.send_buf_[i]);
    }
    printf("\n");
	serial->write((char*)packet_send.sendData(), packet_send.sendByte());

	char resp_buff[RESP_BUFF_SIZE]={0};
	int read_len = serial->read(resp_buff, RESP_BUFF_SIZE, 2000);
	if(0 >= read_len)
	{
		return util::RTData::kCmdNoResp;
	}
    printf("read len:%d\n",read_len);
    for(int i=0;i<read_len;i++)
    {
        printf("%02x ",resp_buff[i]);
    }
    printf("\n");
    if(read_len < 2)
    {
        return  util::RTData::kCmdNoResp;
    }

    // 比较signal 字段的2个byte是否相同
    if (strncmp((const char*)resp_buff, (const char*)&cmd[5],2) != 0)
    {
         return util::RTData::kCmdRespError;
    }

    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {
        im_string param1 = (*it)->conf.get_param1;
        im_string param2 = (*it)->conf.get_param2;
        int len1 = atoi(param1.c_str());
        // 2 = strlen(signal)
        if(read_len != len1 + 2)
        {
            (*it)->value.quality	= util::RTData::kCmdRespError;
            (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
            continue;
        }
        int val = resp_buff[atoi(param2.c_str())];

        (*it)->value.pv			= util::IMVariant(val);
        (*it)->value.quality	= util::RTData::kOk;
        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();

    }
	return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
	if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			return util::RTData::kCommDisconnected;
		}
	}
	util::IMBuffer<16> cmd;
	if(!cmd.fromHex(cmd_feature.cmd.c_str()))
	{
		return util::RTData::kConfigError;
	}

    /*
	unsigned short  data_addr  = 0;
    unsigned char val[256] = {0};
    int  val_bytes = 0;
    int  func       = cmd[0];
    if(tag_conf.set_param1.empty() && cmd_feature.cmd.size() >= 6)  // 使用cmd里面的寄存器地址
    {
        memcpy((unsigned char*)&data_addr, &cmd[1], 2);
    }
    else
    {
        data_addr = atoi(tag_conf.set_param1.c_str());
    }
    */

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

#include "im_dm_devices_dianzong.h"
#include "util_imvariant.h"
#include "util_imbufer.h"
#include "util_datetime.h"
#include "util_commonfun.h"
#include "ae_constant.h"
#include "ae_creater.h"
//#include <windows.h>

#include <stdlib.h>
#include <unistd.h>

Device::Device(void)
{
	serial = nullptr;
}

Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{

	//log_.setLogFile( "dev_" + device_info.id + ".log");

	serial = IMSerialPort::getSerial(device_info.channel_identity.c_str());
	if(!serial->isOpen())
	{
		if(!serial->open(device_info.channel_identity.c_str(), device_info.channel_params.c_str()))
		{
			printf("Open %s failed..(func:open)", device_info.channel_identity.c_str());
			return util::RTData::kCommDisconnected;
		}
	}

	dev_id_ = device_info.id;

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
		printf("Start reopen COMM ..(func:get)");
		if(!serial->reOpen())
		{
			printf("Reopen COMM failed..(func:get)");
			return util::RTData::kCommDisconnected;
		}
	}

	int re = sendDev(cmd_feature);
	if(util::RTData::kOk != re)
	{
		printf("sendDev failed..(func:get)");
		serial->close();
		return re;
	}

	util::IMBuffer<2048> recv_buf;
	re = recvDev(recv_buf);
	if(util::RTData::kOk != re)
	{
		printf("recvDev failed..(func:get)");
		serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // 清空无效的串口缓存数据
		serial->close();
		return re;
	}

	Tags::iterator it;
	for(it = tags.begin(); it != tags.end(); ++it)
	{
		if(TagValue::kDigit == (*it)->conf.data_type)
		{
			bool val = false;
			if(!parseDigitVal(recv_buf, atoi((*it)->conf.get_param1.c_str()),(*it)->conf.get_param2, val))
			{
				(*it)->value.quality	= util::RTData::kConfigError;
				(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
				continue;
			}

			(*it)->value.pv			= util::IMVariant(val);
			(*it)->value.quality	= util::RTData::kOk;
			(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
		}
		else if(TagValue::kAnalog == (*it)->conf.data_type)
		{
			if("f" == (*it)->conf.get_param3)
			{
				float val = 0;
				if(!parseFloatVal(recv_buf, atoi((*it)->conf.get_param1.c_str()), atoi((*it)->conf.get_param2.c_str()), val))
				{
					(*it)->value.quality	= util::RTData::kConfigError;
					(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
					continue;
				}

				(*it)->value.pv			= util::IMVariant(val);

			}
			else
			{
				if("2" == (*it)->conf.get_param2)
				{
					unsigned char val = 0;
					if(!parseByteVal(recv_buf, atoi((*it)->conf.get_param1.c_str()), atoi((*it)->conf.get_param2.c_str()), val))
					{
						(*it)->value.quality	= util::RTData::kConfigError;
						(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
						continue;
					}

					(*it)->value.pv			= util::IMVariant((unsigned short)val);
				}
				else if("4" == (*it)->conf.get_param2)
				{
					unsigned short val = 0;
					if(!parseShortVal(recv_buf, atoi((*it)->conf.get_param1.c_str()), atoi((*it)->conf.get_param2.c_str()), val))
					{
						(*it)->value.quality	= util::RTData::kConfigError;
						(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
						continue;
					}

					(*it)->value.pv			= util::IMVariant(val);

				}
				else if("8" == (*it)->conf.get_param2)
				{
					int val = 0;
					if(!parseIntVal(recv_buf, atoi((*it)->conf.get_param1.c_str()), atoi((*it)->conf.get_param2.c_str()), val))
					{
						(*it)->value.quality	= util::RTData::kConfigError;
						(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
						continue;
					}

					(*it)->value.pv			= util::IMVariant(val);

				}
			}

			(*it)->value.quality	= util::RTData::kOk;
			(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();



		}
		else if(TagValue::kEnumStatus == (*it)->conf.data_type)
		{
			unsigned char val = 0;
			if(!parseEnumVal(recv_buf, atoi((*it)->conf.get_param1.c_str()),(*it)->conf.get_param2, val))
			{
				(*it)->value.quality	= util::RTData::kConfigError;
				(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
				continue;
			}

			(*it)->value.pv			= util::IMVariant((unsigned short)val);
			(*it)->value.quality	= util::RTData::kOk;
			(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
		}
	}

	return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
	if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			printf("Reopen COMM failed..(func:set)");
			return -1;
		}
	}

	if(TagValue::kDigit == tag_conf.data_type)
	{
		im_string key = "K" + tag_value_pv + "-";
		int pos = tag_conf.set_param1.find(key);
		if(-1 != pos)
		{
			int dataset_len = atoi(tag_conf.set_param2.c_str());
			if(dataset_len < 0 || dataset_len + pos >= (int)tag_conf.set_param1.size())
			{
				return util::RTData::kConfigError;
			}
			im_string data_set = tag_conf.set_param1.substr(pos+3, dataset_len);
			if(sendDev(cmd_feature, data_set.c_str()))
			{
				util::IMBuffer<2048> recv_buf;
				if(recvDev(recv_buf))
				{
					serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // 清空无效的串口缓存数据

					printf("recvDev failed..(recvDev:%s, func:set)",recv_buf.ptr(0));
					return util::RTData::kCmdRespError;
				}

				return util::RTData::kOk;
			}
		}
		else
        {
            int re = sendDev(cmd_feature);
            if(util::RTData::kOk != re)
            {
                printf("sendDev failed..(func:set)");
                serial->close();
                return util::RTData::kCmdRespError;
            }
            util::IMBuffer<2048> recv_buf;
            if(recvDev(recv_buf))
            {
                serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // 清空无效的串口缓存数据
                printf("recvDev failed..(recvDev:%s, func:set)",recv_buf.ptr(0));
                return util::RTData::kCmdRespError;
            }

            // 解析BTN
            util::IMBuffer<1> btn;
            btn.fromHex(recv_buf.ptr(7), 2);
            unsigned short BTN = btn[0];
            if (BTN != 0)
            {
                printf("Set failed..(BTN:%d)",BTN);
                return util::RTData::kCmdRespError;
            }
            return util::RTData::kOk;

        }
	}
	else if(TagValue::kAnalog == tag_conf.data_type)
	{

	}

	return util::RTData::kConfigError;
}



unsigned short makeDataLen( int nLen )
{
	unsigned short wSum = (-(nLen + (nLen>>4) + (nLen>>8) )) << 12;

	return (unsigned short)( (nLen&0x0FFF) | wSum );
}

/*
unsigned short makeDataLen( int nLen )
{
	unsigned short wSum = (nLen&0x0000000F) + ((nLen>>4)&0x0000000F) + ((nLen>>8)&0x0000000F) ;

	wSum = wSum%16;
	wSum = ~wSum + 1;

	return wSum;
}*/

unsigned short datalen2LenID(int lenth)
{
	return (lenth&0x0FFF);
}

unsigned short checkSum(const char *frame , const int len)
{
	unsigned short cs = 0;
	for(int i=0; i<len; i++)        // 计算累加和，不包含头字符
	{
		cs += *(frame + i);
	}

	return -cs;
}

int Device::sendDev(const CmdFeature& cmd_feature, const char* data_info)
{
	usleep(500000);
	int pos = 0;
	char ver_addr_cid[9] = {0};
	memcpy(ver_addr_cid, cmd_feature.cmd.c_str(), 8);
	char real_addr[3] = {0};
	im_sprintf(real_addr,3, "%02X", atoi(cmd_feature.addr.c_str()));
	ver_addr_cid[2]	= real_addr[0];
	ver_addr_cid[3]	= real_addr[1];

	char send_buf[33] = {0};
	if(0 == data_info)
	{
		int data_info_pos = cmd_feature.cmd.find("_");
		if(std::string::npos == (unsigned int)data_info_pos)
		{
		   // printf("addr:%s\ncmd_feature:%s,%s\n",cmd_feature.addr.c_str(),cmd_feature.cmd.c_str(),ver_addr_cid);

			unsigned short lenid = makeDataLen(0);
			printf("lenid:%d\n",lenid);
			im_sprintf(send_buf, 14, "~%s%04X\0",ver_addr_cid, lenid);
			pos += 13;   // 1字节头 + 8字节ver_addr_cid + 4字节lenth
		}
		else
		{
			im_string strinfo =  cmd_feature.cmd.substr(data_info_pos+1);
			unsigned short info_len = strinfo.size();
			unsigned short lenid = makeDataLen(info_len);
			im_sprintf(send_buf, 14+info_len, "~%s%04X%s\0",ver_addr_cid, lenid, strinfo.c_str());
			pos += 13 + info_len;
		}

	}
	else
	{
		unsigned short info_len = strlen(data_info);
		unsigned short lenid = makeDataLen(info_len);
		im_sprintf(send_buf, 14+info_len, "~%s%04X%s\0",ver_addr_cid, lenid, data_info);
		pos += 13 + info_len;
	}
	unsigned short cs = checkSum(&send_buf[1], pos -1);
	char cs_tail[5] = {0};
 	im_sprintf(cs_tail, 5,"%04X",cs);
	memcpy(send_buf+pos, cs_tail, 4);
	pos += 4;
	send_buf[pos++] = 0x0D;

	if(pos != serial->write(send_buf, pos))
	{
		printf("Send COMM Data failed(%s)", send_buf);
		return util::RTData::kCmdNoResp;
	}

	return util::RTData::kOk;
}


int Device::recvDev(util::IMBuffer<2048>& recv_buf)
{
	usleep(100000);
	// 1.接收协议头，13个字节
	int head_len = 13;
	int read_len = serial->read(recv_buf.ptr(0, head_len), head_len, 10000);
	if(0 == read_len)
	{
		printf("Command no respond.");
		return util::RTData::kCmdNoResp;
	}
	else if(read_len != head_len)
	{
		printf("Recv HeadLen Failed (head_len:%d, read_len:%d)", head_len, read_len);
		return util::RTData::kCmdRespError;
	}

	// 2.解析LENID
	util::IMBuffer<2> str_lenth;
	str_lenth.fromHex(recv_buf.ptr(head_len-4), 4);
	unsigned short lenid = (str_lenth[0] << 8) + str_lenth[1];
	lenid = datalen2LenID(lenid);

	usleep(100000);
	// 3.接收数据
	int protocol_len = lenid + 5; // 4字节校验位 + 1字节结束符
	read_len = serial->read(recv_buf.ptr(head_len, protocol_len), protocol_len, 10000);
	if(read_len != protocol_len)
	{
		printf("Recv Body Failed (body_len:%d, read_len:%d)", protocol_len, read_len);
		return util::RTData::kCmdRespError;
	}

	// 4.校验验证
	unsigned short cs = checkSum(recv_buf.ptr(1), protocol_len + head_len -2 -4);
	util::IMBuffer<2> recv_str_cs;
	recv_str_cs.fromHex(recv_buf.ptr(recv_buf.size()-5), 4);
	unsigned short recv_cs = (recv_str_cs[0] << 8) + recv_str_cs[1];
	if(cs != recv_cs)
	{
		printf("CheckSum Failed (recv_buf:%s)", recv_buf.ptr(0));
		return util::RTData::kCmdRespError;
	}

	return util::RTData::kOk;

}

bool Device::parseByteVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, unsigned char& val)
{
	int start_base = 13;
	if(pos + val_len + start_base > recv_buf.size() || val_len != 2 )
	{
		return false;
	}

	util::IMBuffer<1> data;
	data.fromHex(recv_buf.ptr(start_base+pos), val_len);
	val = data[0];

	return true;
}
bool Device::parseShortVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, unsigned short& val)
{
	int start_base = 13;
	if(pos + val_len + start_base > recv_buf.size() || val_len != 4 )
	{
		return false;
	}

	util::IMBuffer<2> data;
	data.fromHex(recv_buf.ptr(start_base+pos), val_len);
	val = (data[0] << 8) + data[1];

	return true;
}
bool Device::parseIntVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, int& val)
{
	int start_base = 13;
	if(pos + val_len + start_base > recv_buf.size() || val_len != 8 )
	{
		return false;
	}

	util::IMBuffer<4> data;
	data.fromHex(recv_buf.ptr(start_base+pos), val_len);
	val = (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3];

	return true;
}
bool Device::parseFloatVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, float& val)
{
	int start_base = 13;
	if(pos + val_len + start_base > recv_buf.size() || val_len !=8 )
	{
		return false;
	}

	util::IMBuffer<4> data;
	data.fromHex(recv_buf.ptr(start_base+pos), val_len);

	memcpy(&val, &data[0], 4);

	return true;
}


bool Device::parseDigitVal(const util::IMBuffer<2048>& recv_buf, int pos, const libistring& param2, bool& val)
{
	int start_base = 13;

	if(pos + start_base > recv_buf.size())
	{
		return false;
	}

	util::IMBuffer<1> data;
	data.fromHex(recv_buf.ptr(start_base+pos), 2);
	unsigned char hex =  data[0];

	int flag = param2.find(".");
	if(-1 != flag)		// 取位
	{
		int bit = atoi(param2.substr(flag+1).c_str());
		val	= (hex >> bit) & 1;

		return true;
	}

	flag = param2.find("?");
	if(-1 != flag) // 取值是否相等
	{
		im_string condi = param2.substr(flag+1);
		if(-1 != condi.find(","))
		{
			val = false;
			std::vector<im_string> vec_val = util::spliteString(condi, ',');
			std::vector<im_string>::const_iterator it = vec_val.begin();
			for (it = vec_val.begin(); it != vec_val.end(); it++)
			{
				if(atoi((*it).c_str()) == hex)
				{
					val = true;
					break;
				}
			}

		}
		else
		{
			val = (atoi(condi.c_str()) == hex)?true:false;
		}

		return true;

	}


	//直接取值
	val = hex>0?true:false;
	return true;
}

 bool Device::parseEnumVal(const util::IMBuffer<2048>& recv_buf, int pos, const libistring& param2, unsigned char& val)
 {
	 int start_base = 13;

	 if(pos + start_base > recv_buf.size())
	 {
		 return false;
	 }

	 util::IMBuffer<1> data;
	 data.fromHex(recv_buf.ptr(start_base+pos), 2);
	 val =  data[0];

	 return true;
 }

bool Device::parseString(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, im_string& val)
{
    int start_base = 13;

    if(start_base + pos + val_len > recv_buf.size())
	{
		return false;
	}
	char buff[2048] = {0};
    for(int i=0;i<val_len/2;i++)
    {
        util::IMBuffer<1> data;
        data.fromHex(recv_buf.ptr(start_base+pos+i*2), 2);
        sprintf(buff+i*2,"%02x",data[0]);
    }
    val = buff;

    return true;
}

Time metis_strptime(const char *str_time)
{
	struct tm stm;
	strptime(str_time,"%Y%m%d%H%M%S",&stm);
	Time t= mktime(&stm);
	return t;
}

 bool Device::parseBCDTime(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, Time& val)
 {
    int start_base = 13;

    if(pos + val_len + start_base > recv_buf.size() || val_len !=7 )
	{
		return false;
	}
	im_string val_time;
	if (!parseString(recv_buf,pos,val_len,val_time))
    {
        return false;
    }
	val = metis_strptime(val_time.c_str());
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



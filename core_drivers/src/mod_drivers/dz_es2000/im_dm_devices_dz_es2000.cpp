#include "im_dm_devices_dz_es2000.h"
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

void Device::get_version_attr(const DeviceConf& device_info)
{
    //模板ver填写格式
    //pw:10AA8048_F0E000000
    int pos = 0;
    im_string ver = device_info.ver;

    if( -1 != (pos=ver.find("pw:")))
    {
        pw_cmd_ = ver.substr(pos+3);
        printf("dz_es2000 verify cmd:%s\n",pw_cmd_.c_str());
    }
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
	get_version_attr(device_info);

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
			else if ("rcd_ae" == (*it)->conf.get_param3) // 该解析 针对tstack项目的门禁记录事件 将记录事件转化成告警记录
            {
                // 检测数据长度的有效性
                int start_base = 13;
                int pos = atoi((*it)->conf.get_param1.c_str());
                int val_len = atoi((*it)->conf.get_param2.c_str());
                if(start_base + pos + val_len > recv_buf.size() || val_len !=28 )
                {
                    (*it)->value.quality	= util::RTData::kConfigError;
                    (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                    continue;
                }

                // 过滤已经读取的事件
                bool readed;
                parseDigitVal(recv_buf,12,"12.7",readed);
                if(readed)
                {
                    continue;
                }
                im_string event;

                // 解析日期 7byte
                Time tm;
                parseBCDTime(recv_buf,5,7,tm);

                // 提前解析id卡号
                im_string id;
                parseString(recv_buf,0,10,id);

                // 提前解析状态
                unsigned char status;
                parseByteVal(recv_buf,12,2,status);


                // 解析备注 1byte
                unsigned char remark;
                parseByteVal(recv_buf,26,2,remark);
                // 根据remark再次解析状态(1byte)和事件来源的(5byte)
                if(remark == 0x00)// 合法卡刷卡开门记录
                {

                    bool door;

                    parseDigitVal(recv_buf,12,"12.6",door);
                    if (door)
                    {
                        event = id+":"+"刷卡开门(门处于开状态)";
                    }
                    else
                    {
                        event = id+":"+"刷卡开门(门处于关状态)";
                    }
                }
                else if (remark == 0x02) //远程(由SU)开门记录
                {
                    bool door;
                    parseDigitVal(recv_buf,12,"12.6",door);
                    if (door)
                    {
                        event = "远程开门(门处于开状态)";
                    }
                    else
                    {
                        event = "远程开门(门处于关状态)";
                    }
                }
                else if (remark == 0x03)//手动开门记录
                {
                    bool door;
                    parseDigitVal(recv_buf,12,"12.6",door);
                    if (door)
                    {
                        event = "手动开门(门处于开状态)";
                    }
                    else
                    {
                        event = "手动开门(门处于关状态)";
                    }
                }
                else if (remark == 0x22)//紧急联动事件
                {
                    if(status != 0)
                    {
                        printf("门禁协议错误:remark=22!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    bool emerg_status;
                    parseDigitVal(recv_buf,4,"2",emerg_status);
                    if(emerg_status == 0)
                    {
                        event = "紧急联动事件开始";
                    }
                    else
                    {
                        event = "紧急联动事件结束";
                    }
                }
                else if (remark == 0x05)//报警 (或报警取消) 记录
                {
                    int flag;
                    parseIntVal(recv_buf,0,8,flag);
                    if(flag != 0)
                    {
                        printf("门禁协议错误:remark=5!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    unsigned char as;
                    im_string as_status;
                    parseByteVal(recv_buf,4,2,as);
                    if(as == 2)
                    {
                        as_status = "门开启";
                    }
                    else if (as == 3)
                    {
                        as_status = "门关闭";
                    }
                    else if (as == 6)
                    {
                        as_status = "门禁内部存储器错误,自动初始化";
                    }
                    else if (as == 9)
                    {
                        as_status = "门碰开关监测被关闭";
                    }
                    else if (as == 10)
                    {
                        as_status = "门碰开关监测开启";
                    }

                    im_string hand_status,door_status;
                    if((status >> 1) & 1)
                    {
                        hand_status = "按下";
                    }
                    else
                    {
                        hand_status = "松开";
                    }
                    if ((status >> 3) & 1)
                    {
                        door_status = "开";
                    }
                    else
                    {
                        door_status = "关";
                    }

                    event = as_status + ",按键状态(" + hand_status + "),门状态(" + door_status + ")";
                }
                else if (remark == 0x06)//ES2000掉电记录
                {
                    im_string str,power_down;
                    parseString(recv_buf,0,10,str);
                    if(str.length() != 10)
                    {
                        (*it)->value.quality	= util::RTData::kConfigError;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                    }
                    power_down = str.substr(0,2) + "月"+ str.substr(2,2) + "日" + str.substr(4,2) + "时" + str.substr(6,2) + "分" + str.substr(8,2) + "秒" + str.substr(4,2);
                    im_string hand_status,door_status;
                    if((status >> 1) & 1)
                    {
                        hand_status = "按下";
                    }
                    else
                    {
                        hand_status = "松开";
                    }
                    if ((status >> 3) & 1)
                    {
                        door_status = "开";
                    }
                    else
                    {
                        door_status = "关";
                    }
                    event = "门禁重新上电:" + power_down + ",按键状态(" + hand_status + "),门状态(" + door_status + ")";
                }
                else if (remark == 0x07)//内部控制参数被修改的记录
                {
                    int flag;
                    parseIntVal(recv_buf,0,8,flag);
                    if(flag != 0)
                    {
                        printf("门禁协议错误:remark=7!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    unsigned char modify;
                    parseByteVal(recv_buf,4,2,modify);
                    if(modify&1)
                    {
                       event = "修改了ES2000的密码";
                    }
                    else if ((modify >> 1) & 1)
                    {
                        event = "修改了门的特性控制参数";
                    }
                    else if ((modify >> 2) & 1)
                    {
                        event = "增加了新用户";
                    }
                    else if ((modify >> 3) & 1)
                    {
                        event = "删除了用户资料";
                    }
                    else if ((modify >> 4) & 1)
                    {
                        event = "修改了实时钟";
                    }
                    else if ((modify >> 5) & 1)
                    {
                        event = "修改了控制准进的时段设置";
                    }
                    else if ((modify >> 6) & 1)
                    {
                        event = "修改了节假日列表";
                    }
                    else if ((modify >> 7) & 1)
                    {
                        event = "修改了红外开启（关闭）的设置控制字";
                    }
                }
                else if (remark == 0x08)//无效的用户卡刷卡记录
                {
                    if(status != 0)
                    {
                        printf("门禁协议错误:remark=8!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    event = id + ":" + "无效的用户刷卡";
                }
                else if (remark == 0x09)//用户卡的有效期已过
                {
                    if(status != 0)
                    {
                        printf("门禁协议错误:remark=9!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    event = id + ":"  + "用户卡的有效期已过";
                }
                else if (remark == 0x10)//当前时间该用户卡无进入权限
                {
                    if(status != 0)
                    {
                        printf("门禁协议错误:remark=10!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    event = id + ":" + "当前时间该用户卡无进入权限";
                }
                // 将event存入告警模块
                // TODO:
                //
                ae::Alarm alarm;
                alarm.ciid = dev_id_ + "." + (*it)->conf.id;
                alarm.desc = event;
                alarm.alarmid= util::IMUuid::createUuid();
                alarm.end_State = ae::alarm_continuous;
                alarm.begin_time = util::IMDateTime::currentDateTime().toUTC();
                alarm.level	= 1;
                alarm.dev_name = "dac";
                alarm.devid = dev_id_;

			    ae::creater::AlarmMgr::instance()->post(&alarm);

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
			printf("Reopen COMM failed..(func:set)\n");
			return -1;
		}
	}

	int veri_pos = tag_conf.set_param3.find("pw");
	if(-1 != veri_pos)// need verify
    {
        CmdFeature cmd_verify;
        cmd_verify.addr = cmd_feature.addr;
        cmd_verify.cmd = pw_cmd_;
        int re = sendDev(cmd_verify);
        if(util::RTData::kOk != re)
        {
            printf("sendDev failed..(func:set)\n");
            serial->close();
            return util::RTData::kCmdRespError;
        }
        util::IMBuffer<2048> recv_buf;
        if(recvDev(recv_buf))
        {
            serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // 清空无效的串口缓存数据
            printf("recvDev failed..(recvDev:%s, func:set)\n",recv_buf.ptr(0));
            return util::RTData::kCmdRespError;
        }

        // 解析BTN
        util::IMBuffer<1> rtn;
        rtn.fromHex(recv_buf.ptr(7), 2);
        unsigned short RTN = rtn[0];
        if (RTN != 0)
        {
            printf("Set failed..(BTN:%d)\n",RTN);
            return util::RTData::kCmdRespError;
        }

       // return util::RTData::kOk;
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

					printf("recvDev failed..(recvDev:%s, func:set)\n",recv_buf.ptr(0));
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
                printf("sendDev failed..(func:set)\n");
                serial->close();
                return util::RTData::kCmdRespError;
            }
            util::IMBuffer<2048> recv_buf;
            if(recvDev(recv_buf))
            {
                serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // 清空无效的串口缓存数据
                printf("recvDev failed..(recvDev:%s, func:set)\n",recv_buf.ptr(0));
                return util::RTData::kCmdRespError;
            }

            // 解析BTN
            util::IMBuffer<1> btn;
            btn.fromHex(recv_buf.ptr(7), 2);
            unsigned short BTN = btn[0];
            if (BTN != 0)
            {
                printf("Set failed..(BTN:%d)\n",BTN);
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
		printf("Send COMM Data failed(%s)\n", send_buf);
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
		printf("Command no respond.\n");
		return util::RTData::kCmdNoResp;
	}
	else if(read_len != head_len)
	{
		printf("Recv HeadLen Failed (head_len:%d, read_len:%d\n)", head_len, read_len);
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
		printf("Recv Body Failed (body_len:%d, read_len:%d\n)", protocol_len, read_len);
		return util::RTData::kCmdRespError;
	}

	// 4.校验验证
	unsigned short cs = checkSum(recv_buf.ptr(1), protocol_len + head_len -2 -4);
	util::IMBuffer<2> recv_str_cs;
	recv_str_cs.fromHex(recv_buf.ptr(recv_buf.size()-5), 4);
	unsigned short recv_cs = (recv_str_cs[0] << 8) + recv_str_cs[1];
	if(cs != recv_cs)
	{
		printf("CheckSum Failed (recv_buf:%s\n)", recv_buf.ptr(0));
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



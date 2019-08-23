#include "im_dm_devices_modbus.h"
#include "util_imvariant.h"
#include "util_imbufer.h"
#include "util_datetime.h"
#include "util_commonfun.h"

#include <unistd.h>
#include <math.h>

Device::Device(void)
{
	serial      = nullptr;
	trans_id_   = 0;
	offset_     = 0;
}


Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{
	if("TCP" == util::strupr(device_info.ver) || "TCP_1" == util::strupr(device_info.ver))
	{
		type_ = kTcp;
	}
	else
	{
		type_ = kRtu;
	}
    // 当协议中说明为40001（其为PLC地址表示方法，寻址应该为0）,模板中也配置协议地址为1(所见即所得，后台减少配置的工作量)
	if("RTU_1" == util::strupr(device_info.ver) || "TCP_1" == util::strupr(device_info.ver))
    {
        offset_ = -1;
    }
    else
    {
        offset_ = 0;
    }

	serial = IMSerialPort::getSerial(device_info.channel_identity.c_str());

	if(!serial->isOpen())
	{
		if(!serial->open(device_info.channel_identity.c_str(), device_info.channel_params.c_str()))
		{
			return util::RTData::kCommDisconnected;
		}
	}

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

	int funcation = cmd[0];

	ModbusPacketSend packet_send;
	packet_send.buildPacket(type_, atoi(cmd_feature.addr.c_str()), funcation, (cmd[1]<<8) + cmd[2] + offset_, (cmd[3]<<8) + cmd[4]);
	packet_send.setTransId(trans_id_++);
	serial->write((char*)packet_send.sendData(), packet_send.sendByte());


	ModbusPacketRecv packet_recv(packet_send);
	int read_len = serial->read(packet_recv.preRecv(), packet_recv.preRecvLen());
	if(0 >= read_len)
	{
		return util::RTData::kCmdNoResp;
	}
	if(packet_recv.preRecvLen() != read_len)
	{
		serial->close();
		return util::RTData::kCmdRespError;
	}
	if(!packet_recv.isTransIdOk())
    {
        flush();
        return util::RTData::kCmdRespError;
    }

    if(!packet_recv.isFunCodeOk())
    {
        if(kRtu == type_)  // 收取剩余的2个校验字节
        {
            char temp[3]= {0};
            serial->read(temp,2, 300);
        }

        // 收取可能出现的异常数据
        flush();
        return util::RTData::kCmdRespError;
    }

	if(packet_recv.recvLen() != serial->read(packet_recv.recv(), packet_recv.recvLen()))
	{
		serial->close();
		return util::RTData::kCmdRespError;
	}

	if(!packet_recv.isCmdRespOk())
	{
		return util::RTData::kCmdRespError;
	}
	if(!packet_recv.isCRCOk())
	{
		return util::RTData::kCmdRespError;
	}

	if(0x03 == funcation || 0x04 == funcation)
	{
		Tags::iterator it;
		for(it = tags.begin(); it != tags.end(); ++it)
		{
		    im_string param3 = (*it)->conf.get_param3;
			(*it)->value = Func03Parse(packet_recv, (*it)->conf.data_type,
				(*it)->conf.get_param1, (*it)->conf.get_param2, (*it)->conf.get_param3);
		}
	}
	else if(0x01 == funcation || 0x02 == funcation)
	{
		Tags::iterator it;
		for(it = tags.begin(); it != tags.end(); ++it)
		{
			int addr = 0;

			im_string param1 = (*it)->conf.get_param1;
			im_string param3 = (*it)->conf.get_param3;
			if(0 == param1.find("0x"))
			{
				addr = strtol(param1.substr(2).c_str(), NULL, 16) + offset_;
			}
			else
			{
				addr = atoi(param1.c_str()) + offset_;
			}

			bool is_ok = false;
			if(TagValue::kDigit == (*it)->conf.data_type)
			{
				bool val = packet_recv.getStatusDigit(addr, &is_ok);
				if(is_ok)
				{
					if(std::string::npos != param3.find("!"))
					{
						(*it)->value.pv			= util::IMVariant(!val);
					}
					else
					{
						(*it)->value.pv			= util::IMVariant(val);
					}


                    (*it)->value.quality	= util::RTData::kOk;
					(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
				}
			}
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
			return util::RTData::kCommDisconnected;
		}
	}
	util::IMBuffer<16> cmd;
	if(!cmd.fromHex(cmd_feature.cmd.c_str()))
	{
		return util::RTData::kConfigError;
	}

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

    if(0x05 == func)
    {
        if(util::IMVariant(tag_value_pv.c_str()).toBool())
		{
			val[0] = 0xFF;
		}
		else
		{
			val[0] = 0x00;
		}
		val[1] = 0x00;
		val_bytes = 2;
    }
	else
	{
        float set_val = util::IMVariant(tag_value_pv.c_str()).toFloat();
        if(0 == tag_conf.set_param3.find("*"))
        {
            if(tag_conf.set_param3.size() >= 2)
            {
                set_val = set_val * atoi(tag_conf.set_param3.substr(1).c_str());
            }
        }

        if(0x06 == func)
        {
            unsigned int iset_val = set_val;
            val[0] = (iset_val >> 8)&0xFF;
            val[1] = (iset_val)&0xFF;
            val_bytes = 2;
        }
        else if(0x10 == func)
        {
            if(0 == tag_conf.get_param2.find("unsigned int") || 0 == tag_conf.get_param2.find("ui") || 0 == tag_conf.get_param2.find("int") || 0 == tag_conf.get_param2.find("i"))
            {
                unsigned int iset_val = set_val;
                if(std::string::npos != tag_conf.get_param2.find("_inv"))
                {
                    val[0] = (iset_val >> 24)&0xFF;
                    val[1] = (iset_val >> 16)&0xFF;
                    val[2] = (iset_val >> 8)&0xFF;
                    val[3] = (iset_val)&0xFF;
                }
                else
                {
                    val[2] = (iset_val >> 24)&0xFF;
                    val[3] = (iset_val >> 16)&0xFF;
                    val[0] = (iset_val >> 8)&0xFF;
                    val[1] = (iset_val)&0xFF;
                }
                val_bytes = 4;
            }
            else if(tag_conf.get_param2.empty() || 0 == tag_conf.get_param2.find("unsigned short") || 0 == tag_conf.get_param2.find("us") || 0 == tag_conf.get_param2.find("short")|| 0 == tag_conf.get_param2.find("s"))
            {
                unsigned int iset_val = set_val;
                if(std::string::npos != tag_conf.get_param2.find("_inv"))
                {
                    val[0] = (iset_val >> 8)&0xFF;
                    val[1] = (iset_val)&0xFF;
                }
                else
                {
                    val[1] = (iset_val >> 8)&0xFF;
                    val[0] = (iset_val)&0xFF;
                }

                val_bytes = 2;
            }
            else
            {
                return util::RTData::kConfigError;
            }

        }
        else
        {
            return util::RTData::kConfigError;
        }
    }

    ModbusSetCmdPacketSend packet_send;
    if(0 != packet_send.buildPacket(type_, atoi(cmd_feature.addr.c_str()), func, data_addr+offset_, val, val_bytes))
    {
         return util::RTData::kConfigError;
    }
    packet_send.setTransId(trans_id_++);
	serial->write((char*)packet_send.sendData(), packet_send.sendByte());

	ModbusSetCmdPacketRecv packet_recv(packet_send);
	int read_len = serial->read(packet_recv.preRecv(), packet_recv.preRecvLen());
	if(0 >= read_len)
	{
		return util::RTData::kCmdNoResp;
	}
	if(packet_recv.preRecvLen() != read_len)
	{
		return util::RTData::kCmdRespError;
	}

    if(!packet_recv.isTransIdOk())
    {
        serial->close();
        return util::RTData::kCmdRespError;
    }

    if(!packet_recv.isFunCodeOk())
    {
        char temp[3]= {0};
        if(kRtu == type_)  // 收取剩余的2个校验字节
        {
            serial->read(temp,2, 300);
        }

        // 收取可能出现的异常数据
        int try_recv_abnormal_cnt = 200;
        while(serial->read(temp,1, 300) > 0)
        {
            if(try_recv_abnormal_cnt-- <= 0)
            {
                serial->close();
                break;
            }

        }

        return util::RTData::kCmdRespError;
    }

	if(packet_recv.recvLen() != serial->read(packet_recv.recv(), packet_recv.recvLen()))
	{
		serial->close();
		return util::RTData::kCmdRespError;
	}

	if(!packet_recv.isCRCOk())
	{
		return util::RTData::kCmdRespError;
	}

	return util::RTData::kOk;
}


 /*
	 param1: 寄存器地址
	 param2: data_type为kAnalog时或kEnumStatus：
				param2=""						-> 取2字节，无符号
				param2="unsigned short" 或"us"	-> 取2字节，无符号
				param2="short" 或 "s"			-> 取2字节，有符号
				param2="unsigned int" 或 "ui"   -> 取4字节，无符号
				param2="int"  或 "i"			-> 取4字节，有符号
				param2= x-y						-> 取2字节	第x位到第y位的值，如0-2
				param2 = ui_h1000               -> 取4字节， 值=高2字节*1000 + 低2字节
				param2 = ui_2L					-> 取8字节，但每4字节的低位字
				param2 = float 或 "f"			-> 取4字节浮点型
				param2 = f_inv					-> 高低位顺序相反

	          data_type为kDigit时:
			   param2=""                        ->取寄存器值作为bool量
			   param2="0"						->取寄存器值的第0位作为bool量,
			   param2="1"						->取第1位
			   ....
			   param2="15"                      ->取第15位
			   param2= x-y?v					-> 取2字节	第x位到第y位的值是否与v相等，如0-2?6

	 param3: 运算函数，目前支持abs()，可填多个，";"号隔开
			  param3="abs()"					->取绝对值, 往往配合有符号值使用
	 */


util::RTValue Device::Func03Parse(ModbusPacketRecv& packet_recv, TagValue::ValType data_type,
					const libistring& param1, const libistring& param2, const libistring& param3)
{
	util::RTValue re_value;
	re_value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
	re_value.quality	= util::RTData::kOk;

	int addr = 0;
	if(0 == param1.find("0x")) // 16进制表示的地址
	{
		addr = strtol(param1.substr(2).c_str(), NULL, 16) + offset_;
	}
	else
	{
		addr = atoi(param1.c_str()) + offset_ ;
	}

	if(TagValue::kAnalog == data_type || TagValue::kEnumStatus == data_type)
	{
		if(param2.empty() || "unsigned short" == param2 || "us" == param2 || "short" == param2 || "s" == param2)
		{
			unsigned char val[2] = {0};
			if(!packet_recv.getRegister(addr, val, 2))
			{
				re_value.quality	= util::RTData::kConfigError;
				return re_value;
			}
			else
			{
				if(param2.empty() || "unsigned short" == param2 || "us" == param2) //无符号
				{
					unsigned short data = (val[0] << 8) + val[1];
					re_value.pv = util::IMVariant(UInt32(data));
				}
				else  // 有符号
				{
					short data = (val[0] << 8) + val[1];
					if(im_string::npos != param3.find("abs"))
					{
						UInt32 abs_d =  ::abs(data);
						re_value.pv = util::IMVariant(abs_d);
					}
					else
					{
						re_value.pv = util::IMVariant(Int32(data));
					}

				}
			}

		}
		else if( 0 == param2.find("unsigned int") || 0 == param2.find("ui") ||  0 == param2.find("int") || 0 == param2.find("i"))
		{
			unsigned char val[4] = {0};
			if(!packet_recv.getRegister(addr, val, 4))
			{
				re_value.quality	= util::RTData::kConfigError;
				return re_value;
			}
			else
            {
                unsigned int data  = 0;
                if(std::string::npos != param2.find("_inv"))
                {
                    data = ModbusUtil::toInt32(val, kBigEndian);
                }
                else
                {
                    data = ModbusUtil::toInt32(val, kLittleEndianWordInverted);
                }

                if("unsigned int" == param2 || "ui" == param2) //无符号
                {
                    re_value.pv = util::IMVariant(UInt32(data));
                }
                else // 有符号
                {
                    Int32 idata = data;
                    if(im_string::npos != param3.find("abs"))
                    {
                        UInt32 abs_d =  ::abs(data);
                        re_value.pv = util::IMVariant(abs_d);
                    }
                    else
                    {
                        re_value.pv = util::IMVariant(idata);
                    }
                }

			}
		}
		else if(std::string::npos != param2.find("-"))  // 取2字节	第x位到第y位的值，如0-2
		{
			unsigned char val[2] = {0};
			if(!packet_recv.getRegister(addr, val, 2))
			{
				re_value.quality	= util::RTData::kConfigError;
				return re_value;
			}
			else
			{
				int pos =  param2.find("-");
				int b1 = atoi(param2.substr(0, pos).c_str());
				int b2 = atoi(param2.substr(pos+1).c_str());
				if (b1 <0 || b2>15)
				{
					re_value.quality	= util::RTData::kConfigError;
					return re_value;
				}
				unsigned short data = (val[0] << 8) + val[1];
				data = (data >> b1) & ((1<<(b2-b1+1)) -1);

				re_value.pv = util::IMVariant(UInt32(data));
			}

		}
		else if("ui_h1000" == param2)  // 取4字节， 值=高2字节*1000 + 低2字节
		{
			unsigned char val[4] = {0};
			if(!packet_recv.getRegister(addr, val, 4))
			{
				re_value.quality	= util::RTData::kConfigError;
				return re_value;
			}
			else
			{
				unsigned short hi =  (val[0] << 8) + val[1];
				unsigned short lo =  (val[2] << 24) + val[3];
				unsigned int  data =  hi*1000 + lo;
				re_value.pv = util::IMVariant(UInt32(data));
			}
		}
		else if("ui_2L" == param2)  // 取8字节，但每4字节的低位字
		{
			unsigned char val[8] = {0};
			if(!packet_recv.getRegister(addr, val, 8))
			{
				re_value.quality	= util::RTData::kConfigError;
				return re_value;
			}
			else
			{
				unsigned short lo =  (val[0] << 8) + val[1];
				unsigned short hi =  (val[4] << 24) + val[5];
				unsigned int  data =  ((hi << 16)&0xFFFF0000) + lo;
				re_value.pv = util::IMVariant(UInt32(data));
			}

		}
		else if(0 == param2.find("f")) // float
		{
			unsigned char val[4] = {0};
			if(!packet_recv.getRegister(addr, val, 4))
			{
				re_value.quality	= util::RTData::kConfigError;
				return re_value;
			}
			else
			{
                float fval = 0;
				if(std::string::npos != param2.find("_inv"))
				{
					fval = ModbusUtil::toFloat(val, kBigEndian);
				}
				else
				{
                    fval = ModbusUtil::toFloat(val, kLittleEndianWordInverted);
				}

				if(im_string::npos != param3.find("abs"))
                {
                    fval = ::fabs(fval);
                }

                re_value.pv = util::IMVariant(fval);

			}

		}
	}
	else if(TagValue::kDigit == data_type)
	{
		unsigned char val[2] = {0};
		if(!packet_recv.getRegister(addr, val, 2))
		{
			re_value.quality	= util::RTData::kConfigError;
			return re_value;
		}
		if(param2.empty()) // 取寄存器16位值作为bool量
		{
			unsigned short data		= (val[0] << 8) + val[1];
			if(im_string::npos != param3.find("trlist"))  // 对照表翻译   // trlist(3=0,7=1)
			{
				re_value.pv 			= util::IMVariant(false);
				libistring str_data = util::IMVariant(data).toString() + "=";
				int pos = param3.find(str_data);
				if(-1 != pos)
				{
					if(pos+str_data.size()+1 <= param3.size() &&    //合法性判断
						"1" == param3.substr(pos+str_data.size(), 1))
					{
						re_value.pv 			= util::IMVariant(true);
					}

				}
				else
				{
                    if(1 == data)
                    {
                        re_value.pv = util::IMVariant(true);
                    }
                    else
                    {
                        re_value.pv = util::IMVariant(false);
                    }

				}
			}
			else if(im_string::npos != param3.find("isval")) // 与值判断是否为真 		// idval(3);
			{
				re_value.pv 			= util::IMVariant(false);
				libistring str_data = util::IMVariant(data).toString();
				if(im_string::npos != param3.find(str_data))
				{
					re_value.pv 		= util::IMVariant(true);
				}

			}
			else
			{
				if(1 == data)
                {
                        re_value.pv = util::IMVariant(true);
                }
                else
                {
                        re_value.pv = util::IMVariant(false);
                }
			}

		}
		else // 取寄存器位
		{
			if(std::string::npos != param2.find("-"))  // 取2字节	第x位到第y位的值是否与?后面值相等
			{
				unsigned char val[2] = {0};
				if(!packet_recv.getRegister(addr, val, 2))
				{
					re_value.quality	= util::RTData::kConfigError;
					return re_value;
				}
				else
				{
					size_t pos =  param2.find("-");
					int b1 = atoi(param2.substr(0, pos).c_str());
					int b2 = atoi(param2.substr(pos+1).c_str());
					if (b1 <0 || b2>15)
					{
						re_value.quality	= util::RTData::kConfigError;
						return re_value;
					}
					pos = param2.find("?");
					if(std::string::npos == pos)
					{
						re_value.quality	= util::RTData::kConfigError;
						return re_value;
					}
					unsigned short v = atoi(param2.substr(pos+1).c_str());
					unsigned short data = (val[0] << 8) + val[1];
					bool bvalue = data==v?true:false;
					re_value.pv				=  util::IMVariant(bvalue);
				}

			}
			else
			{
				unsigned short data		= (val[0] << 8) + val[1];
				bool bvalue				= (data >> atoi(param2.c_str())) & 1;
				re_value.pv				=  util::IMVariant(bvalue);

			}

		}

		if(std::string::npos != param3.find("!"))
		{
			re_value.pv = util::IMVariant(!re_value.pv.toBool());
		}

	}

	return re_value;
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

#include "im_dm_devices_modbus_dyxn.h"
#include "util_imvariant.h"
#include "util_imbufer.h"
#include "util_datetime.h"
#include "util_commonfun.h"

#include <unistd.h>

Device::Device(void)
{
	serial = nullptr;
	is_modbus_tcp_ = false;
}


Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{
	if("TCP" == util::strupr(device_info.ver))
	{
		is_modbus_tcp_ = true;
	}
	else
	{
		is_modbus_tcp_ = false;
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

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
	if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			return util::RTData::kCommDisconnected;
		}
	}

	/*char temp[1] = {0};
	int try_cnt = 0;
	while(serial->read(temp,1,10) > 0)
	{
        printf("------serial->read(temp,1,10)\n");
		if(try_cnt++ > 1000)
		{
			serial->close();
			return util::RTData::kCmdNoResp;
		}
	}*/

 	util::IMBuffer<16> cmd;
	if(!cmd.fromHex(cmd_feature.cmd.c_str()))
	{
		return util::RTData::kConfigError;
	}

	int funcation = cmd[0];

	ModbusPacketSend packet_send;
	if(is_modbus_tcp_)
	{
		packet_send.buildPacket(kTcp, atoi(cmd_feature.addr.c_str()), funcation, (cmd[1]<<8) + cmd[2], (cmd[3]<<8) + cmd[4]);
	}
	else
	{
		packet_send.buildPacket(kRtu, atoi(cmd_feature.addr.c_str()), funcation, (cmd[1]<<8) + cmd[2], (cmd[3]<<8) + cmd[4]);
	}
	serial->write((char*)packet_send.sendData(), packet_send.sendByte());


	ModbusPacketRecv packet_recv(packet_send);
	int read_len = serial->read(packet_recv.preRecv(), packet_recv.preRecvLen());
	if(0 >= read_len)
	{
		serial->close();
		return util::RTData::kCmdNoResp;
	}
	if(packet_recv.preRecvLen() != read_len)
	{
		serial->close();
		return util::RTData::kCmdRespError;
	}

	if(packet_recv.recvLen() != serial->read(packet_recv.recv(), packet_recv.recvLen()))
	{
		serial->close();
		return util::RTData::kCmdRespError;
	}

    if(!packet_recv.isTransIdOk())
    {
        serial->close();
        return util::RTData::kCmdRespError;
    }
    if(!packet_recv.isFunCodeOk())
    {
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
				addr = strtol(param1.substr(2).c_str(), NULL, 16);
			}
			else
			{
				addr = atoi(param1.c_str());
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
	int pos = 0;
	util::IMBuffer<255> send_buf;
	int addr = util::IMVariant(cmd_feature.addr.c_str()).toInt();
	send_buf[pos] = (addr >> 8) & 0xFF;
	pos += 1;
	send_buf[pos++] = cmd[0];
	send_buf[pos++] = addr  & 0xFF;
	memcpy(&send_buf[pos], &cmd[1], (cmd_feature.cmd.size()-2)/2);
	pos += (cmd_feature.cmd.size()-2)/2;
	if(0x05 == cmd[0])					// ״ֵ̬
	{
		if(util::IMVariant(tag_value_pv.c_str()).toBool())
		{
			send_buf[pos++] = 0xFF;
		}
		else
		{
			send_buf[pos++] = 0x00;
		}
		send_buf[pos++] = 0x00;
	}
	else if(0x06 == cmd[0])				// �Ĵ���ֵ
	{
		//int set_val = util::IMVariant(tag_value_pv.c_str()).toInt();
		float set_val = util::IMVariant(tag_value_pv.c_str()).toFloat();
		if(0 == tag_conf.set_param1.find("*"))
		{
			if(tag_conf.set_param1.size() >= 2) //�Ϸ����ж�
			{
				set_val = set_val * atoi(tag_conf.set_param1.substr(1).c_str());
			}
		}
		send_buf[pos++] = (int)set_val >> 8;
		send_buf[pos++] = (int)set_val & 0x00FF;
	}
	else if (0x10 == cmd[0])
	{
        int reg_count = (send_buf[pos-2] << 8) + send_buf[pos-1];
        send_buf[pos++] = reg_count*2;
        if(2 == reg_count)
        {
           int set_val = util::IMVariant(tag_value_pv.c_str()).toInt();
           if(0 == tag_conf.set_param1.find("*"))
           {
                if(tag_conf.set_param1.size() >= 2)
                {
                    set_val = set_val * atoi(tag_conf.set_param1.substr(1).c_str());
                }
            }
            send_buf[pos++] = (set_val >> 24)&0xFF;
            send_buf[pos++] = (set_val >> 16)&0xFF;
            send_buf[pos++] = (set_val >> 8)&0xFF;
            send_buf[pos++] = (set_val)&0xFF;
        }

    }

	uint16_t crc = ModbusUtil::crc16((uint8_t*)&send_buf[0], pos);
	send_buf[pos++] = crc >> 8;
	send_buf[pos++] = crc & 0x00FF;
	serial->write((char*)&send_buf[0], pos);//130123 meiqiu modified

	util::IMBuffer<9> recv_buf;
	int read_len = serial->read((char*)&recv_buf[0], 9);
	if(0 == read_len)
	{
		return util::RTData::kCmdNoResp;
	}
	if(9 != read_len)
	{
		return util::RTData::kCmdRespError;
	}

	uint16_t recv_crc = ModbusUtil::crc16((uint8_t*)&recv_buf[0], 7);
	if((uint8_t)recv_buf[7] != (recv_crc >> 8) ||		//130123 meiqiu
		(uint8_t)recv_buf[8] != (recv_crc & 0x00FF))	//130123 meiqiu
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
				param2 = f						-> 取4字节浮点型
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
		addr = strtol(param1.substr(2).c_str(), NULL, 16);
	}
	else
	{
		addr = atoi(param1.c_str());
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
		else if("unsigned int" == param2 || "ui" == param2 || "int" == param2 || "i" == param2)
		{
			unsigned char val[4] = {0};
			if(!packet_recv.getRegister(addr, val, 4))
			{
				re_value.quality	= util::RTData::kConfigError;
				return re_value;
			}
			else
			{
				if(param2.empty() || "unsigned int" == param2 || "ui" == param2) //无符号
				{
					unsigned int data  = 0;
					if(std::string::npos != param2.find("_H")) // Hi begin
					{
						data = (val[0] << 24) + (val[1] << 16) + (val[2] << 8) + val[3];
					}
					else
					{
						data = (val[2] << 24) + (val[3] << 16) + (val[0] << 8) + val[1];
					}

					re_value.pv = util::IMVariant(UInt32(data));
				}
				else  // 有符号
				{
					int data = 0;
					if(std::string::npos != param2.find("_H")) // Hi begin
					{
						data = (val[0] << 24) + (val[1] << 16) + (val[2] << 8) + val[3];
					}
					else
					{
						data = (val[2] << 24) + (val[3] << 16) + (val[0] << 8) + val[1];
					}

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
		else if("ui_H" == param2 )
		{
			unsigned char val[4] = {0};
			if(!packet_recv.getRegister(addr, val, 4))
			{
				re_value.quality	= util::RTData::kConfigError;
				return re_value;
			}
			else
			{

				unsigned int data  = (val[0] << 24) + (val[1] << 16) + (val[2] << 8) + val[3];
				re_value.pv = util::IMVariant(UInt32(data));
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
			else if ("date" == param3)
			{
			    UInt64 date;
				date = ((val[0]<<8) + val[1])*1000000 + ((val[2]<<8) + val[3])*10000+ ((val[4]<<8) + val[5])*100 + ((val[6]<<8) + val[7])*1;
				re_value.pv = util::IMVariant(date);
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
				bool is_inverse = false;
				if(std::string::npos != param2.find("_inv"))
				{
					is_inverse = true;
				}
				re_value.pv = util::IMVariant(ModbusUtil::toFloat(val, is_inverse));
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

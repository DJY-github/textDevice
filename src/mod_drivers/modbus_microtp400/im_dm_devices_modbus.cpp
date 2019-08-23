#include "im_dm_devices_modbus.h"
#include "imutil/util_imvariant.h"
#include "imutil/util_imbufer.h"
#include "imutil/util_datetime.h"
#include <math.h>
#include <pthread.h>
#include <unistd.h>

Device::Device(void)
{
	serial = nullptr;
	is_modbus_tcp_ = false;
	crc_err_cnt = 0;
}


Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{
	if("TCP" == device_info.iftype)
	{
		is_modbus_tcp_ = true;
	}
	else
	{
		is_modbus_tcp_ = false;
	}

	chname = device_info.channel_identity;

	serial = IMSerialPort::getSerial(device_info.channel_identity.c_str());

	if(!serial->isOpen())
	{
		if(!serial->open(device_info.channel_identity.c_str(), device_info.channel_params.c_str()))
		{
			return kCommDisconnected;
		}
	}

	return kOk;
}
int Device::close()
{
	if(nullptr != serial)
	{
		serial->close();
	}

	return kOk;
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
	usleep(100000);
	if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			return kCommDisconnected;
		}
	}

 	util::IMBuffer<16> cmd;
	if(!cmd.fromHex(cmd_feature.cmd.c_str()))
	{
		return kConfigError;
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

	ModbusPacketRecv packet_recv(packet_send);
	int read_len = 0;
	int try_count = 3;
	int timeout = 2000;

	time_t tm_begin = time(NULL);
	for (int i = 0; i < try_count; ++i)
	{
		if(i > 0)
		{
			timeout = 1000;
		}
		serial->write((char*)packet_send.sendData(), packet_send.sendByte());
		read_len = serial->read(packet_recv.preRecv(), packet_recv.preRecvLen(),timeout);
		if(read_len > 0)
		{
			break;
		}
		usleep(100000);
	}


	if(0 >= read_len)
	{
		printf("%s: read timeout (%d s).\n", chname.c_str(), time(NULL)-tm_begin);
		serial->close();
		return kCmdNoResp;
	}


	if(packet_recv.preRecvLen() != read_len)
	{
		printf("%s: packet_recv.preRecvLen() != read_len.\n", chname.c_str());
		serial->close();
		return kCmdRespError;
	}

	//printf("%d: 3\n", pthread_self());
	if(packet_recv.recvLen() != serial->read(packet_recv.recv(), packet_recv.recvLen()))
	{
		printf("%s: packet_recv.recvLen() != read_len.\n", chname.c_str());
		serial->close();
		return kCmdRespError;
	}
	//printf("%d: 4\n", pthread_self());

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
		return kCmdRespError;
	}

	if(!packet_recv.isCRCOk())
	{
		printf("%s: crc err.\n", chname.c_str());
		if(crc_err_cnt++ > 3)
        {
            serial->close();
            crc_err_cnt = 0;
        }
		return kCmdRespError;
	}

    crc_err_cnt = 0;

	if(0x03 == funcation || 0x04 == funcation)
	{
		Tags::iterator it;
		for(it = tags.begin(); it != tags.end(); ++it)
		{
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
			if(0 == param1.find("0x")) // 16���Ʊ�ʾ�ĵ�ַ
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
					(*it)->value.pv			= util::IMVariant(val);
					(*it)->value.quality	= util::RTData::kOk;
					(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
				}
			}
		}
	}


	return kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
	if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			return kCommDisconnected;
		}
	}
	util::IMBuffer<16> cmd;
	if(!cmd.fromHex(cmd_feature.cmd.c_str()))
	{
		return kConfigError;
	}

	int pos = 0;
	util::IMBuffer<16> send_buf;//130123 meiqiu
	send_buf[pos] = (unsigned char)util::IMVariant(cmd_feature.addr.c_str()).toInt();
	pos += 1;
	memcpy(&send_buf[pos], &cmd[0], 3);	// 1�ֽڹ����� + 2�ֽڵ�ַ
	pos += 3;
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
		int set_val = util::IMVariant(tag_value_pv.c_str()).toInt();
		if(0 == tag_conf.set_param1.find("*"))
		{
			if(tag_conf.set_param1.size() >= 2) //�Ϸ����ж�
			{
				set_val = set_val * atoi(tag_conf.set_param1.substr(1).c_str());
			}
		}
		send_buf[pos++] = set_val >> 8;
		send_buf[pos++] = set_val & 0x00FF;
	}
	else if (0x10 == cmd[0])//130123 meiqiu added for M44
	{
		send_buf[pos++] = 0x00;//�Ĵ�������
		send_buf[pos++] = 0x01;//�Ĵ�������
		send_buf[pos++] = 0x02;//ֵ�ĳ���
		send_buf[pos++] = 0x00;//ֵ�ĸ�λ
		if(util::IMVariant(tag_value_pv.c_str()).toBool())//ֵ�ĵ�λ
		{
			send_buf[pos++] = 0x01;
		}
		else
		{
			send_buf[pos++] = 0x00;
		}
	}//110123 added end

	uint16_t crc = ModbusUtil::crc16((uint8_t*)&send_buf[0], pos);
	send_buf[pos++] = crc >> 8;
	send_buf[pos++] = crc & 0x00FF;

	serial->write((char*)&send_buf[0], pos);//130123 meiqiu modified

	util::IMBuffer<8> recv_buf;
	int read_len = serial->read((char*)&recv_buf[0], 8);
	if(0 == read_len)
	{
		// TODO:������
		return kCmdNoResp;
	}
	if(8 != read_len)
	{
		return kCmdRespError;
	}

	uint16_t recv_crc = ModbusUtil::crc16((uint8_t*)&recv_buf[0], 6);
	if((uint8_t)recv_buf[6] != (recv_crc >> 8) ||		//130123 meiqiu
		(uint8_t)recv_buf[7] != (recv_crc & 0x00FF))	//130123 meiqiu
	{
		return kCmdRespError;
	}

	return kOk;
}


 /*
	 param1: �Ĵ�����ַ
	 param2: data_typeΪkAnalogʱ��kEnumStatus��
				param2=""						-> ȡ2�ֽڣ��޷���
				param2="unsigned short" ��"us"	-> ȡ2�ֽڣ��޷���
				param2="short" �� "s"			-> ȡ2�ֽڣ��з���
				param2="unsigned int" �� "ui"   -> ȡ4�ֽڣ��޷���
				param2="int"  �� "i"			-> ȡ4�ֽڣ��з���
				param2= x-y						-> ȡ2�ֽ�	��xλ����yλ��ֵ����0-2
				param2 = ui_h1000               -> ȡ4�ֽڣ� ֵ=��2�ֽ�*1000 + ��2�ֽ�
				param2 = ui_2L					-> ȡ8�ֽڣ���ÿ4�ֽڵĵ�λ��

	          data_typeΪkDigitʱ:
			   param2=""                        ->ȡ�Ĵ���ֵ��Ϊbool��
			   param2="0"						->ȡ�Ĵ���ֵ�ĵ�0λ��Ϊbool��,
			   param2="1"						->ȡ��1λ
			   ....
			   param2="15"                      ->ȡ��15λ
			   param2= x-y?v					-> ȡ2�ֽ�	��xλ����yλ��ֵ�Ƿ���v��ȣ���0-2?6

	 param3: ���㺯����Ŀǰ֧��abs()����������";"�Ÿ���
			  param3="abs()"					->ȡ����ֵ, ��������з���ֵʹ��
	 */


util::RTValue Device::Func03Parse(ModbusPacketRecv& packet_recv, TagValue::ValType data_type,
					const libistring& param1, const libistring& param2, const libistring& param3)
{
	util::RTValue re_value;
	re_value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
	re_value.quality	= util::RTData::kOk;

	int addr = 0;
	if(0 == param1.find("0x")) // 16���Ʊ�ʾ�ĵ�ַ
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
				if(param2.empty() || "unsigned short" == param2 || "us" == param2) //�޷���
				{
					unsigned short data = (val[0] << 8) + val[1];
					re_value.pv = util::IMVariant(UInt32(data));
				}
				else  // �з���
				{
					short data = (val[0] << 8) + val[1];
					if(-1 != param3.find("abs"))
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
				if(param2.empty() || "unsigned int" == param2 || "ui" == param2) //�޷���
				{
					unsigned int data = (val[2] << 24) + (val[3] << 16) + (val[0] << 8) + val[1];
					re_value.pv = util::IMVariant(UInt32(data));
				}
				else  // �з���
				{
					int  data = (val[2] << 24) + (val[3] << 16) + (val[0] << 8) + val[1];
					if(-1 != param3.find("abs"))
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
		else if(std::string::npos != param2.find("-"))  // ȡ2�ֽ�	��xλ����yλ��ֵ����0-2
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
		else if("ui_h1000" == param2)  // ȡ4�ֽڣ� ֵ=��2�ֽ�*1000 + ��2�ֽ�
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
		else if("ui_2L" == param2)  // ȡ8�ֽڣ���ÿ4�ֽڵĵ�λ��
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
	}
	else if(TagValue::kDigit == data_type)
	{
		unsigned char val[2] = {0};
		if(!packet_recv.getRegister(addr, val, 2))
		{
			re_value.quality	= util::RTData::kConfigError;
			return re_value;
		}
		if(param2.empty()) // ȡ�Ĵ���16λֵ��Ϊbool��
		{
			unsigned short data		= (val[0] << 8) + val[1];
			if(-1 != param3.find("trlist"))  // ���ձ���   // trlist(3=0,7=1)
			{
				re_value.pv 			= util::IMVariant(false);
				libistring str_data = util::IMVariant(data).toString() + "=";
				int pos = param3.find(str_data);
				if(-1 != pos)
				{
					if(pos+str_data.size()+1 <= param3.size() &&    //�Ϸ����ж�
						"1" == param3.substr(pos+str_data.size(), 1))
					{
						re_value.pv 			= util::IMVariant(true);
					}

				}
			}
			else if(-1 != param3.find("isval")) // ��ֵ�ж��Ƿ�Ϊ�� 		// idval(3);
			{
				re_value.pv 			= util::IMVariant(false);
				libistring str_data = util::IMVariant(data).toString();
				if(-1 != param3.find(str_data))
				{
					re_value.pv 		= util::IMVariant(true);
				}

			}
			else
			{
				re_value.pv 			= util::IMVariant((bool)data);
			}

		}
		else // ȡ�Ĵ���λ
		{
			if(std::string::npos != param2.find("-"))  // ȡ2�ֽ�	��xλ����yλ��ֵ�Ƿ���?����ֵ���
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

	}

	return re_value;
}

DRV_EXPORT const im_char*  getDrvVer()
{
	return "V.01";
}
DRV_EXPORT int initDrv(const im_char* dm_ver)
{
	return kOk;
}
DRV_EXPORT int uninitDrv()
{
	return kOk;
}
DRV_EXPORT IDevice* createDevice()
{
	return new Device;
}
DRV_EXPORT int releaseDevice(IDevice* device)
{
	delete device;
	return kOk;
}

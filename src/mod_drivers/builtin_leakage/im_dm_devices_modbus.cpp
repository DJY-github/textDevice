#include "im_dm_devices_modbus.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>



#include "util_imvariant.h"
#include "util_imbufer.h"
#include "util_datetime.h"
#include "util_commonfun.h"
#include "util_imdir.h"


#include <unistd.h>

#define GPIO_NO             "44"
#define GPIO_DIR            "/sys/class/gpio/gpio" GPIO_NO
#define GPIO_DIRECT_FILE    GPIO_DIR "/direction"
#define GPIO_VALUE_FILE     GPIO_DIR "/value"
#define GPIO_EXPORT_FILE    "/sys/class/gpio/export"

#define LEAKAGE_STATUS "conf/leakage.conf"

int Device::sensitivity_ = -1;
bool Device::have_read_once_ = false;

Device::Device(void)
{
    fd_ = 0;
	gpio_value_fd_ = -1;
}

Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{
    if(!util::IMFile::isFileExist(LEAKAGE_STATUS))
    {
        fd_ = ::open(LEAKAGE_STATUS, O_CREAT|O_RDWR,0644);
        if(0 >= fd_)
        {
            return util::RTData::kCommDisconnected;
        }
        ::close(fd_);
    }
    dev_info_ = device_info;
	return openGPIO();
}
int Device::close()
{
    closeGPIO();

	return util::RTData::kOk;
}
int  Device::openGPIO()
{
    if(!util::IMFile::isFileExist(GPIO_DIR))
    {
        int fd = ::open(GPIO_EXPORT_FILE, O_WRONLY);
        if(-1 == fd)
        {
            printf("Open %s failed.\n", GPIO_EXPORT_FILE);
            return -1;
        }
        ::write(fd, GPIO_NO, sizeof(GPIO_NO));
        ::close(fd);

        fd = ::open(GPIO_DIRECT_FILE, O_WRONLY);
        if(-1 == fd)
        {
            printf("Open %s failed.\n", GPIO_DIRECT_FILE);
            return -1;
        }
        ::write(fd, "in", 2);
        ::close(fd);
    }

    gpio_value_fd_ = ::open(GPIO_VALUE_FILE, O_RDONLY);
    if( -1 == gpio_value_fd_)
    {
        printf("Open %s failed.\n", GPIO_VALUE_FILE);
        return -1;
    }

	return util::RTData::kOk;
}
void Device::closeGPIO()
{
    if(-1 != gpio_value_fd_)
    {
        ::close(gpio_value_fd_);
        gpio_value_fd_ = -1;
    }
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{

    if(cmd_feature.cmd.empty())  // 漏水状态，从GPIO读
    {
        if(-1 == gpio_value_fd_)
        {
            openGPIO();
        }
        if(-1 == gpio_value_fd_)
        {
            return util::RTData::kCommDisconnected;
        }

        Tags::iterator it;
		for(it = tags.begin(); it != tags.end(); ++it)
		{
            (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
            char val = 0;
            lseek(gpio_value_fd_,0,SEEK_SET);
            int re = ::read(gpio_value_fd_, &val, 1);
            if(-1 == re)
            {
                (*it)->value.quality	= util::RTData::kCmdReadError;
                return util::RTData::kCmdReadError;
            }

            (*it)->value.quality	= util::RTData::kOk;
            (*it)->value.pv         = util::IMVariant(('1'==val)?0:1);

            break;
		}
    }
    else
    {
        if(have_read_once_)
        {
            //printf("-----have_read_once_:%d\n", sensitivity_);

            Tags::iterator it;
            for(it = tags.begin(); it != tags.end(); ++it)
            {
                (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                (*it)->value.quality	= util::RTData::kOk;
                (*it)->value.pv         = util::IMVariant(sensitivity_);
                if (-1 == sensitivity_)
                {
                    (*it)->value.quality	= util::RTData::kCommDisconnected;
                }
                break;
            }
        }
        else
        {
            //printf("-----open serial\n");
            have_read_once_ = true;
            IMSerialPort *serial = IMSerialPort::getSerial(dev_info_.channel_identity.c_str());
            if(!serial->isOpen())
            {
                if(!serial->open(dev_info_.channel_identity.c_str(), dev_info_.channel_params.c_str()))
                {
                    return util::RTData::kCommDisconnected;
                }
            }

            int re = getModbus(serial, cmd_feature, tags);

            serial->close();

            return re;

        }
    }
    if (-1 == sensitivity_)
    {
        return util::RTData::kCommDisconnected;
    }
	return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{

    IMSerialPort *serial = IMSerialPort::getSerial(dev_info_.channel_identity.c_str());
    if(!serial->isOpen())
    {
        if(!serial->open(dev_info_.channel_identity.c_str(), dev_info_.channel_params.c_str()))
        {
            return util::RTData::kCommDisconnected;
        }
    }

	util::IMBuffer<16> cmd;
	if(!cmd.fromHex(cmd_feature.cmd.c_str()))
	{
        serial->close();
		return util::RTData::kConfigError;
	}

	int pos = 0;
	util::IMBuffer<16> send_buf;//130123 meiqiu
	send_buf[pos] = (unsigned char)util::IMVariant(cmd_feature.addr.c_str()).toInt();
	pos += 1;
	memcpy(&send_buf[pos], &cmd[0], cmd_feature.cmd.size()/2);
	pos += cmd_feature.cmd.size()/2;
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
	else if(0x06 == cmd[0])
	{
        int set_val = util::IMVariant(tag_value_pv.c_str()).toInt();
        if(0 == tag_conf.set_param1.find("*"))
        {
            if(tag_conf.set_param1.size() >= 2)
            {
                set_val = set_val * atoi(tag_conf.set_param1.substr(1).c_str());
            }
        }

		send_buf[pos++] = set_val >> 8;
		send_buf[pos++] = set_val & 0x00FF;
	}
	else if (0x10 == cmd[0])
	{
        int reg_count = (send_buf[pos-1] << 8) + send_buf[pos];
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

	util::IMBuffer<8> recv_buf;
	int read_len = serial->read((char*)&recv_buf[0], 8);
	if(0 == read_len)
	{
		serial->close();
		return util::RTData::kCmdNoResp;
	}
	if(8 != read_len)
	{
        serial->close();
		return util::RTData::kCmdRespError;
	}

	uint16_t recv_crc = ModbusUtil::crc16((uint8_t*)&recv_buf[0], 6);
	if((uint8_t)recv_buf[6] != (recv_crc >> 8) ||		//130123 meiqiu
		(uint8_t)recv_buf[7] != (recv_crc & 0x00FF))	//130123 meiqiu
	{
        serial->close();
		return util::RTData::kCmdRespError;
	}


    sensitivity_ = util::IMVariant(tag_value_pv.c_str()).toInt();
    fd_ = ::open(LEAKAGE_STATUS, O_RDWR);
    lseek(fd_, 0, SEEK_SET);
    ::write(fd_, &sensitivity_, sizeof(int));
    ::close(fd_);
	serial->close();

	return util::RTData::kOk;
}

int  Device::getModbus(IMSerialPort*	serial, const CmdFeature& cmd_feature, Tags& tags)
{
    util::IMBuffer<16> cmd;
    if(!cmd.fromHex(cmd_feature.cmd.c_str()))
    {
        return util::RTData::kConfigError;
    }

    int funcation = cmd[0];

    ModbusPacketSend packet_send;
    packet_send.buildPacket(kRtu, atoi(cmd_feature.addr.c_str()), funcation, (cmd[1]<<8) + cmd[2], (cmd[3]<<8) + cmd[4]);
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
            (*it)->value = Func03Parse(packet_recv, (*it)->conf.data_type,
                        (*it)->conf.get_param1, (*it)->conf.get_param2, (*it)->conf.get_param3);

            if(util::RTData::kOk == (*it)->value.quality)
            {
                if("0x1000" == (*it)->conf.get_param1)
                {
                    sensitivity_ = (*it)->value.pv.toInt();
                    break;
                }

            }

        }
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

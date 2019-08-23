#include "modbus_packet.h"
#include "string.h"

/* Table of CRC values for high-order byte */
static const uint8_t table_crc_hi[] = {
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

/* Table of CRC values for low-order byte */
static const uint8_t table_crc_lo[] = {
	0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
	0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
	0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
	0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
	0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
	0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
	0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
	0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
	0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
	0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
	0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
	0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
	0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
	0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
	0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
	0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
	0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
	0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
	0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
	0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
	0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
	0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
	0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
	0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
	0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
	0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};


uint16_t ModbusUtil::crc16(uint8_t *buffer, uint16_t buffer_length)
{
	uint8_t crc_hi = 0xFF; /* high CRC byte initialized */
	uint8_t crc_lo = 0xFF; /* low CRC byte initialized */
	unsigned int i; /* will index into CRC lookup */

	/* pass through message buffer */
	while (buffer_length--)
	{
		i = crc_hi ^ *buffer++; /* calculate the CRC  */
		crc_hi = crc_lo ^ table_crc_hi[i];
		crc_lo = table_crc_lo[i];
	}

	return (crc_hi << 8 | crc_lo);

}
unsigned short  ModbusUtil::toInt16(uint8_t* byte)
{
    return (byte[0] << 8) + byte[1];
}
unsigned int  ModbusUtil::toInt32(uint8_t* byte, unsigned char byte_order)
{
    unsigned int re = 0;
    switch(byte_order)
    {
    case kLittleEndian:
        re = ((unsigned int)byte[3] << 24) + ((unsigned int)byte[2] << 16) + ((unsigned int)byte[1] << 8) + byte[0];
        break;
    case kBigEndianWordInverted:
        re = ((unsigned int)byte[1] << 24) + ((unsigned int)byte[0] << 16) + ((unsigned int)byte[3] << 8) + byte[2];
        break;
    case kLittleEndianWordInverted:
        re = ((unsigned int)byte[2] << 24) + ((unsigned int)byte[3] << 16) + ((unsigned int)byte[0] << 8) + byte[1];
        break;
    default:
        re = ((unsigned int)byte[0] << 24) + ((unsigned int)byte[1] << 16) + ((unsigned int)byte[2] << 8) + byte[3];
        break;
    }

    return re;
}
uint64_t  ModbusUtil::toInt64(uint8_t* byte, unsigned char byte_order)
{
    uint64_t re = 0;
    switch(byte_order)
    {
    case kLittleEndian:
        re = ((uint64_t)byte[7] << 56) + ((uint64_t)byte[6] << 48) + ((uint64_t)byte[5] << 40) + ((uint64_t)byte[4] << 32) +
               ((uint64_t)byte[3] << 24) + ((uint64_t)byte[2] << 16) + ((uint64_t)byte[1] << 8) + byte[0];

        break;
    case kBigEndianWordInverted:
       re = ((uint64_t)byte[1] << 56) + ((uint64_t)byte[0] << 48) + ((uint64_t)byte[3] << 40) + ((uint64_t)byte[2] << 32) +
               ((uint64_t)byte[5] << 24) + ((uint64_t)byte[4] << 16) + ((uint64_t)byte[7] << 8) + byte[6];
        break;
    case kLittleEndianWordInverted:
        re = ((uint64_t)byte[6] << 56) + ((uint64_t)byte[7] << 48) + ((uint64_t)byte[4] << 40) + ((uint64_t)byte[5] << 32) +
               ((uint64_t)byte[2] << 24) + ((uint64_t)byte[3] << 16) + ((uint64_t)byte[0] << 8) + byte[1];
        break;
    default:
         re = ((uint64_t)byte[0] << 56) + ((uint64_t)byte[1] << 48) + ((uint64_t)byte[2] << 40) + ((uint64_t)byte[3] << 32) +
               ((uint64_t)byte[4] << 24) + ((uint64_t)byte[5] << 16) + ((uint64_t)byte[6] << 8) + byte[7];
        break;
    }
    return re;
}
float    ModbusUtil::toFloat(uint8_t* byte, unsigned char byte_order)
{
    float re = 0.0f;
    char tmp[4] = {0};
    switch(byte_order)
    {
    case kLittleEndian:
        for(int i=0; i<4; i++)
        {
            tmp[i] = byte[i];
        }
        break;
    case kBigEndianWordInverted:
        tmp[0] = byte[2];
        tmp[1] = byte[3];
        tmp[2] = byte[0];
        tmp[3] = byte[1];
        break;
    case kLittleEndianWordInverted:
        tmp[0] = byte[1];
        tmp[1] = byte[0];
        tmp[2] = byte[3];
        tmp[3] = byte[2];
        break;
    default:
        for(int i=0; i<4; i++)
        {
            tmp[i] = byte[3-i];
        }
        break;
    }

    memcpy(&re, tmp, 4);
    return re;
}
double   ModbusUtil::toDouble(uint8_t* byte, unsigned char byte_order)
{
    double re = 0.0f;
    char tmp[8] = {0};
    switch(byte_order)
    {
    case kLittleEndian:
        for(int i=0; i<8; i++)
        {
            tmp[i] = byte[i];
        }
        break;
    case kBigEndianWordInverted:
        tmp[0] = byte[6];
        tmp[1] = byte[7];
        tmp[2] = byte[4];
        tmp[3] = byte[5];
        tmp[4] = byte[2];
        tmp[5] = byte[3];
        tmp[6] = byte[0];
        tmp[7] = byte[1];
        break;
    case kLittleEndianWordInverted:
        tmp[0] = byte[1];
        tmp[1] = byte[0];
        tmp[2] = byte[3];
        tmp[3] = byte[2];
        tmp[4] = byte[5];
        tmp[5] = byte[4];
        tmp[6] = byte[7];
        tmp[7] = byte[6];
        break;
    default:
        for(int i=0; i<8; i++)
        {
            tmp[i] = byte[7-i];
        }
        break;
    }

    memcpy(&re, tmp, 8);
    return re;
}


void ModbusPacketSend::buildPacket(PacketType type, int device_addr, int function , int data_addr, int data_count)
{
	type_ = type;
	function_	= function;
	data_addr_	= data_addr;
	data_count_	= data_count;

	if(kRtu == type_)
	{
		send_buf_[0] = device_addr;
		send_buf_[1] = function;
		send_buf_[2] = data_addr >> 8;
		send_buf_[3] = data_addr & 0x00FF;
		send_buf_[4] = data_count >> 8;
		send_buf_[5] = data_count & 0x00FF;

		uint16_t crc = ModbusUtil::crc16((uint8_t*)send_buf_, 6);
		send_buf_[6] = crc >> 8;
		send_buf_[7] = crc & 0x00FF;
	}
	else
	{
		static uint16_t t_id = 0;
		/* Transaction ID */
		if (t_id < 65535)
			t_id++;
		else
			t_id = 0;
		send_buf_[0] = t_id >> 8;
		send_buf_[1] = t_id & 0x00ff;

		/* Protocol Modbus */
		send_buf_[2] = 0;
		send_buf_[3] = 0;

		send_buf_[4] = 0;
		send_buf_[5] = 6;

		send_buf_[6] = device_addr;
		send_buf_[7] = function;
		send_buf_[8] = data_addr >> 8;
		send_buf_[9] = data_addr & 0x00ff;
		send_buf_[10] = data_count >> 8;
		send_buf_[11] = data_count & 0x00ff;
	}

}

const char* ModbusPacketSend::sendData() const
{
	return send_buf_;
}
const int ModbusPacketSend::sendByte() const
{
	if(kRtu == type_)
	{
		return 8;
	}
	else
	{
		return 12;
	}

}

void ModbusPacketSend::setTransId(unsigned short trans_id)
{
    if(kTcp == type_)
    {
        send_buf_[0] = trans_id >> 8;
		send_buf_[1] = trans_id & 0x00ff;
    }
}

ModbusPacketRecv::ModbusPacketRecv(const ModbusPacketSend& packet_send):packet_send_(packet_send)
{
	memset(recv_buf_,0,kRecvBufByte);
	recv_data_byte_ = 0;
	if(kRtu == packet_send_.type_)
	{
		//recv_data_offset_ = 3;
		recv_data_offset_ = 4;
	}
	else
	{
		recv_data_offset_ = 9;
	}
}
ModbusPacketRecv::~ModbusPacketRecv()
{
	memset(recv_buf_,0,kRecvBufByte);
	recv_data_byte_ = 0;
}

char* ModbusPacketRecv::preRecv()
{
	return recv_buf_;
}
int	ModbusPacketRecv::preRecvLen()
{
	if(kRtu == packet_send_.type_)
	{
		//return 3;
		return 4;
	}
	else
	{
		return 9;
	}

}
char* ModbusPacketRecv::recv()
{
	if(kRtu == packet_send_.type_)
	{
		//return &recv_buf_[3];
		return &recv_buf_[4];
	}
	else
	{
		return &recv_buf_[9];
	}
}
int ModbusPacketRecv::recvLen()
{
	if(kRtu == packet_send_.type_)
	{
		//recv_data_byte_ = (unsigned char)recv_buf_[2];
		recv_data_byte_ = (unsigned char)recv_buf_[3];
		return  recv_data_byte_ + 2;
	}
	else
	{
		recv_data_byte_ = (unsigned char)recv_buf_[8];
		return recv_data_byte_;
	}

}

bool ModbusPacketRecv::isCmdRespOk() const
{
	if(1 == packet_send_.function_ || 2 == packet_send_.function_)
	{
		int real_len = packet_send_.data_count_/8;
		if(0 != packet_send_.data_count_%8)
		{
			real_len += 1;
		}

		if(real_len == (unsigned char)recv_data_byte_)
		{
			return true;
		}
	}
	else if(3 == packet_send_.function_ || 4 == packet_send_.function_)
	{
		if(packet_send_.data_count_*2 == (unsigned char)recv_data_byte_)
		{
			return true;
		}
	}

	return false;
}

bool ModbusPacketRecv::isCRCOk() const
{
	if(kRtu == packet_send_.type_)
	{
		uint16_t crc = ModbusUtil::crc16((uint8_t*)&recv_buf_[1], recv_data_byte_ + 3);

        if((uint8_t)recv_buf_[recv_data_byte_+4] == (crc >> 8) &&
            (uint8_t)recv_buf_[recv_data_byte_+5] == (crc & 0x00FF))
        {
            return true;
        }

        return false;
	}
	else
	{
		if(packet_send_.send_buf_[7] != recv_buf_[7]) // 功能码不相同
		{
			return false;
		}
		return true;
	}
}

bool ModbusPacketRecv::isFunCodeOk() const
{
    unsigned char fun = recv_buf_[recv_data_offset_-2];
    if( 0x80 == (fun & 0x80) )
    {
        return false;
    }
    return true;
}
bool ModbusPacketRecv::isTransIdOk() const
{
    if(packet_send_.send_buf_[0] != recv_buf_[1] || packet_send_.send_buf_[1] != recv_buf_[2])
    {
        return false;
    }

    return true;
}


uint16_t ModbusPacketRecv::getRegister(int data_attr, bool* is_ok)
{
	int data_byte = 2;
	int	pos = recv_data_offset_ + (data_attr - packet_send_.data_addr_)*2;
	if(0 > pos || (pos-recv_data_offset_ + data_byte) > recv_data_byte_)
	{
		if(0 != is_ok)
		{
			*is_ok = false;
		}
		return 0;
	}

	if(0 != is_ok)
	{
		*is_ok = true;
	}

	union MyUnion
	{
		uint16_t		i;
		unsigned char	by[2];
	}val;

	val.by[0] = recv_buf_[pos+1];
	val.by[1] = recv_buf_[pos];

	return val.i;
}
bool ModbusPacketRecv::getRegister(int data_attr, unsigned char* val, int val_size)
{
	int pos = recv_data_offset_ + (data_attr - packet_send_.data_addr_)*2;
    if(0 > pos || (pos-recv_data_offset_ + val_size) > recv_data_byte_)
	{
		return false;
	}

	memcpy(val, &recv_buf_[pos], val_size);

	return true;
}

bool ModbusPacketRecv::getStatusDigit(int data_attr, bool* is_ok)
{
	int	pos = recv_data_offset_ + (data_attr - packet_send_.data_addr_)/8;
	if(0 > pos || (pos-recv_data_offset_  > recv_data_byte_) )
	{
		if(0 != is_ok)
		{
			*is_ok = false;
		}
		return false;
	}
	//int bit = 7 - (data_attr - packet_send_.data_addr_)%8;
	int bit = (data_attr - packet_send_.data_addr_)%8;

	if(0 != is_ok)
	{
		*is_ok = true;
	}
	return  (((unsigned char)recv_buf_[pos] >> bit) & 1);
}



int ModbusSetCmdPacketSend::buildPacket(PacketType type, int device_addr, int function, int data_addr, uint8_t* val, int val_bytes)
{
    send_bytes_ = 0;

    type_       = type;
    function_   = function;


    int pos = 0;

    if(kTcp == type_)
    {
		/* Transaction ID */
		send_buf_[0] = 0;
		send_buf_[1] = 0;

		/* Protocol Modbus */
		send_buf_[2] = 0;
		send_buf_[3] = 0;

		// 数据长度 后面再赋值
		send_buf_[4] = 0;
		send_buf_[5] = 0;

		pos += 6;
    }

    send_buf_[pos++] = device_addr;
    send_buf_[pos++] = function;
    send_buf_[pos++] = data_addr >> 8;
    send_buf_[pos++] = data_addr & 0x00FF;

    if(0x05 == function_ || 0x06 == function_)  // 单个寄存器
    {
        send_buf_[pos++] = val[0];
        send_buf_[pos++] = val[1];
    }
    else if (0x10 == function_)  // 多个寄存器
    {
        int reg_count = val_bytes/2;

        // 寄存器数量
        send_buf_[pos++] = reg_count >> 8;
        send_buf_[pos++] = reg_count & 0x00FF;
        // 寄存器个数
        send_buf_[pos++] = reg_count*2;

        for(int i=0; i<val_bytes; i++)
        {
             send_buf_[pos++] = val[i];
        }

    }
    if(kTcp == type_)
    {
        unsigned short pack_body_len = pos - 6;
        send_buf_[4] = pack_body_len >> 8;
		send_buf_[5] = pack_body_len & 0x00FF;
    }
    else
    {
        uint16_t crc = ModbusUtil::crc16((uint8_t*)&send_buf_[0], pos);
        send_buf_[pos++] = crc >> 8;
        send_buf_[pos++] = crc & 0x00FF;
    }

    send_bytes_ = pos;

    return 0;
}
const char* ModbusSetCmdPacketSend::sendData() const
{
    return send_buf_;
}
const int   ModbusSetCmdPacketSend::sendByte() const
{
    return send_bytes_;
}

void ModbusSetCmdPacketSend::setTransId(unsigned short trans_id)
{
    if(kTcp == type_)
    {
        send_buf_[0] = trans_id >> 8;
		send_buf_[1] = trans_id & 0x00ff;
    }
}



ModbusSetCmdPacketRecv::ModbusSetCmdPacketRecv(const ModbusSetCmdPacketSend& packet_send):packet_send_(packet_send)
{
    memset(recv_buf_,0,kRecvBufByte);
}

ModbusSetCmdPacketRecv::~ModbusSetCmdPacketRecv()
{
    memset(recv_buf_,0,kRecvBufByte);
}

char* ModbusSetCmdPacketRecv::preRecv()
{
	return recv_buf_;
}

int	ModbusSetCmdPacketRecv::preRecvLen()
{
	if(kRtu == packet_send_.type_)
	{
		return 3;
	}
	else
	{
		return 9;
	}

}

char* ModbusSetCmdPacketRecv::recv()
{
    if(kRtu == packet_send_.type_)
	{
		return &recv_buf_[3];
	}
	else
	{
		return &recv_buf_[9];
	}
}
int	ModbusSetCmdPacketRecv::recvLen()
{
    if(kRtu == packet_send_.type_)
	{
		return  1 + 2 + 2;
	}
	else
	{

		return 1 + 2;
	}
}

bool ModbusSetCmdPacketRecv::isCRCOk() const
{
	if(kRtu == packet_send_.type_)
	{
		uint16_t crc = ModbusUtil::crc16((uint8_t*)recv_buf_, 6);
			if((uint8_t)recv_buf_[6] == (crc >> 8) &&
				(uint8_t)recv_buf_[7] == (crc & 0x00FF))
			{
				return true;
			}

			return false;
	}
	else
	{
		if(packet_send_.send_buf_[7] != recv_buf_[7]) // 功能码不相同
		{
			return false;
		}
		return true;
	}
}

bool ModbusSetCmdPacketRecv::isFunCodeOk() const
{
    int err_fun_pos = 0;
    if(kRtu == packet_send_.type_)
    {
        err_fun_pos = 1;
    }
    else
    {
        err_fun_pos = 7;
    }

    unsigned char fun = recv_buf_[err_fun_pos];
    if( 0x80 == (fun & 0x80)  || 0x90 == (fun & 0x90))
    {
        return false;
    }
    return true;
}

bool ModbusSetCmdPacketRecv::isTransIdOk() const
{
    if(packet_send_.send_buf_[0] != recv_buf_[0] || packet_send_.send_buf_[1] != recv_buf_[1])
    {
        return false;
    }

    return true;
}



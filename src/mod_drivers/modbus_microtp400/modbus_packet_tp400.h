#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "imutil/util_imbufer.h"

enum PacketType
{
	kRtu,
	kTcp
};

class ModbusPacketSend
{
public:
	void		buildPacket(PacketType type, int device_addr, int function , int data_addr, int data_count);
	const char* sendData()const;
	const int   sendByte() const;

private:
	friend class ModbusPacketRecv;
	int			function_;
    int			data_addr_;
	int			data_count_;
private:
	static const int	kMaxSendBufByte = 16;
	char				send_buf_[kMaxSendBufByte];
	PacketType	type_;

};

class ModbusPacketRecv
{
public:
	ModbusPacketRecv(const ModbusPacketSend& packet_send);
	char*		preRecv();
	int			preRecvLen() ;
	char*		recv();
	int			recvLen();

	bool        isTransIdOk() const;
	bool        isFunCodeOk() const;
	bool		isCmdRespOk() const;
	bool		isCRCOk() const;

	uint16_t	getRegister(int data_attr, bool* is_ok = 0); // 该函数废弃
	bool		getRegister(int data_attr, unsigned char* val, int val_size);
	//float		getRegisterAnalog(int data_attr, int data_byte, bool* is_ok = 0);	// 从RTU寄存器中取模拟量
	//bool		getRegisterDigit(int data_attr, int bit, bool* is_ok = 0);			// 从RTU寄存器中取数字量 bit表示取第几位
	bool		getStatusDigit(int data_attr, bool* is_ok = 0);						// 从RTU状态奇中取数字量


	// 根据接受buf偏移量获取数据
	void		getRecvBuf(int pos, int bytes, uint8_t* data);
private:
	const ModbusPacketSend&	packet_send_;
	static const int		kRecvBufByte = 1 + 1 + 1 + 255 + 2;	// 地址位+功能码位+数据长度位 + 最大数据长度 + 校验长度
	char					recv_buf_[kRecvBufByte];
	int						recv_data_byte_;					// 数据位的长度
	int						recv_data_offset_;				   //  数据在协议包中的起始位置
};


class ModbusUtil
{
public:
	static uint16_t crc16(uint8_t *buffer, uint16_t buffer_length);
	static float    toFloat(uint8_t data, bool unsign = true);
	static float    toFloat(uint16_t data, bool unsign = true);
	static float    toFloat(uint32_t data, bool unsign = true);
	static float    toFloat(uint64_t data, bool unsign = true);
	static bool     toBit(uint8_t data, char bit_pos);
	static bool     toBit(uint16_t data, char bit_pos);
	static bool     toBit(uint32_t data, char bit_pos);
	static bool     toBit(uint64_t data, char bit_pos);

};

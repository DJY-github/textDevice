#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum PacketType
{
	kRtu,
	kTcp
};

enum ByteOrder
{
    kBigEndian                  = 0,   // 大端模式， 高位在前 ABCD
    kLittleEndian               = 1,   // 小端模式，低位在前 DCBA
    kBigEndianWordInverted      = 2,   // 大端模式，但字反序 BADC
    kLittleEndianWordInverted   = 3    // 小端模式，但字反序 CDAB
};

class ModbusPacketSend
{
public:
	void		buildPacket(PacketType type, int device_addr, int function , int data_addr, int data_count);
	const char* sendData()const;
	const int   sendByte() const;

	void        setTransId(unsigned short trans_id);

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
	~ModbusPacketRecv();
	char*		preRecv();
	int			preRecvLen() ;
	char*		recv();
	int			recvLen();

	bool        isTransIdOk() const;
	bool        isFunCodeOk() const;
	bool		isCmdRespOk() const;
	bool		isCRCOk() const;

	uint16_t	getRegister(int data_attr, bool* is_ok = 0);
	bool		getRegister(int data_attr, unsigned char* val, int val_size);
	bool		getStatusDigit(int data_attr, bool* is_ok = 0);



	void		getRecvBuf(int pos, int bytes, uint8_t* data);
private:
	const ModbusPacketSend&	packet_send_;
	static const int		kRecvBufByte = 1 + 1 + 1 + 255 + 2 + 7;
	char					recv_buf_[kRecvBufByte];
	int						recv_data_byte_;
	int						recv_data_offset_;
};


class ModbusSetCmdPacketSend
{
public:
	int		buildPacket(PacketType type, int device_addr, int function, int data_addr, uint8_t* val, int val_bytes);
	const char* sendData()const;
	const int   sendByte() const;

	void        setTransId(unsigned short trans_id);

private:
	friend class ModbusSetCmdPacketRecv;
	int			function_;
private:
	static const int	kMaxSendBufByte = 512;
	char				send_buf_[kMaxSendBufByte];
	PacketType	type_;
    int         send_bytes_;

};

class ModbusSetCmdPacketRecv
{
public:
	ModbusSetCmdPacketRecv(const ModbusSetCmdPacketSend& packet_send);
	~ModbusSetCmdPacketRecv();
	char*		preRecv();
	int			preRecvLen() ;
	char*		recv();
	int			recvLen();

	bool        isTransIdOk() const;
	bool        isFunCodeOk() const;
	bool		isCRCOk() const;
private:
	const ModbusSetCmdPacketSend&	packet_send_;
	static const int		kRecvBufByte = 32;
	char					recv_buf_[kRecvBufByte];
};


class ModbusUtil
{
public:
	static uint16_t crc16(uint8_t *buffer, uint16_t buffer_length);
    static unsigned short  toInt16(uint8_t* byte);
    static unsigned int    toInt32(uint8_t* byte, unsigned char byte_order);
    static uint64_t        toInt64(uint8_t* byte, unsigned char byte_order);
	static float           toFloat(uint8_t* byte, unsigned char byte_order);
    static double          toDouble(uint8_t* byte, unsigned char byte_order);

};

#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
	~ModbusPacketRecv();
	char*		preRecv();
	int			preRecvLen() ;
	char*		recv();
	int			recvLen();

	bool        isTransIdOk() const;
	bool        isFunCodeOk() const;
	bool		isCmdRespOk() const;
	bool		isCRCOk() const;

	uint16_t	getRegister(int data_attr, bool* is_ok = 0); // �ú������
	bool		getRegister(int data_attr, unsigned char* val, int val_size);
	//float		getRegisterAnalog(int data_attr, int data_byte, bool* is_ok = 0);	// ��RTU�Ĵ�����ȡģ����
	//bool		getRegisterDigit(int data_attr, int bit, bool* is_ok = 0);			// ��RTU�Ĵ�����ȡ������ bit��ʾȡ�ڼ�λ
	bool		getStatusDigit(int data_attr, bool* is_ok = 0);						// ��RTU״̬����ȡ������


	// ��ݽ���bufƫ������ȡ���
	void		getRecvBuf(int pos, int bytes, uint8_t* data);
private:
	const ModbusPacketSend&	packet_send_;
	static const int		kRecvBufByte = 1 + 1 + 1 + 255 + 2;	// ��ַλ+������λ+��ݳ���λ + �����ݳ��� + У�鳤��
	char					recv_buf_[kRecvBufByte];
	int						recv_data_byte_;					// ���λ�ĳ���
	int						recv_data_offset_;				   //  �����Э����е���ʼλ��
};


class ModbusUtil
{
public:
	static uint16_t crc16(uint8_t *buffer, uint16_t buffer_length);
	static float    toFloat(uint8_t* byte,  bool is_inverse = false);

};

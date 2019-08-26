#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


class ZchwPacketSend
{
public:
    // 目的地址，源地址，目的进程号，源进程号，数据的长度，信号码，消息参数,消息长度
	void		buildPacket(int des_addr,int origin_addr,int des_pro_num,int origin_pro_num, int data_count,unsigned char* signal,unsigned char *packet,int packet_len);
	const char* sendData()const;
	const int   sendByte() const;
    static const int	kMaxSendBufByte = 64;
    char				send_buf_[kMaxSendBufByte];
private:

	int			cmd_len_;


private:


	 char           end_str_;
     char           start_str_;
     char       signal_[2];

};



class XmodemUtil
{
public:
	static uint16_t crc16_xmodem(uint8_t *buffer, uint16_t buffer_length);
};




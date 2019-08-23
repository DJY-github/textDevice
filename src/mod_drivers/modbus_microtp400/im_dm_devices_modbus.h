#pragma once
#include "../../include/driver_module.h"
#include "imutil/util_serialport.h"

#include "modbus_packet_tp400.h"

using namespace drv;
using namespace util::serial;
class Device : public IDevice
{
public:
	Device(void);
	~Device(void);

	 int			open(const DeviceConf& device_info);
	 int			close();
	 int			get(const CmdFeature& cmd_feature, Tags& tags);
	 int			set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv);
private:
	 IMSerialPort*	serial;
	 im_string chname;

	 /*
	 param1: 寄存器地址
	 param2: data_type为kAnalog时：
				param2=""						-> 取2字节，无符号
				param2="unsigned short" 或"us"	-> 取2字节，无符号
				param2="short" 或 "s"			-> 取2字节，有符号
				param2="unsigned int" 或 "ui"   -> 取4字节，无符号
				param2="int"  或 "i"			-> 取4字节，有符号

	          data_type为kDigit时:
			   param2=""                        ->取寄存器值作为bool量
			   param2="0"						->取寄存器值的第1位作为bool量, 高位在前
			   param2="1"						->取第2位
			   ....
			   param2="15"                      ->取第16位

	 param3: 运算函数，目前支持abs(),可填多个，";"号隔开
			  param3="abs()"					->取绝对值, 往往配合有符号值使用
	 */
	 util::RTValue Func03Parse(ModbusPacketRecv& packet_recv, TagValue::ValType data_type,
					const libistring& param1, const libistring& param2, const libistring& param3);


	 bool	is_modbus_tcp_;

	 unsigned int crc_err_cnt;


};


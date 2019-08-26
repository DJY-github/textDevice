#pragma once
#include "../../include/driver_module.h"
#include "imutil/util_serialport.h"

#include "modbus_packet.h"

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

	 /*
	 param1: �Ĵ�����ַ
	 param2: data_typeΪkAnalogʱ��
				param2=""						-> ȡ2�ֽڣ��޷��
				param2="unsigned short" ��"us"	-> ȡ2�ֽڣ��޷��
				param2="short" �� "s"			-> ȡ2�ֽڣ��з��
				param2="unsigned int" �� "ui"   -> ȡ4�ֽڣ��޷��
				param2="int"  �� "i"			-> ȡ4�ֽڣ��з��

	          data_typeΪkDigitʱ:
			   param2=""                        ->ȡ�Ĵ���ֵ��Ϊbool��
			   param2="0"						->ȡ�Ĵ���ֵ�ĵ�1λ��Ϊbool��, ��λ��ǰ
			   param2="1"						->ȡ��2λ
			   ....
			   param2="15"                      ->ȡ��16λ

	 param3: ���㺯��Ŀǰ֧��abs(),��������";"�Ÿ���
			  param3="abs()"					->ȡ���ֵ, ��������з��ֵʹ��
	 */
	 util::RTValue Func03Parse(ModbusPacketRecv& packet_recv, TagValue::ValType data_type,
					const libistring& param1, const libistring& param2, const libistring& param3);


	 bool	is_modbus_tcp_;


};


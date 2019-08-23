#pragma once
#include "driver_module.h"
#include "util_serialport.h"

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
     int            openGPIO();
     void           closeGPIO();
     int            getModbus(IMSerialPort*	serial, const CmdFeature& cmd_feature, Tags& tags);

     int            readSensitivity();

	 util::RTValue Func03Parse(ModbusPacketRecv& packet_recv, TagValue::ValType data_type,
					const libistring& param1, const libistring& param2, const libistring& param3);


	 int            gpio_value_fd_;
	 static  int    sensitivity_;
	 int            fd_;
	 static bool    have_read_once_;

	 DeviceConf     dev_info_;

};


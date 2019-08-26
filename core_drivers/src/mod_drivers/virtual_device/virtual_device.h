#pragma once
#include "driver_module.h"
#include "util_serialport.h"

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
    bool            reopen();
    IMSerialPort*	serial;
     int            fd_;
     Int64      analog;
     int            status;
     int            cycle;
     int            seconds;
     int            minutes;
     int            status_min;

     int            last_second_;
};


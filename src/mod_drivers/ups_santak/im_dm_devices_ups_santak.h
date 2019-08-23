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
    bool            update_dofile(int cmd, int do_no);
private:
    IMSerialPort*	serial;
     int            fd_;
     char           end_str_;
     char           start_str_;
     bool           check_start_;
};


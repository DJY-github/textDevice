#pragma once
#include "driver_module.h"
#include "ycapi.h"

using namespace drv;
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
     int            fd_;
    Ycapi           ibox_dio;
    int             do_status;
};


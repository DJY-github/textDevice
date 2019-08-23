#pragma once
#include "driver_module.h"


#define NUM_SYS_VBS	6
using namespace std;
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

private:
    std::string  channel_;
    std::string  device_id_;
};



#pragma once
#include "driver_module.h"

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <math.h>
#include <list>

using namespace drv;
using namespace std;

typedef struct IP_DATE{
    float   delay_time;      //延时
    int     timestamp;       //时间戳
    int     net_status;      //网络状态
} ip_date;

typedef list<ip_date> list_date;

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
    void        get_comm_to_buff(string &cmd, char* buffer);
    void        buff_to_string(char *buffer, string &str_buff);
    void        string_to_float(string &str,float &ftime);

    void        within60(list_date &date);
    float       handle_pack_lost(list_date &date);
    float       handle_delay(list_date& date);
private:
    im_string ip_;

    list_date list_ip_status_;
    float pack_lost_;
    float ave_delay_;
};


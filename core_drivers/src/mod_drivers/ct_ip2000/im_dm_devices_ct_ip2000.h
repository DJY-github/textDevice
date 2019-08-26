#pragma once
#include "driver_module.h"
#include "serial_port.h"
#include <string>
#include <map>

//using namespace std;

#define MAX_BUFF_LEN 1024

using namespace drv;

//using namespace util::serial;
class Device : public IDevice
{
public:
	Device(void);
	~Device(void);

	struct Arm_attr{
        int arm_num;
        int ae_status;
        int ae_bypass_status;
        int arm_loss;
	};

    int			open(const DeviceConf& device_info);
    int			close();
    int			get(const CmdFeature& cmd_feature, Tags& tags);
    int			set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv);
    bool            update_dofile(int cmd, int do_no);
private:
    PortUDP*	serial;
    int            fd_;
    int            subsys_;
    int            arm_num_;
    int            recv_len_;
    char           recv_buff_[MAX_BUFF_LEN];

    bool           parase_recv_len();
    void           parase_subsys();
    bool           parase_recv_status();
    bool           parase_arm_map();
    void           parase_ae_status();
    void           try_recovery_normal();
    void           init_all_arm_zone();

    bool    is_ae_;
};


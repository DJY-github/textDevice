#pragma once
#include "driver_module.h"
#include "util_serialport.h"
#include "util_singleton.h"
#include <sys/socket.h>
#include "snmp_pp/snmp_pp.h"



#define NUM_SYS_VBS	6
#define coldStart	"1.3.6.1.6.3.1.1.4.3.0.1"
using namespace std;
using namespace drv;
using namespace util::serial;
using namespace Snmp_pp;

typedef     vector< vector< pair<int,string> > >    vec_table;


typedef struct _SnmpConf
{
    im_string host;
    int       port;
    im_string ver;
    im_string read_community;
    im_string write_community;
    im_string user;
    im_string auth_proto;
    im_string auth_pwd;
    im_string pri_proto;
    im_string pri_pwd;
}SnmpConf;


class Device : public IDevice
{
public:
    Device(void);
    ~Device(void);

    int			open(const DeviceConf& device_info);
    int			close();
    int			get(const CmdFeature& cmd_feature, Tags& tags);
    int			set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const im_string& tag_value_pv);
    snmp_version    version_;


private:
    DeviceConf  device_info_;
    im_string   ip_;
    bool        is_open_;
    im_string   community_;
    CTarget*    target_;
    UTarget*    utarget_;
    v3MP*       v3mp_;
    int         security_level_;
    SnmpConf    conf_;
    int         fd_;

    bool        get_version_attr(const DeviceConf& device_info);
    bool        Get_Table(Oid oid,vec_table & res);
    int         get_cpu_used();
    int         get_mem_used();
    int         get_Disk_used();
};


class SnmpObj : public util::Singleton <SnmpObj>
{
public:
    SnmpObj();
    ~SnmpObj();
    Snmp* snmp() {return snmp_;}
private:
    int         status_;
    Snmp        *snmp_;
};

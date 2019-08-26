#pragma once
#include "driver_module.h"
#include "util_serialport.h"
#include "util_singleton.h"
#include <sys/socket.h>
#include "snmp_pp/snmp_pp.h"

using namespace drv;
using namespace util::serial;
using namespace Snmp_pp;

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
    bool            update_dofile(int cmd, int do_no);
    snmp_version    version_;

private:
    int             parseNumberVal(const util::IMVariant& val, Tags& tags);
    int             parseStringVal(const im_string& strval, Tags& tags);

private:
    DeviceConf device_info_;
    im_string       community_;
    bool             get_version_attr(const DeviceConf& device_info);
    int             fd_;
    bool             is_open_;
    CTarget*         target_;
    UTarget*         utarget_;
    v3MP*            v3mp_;
    int             security_level_;
    SnmpConf       conf_;
};

class SnmpObj : public util::Singleton<SnmpObj>
{
public:
    SnmpObj();
    ~SnmpObj();
    Snmp* snmp() {return snmp_;}
private:
    int     status_;
    Snmp    *snmp_;

};

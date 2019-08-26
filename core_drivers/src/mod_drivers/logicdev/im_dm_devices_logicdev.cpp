#include "im_dm_devices_logicdev.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "util_imvariant.h"
#include "util_datetime.h"
#include "util_datastruct.h"
#include "rtdb_lib_api.h"
#include "restful_lib_api.h"


Device::Device(void)
{
}


Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{
	return util::RTData::kOk;
}

int Device::close()
{

	return util::RTData::kOk;
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {
        util::RTValue val;
        if(0 == rtdb::get((*it)->conf.get_param2, val))
        {
            (*it)->value.quality	= util::RTData::kOk;
            (*it)->value.pv         = val.pv;
            (*it)->value.timestamp	= val.timestamp;
        }
        else
        {
            (*it)->value.quality	= util::RTData::kCmdNoResp;
            (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
        }

    }

    return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
    restful::Request		req_ctl;
    req_ctl.uri_path_	= "/ctl/"  + tag_conf.set_cmd;
    req_ctl.in_content_.setIdent("val", tag_value_pv);

    util::IMData resp;
    if(restful::kOk  != restful::put(req_ctl, resp))
    {
        return util::RTData::kCmdNoResp;
    }

	return util::RTData::kOk;
}


DRV_EXPORT const im_char*  getDrvVer()
{
	return "V.01";
}
DRV_EXPORT int initDrv(const im_char* dm_ver)
{
	return util::RTData::kOk;
}
DRV_EXPORT int uninitDrv()
{
	return util::RTData::kOk;
}
DRV_EXPORT IDevice* createDevice()
{
	return new Device;
}
DRV_EXPORT int releaseDevice(IDevice* device)
{
	delete device;
	return util::RTData::kOk;
}

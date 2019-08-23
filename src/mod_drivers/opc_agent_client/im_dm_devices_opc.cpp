#include "im_dm_devices_opc.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "util_imvariant.h"
#include "util_datetime.h"
#include "util_datastruct.h"
#include "util_commonfun.h"
#include "json/json.h"

#include "opc_agent_client.h"

#include <sstream>
#include <iostream>

using namespace std;
#define RESP_BUFF_LEN       256
#define RESP_BUFF_SIZE      256
#define MAXBUFFERSIZE       400

Device::Device(void)
{
}

Device::~Device(void)
{

}

int Device::open( const DeviceConf& device_info)
{
    device_id_ = device_info.id;
    channel_    = device_info.channel_identity;
    if(!OpcAgentClientMgr::instance()->agent(channel_)->connect())
    {
            return util::RTData::kCommDisconnected;
    }

    return util::RTData::kOk;
}

int Device::close()
{
    OpcAgentClientMgr::instance()->agent(channel_)->asyncRemoveSubscribeItems(device_id_);
    OpcAgentClientMgr::instance()->agent(channel_)->disconnect();
    return util::RTData::kOk;
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    // 与Agent Socket中断 或者 Agent端与OPC Server通讯中断
    if(!OpcAgentClientMgr::instance()->agent(channel_)->isConnected() )
	{
			return util::RTData::kCommDisconnected;
	}

    if(DeviceSubscribeInfo::kUnSubscribe ==
            OpcAgentClientMgr::instance()->agent(channel_)->subscribeStatus(device_id_))
    {
            std::vector<std::string> vec_items;
            vec_items.resize(tags.size());
            int i = 0;
            for (Tags::iterator it = tags.begin(); it != tags.end(); ++it)
            {
                    vec_items[i++] =  util::replaceString((*it)->conf.get_param1, "*", cmd_feature.auxiliary.c_str());
            }
            OpcAgentClientMgr::instance()->agent(channel_)->asyncSubscribeItems(device_id_,  vec_items);
            printf("-----Device :%s asyncSubscribeItems\n", device_id_.c_str());
    }

    if(!OpcAgentClientMgr::instance()->agent(channel_)->checkAgentOpcData())
    {
            return util::RTData::kCommDisconnected;
    }

    for (Tags::iterator  it = tags.begin(); it != tags.end(); ++it)
    {
        im_string item = util::replaceString((*it)->conf.get_param1, "*", cmd_feature.auxiliary.c_str());

        OpcVal opcval;
         if(OpcAgentClientMgr::instance()->agent(channel_)->getItemVal(item,  opcval))
         {
                (*it)->value.pv = util::IMVariant(opcval.val.c_str(), opcval.val.size());
                (*it)->value.quality	= opcval.qua;
         }
         else
         {
               (*it)->value.quality	= util::RTData::kCmdNoResp;
         }

        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
    }

    return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
    return util::RTData::kDisable;
}

DRV_EXPORT const im_char*  getDrvVer()
{
    return "V.01";
}
DRV_EXPORT int initDrv(const im_char* dm_ver)
{
    OpcAgentClientMgr::instance();
    return util::RTData::kOk;
}
DRV_EXPORT int uninitDrv()
{
    OpcAgentClientMgr::instance()->removeAll();
    OpcAgentClientMgr::release();

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

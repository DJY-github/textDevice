#pragma once
#include "source_config.h"
#include "util_singleton.h"
#include "util_thread.h"
#include "socket_connector.h"
#include "json/value.h"
#include "util_datastruct.h"
#include "util_thread.h"
#include "util_timer.h"

#define FUNC_SUBSCRIBE_ITEM "subitem"
#define FUNC_REMOVE_ITEM "removeitem"
//#define FUNC_ASYNC_READALL_ITEM "sync_readall"
#define FUNC_ITEM_VAL "itemval"
#define FUNC_REFRESH_ITEM "refresh"

#define CHECK_AGENT_DATA_TIME_SECONDS 30
#define CHECK_AGENT_DATA_TIMEOUT ( CHECK_AGENT_DATA_TIME_SECONDS*3)

typedef struct _DeviceSubscribeInfo
{
     enum SubscribeStatus
     {
         kUnSubscribe = 0,
         kSubscribing   = 1,
         kSubscribed    = 2
     };
     int                                            subscribe_status;   // 订阅状态
     time_t                                     tm;                             //  订阅时间
     std::vector<im_string>     vec_items;

    _DeviceSubscribeInfo()
    {
            subscribe_status = kUnSubscribe;
            tm = time(NULL);
    }

}DeviceSubscribeInfo;

typedef struct _OpcVal
{
    im_string                               val;
    util::RTData::Quality         qua;
    time_t                                    tm;
}OpcVal;

class OpcAgentClient
{
public:
    // 枚举代码需与Agent端保持一致
    enum OpcErrCode
    {
        kOpcOk                                  = 0,
        kOpcDisconnected            = -1,
        kOpcSubcribeFailed         = -2,
        kOpcUnknownErr              = -99

    };

    OpcAgentClient(const im_string&  channel_identity );
    ~OpcAgentClient();

    bool  connect();
    void  disconnect();
    bool  isConnected();
    bool  checkAgentOpcData();

    int     subscribeStatus(const im_string& device_id);
    void asyncSubscribeItems(const im_string& device_id,  const std::vector<im_string>& vec_items);
    void asyncRemoveSubscribeItems(const im_string& device_id);
    void asyncRefresh();

    bool getItemVal(const im_string& id, OpcVal& val);

private:
    typedef std::map<im_string, DeviceSubscribeInfo*> MapDevicesSubscribeInfo;
    typedef std::map<im_string, OpcVal> MapVals;

    static void reconnected(void* arg);
    static void dataRecved(const im_string& data,  void* arg);

    void setItemQuality(const im_string& id,  util::RTData::Quality qua);
    void initItemIfNotExist(const im_string& id,  util::RTData::Quality qua);

    void processSubscribeItemsResp(Json::Value& jval);
    void processRemoveDeviceItemsResp(Json::Value& jval);
    void processValsResp(Json::Value& jval);
    void processRefreshResp(Json::Value& jval);

    util::RTData::Quality convertOpcQuality(int opc_quality);
private:
    MapDevicesSubscribeInfo  map_device_subcribe_;
    util::IMMutex                           mutex_map_device_subcribe_;

    MapVals                     map_vals_;
    util::IMMutex           mutex_map_vals_;

private:
    im_string   agent_ips_;
    int                agent_port_;
    bool            is_conn_called_;
    int                conn_ref_;
    time_t         last_check_agent_tm_;
    time_t         last_socket_msg_tm_;
    bool             is_agent_opc_conned_;

    Connector conn_;
};

class OpcAgentClientMgr : public util::Singleton<OpcAgentClientMgr>
{
public:
    OpcAgentClientMgr();
    ~OpcAgentClientMgr();
    OpcAgentClient*  agent(const im_string&  channel_identity );
    void removeAll();

private:
    typedef std::map<im_string, OpcAgentClient*> MapAgents;
    MapAgents map_agents_;
    util::IMMutex  mutex_agent_;
};

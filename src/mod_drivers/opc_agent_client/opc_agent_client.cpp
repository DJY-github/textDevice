#include "opc_agent_client.h"
#include <stdlib.h>
#include "json/reader.h"
#include "json/writer.h"

OpcAgentClient::OpcAgentClient(const im_string&  channel_identity)
{
    last_check_agent_tm_ = time(NULL);
    last_socket_msg_tm_ = time(NULL);

    is_agent_opc_conned_ =  true;
    is_conn_called_ = false;
    conn_ref_ = 0;
    int  pos = 0;
    if(-1 != (pos=im_string(channel_identity).find(":")))
	{
		agent_ips_ = channel_identity.substr(0, pos);
		im_string port = channel_identity.substr(pos+1);
		agent_port_ = atoi(port.c_str());
	}
}

OpcAgentClient::~OpcAgentClient()
{
    MapDevicesSubscribeInfo::iterator it;
    for(it = map_device_subcribe_.begin(); it != map_device_subcribe_.end(); it++)
    {
        it->second->vec_items.clear();
        delete it->second;
    }
    map_device_subcribe_.clear();
}

bool OpcAgentClient::connect()
{
    conn_ref_++;
    if(!is_conn_called_)
    {
        is_conn_called_ = true;
        conn_.setCallback(reconnected, dataRecved, this);
        return  conn_.init(agent_ips_.c_str(), agent_port_);
    }
    else
    {
        return conn_.isConnected();
    }
}
void OpcAgentClient::disconnect()
{
    conn_ref_--;

    if(0 == conn_ref_)
    {
        conn_.uninit();
        is_conn_called_ = false;
    }
}
bool OpcAgentClient::isConnected()
{
    return conn_.isConnected();
}

int  OpcAgentClient::subscribeStatus(const im_string& device_id)
{
    util::IMMutexLockGuard locker(mutex_map_device_subcribe_);

    MapDevicesSubscribeInfo::iterator it_dev = map_device_subcribe_.find(device_id);
    if(it_dev != map_device_subcribe_.end())
    {
            return it_dev->second->subscribe_status;
    }

    return DeviceSubscribeInfo::kUnSubscribe;
}


void OpcAgentClient::asyncSubscribeItems(const im_string& device_id,  const std::vector<im_string>& vec_items)
{

    {
        util::IMMutexLockGuard locker(mutex_map_device_subcribe_);

        DeviceSubscribeInfo* subscribe = NULL;
        MapDevicesSubscribeInfo::iterator it = map_device_subcribe_.find(device_id);
        if( it != map_device_subcribe_.end())
        {
            subscribe = it->second;

            subscribe->subscribe_status = DeviceSubscribeInfo::kSubscribing;
            subscribe->tm = time(NULL);
            subscribe->vec_items.clear();

        }
        else
        {
            subscribe = new DeviceSubscribeInfo();
            subscribe->subscribe_status = DeviceSubscribeInfo::kSubscribing;
            subscribe->tm = time(NULL);

            map_device_subcribe_.insert(MapDevicesSubscribeInfo::value_type(device_id, subscribe));
        }

        subscribe->vec_items.assign(vec_items.begin(), vec_items.end());
    }

    Json::Value jval;
    jval["func"] = FUNC_SUBSCRIBE_ITEM;
    jval["payload"]["subid"] = device_id;

    for(unsigned int i=0; i<vec_items.size(); i++)
    {
        jval["payload"]["items"][i] = vec_items[i];

        //  同时初始化测点
        initItemIfNotExist(vec_items[i], util::RTData::kNeedWait);
    }

//    util::IMMutexLockGuard locker(mutex_send_);
    Json::FastWriter writer;
    conn_.send(writer.write(jval));
}

void OpcAgentClient::asyncRemoveSubscribeItems(const im_string& device_id)
{
    Json::Value jval;

    {
        util::IMMutexLockGuard locker(mutex_map_device_subcribe_);

        DeviceSubscribeInfo* subscribe = NULL;
        MapDevicesSubscribeInfo::iterator it = map_device_subcribe_.find(device_id);
        if( it == map_device_subcribe_.end())
        {
            return;
        }

        subscribe = it->second;

        jval["func"] = FUNC_REMOVE_ITEM;
        jval["payload"]["subid"] = device_id;
        for(unsigned int i=0; i<subscribe->vec_items.size(); i++)
        {
            jval["payload"]["items"][i] = subscribe->vec_items[i];
        }

        map_device_subcribe_.erase(it);
        subscribe->vec_items.clear();
        delete subscribe;
    }

    {
       // util::IMMutexLockGuard locker(mutex_send_);
        Json::FastWriter writer;
        conn_.send(writer.write(jval));
    }
}


void OpcAgentClient::asyncRefresh()
{
    Json::Value jval;
    jval["func"] = FUNC_REFRESH_ITEM;
    jval["payload"] = Json::Value();

    Json::FastWriter writer;
    conn_.send(writer.write(jval));
}

bool OpcAgentClient::getItemVal(const im_string& id, OpcVal& val)
{
    util::IMMutexLockGuard locker(mutex_map_vals_);

    MapVals::iterator it = map_vals_.find(id);
    if(it == map_vals_.end())
    {
        return false;
    }
    val = it->second;
    return true;
}

void OpcAgentClient::setItemQuality(const im_string& id,  util::RTData::Quality qua)
{
    util::IMMutexLockGuard locker(mutex_map_vals_);

    MapVals::iterator it = map_vals_.find(id);
    if(it != map_vals_.end())
    {
        it->second.qua = qua;
        it->second.tm = time(NULL);
    }
    else
    {
        OpcVal opcval;
        opcval.qua = qua;
        opcval.tm = time(NULL);
        map_vals_.insert(MapVals::value_type(id, opcval));
    }
}
void OpcAgentClient::initItemIfNotExist(const im_string& id,  util::RTData::Quality qua)
{
    util::IMMutexLockGuard locker(mutex_map_vals_);

    MapVals::iterator it = map_vals_.find(id);
    if(it == map_vals_.end())
    {
        OpcVal opcval;
        opcval.qua = qua;
        opcval.tm = time(NULL);
        map_vals_.insert(MapVals::value_type(id, opcval));
    }
}

//  发生重连后，更改设备订阅状态 （使驱动逻辑重新订阅）
void OpcAgentClient::reconnected(void* arg)
{
    printf("---------reconnected\n");
    OpcAgentClient *agent = (OpcAgentClient *)arg;

    {
        util::IMMutexLockGuard locker(agent->mutex_map_device_subcribe_);

        MapDevicesSubscribeInfo::iterator it_dev;
        for(it_dev=agent->map_device_subcribe_.begin(); it_dev!=agent->map_device_subcribe_.end();it_dev++)
        {
            DeviceSubscribeInfo * subcribe = it_dev->second;
            subcribe = it_dev->second;
            subcribe->subscribe_status = DeviceSubscribeInfo::kUnSubscribe;
            subcribe->tm = time(NULL);
        }
    }

}

void OpcAgentClient::dataRecved(const im_string& data,  void* arg)
{
    OpcAgentClient* agent = (OpcAgentClient*)arg;

    agent->last_socket_msg_tm_ = time(NULL);

    Json::Value jval;
    Json::Reader reader;
    if(!reader.parse(data, jval))
    {
        return;
    }

    im_string func = jval["func"].asString();
    if(0 == strcmp(FUNC_SUBSCRIBE_ITEM, func.c_str()))
    {
        agent->processSubscribeItemsResp(jval);
    }
    else if(0 == strcmp(FUNC_ITEM_VAL, func.c_str()))
    {
         agent->processValsResp(jval);
    }
    else if(0 == strcmp(FUNC_REFRESH_ITEM, func.c_str()))
    {
        agent->processRefreshResp(jval);
    }
}

void OpcAgentClient::processSubscribeItemsResp(Json::Value& jval)
{
    util::RTData::Quality qua = util::RTData::kOk;

    im_string subid = jval["payload"]["subid"].asString();

    util::IMMutexLockGuard locker(mutex_map_device_subcribe_);

    MapDevicesSubscribeInfo::iterator it_dev = map_device_subcribe_.find(subid);
    if(it_dev != map_device_subcribe_.end())
    {
            DeviceSubscribeInfo *subscribe  = it_dev->second;
            int re = jval["errcode"].asInt();
            if(kOpcOk==  re)
            {
                is_agent_opc_conned_ = true;

                subscribe->subscribe_status = DeviceSubscribeInfo::kSubscribed;

                // 更新测点数据质量
                Json::Value *items = &jval["payload"]["items"];
                if(!items->isNull() && items->isArray())
                {
                    Json::Value *item = NULL;
                    for(unsigned int i=0; i<items->size(); i++)
                    {
                        item = &(*items) [i];
                        if(0 == (*item)["succeeded"].asInt())
                        {
                            qua = util::RTData::kConfigError;
                        }
                        setItemQuality((*item)["id"].asString(), qua);
                    }

                }

            }
            else
            {
                if(kOpcDisconnected == re)
                {
                    is_agent_opc_conned_ = false;
                }

                subscribe->subscribe_status = DeviceSubscribeInfo::kUnSubscribe;

                if(kOpcDisconnected == re || kOpcSubcribeFailed == re)
                {
                    qua = util::RTData::kOutOfService;
                }
                else
                {
                    qua = util::RTData::kUncertain;
                }

                 // 更新测点数据质量
                 for(unsigned int i=0; i<subscribe->vec_items.size(); i++)
                 {
                    setItemQuality(subscribe->vec_items[i],  qua);
                 }
        }
    }
}
void OpcAgentClient::processRemoveDeviceItemsResp(Json::Value& jval)
{
    //do nothing
}
void OpcAgentClient::processValsResp(Json::Value& jval)
{
    int errcode = jval["errcode"].asInt();
    if(kOpcOk ==  errcode)
    {
        is_agent_opc_conned_ = true;

         util::IMMutexLockGuard locker(mutex_map_vals_);

         if(!jval["payload"].isNull() && jval["payload"].isArray())
         {
             unsigned int cnt = jval["payload"].size();
            for(unsigned int i=0; i<cnt;i++)
            {
                Json::Value *jitem = &jval["payload"][i];

                OpcVal item_val;
                item_val.tm = time(NULL);
                item_val.val = (*jitem)["val"].asString();
                item_val.qua = convertOpcQuality((*jitem)["qua"].asInt());

                std::pair<MapVals::iterator, bool> is_ok = map_vals_.insert(
                                            MapVals::value_type((*jitem)["id"].asString(), item_val));
                if(!is_ok.second)
                {
                    is_ok.first->second = item_val;
                }
            }
         }
    }
    else  if(kOpcDisconnected == errcode)
    {
        is_agent_opc_conned_ = false;
    }
}

void OpcAgentClient::processRefreshResp(Json::Value& jval)
{
    int errcode = jval["errcode"].asInt();
    printf("-------refresh--%d\n", errcode);
    if(kOpcOk ==  errcode)
    {
        is_agent_opc_conned_ = true;
    }
    else  if(kOpcDisconnected == errcode)
    {
        is_agent_opc_conned_ = false;
    }
}

// 1.以一定的间隔时间刷新组的数据,
// 2.检查socket消息的接收时间，超时判定socket连接中断
// 3.检查本地测点值的时间戳，如果时间戳超时未更新，则置测点质量为无响应
bool OpcAgentClient::checkAgentOpcData()
{
    if(!is_agent_opc_conned_)
    {
        if(conn_.isConnected())         // 切换Socket连接
        {
                conn_.switchConnection();

                // 等待切换连接后，OPC数据返回 .
                // 如果返回成功，dataRecv线程会置is_agent_opc_conned_为true
                sleep(6);
        }


        return is_agent_opc_conned_;
    }

    time_t time_now = time(NULL);
    if(time_now - last_check_agent_tm_ < CHECK_AGENT_DATA_TIME_SECONDS)
    {
        // 未到检测时间
         return conn_.isConnected();
    }

    if(!conn_.isConnected())
    {
        last_check_agent_tm_ = time(NULL);
        return false;
    }

    printf("------ checkAgentOpcData ing %lu\n", time_now);
    asyncRefresh();

    if(time_now - last_socket_msg_tm_ > CHECK_AGENT_DATA_TIMEOUT)
    {
        printf("------ last_socket_msg_tm_ timeout\n");
        conn_.closeSocket();
    }

    /*
    util::IMMutexLockGuard locker(mutex_map_vals_);
    MapVals::iterator it ;
    for(it = map_vals_.begin(); it != map_vals_.end(); it++)
    {
        if(time_now - it->second.tm > CHECK_AGENT_DATA_TIMEOUT)
        {
            printf("------%s val timeout\n", it->first.c_str());
            if(util::RTData::kConfigError != it->second.qua)
            {
                it->second.qua = util::RTData::kCmdNoResp;
            }
        }
    }
    */

    last_check_agent_tm_ = time(NULL);
    return true;
}

util::RTData::Quality OpcAgentClient::convertOpcQuality(int opc_quality)
{
    util::RTData::Quality qua = util::RTData::kOk;

    if(192 != opc_quality)
    {
        qua = util::RTData::kCmdReadError;
    }
    return qua;
}

OpcAgentClientMgr::OpcAgentClientMgr()
{

}
OpcAgentClientMgr::~OpcAgentClientMgr()
{
    MapAgents::iterator it;
    for(it = map_agents_.begin(); it != map_agents_.end(); it++)
    {
        delete it->second;
    }
    map_agents_.clear();
}
OpcAgentClient*  OpcAgentClientMgr::agent(const im_string&  channel_identity )
{
    util::IMMutexLockGuard locker(mutex_agent_);

    OpcAgentClient *agent = NULL;
    MapAgents::iterator it = map_agents_.find(channel_identity);
    if(it != map_agents_.end())
    {
        agent = it->second;
    }
    else
    {
        agent = new OpcAgentClient(channel_identity);
        map_agents_.insert(MapAgents::value_type(channel_identity, agent));
    }

    return agent;
}

void OpcAgentClientMgr::removeAll()
{

}


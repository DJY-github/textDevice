#include "opc_ba.h"

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

#include <sstream>
#include <iostream>

using namespace std;
#define RESP_BUFF_LEN       256
#define RESP_BUFF_SIZE      256
#define MAXBUFFERSIZE       400

Device::Device(void)
{
    serial = NULL;
    fd_ = 0;
}

Device::~Device(void)
{

}

int Device::open( const DeviceConf& device_info)
{
    serial = IMSerialPort::getSerial(device_info.channel_identity.c_str());

	if(!serial->isOpen())
	{
		if(!serial->open(device_info.channel_identity.c_str(), device_info.channel_params.c_str()))
		{
			return util::RTData::kCommDisconnected;
		}
	}

    return util::RTData::kOk;
}

int Device::close()
{
    CloseSubcription();
    if(nullptr != serial)
	{
		serial->close();
	}

    return util::RTData::kOk;

}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			return util::RTData::kCommDisconnected;
		}
	}
    //发送获取数据请求,首次请求开始OPC订阅
	sendRequest(tags, cmd_feature.cmd, (string)cmd_feature.auxiliary);
    //接收数据回应
    int dataLength = reciveLength();
    if (dataLength == -1)
    {
        serial->close();
        return util::RTData::kCmdNoResp;
    }
    char* pack_data = (char*)malloc(dataLength + 1);
    if (0 != reciveData(pack_data, dataLength))
    {
        serial->close();
        free(pack_data);
        return util::RTData::kCmdNoResp;
    }
    //printf("reslen %d\nsizeof data %d\n", reslen, head);

    //解析数据
    map<string, string> res ;
    if (0 != parseResponse(pack_data, res))
    {
        free(pack_data);
        return util::RTData::kConfigError;
    }

    free(pack_data);
    Tags::iterator it;
    //模板数据点赋值
    for (it = tags.begin(); it != tags.end(); ++it)
    {
        im_string param1 = util::replaceString((*it)->conf.get_param1, "*", cmd_feature.auxiliary.c_str());
        string val = res[(string)param1];
        //cout<<cmd_feature.auxiliary<<"   "<<param1<<"   val="<<val<<endl;
        (*it)->value.pv = util::IMVariant(val.c_str(), val.size());
        (*it)->value.quality	= util::RTData::kOk;
        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
    }
    res.clear();
    return util::RTData::kOk;
}


//解除OPC订阅
void Device::CloseSubcription()
{
    printf("\n==============\nclose subscription res %s\n==============\n", groupname.c_str());
    Json::Value getreqjson;
    Json::Value itemArr;
    getreqjson["operate"] = "delete";
    getreqjson["group_name"] = groupname;
    string getreqjsonstr = getreqjson.toStyledString();
    string getreq = buildRequest(getreqjsonstr);
    serial->write(const_cast<char*>(getreq.c_str()), getreq.size());

    int dataLength = reciveLength();
    char* pack_data = (char*)malloc(dataLength+1);
    reciveData(pack_data, dataLength);
//    printf("reslen %d\nsizeof data %d\n", reslen, head);
    map<string, string> res;
    parseResponse(pack_data, res);
    free(pack_data);
}

//请求数据打包
string Device::buildRequest(const string& addreqjsonstr)
{
    int len = addreqjsonstr.length();
    stringstream stream;
    stream << len;
    string lenstr = stream.str();
    lenstr.append(10-lenstr.size(), '\0');
    lenstr += addreqjsonstr;
    return lenstr;
}

//获取返回数据包头，数据长度
int Device::reciveLength()
{
    int len;
    char pack_head[11] = {0};
    int read_len = serial->read(pack_head, 10);
    if(0 >= read_len)
	{
		return -1;
	}
    string lens(pack_head);
    stringstream stream(lens);
    stream  >> len;
    return len;
}

//获取返回数据
int  Device::reciveData(char* pack_data, int head)
{
    int reslen = serial->read(pack_data, head);
    if(0 >= reslen)
    {
        return -1;
    }
    if(reslen != head)
    {
        return -1;
    }
    //printf("\n==============\nres %s\n==============\n", pack_data);
    //printf("reslen %d\nsizeof data %d\n strlen pack_data:%d\n", reslen, head,strlen(pack_data));
    return 0;
}

void Device::sendRequest(Tags& tags, string groupname, string auxiliary)
{
    Tags::iterator it;
    Json::Value getreqjson;
    Json::Value itemArr;
    getreqjson["operate"] = "get";
    getreqjson["group_name"] = groupname;
    int i = 0;
   /* for (it = tags.begin(); it != tags.end(); ++it)
    {
        im_string param1 = util::replaceString((*it)->conf.get_param1, "*", auxiliary.c_str());
        //util::replaceString(param1, )

        itemArr.append((string)param1);
        i++;
    }*/
    for(int i=0; i<6000; i++)
    {
            char item_id[128] = {0};
            snprintf(item_id, 127, "Simulation Examples.Device1.Group1.%d", i);
             itemArr.append((string)item_id);
    }
    getreqjson["item_names"] = itemArr;
    getreqjson["item_num"] = i;
    string getreqjsonstr = getreqjson.toStyledString();
    string getreq = buildRequest(getreqjsonstr);
    serial->write(const_cast<char*>(getreq.c_str()), getreq.size());
}

int Device::parseResponse(char* resjson, map<string, string>& res)
{
    string jsonstr(resjson);
    Json::Reader reader;
    Json::Value res_json;
    Json::Value item;
    int errorcode = -1;
    if (reader.parse(jsonstr, res_json))
    {
        errorcode = res_json["errorcode"].asInt();
        if(errorcode != 0)
        {
            return -1;
        }
        item = res_json["Items"];
        int item_size = item.size();
        for (int i = 0; i < item_size; i ++)
        {
            std::pair<map<string,string>::iterator, bool> is_ok = res.insert(map<string,string>::value_type(item[i]["item_name"].asString(), item[i]["value"].asString()));
            if(!is_ok.second)
            {
                is_ok.first->second  =  item[i]["value"].asString();
            }
            cout<< "item_name="<< item[i]["item_name"].asString() <<"   value="<<item[i]["value"].asString()<<endl;
        }
    }
    return 0;
}



int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
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

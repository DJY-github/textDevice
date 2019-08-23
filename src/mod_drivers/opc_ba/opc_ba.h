#pragma once
#include "driver_module.h"
#include "util_serialport.h"
#include "util_singleton.h"
#include <sys/socket.h>

#define NUM_SYS_VBS	6
using namespace std;
using namespace drv;
using namespace util::serial;

class OpcData {
    public:
        string groupname;
    private:
        bool is_first;
};

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
    int        GetSubscriptData();
    void        CloseSubcription();

    string      buildRequest(const string& reqjson);
    void        sendRequest(Tags& tags, string groupname, string auxiliary);
    int         reciveLength();
    int         reciveData(char* pack_data, int head);
    int         parseResponse(char* resjson, map<string, string>& res);

private:

    IMSerialPort*	serial;
    string          groupname;
    int             fd_;

};



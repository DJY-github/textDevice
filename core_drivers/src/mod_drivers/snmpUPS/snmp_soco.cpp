#include "snmp_soco.h"

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

#include <iostream>

using namespace std;
using namespace Snmp_pp;
#define RESP_BUFF_LEN       256
#define RESP_BUFF_SIZE      256

#define V1                  "ver:v1"
#define COMMUNITY           "_community:"

#define V3                  "ver:v3"
#define USER                "_user:"
#define AUTH_PROTO          "_authproto:"
#define AUTH_PWD            "_authpwd:"
#define PRI_PROTO           "_priproto:"
#define PRI_PWD             "_pripwd:"

Device::Device(void)
{
    fd_ = 0;
    target_ = NULL;
    utarget_ = NULL;
    v3mp_ = NULL;
    is_open_ = false;
    community_ = "public";
}


Device::~Device(void)
{
    Snmp::socket_cleanup();  // Shut down socket subsystem
}

bool Device::get_version_attr(const DeviceConf& device_info)
{
    //模板ver填写格式
    //ver:v1_community:public
    //ver:v3_user:xxx_authproto:xxx_authpwd:_priproto:xxx_pripwd:xxx
    //ver:v3_user:admin_authproto:SHA_authpwd:123*&^123_priproto:AES192_pripwd:1298masd&^%
    int pos = 0;
    im_string ver = device_info.ver;
    if ( -1 != (pos = ver.find(V1)))
    {
        version_ = version1;
        if( -1 != (pos = ver.find(COMMUNITY)))
        {
            community_ = ver.substr(pos+11);
            return true;
        }
        return false;
    }

    else if( -1 != (pos = ver.find(V3)))
    {
        int pos_pri, pos_last;
        version_ = version3;

        if(-1 == (pos_pri = ver.find(USER)))
        {
            return false;
        }

        pos_pri += strlen(USER);
        if(-1 == (pos_last=ver.find(AUTH_PROTO)))
        {
            return false;
        }
		conf_.user = ver.substr(pos_pri , pos_last-(pos_pri));
		cout << "user: " << conf_.user << endl;

		pos_pri = pos_last + strlen(AUTH_PROTO);
		if(-1 == (pos_last=ver.find(AUTH_PWD)))
        {
            return false;
        }
		conf_.auth_proto = ver.substr(pos_pri , pos_last-(pos_pri));
		cout << "auth_proto: " << conf_.auth_proto << endl;

		pos_pri = pos_last + strlen(AUTH_PWD) ;
		if(-1 == (pos_last=ver.find(PRI_PROTO)))
        {
            return false;
        }
		conf_.auth_pwd = ver.substr(pos_pri , pos_last-(pos_pri));
		cout << "auth_pwd: " << conf_.auth_pwd << endl;

		pos_pri = pos_last +strlen(PRI_PROTO) ;
		if(-1 == (pos_last=ver.find(PRI_PWD)))
        {
            return false;
        }
		conf_.pri_proto = ver.substr(pos_pri , pos_last-(pos_pri));
		cout << "pri_proto: " << conf_.pri_proto << endl;

		pos_pri = pos_last + strlen(PRI_PWD);
		conf_.pri_pwd = ver.substr(pos_pri);
		cout << "pri_pwd: " << conf_.pri_pwd << endl;
		return true;
    }
//    else
//    {
//        return false;
//    }
    return true;
}

bool Device::Get_Table(Oid oid , vec_table& res){
    int status = SNMP_CLASS_SUCCESS;
    Pdu pdu;
    Oid start_oid = oid;
    Oid end_oid = oid;
    Oid now_oid = oid;
    now_oid += ".1.2";
    start_oid += ".1";
    end_oid += ".2";
    int biger = 2;
    Vb vb(start_oid);
    pdu += vb;
    vector< pair<int,string> > vec;
    try {
        SnmpTarget *target = NULL;
        if (version3 == version_)

        {
            target = utarget_;
            pdu.set_security_level(security_level_);

        }
        else
        {
            target = target_;
        }
        while (status = SnmpObj::instance()->snmp()->get_next(pdu, *target)==SNMP_CLASS_SUCCESS){
            pdu.get_vb(vb,0);
            if(vb.get_oid() < now_oid){
                int instance = 0;
                string ins_str = "";
                string oid_str = vb.get_printable_oid();
                for (int i = oid_str.length()-1; i > 0; i--){
                    if (oid_str[i] == '.') break;
                    ins_str = oid_str[i]+ins_str;
                }
                instance = atoi (ins_str.c_str());
                vec.push_back (make_pair (instance,vb.get_printable_value()));
            } else {
                now_oid.trim(1);
                biger++;
                now_oid += biger;
                res.push_back(vec);
                vec.clear();
                int instance = 0;
                string ins_str = "";
                string oid_str = vb.get_printable_oid();
                for (int i = oid_str.length()-1; i >0; i--){
                    if(oid_str[i] == '.')break;
                    ins_str = oid_str[i]+ins_str;
                }
                instance = atoi(ins_str.c_str());
                vec.push_back(make_pair(instance, vb.get_printable_value()));
                if(vb.get_oid() >= end_oid)break;
            }
        }
        for (unsigned int i = 0; i < res.size() ;i++) {
            for (unsigned int j = 0; j < res[i].size() ;j++) {
                if (res[i][j].first != res[0][j].first) {
                    res[i].insert(res[i].begin() + j, make_pair(res[0][j].first, "0"));
                }
            }
        }
    }
    catch (exception& e) {
//        cout<<"_______catched wrong_______\n------------"<<status<<"-------------"<<endl;
        return false;
    }
    if (status == 0) {
//        cout<<"_______something wrong_______\n------------"<<status<<"-------------"<<endl;
        return false;
    }
    return true;
}

int Device::open( const DeviceConf& device_info)
{
    int  pos = 0;
    im_string port;

    device_info_ = device_info;
//    cout<<"\n\n\n------opening------"<<endl;
    if(-1 != (pos = im_string(device_info.channel_identity).find(":")))
    {
        ip_ = im_string(device_info.channel_identity).substr(0, pos);   //get id
        port = im_string(device_info.channel_identity).substr(pos+1);   //get port
    }
    if(!get_version_attr(device_info_))
    {
        printf("config error!\n");
        close();
        return util::RTData::kConfigError;
    }
    UdpAddress address(ip_.c_str());
    address.set_port(atoi(port.c_str()));
    if(version_ == version3)
    {
        int securityModel = SNMP_SECURITY_MODEL_USM;
        security_level_ = SNMP_SECURITY_LEVEL_NOAUTH_NOPRIV;
        long authProtocol = SNMP_AUTHPROTOCOL_NONE;
        long privProtocol = SNMP_PRIVPROTOCOL_NONE;
        if("SHA" == conf_.auth_proto)
        {
            authProtocol = SNMP_AUTHPROTOCOL_HMACSHA;
        }
        if("MD5" == conf_.auth_proto)
        {
            authProtocol = SNMP_AUTHPROTOCOL_HMACMD5;
        }

        if("DES" == conf_.pri_proto)
        {
            privProtocol = SNMP_PRIVPROTOCOL_DES;
        }
        else if("3DES" == conf_.pri_proto)
        {
            privProtocol = SNMP_PRIVPROTOCOL_3DESEDE;
        }
        else if("IDEA" == conf_.pri_proto)
        {
            privProtocol = SNMP_PRIVPROTOCOL_IDEA;
        }
        else if("AES128" == conf_.pri_proto)
        {
            privProtocol = SNMP_PRIVPROTOCOL_AES128;
        }
        else if("AES192" == conf_.pri_proto)
        {
            privProtocol = SNMP_PRIVPROTOCOL_AES192;
        }
        else if("AES256" == conf_.pri_proto)
        {
            privProtocol = SNMP_PRIVPROTOCOL_AES256;
        }

        if( authProtocol > SNMP_AUTHPROTOCOL_NONE )
        {
            security_level_ = SNMP_SECURITY_LEVEL_AUTH_NOPRIV;
            if(privProtocol > SNMP_PRIVPROTOCOL_NONE)
            {
                security_level_ = SNMP_SECURITY_LEVEL_AUTH_PRIV;
            }
        }
        const char *engineId = "snmpGet";
        const char *filename = "snmpv3_boot_counter";
        unsigned int snmpEngineBoots = 0;
        int status = getBootCounter(filename, engineId, snmpEngineBoots);
        if ((status != SNMPv3_OK) && (status < SNMPv3_FILEOPEN_ERROR))
        {
            printf("Error loading snmpEngineBoots counter: %d\n",status);
            close();
            return util::RTData::kCommDisconnected;;
        }
        snmpEngineBoots++;
        status = saveBootCounter(filename, engineId, snmpEngineBoots);
        if (status != SNMPv3_OK)
        {
            printf("Error saving snmpEngineBoots counter: %d\n",status);
            close();
            return util::RTData::kCommDisconnected;;
        }
        if(NULL == v3mp_)
        {
            int construct_status;
            v3mp_ = new v3MP(engineId, snmpEngineBoots, construct_status);
            if (construct_status != SNMPv3_MP_OK)
            {
                printf("Error initializing v3MP:  %d\n",status);
                close();
                return util::RTData::kCommDisconnected;;
            }
        }
        USM *usm = v3mp_->get_usm();
        OctetStr oct_user(conf_.user.c_str());
        OctetStr oct_APW(conf_.auth_pwd.c_str());
        OctetStr oct_PPW(conf_.pri_pwd.c_str());

        usm->add_usm_user(oct_user,
                          authProtocol, privProtocol,
                          oct_APW, oct_PPW);

        utarget_ = new UTarget(address);
        utarget_->set_version(version_);
        utarget_->set_retry(1);
        utarget_->set_timeout(200);
        utarget_->set_security_model(securityModel);
        utarget_->set_security_name(conf_.user.c_str());
    }else
    {

        OctetStr community(community_.c_str());
        target_ = new CTarget(address);
        target_->set_version(version_);
        target_->set_retry(1);
        target_->set_timeout(200);
//        cout<<"---ip:"<<ip_.c_str()<<"    port:"<<port.c_str()<<endl;
 //       cout<<"-------community:"<<community_.c_str()<<"-------"<<endl;
        target_->set_readcommunity(community_.c_str());
    }
    is_open_ = true;
    return util::RTData::kOk;
}

int Device::close()
{
    if(NULL != target_)
    {
        delete target_;
        target_ = NULL;
    }
    if(NULL != utarget_)
    {
        delete utarget_;
        utarget_ = NULL;
    }
    if(NULL != v3mp_)
    {
        delete v3mp_;
        v3mp_ = NULL;
    }
    is_open_ = false;
    return util::RTData::kOk;
}

im_string Device::str_to_bin(im_string data_str)
{
    im_string res_bin = "";
    if (data_str == "") {
        return res_bin;
    }
    for (int i = 0; i < data_str.size(); i++) {
        unsigned char k = 0x80;
        for (int j=0; j<8; j++, k >>= 1)
        {
            if (data_str[i] & k)
                res_bin += "1";
            else
                res_bin += "0";
        }
    }
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
    if(!is_open_) //open?
    {
       // cout<<"\n\n\n\n-----reopen-----"<<endl;
        if(util::RTData::kOk != open(device_info_))
        {
            close();
            return util::RTData::kCommDisconnected;
        }
    }

    int res = 0;
    vec_table res_table;
    //printf(cmd_feature.cmd.c_str());
    printf("\n");
    Oid oid(cmd_feature.cmd.c_str());
    Vb vb(oid);
    Pdu pdu;
    pdu += vb;
    if (strcmp((*tags.begin())->conf.get_param3.c_str(), "table") == 0)
    {

        Oid oid_table(cmd_feature.cmd.c_str());
        if ( !Get_Table(oid_table,res_table) || res_table.size() <= 2) {
            (*tags.begin())->value.quality	= util::RTData::kCmdNoResp;
            close();
            return util::RTData::kCmdNoResp;;
        }
    }
    else
    {
        SnmpTarget *target = NULL;

        if(version3 == version_)
        {
            target = utarget_;
            pdu.set_security_level(security_level_);
        }
        else
        {
            target = target_;
        }

        int status = SnmpObj::instance()->snmp()->get(pdu, *target);
        if(SNMP_CLASS_SUCCESS != status)
        {
            return util::RTData::kCmdNoResp;
        }
        pdu.get_vb(vb,0);

    }

    Tags::iterator it;
    for (it = tags.begin(); it != tags.end(); ++it)
    {

        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
        const char* param3 = "";
        param3 = (*it)->conf.get_param3.c_str();
        // param3 = "table"
        if (strcmp(param3, "table") == 0 )
        {

            int param1 = 0;
            param1 = atoi((*it)->conf.get_param1.c_str());
            const char* param2 = "";
            param2 = (*it)->conf.get_param2.c_str();
            res = 0;
            for(int i = 0;i < res_table[param1].size(); i++) {
                if(strcmp(res_table[param1][i].second.c_str(), param2) == 0){
                    res = 1;
                    break;
                }
            }
            (*it)->value.pv		= util::IMVariant(res);
        } else if(strcmp(param3, "bit") == 0 ){
            int res = 0;
            int param1 = 0;
            param1 = atoi((*it)->conf.get_param1.c_str());
            im_string val = vb.get_printable_value();
            im_string val_b = str_to_bin(val);
            if (param1 > val_b.size() || param1 <= 0) {
                (*it)->value.quality	= util::RTData::kCmdNoResp;
                close();
                return util::RTData::kCmdNoResp;;
            }
            if (val_b[(param1-1)] == '1'){
                res = 1;
            }
            (*it)->value.pv = util::IMVariant(res);

        }
        else {
             if(sNMP_SYNTAX_INT == vb.get_syntax() || sNMP_SYNTAX_INT32 == vb.get_syntax())
            {
                int i = 0;
                vb.get_value(i);
                (*it)->value.pv = util::IMVariant(i);
            }
            else if(sNMP_SYNTAX_UINT32 == vb.get_syntax() ||
                sNMP_SYNTAX_CNTR32 == vb.get_syntax() ||
                sNMP_SYNTAX_GAUGE32 == vb.get_syntax() ||
                sNMP_SYNTAX_TIMETICKS == vb.get_syntax())
            {
                unsigned int i;
                vb.get_value(i);
                (*it)->value.pv = util::IMVariant(i);
            }
            else if(sNMP_SYNTAX_CNTR64 == vb.get_syntax())
            {
                pp_uint64 i;
                vb.get_value(i);
                (*it)->value.pv = util::IMVariant(i);
            }
            else
            {
                im_string val = vb.get_printable_value();
                (*it)->value.pv = util::IMVariant(val.c_str(), val.size());
            }
            break;
        }

        (*it)->value.quality	= util::RTData::kOk;
        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
    }
    return util::RTData::kOk;
}

SnmpObj::SnmpObj()
{
    snmp_ = new Snmp(status_);
}

SnmpObj::~SnmpObj()
{
    delete snmp_;
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

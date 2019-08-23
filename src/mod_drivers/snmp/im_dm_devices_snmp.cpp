#include "im_dm_devices_snmp.h"

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
}

Device::~Device(void)
{
    close();
}

bool Device::get_version_attr(const DeviceConf& device_info)
{
    //模板ver填写格式
    //ver:v1_community:public
    //ver:v3_user:xxx_authproto:xxx_authpwd:_priproto:xxx_pripwd:xxx
    //ver:v3_user:admin_authproto:SHA_authpwd:123*&^123_priproto:AES192_pripwd:1298masd&^%
    int pos = 0;
    im_string ver = device_info.ver;
    if( -1 != (pos=ver.find(V1)))
    {
        version_ = version1;
        if( -1 != (pos=ver.find(COMMUNITY)))
        {
            community_ = ver.substr(pos+11);
        }
        else
        {
            return false;
        }

    }
    else if( -1 != (pos=ver.find(V3)))
    {
        int pos_pri, pos_last;
        version_ = version3;

        if(-1 == (pos_pri=ver.find(USER)))
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

    }
    else
    {
        version_ = version1;
        community_ = "public";
    }

    return true;
}

int Device::open(const DeviceConf& device_info)
{
	int  pos = 0;
	im_string ip, port;
	device_info_ = device_info;
	if(-1 != (pos=im_string(device_info.channel_identity).find(":")))
	{
        ip = im_string(device_info.channel_identity).substr(0, pos);
		port = im_string(device_info.channel_identity).substr(pos+1);
	}

    if(!get_version_attr(device_info_))
    {
        printf("config error!\n");
        //close();
        return util::RTData::kConfigError;
    }

    UdpAddress address(ip.c_str());
    address.set_port(atoi(port.c_str()));

    if(version_ == version1)
    {
        OctetStr community(community_.c_str());
        target_ = new CTarget(address);
        target_->set_version(version_);
        target_->set_retry(1);
        target_->set_timeout(200);
        target_->set_readcommunity(community_.c_str());
    }
    else if(version_ == version3)
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
        usm->add_usm_user(conf_.user.c_str(),
		       authProtocol, privProtocol,
		       conf_.auth_pwd.c_str(), conf_.pri_pwd.c_str());

        utarget_ = new UTarget(address);
        utarget_->set_version(version_);
        utarget_->set_retry(1);
        utarget_->set_timeout(200);
        utarget_->set_security_model(securityModel);
        utarget_->set_security_name(conf_.user.c_str());
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

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
	if(!is_open_)
    {
        if(util::RTData::kOk != open(device_info_))
        {
            close();
            return util::RTData::kCommDisconnected;
        }
    }
    Vb vb(cmd_feature.cmd.c_str());
    Pdu pdu;
    pdu += vb;
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

    pdu.get_vb( vb,0);

    util::IMVariant val;
    if(sNMP_SYNTAX_INT == vb.get_syntax() || sNMP_SYNTAX_INT32 == vb.get_syntax())
    {
        int i = 0;
        vb.get_value(i);
        val = util::IMVariant(i);
    }
    else if(sNMP_SYNTAX_UINT32 == vb.get_syntax() ||
            sNMP_SYNTAX_CNTR32 == vb.get_syntax() ||
            sNMP_SYNTAX_GAUGE32 == vb.get_syntax() ||
            sNMP_SYNTAX_TIMETICKS == vb.get_syntax())
    {
        unsigned int i;
        vb.get_value(i);
        val = util::IMVariant(i);
    }
    else if(sNMP_SYNTAX_CNTR64 == vb.get_syntax())
    {
        pp_uint64 i;
        vb.get_value(i);
        val = util::IMVariant(i);
    }
    else
    {

        im_string strval = vb.get_printable_value();
        return parseStringVal(strval, tags);

    }

    return parseNumberVal(val, tags);

}

int   Device::parseNumberVal(const util::IMVariant& val, Tags& tags)
{
    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {
        (*it)->value.quality	= util::RTData::kOk;
        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();

        if((*it)->conf.get_param1.empty())
        {
            if(!(*it)->conf.get_param3.empty())
            {
                // param3=replace 65535 0  表示当值=65535时，替换值为0
                if(std::string::npos != (*it)->conf.get_param3.find("replace"))
                {
                    std::vector<string> vec_item =util::spliteString((*it)->conf.get_param3,' ');
                    if(3 !=  vec_item.size())
                    {
                         (*it)->value.quality	= util::RTData::kConfigError;
                    }
                    else
                    {
                        if(val.toString() == vec_item[1])
                        {
                            (*it)->value.pv = util::IMVariant(atoi(vec_item[2].c_str()));
                        }
                    }

                }

            }
            else
            {
                (*it)->value.pv = val;
            }


        }
        else
        {
            //TODO
            (*it)->value.quality	= util::RTData::kOutOfService;
        }

    }


    return util::RTData::kOk;

}
int   Device::parseStringVal(const im_string& strval, Tags& tags)
{
    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {
        (*it)->value.quality	= util::RTData::kOk;
        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();

        if((*it)->conf.get_param1.empty())   // val值本身代表测点的值
        {
             (*it)->value.pv = util::IMVariant(strval.c_str(), strval.size());

        }
        else  // val值包含多个测点的值
        {

            if(std::string::npos != (*it)->conf.get_param2.find("bit_str"))   // 字符串二进制位 "0000111100001111000001" (APC PowerNet)
            {

                int bit_pos = atoi((*it)->conf.get_param1.c_str());

                if( bit_pos < 1 || bit_pos > (int)strval.size())
                {
                    (*it)->value.quality	= util::RTData::kConfigError;
                    continue;
                }

                bool bitval = false;
                if(strval.at(bit_pos-1) == '1')
                {
                    bitval = true;
                }

                (*it)->value.pv = util::IMVariant(bitval);
            }
        }
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

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const im_string& tag_value_pv)
{
    if(0 == fd_)
    {
        return util::RTData::kCommDisconnected;
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

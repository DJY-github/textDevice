#pragma once

#include "source_config.h"
#include "util_datastruct.h"

typedef util::RTValue	TagValue;

#define DRV_EXPORT	extern "C" DLL_EXPORT


namespace drv{


struct DeviceConf
{
	libistring		id;
	libistring		name;
	libistring		drv_lib;
	libistring		channel_type;
	libistring		channel_identity;
	libistring		channel_params;
	libistring		ver;
};

struct Tag
{
	struct Conf
	{
        char                upload_type;
		libistring			id;
		libistring			name;
		TagValue::ValType	data_type;
		libistring			get_cmd;
		libistring			get_param1;
		libistring			get_param2;
		libistring			get_param3;
		libistring			set_cmd;
		libistring			set_param1;
		libistring			set_param2;
		libistring			set_param3;
	};
	Tag(const Tag::Conf& tag_conf)
		:conf(tag_conf)
	{

	}

	const Conf		conf;
	util::RTValue	value;
};
typedef std::vector<Tag*>		Tags;

struct CmdFeature
{
	libistring	cmd;
	libistring	addr;
	libistring	auxiliary;
};




class IDevice;

DRV_EXPORT const char*		getDrvVer();


DRV_EXPORT int				initDrv(const char* dm_ver);


DRV_EXPORT int				uninitDrv();


DRV_EXPORT IDevice*			createDevice();


DRV_EXPORT int				releaseDevice(IDevice* device);



class IDevice
{
public:
    IDevice(){}
    virtual ~IDevice(){}
	virtual int			open(const DeviceConf& device_info) = 0;


	virtual int			close() = 0;


	virtual int			get(const CmdFeature& cmd_feature, Tags& tags) = 0;


	virtual int			set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv) = 0;

};


} // end namespace

#pragma once
#include "driver_module.h"
#include "util_serialport.h"
#include "util_imbufer.h"
//#include "Log.h"


using namespace drv;
using namespace util::serial;


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
	 int			sendDev(const CmdFeature& cmd_feature, const char* data_info = 0);
	 int			recvDev(util::IMBuffer<2048>& recv_buf);

	 bool			parseByteVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, unsigned char& val);
	 bool			parseShortVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, unsigned short& val);
	 bool			parseIntVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, int& val);
	 bool			parseFloatVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, float& val);

	 bool			parseDigitVal(const util::IMBuffer<2048>& recv_buf, int pos, const libistring& param2, bool& val);
	 bool			parseEnumVal(const util::IMBuffer<2048>& recv_buf, int pos, const libistring& param2, unsigned char& val);
     bool           parseString(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, im_string& val);
     bool           parseBCDTime(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, Time& val);
private:
	 IMSerialPort*	serial;
	 CIID           dev_id_;
	 im_string      pw_cmd_;
	 void           get_version_attr(const DeviceConf& device_info);

	 //Log			log_;
};


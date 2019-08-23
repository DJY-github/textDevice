#pragma once
#include "source_config.h"
#include "util_imvariant.h"

namespace util {


struct RTData	// 实时数据
{
	enum Quality  // 质量
	{
		kOk					= 0,    // 正常
        kUncertain			= -1,   // 未知
        kCommDisconnected	= -2,   // 通讯中断
        kCmdNoResp			= -3,   // 无响应
        kCmdReadError       = -4,   // 通讯错误
        kCmdRespError		= -5,   // 异常响应
        kCmdCrcError        = -6,   // 校验错误
        kConfigError		= -7,    // 配置错误
        kOutOfService       = -8,      //
        kOutOfValRange      = -9,     // 无效值
        kDisable                = -10,     // 未启用
        kAlarm                  = 1,      // 告警
        kNeedWait           = 2
 	};

	struct Value		// ֵ
	{
		enum ValType
		{
			kUnknown	= -1,
			kAnalog		= 1,
			kDigit		= 2,
			kEnumStatus	= 3,
			kString		= 4,
			kObject		= 5
		};
		ValType		type;
		IMVariant	pv;
		im_string	vdes;
		Time		timestamp;
		Quality		quality;
		Value()
		{
			quality	= kUncertain;
		}
	};
	struct Alarm
	{
		im_string	conf_id;
		im_string   alm_id;
		im_string   first_almste_id;
		bool        alm_ste;
		Int32		level;
		IMVariant	value;
		im_string	desc;
		Time		time;
		im_string	propose;
		Alarm()
		{
            alm_ste = false,
            level   = 0;
		}
	};
	Value	    val;
	Alarm	    alarm;
	uint16_t    modbus_addr;
	char        upload_type;
};

enum UploadType
{
    kUploadUnknown    = -1,
    kUploadStatus = 0,
    kUploadRealtime = 1,
    kUploadNoNeed = 2  // 不需上传
};

struct DataPoint
{
	CIID	id;
	bool   bchange;
	RTData	rtd;
};

typedef RTData::Value	RTValue;
typedef RTData::Alarm	RTAlarm;

}


#pragma once
#include "driver_module.h"

typedef enum {
    CMD_TRIG_TEMP_POLL      = 0xF3, // command trig. temp meas. no hold master
    CMD_TRIG_HUMI_POLL      = 0xF5, // command trig. humidity meas. no hold master
    CMD_SOFT_RESET          = 0xFE, // command soft reset
    CMD_MEASURE_READ        = 0x00, // read measured value (for software convenience)
} SHT2x_COMMAND;

typedef enum {
    OPT_RES_12_14BIT        = 0x00, // RH=12bit, T=14bit
    OPT_RES_8_12BIT         = 0x01, // RH= 8bit, T=12bit
    OPT_RES_10_13BIT        = 0x80, // RH=10bit, T=13bit
    OPT_RES_11_11BIT        = 0x81  // RH=11bit, T=11bit
} SHT2x_OPTION;

using namespace drv;
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
     int            fd_io;
     int            fd_th;
     int            th_exec_measure_read(SHT2x_COMMAND cmd, uint16_t *value);
     int            th_exec_command(SHT2x_COMMAND cmd, uint16_t *value);
     float          tem_;
     float          hum_;
     int            net_status_;
     Int64          total_flow_;
     char           file_full_path_ [255];
     char           temp_buff_[255];
     bool           findDir(const char *file_path, const char *adapt_name);
     bool           get_board_temp();
     bool           get_board_net();
     bool           get_board_interface_flow(im_string interface_name);

};


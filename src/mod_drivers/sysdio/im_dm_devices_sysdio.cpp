#include "im_dm_devices_sysdio.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <getopt.h>
#include <linux/i2c-dev.h>
#include <dirent.h>

#include "util_imvariant.h"
#include "util_datetime.h"
#include "util_datastruct.h"

#include <iostream>


#define     FYGPIO_MAGIC            'S'
#define     FYGPIO_SET_OUTPUT_OPEN      _IO(FYGPIO_MAGIC, 0)
#define     FYGPIO_SET_OUTPUT_CLOSE     _IO(FYGPIO_MAGIC, 1)
#define     FYGPIO_GET_VALUE           _IO(FYGPIO_MAGIC, 2)
#define     FYGPIO_SET_INPUT           _IO(FYGPIO_MAGIC, 3)

#define VERSION             "0.0.1"

#define I2C_DEV_ADDR        0x40
#define CRC_POLY            0x131   // P(x)=x^8+x^5+x^4+1 = 100110001
#define MEASURE_RETRY_WAIT  85000   // 85ms = maximum measurement time

using namespace std;

Device::Device(void)
{
    fd_io = 0;
    fd_th = 0;
    tem_ = 0;
    hum_ = 0;
    net_status_ = 0;
    memset(file_full_path_, 0, sizeof(file_full_path_));
}

Device::~Device(void)
{
}
int Device::open(const DeviceConf& device_info)
{
    if(0 == fd_io)
    {
        fd_io = ::open("/dev/fygpios", 0);
        if(0 >= fd_io)
        {
            printf("fygpio errno:%d\n", errno);
            return util::RTData::kCommDisconnected;
        }
    }
/*
    if(0 == fd_th)
    {
        fd_th = ::open("/dev/i2c-1", O_RDWR);
        if(0 >= fd_th)
        {
            return util::RTData::kCommDisconnected;
        }
    }

    if (ioctl(fd_th, I2C_SLAVE, I2C_DEV_ADDR) < 0)
    {
        return util::RTData::kCommDisconnected;
    }
*/

	return util::RTData::kOk;
}

int Device::close()
{
    if(0 != fd_io)
    {
        ::close(fd_io);
    }
/*
    if(0 != fd_th)
    {
        ::close(fd_th);
    }
    */

	return util::RTData::kOk;
}

uint8_t calc_crc(uint8_t data[], uint8_t size)
{
    uint16_t crc = 0;

    for (uint8_t i = 0; i < size; i++)
    {
        crc ^= (data[i]);
        for (uint8_t j = 0; j < 8; j++)
        {
            crc = (crc & 0x80) ? ((crc << 1) ^ CRC_POLY) : (crc << 1);
        }
    }
    return crc;
}

int check_crc(uint8_t data[], uint8_t size, uint8_t crc)
{
    return (calc_crc(data, size) == crc) ? 0 : -1;
}

float calc_temp(uint16_t value)
{
    if ((value & 0x2) != 0) {
        fprintf(stderr, "1ERROR: invalid value\n");
        return -1;
    }
    return -46.85 + (175.72 * (value & 0xFFFC)) / (1 << 16);
}

float calc_humi(uint16_t value)
{
    if ((value & 0x2) == 0) {
        fprintf(stderr, "2ERROR: invalid value\n");
        return -1;
    }

    return -6 + (125.0 * (value & 0xFFFC)) / (1 << 16);
}

int Device::th_exec_command(SHT2x_COMMAND cmd, uint16_t *value)
{
    switch (cmd)
    {
        case CMD_TRIG_TEMP_POLL:
        case CMD_TRIG_HUMI_POLL:
        case CMD_SOFT_RESET:
        if ((::write(fd_th, &cmd, 1)) != 1)
        {
            fprintf(stderr, "ERROR: i2c write\n");
            return -1;
        }
        break;
        case CMD_MEASURE_READ:
        if (th_exec_measure_read(cmd, value) == -1)
        {
            return -1;
        }
        break;
    }
    usleep(10000); // wait 10ms

    return 0;
}

int Device::th_exec_measure_read(SHT2x_COMMAND cmd, uint16_t *value)
{
    uint8_t buf[3];
    for (uint8_t i = 0; i < 2; i++)
    {
        if (::read(fd_th, buf, 3) != 3)
        {
            usleep(MEASURE_RETRY_WAIT);
            continue;
        }
        if (check_crc(buf, 2, buf[2]))
        {
            return util::RTData::kCmdCrcError;
        }
        if (value == NULL)
        {
            fprintf(stderr, "ERROR: invalid function call\n");
            return -1;
        }
        *value = buf[0] << 8 | buf[1];
        return 0;
    }
    fprintf(stderr, "ERROR: i2c read\n");
    return -1;
}

 bool Device::findDir(const char *file_path, const char *adapt_name)
 {
    struct dirent* ent = NULL;
	DIR *pDir = NULL;

	pDir = opendir(file_path);
	if (pDir == NULL)
	{
		//被当作目录，但是执行opendir后发现又不是目录，比如软链接就会发生这样的情况。
		return false;
	}
	while (NULL != (ent = readdir(pDir)))
	{
		if (ent->d_type == 4)
		{
		    if (strncmp(ent->d_name, adapt_name, 2) == 0)
            {
                sprintf(file_full_path_, "%s/%s/w1_slave", file_path, ent->d_name);
                closedir(pDir);
                return true;
            }
		}
	}
	closedir(pDir);
	return false;
 }

 bool Device::get_board_temp()
 {
    if(findDir("/sys/devices/w1_bus_master1", "28-"))
    {
        int fd_tmp = ::open(file_full_path_, O_RDONLY);
        ::read(fd_tmp, temp_buff_, sizeof(temp_buff_));
        ::close(fd_tmp);
        im_string buff = temp_buff_;
        if(buff.find("YES") < 0)
        {
            return util::RTData::kCmdRespError;
        }
        int p_tmp = buff.find("t=");
        memset(temp_buff_, 0, sizeof(temp_buff_));
        char buff_str[256];
        strncpy(buff_str, buff.c_str(), 256);
        memcpy(temp_buff_, &buff_str[p_tmp+2], 5);
        temp_buff_[5] = '\0';
        tem_ = atoi(temp_buff_)/1000.0;
        if (tem_ < 0 || tem_ > 100)
        {
            return false;
        }
        return true;
    }
    else
    {
       return false;
    }
 }

void trim_space(im_string &s)
{
	while(s.find(" ") != im_string::npos)
	{
		s = s.replace(s.find(" "), 1, "");
	}
}

void trim_br(im_string &s)
{
	while(s.find('\n') != im_string::npos)
	{
		s = s.replace(s.find('\n'), 1, "");
	}
}

bool read_str(im_string path, im_string &r)
{

    char r_buff[32] = {0};
    int fd_tmp = 0;
    fd_tmp = ::open(path.c_str(), O_RDONLY);
    if (fd_tmp < 0)
    {
        return false;
    }
    ::read(fd_tmp, r_buff, sizeof(r_buff));
    ::close(fd_tmp);

    im_string s0(r_buff);

    trim_br(s0);
    trim_space(s0);

    r = s0;
    return true;
}

bool Device::get_board_net()
{
    im_string e0_path = "/sys/class/net/eth0/carrier";
    im_string e1_path = "/sys/class/net/eth1/carrier";
    im_string wireless_path = "/tmp/fynetworkd/3gstatus";

    im_string e0,e1, wireless;
    int e0_status, e1_status, wireless_status;
    if(!read_str(e0_path, e0))
    {
        return false;
    }
    else
    {
        e0_status = atoi(e0.c_str());
    }
    if(!read_str(e1_path, e1))
    {
        return false;
    }
    else
    {
        e1_status = atoi(e1.c_str());
    }
    if(!read_str(wireless_path, wireless))
    {
        return false;
    }


    if(wireless.find("=1") != im_string::npos)
    {
        wireless_status = 1;
    }
    else
    {
        wireless_status = 0;
    }

    //judge net status according e0,e1,wireless
    if(wireless_status == 1)
    {
        net_status_ = 3;
    }
    else if (e0_status == 1)
    {
        net_status_ = 2;
    }
    else if(e1_status == 1)
    {
        net_status_ = 1;
    }
    else
    {
        net_status_ = 0;
    }
    return true;
}

bool Device::get_board_interface_flow(im_string interface_name)
{
    total_flow_ = 0;
    im_string rx_str = "/sys/class/net/"+ interface_name + "/statistics/rx_bytes";
    im_string tx_str = "/sys/class/net/"+ interface_name + "/statistics/tx_bytes";
    im_string rx, tx;
    Int64 rx0,tx0;
    if(!read_str(rx_str, rx))
    {
        return false;
    }
    else
    {
        rx0 = atoi(rx.c_str());
    }
    if(!read_str(tx_str, tx))
    {
        return false;
    }
    else
    {
        tx0 = atoi(tx.c_str());
    }
    total_flow_ = (rx0 + tx0)/(1024*1024);

    return true;
}

int Device::get(const CmdFeature& cmd_feature, Tags& tags)
{
/*
    if(0 == fd_io || 0 == fd_th)
    {
        return util::RTData::kCommDisconnected;
    }
    */
    if("extmod" == cmd_feature.cmd)   // 测点数据由外部系统提供
    {
        return util::RTData::kNeedWait;
    }

    if(0 == fd_io)
    {
        return util::RTData::kCommDisconnected;
    }

    int status = 0;
    if(-1 == ::read(fd_io,&status,sizeof(status)))
    {
        ::close(fd_io);
        fd_io = 0;
        return util::RTData::kCommDisconnected;
    }

/*
    if(th_cycle_read %5 == 0)
    {
        th_cycle_read = 0;
        uint16_t temp, humi;
        temp = 0;
        humi = 0;

        if(th_exec_command(CMD_SOFT_RESET, NULL)<0)
        {
            return util::RTData::kCmdReadError;
        }

        if(th_exec_command(CMD_TRIG_TEMP_POLL, NULL)<0)
        {
            return util::RTData::kCmdReadError;
        }
        if(th_exec_command(CMD_MEASURE_READ, &temp)<0)
        {
            return util::RTData::kCmdReadError;
        }

        if(th_exec_command(CMD_TRIG_HUMI_POLL, NULL)<0)
        {
            return util::RTData::kCmdReadError;
        }
        if(th_exec_command(CMD_MEASURE_READ, &humi)<0)
        {
            return util::RTData::kCmdReadError;
        }

        tem_ = calc_temp(temp);
        hum_ = calc_humi(humi);
        if(-1 == tem_ || -1 == hum_)
        {
            return util::RTData::kCmdRespError;
        }
        printf("%ld---%d:read tem and hum once!!!\n", util::IMDateTime::currentDateTime().toUTC(), th_cycle_read);
    }
*/

    Tags::iterator it;
    for(it = tags.begin(); it != tags.end(); ++it)
    {
        if(strcmp((*it)->conf.get_param1.c_str(),"") != 0)
        {
            int     index  = atoi((*it)->conf.get_param1.c_str());
            int     val = (status>>(index-1))&0x1;
            (*it)->value.pv			= util::IMVariant((1==val)?true:false);
            (*it)->value.quality	= util::RTData::kOk;
        }
        else if(strcmp((*it)->conf.get_param2.c_str(),"T") == 0)
        {
            if (get_board_temp())
            {
               (*it)->value.pv			= util::IMVariant(tem_);
               (*it)->value.quality	= util::RTData::kOk;
            }
            else
            {
                (*it)->value.quality	= util::RTData::kCmdReadError;
            }

        }
        else if(strcmp((*it)->conf.get_param2.c_str(),"net_ste") == 0)
        {
            if (get_board_net())
            {
               (*it)->value.pv			= util::IMVariant(net_status_);
               (*it)->value.quality	= util::RTData::kOk;
            }
            else
            {
                (*it)->value.quality	= util::RTData::kCmdReadError;
            }
        }
        else if(strcmp((*it)->conf.get_param2.c_str(),"lan1") == 0)
        {
            if (get_board_interface_flow("eth1"))
            {
               (*it)->value.pv			= util::IMVariant(total_flow_);
               (*it)->value.quality	= util::RTData::kOk;
            }
            else
            {
                (*it)->value.quality	= util::RTData::kCmdReadError;
            }
        }
        else if(strcmp((*it)->conf.get_param2.c_str(),"lan2") == 0)
        {
            if (get_board_interface_flow("eth0"))
            {
               (*it)->value.pv			= util::IMVariant(total_flow_);
               (*it)->value.quality	= util::RTData::kOk;
            }
            else
            {
                (*it)->value.quality	= util::RTData::kCmdReadError;
            }
        }
        else if(strcmp((*it)->conf.get_param2.c_str(),"sim") == 0)
        {
            if (get_board_interface_flow("ppp0"))
            {
               (*it)->value.pv			= util::IMVariant(total_flow_);
               (*it)->value.quality	= util::RTData::kOk;
            }
            else
            {
                (*it)->value.pv			= util::IMVariant(0);
                (*it)->value.quality	= util::RTData::kOk;
            }
        }
        else if(strcmp((*it)->conf.get_param2.c_str(),"H") == 0)
        {
            (*it)->value.pv			= util::IMVariant(hum_);
            (*it)->value.quality	= util::RTData::kOk;
        }

        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
    }
    return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
//    printf("-----Device:set %s %s \n", tag_conf.id.c_str(), tag_value_pv.c_str());
    if(0 == fd_io)
    {
        return util::RTData::kCommDisconnected;
    }

    int cmd = FYGPIO_SET_OUTPUT_CLOSE;
    int val = util::IMVariant(tag_value_pv.c_str()).toInt();
    if(val > 0)
    {
        cmd = FYGPIO_SET_OUTPUT_OPEN;
    }
    int do_no = util::IMVariant(tag_conf.get_param1.c_str()).toInt() -1 ;

    if(0 != ::ioctl(fd_io, cmd, do_no))
    {
        return util::RTData::kCmdRespError;
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

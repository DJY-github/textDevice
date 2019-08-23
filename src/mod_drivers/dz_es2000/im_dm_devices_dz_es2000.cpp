#include "im_dm_devices_dz_es2000.h"
#include "util_imvariant.h"
#include "util_imbufer.h"
#include "util_datetime.h"
#include "util_commonfun.h"
#include "ae_constant.h"
#include "ae_creater.h"
//#include <windows.h>

#include <stdlib.h>
#include <unistd.h>

Device::Device(void)
{
	serial = nullptr;
}

Device::~Device(void)
{
}

void Device::get_version_attr(const DeviceConf& device_info)
{
    //ģ��ver��д��ʽ
    //pw:10AA8048_F0E000000
    int pos = 0;
    im_string ver = device_info.ver;

    if( -1 != (pos=ver.find("pw:")))
    {
        pw_cmd_ = ver.substr(pos+3);
        printf("dz_es2000 verify cmd:%s\n",pw_cmd_.c_str());
    }
}

int Device::open(const DeviceConf& device_info)
{

	//log_.setLogFile( "dev_" + device_info.id + ".log");

	serial = IMSerialPort::getSerial(device_info.channel_identity.c_str());
	if(!serial->isOpen())
	{
		if(!serial->open(device_info.channel_identity.c_str(), device_info.channel_params.c_str()))
		{
			printf("Open %s failed..(func:open)", device_info.channel_identity.c_str());
			return util::RTData::kCommDisconnected;
		}
	}

	dev_id_ = device_info.id;
	get_version_attr(device_info);

	return util::RTData::kOk;
}
int Device::close()
{
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
		printf("Start reopen COMM ..(func:get)");
		if(!serial->reOpen())
		{
			printf("Reopen COMM failed..(func:get)");
			return util::RTData::kCommDisconnected;
		}
	}

	int re = sendDev(cmd_feature);
	if(util::RTData::kOk != re)
	{
		printf("sendDev failed..(func:get)");
		serial->close();
		return re;
	}

	util::IMBuffer<2048> recv_buf;
	re = recvDev(recv_buf);
	if(util::RTData::kOk != re)
	{
		printf("recvDev failed..(func:get)");
		serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // �����Ч�Ĵ��ڻ�������
		serial->close();
		return re;
	}

	Tags::iterator it;
	for(it = tags.begin(); it != tags.end(); ++it)
	{
		if(TagValue::kDigit == (*it)->conf.data_type)
		{
			bool val = false;
			if(!parseDigitVal(recv_buf, atoi((*it)->conf.get_param1.c_str()),(*it)->conf.get_param2, val))
			{
				(*it)->value.quality	= util::RTData::kConfigError;
				(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
				continue;
			}

			(*it)->value.pv			= util::IMVariant(val);
			(*it)->value.quality	= util::RTData::kOk;
			(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
		}
		else if(TagValue::kAnalog == (*it)->conf.data_type)
		{
			if("f" == (*it)->conf.get_param3)
			{
				float val = 0;
				if(!parseFloatVal(recv_buf, atoi((*it)->conf.get_param1.c_str()), atoi((*it)->conf.get_param2.c_str()), val))
				{
					(*it)->value.quality	= util::RTData::kConfigError;
					(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
					continue;
				}

				(*it)->value.pv			= util::IMVariant(val);

			}
			else if ("rcd_ae" == (*it)->conf.get_param3) // �ý��� ���tstack��Ŀ���Ž���¼�¼� ����¼�¼�ת���ɸ澯��¼
            {
                // ������ݳ��ȵ���Ч��
                int start_base = 13;
                int pos = atoi((*it)->conf.get_param1.c_str());
                int val_len = atoi((*it)->conf.get_param2.c_str());
                if(start_base + pos + val_len > recv_buf.size() || val_len !=28 )
                {
                    (*it)->value.quality	= util::RTData::kConfigError;
                    (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                    continue;
                }

                // �����Ѿ���ȡ���¼�
                bool readed;
                parseDigitVal(recv_buf,12,"12.7",readed);
                if(readed)
                {
                    continue;
                }
                im_string event;

                // �������� 7byte
                Time tm;
                parseBCDTime(recv_buf,5,7,tm);

                // ��ǰ����id����
                im_string id;
                parseString(recv_buf,0,10,id);

                // ��ǰ����״̬
                unsigned char status;
                parseByteVal(recv_buf,12,2,status);


                // ������ע 1byte
                unsigned char remark;
                parseByteVal(recv_buf,26,2,remark);
                // ����remark�ٴν���״̬(1byte)���¼���Դ��(5byte)
                if(remark == 0x00)// �Ϸ���ˢ�����ż�¼
                {

                    bool door;

                    parseDigitVal(recv_buf,12,"12.6",door);
                    if (door)
                    {
                        event = id+":"+"ˢ������(�Ŵ��ڿ�״̬)";
                    }
                    else
                    {
                        event = id+":"+"ˢ������(�Ŵ��ڹ�״̬)";
                    }
                }
                else if (remark == 0x02) //Զ��(��SU)���ż�¼
                {
                    bool door;
                    parseDigitVal(recv_buf,12,"12.6",door);
                    if (door)
                    {
                        event = "Զ�̿���(�Ŵ��ڿ�״̬)";
                    }
                    else
                    {
                        event = "Զ�̿���(�Ŵ��ڹ�״̬)";
                    }
                }
                else if (remark == 0x03)//�ֶ����ż�¼
                {
                    bool door;
                    parseDigitVal(recv_buf,12,"12.6",door);
                    if (door)
                    {
                        event = "�ֶ�����(�Ŵ��ڿ�״̬)";
                    }
                    else
                    {
                        event = "�ֶ�����(�Ŵ��ڹ�״̬)";
                    }
                }
                else if (remark == 0x22)//���������¼�
                {
                    if(status != 0)
                    {
                        printf("�Ž�Э�����:remark=22!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    bool emerg_status;
                    parseDigitVal(recv_buf,4,"2",emerg_status);
                    if(emerg_status == 0)
                    {
                        event = "���������¼���ʼ";
                    }
                    else
                    {
                        event = "���������¼�����";
                    }
                }
                else if (remark == 0x05)//���� (�򱨾�ȡ��) ��¼
                {
                    int flag;
                    parseIntVal(recv_buf,0,8,flag);
                    if(flag != 0)
                    {
                        printf("�Ž�Э�����:remark=5!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    unsigned char as;
                    im_string as_status;
                    parseByteVal(recv_buf,4,2,as);
                    if(as == 2)
                    {
                        as_status = "�ſ���";
                    }
                    else if (as == 3)
                    {
                        as_status = "�Źر�";
                    }
                    else if (as == 6)
                    {
                        as_status = "�Ž��ڲ��洢������,�Զ���ʼ��";
                    }
                    else if (as == 9)
                    {
                        as_status = "�������ؼ�ⱻ�ر�";
                    }
                    else if (as == 10)
                    {
                        as_status = "�������ؼ�⿪��";
                    }

                    im_string hand_status,door_status;
                    if((status >> 1) & 1)
                    {
                        hand_status = "����";
                    }
                    else
                    {
                        hand_status = "�ɿ�";
                    }
                    if ((status >> 3) & 1)
                    {
                        door_status = "��";
                    }
                    else
                    {
                        door_status = "��";
                    }

                    event = as_status + ",����״̬(" + hand_status + "),��״̬(" + door_status + ")";
                }
                else if (remark == 0x06)//ES2000�����¼
                {
                    im_string str,power_down;
                    parseString(recv_buf,0,10,str);
                    if(str.length() != 10)
                    {
                        (*it)->value.quality	= util::RTData::kConfigError;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                    }
                    power_down = str.substr(0,2) + "��"+ str.substr(2,2) + "��" + str.substr(4,2) + "ʱ" + str.substr(6,2) + "��" + str.substr(8,2) + "��" + str.substr(4,2);
                    im_string hand_status,door_status;
                    if((status >> 1) & 1)
                    {
                        hand_status = "����";
                    }
                    else
                    {
                        hand_status = "�ɿ�";
                    }
                    if ((status >> 3) & 1)
                    {
                        door_status = "��";
                    }
                    else
                    {
                        door_status = "��";
                    }
                    event = "�Ž������ϵ�:" + power_down + ",����״̬(" + hand_status + "),��״̬(" + door_status + ")";
                }
                else if (remark == 0x07)//�ڲ����Ʋ������޸ĵļ�¼
                {
                    int flag;
                    parseIntVal(recv_buf,0,8,flag);
                    if(flag != 0)
                    {
                        printf("�Ž�Э�����:remark=7!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    unsigned char modify;
                    parseByteVal(recv_buf,4,2,modify);
                    if(modify&1)
                    {
                       event = "�޸���ES2000������";
                    }
                    else if ((modify >> 1) & 1)
                    {
                        event = "�޸����ŵ����Կ��Ʋ���";
                    }
                    else if ((modify >> 2) & 1)
                    {
                        event = "���������û�";
                    }
                    else if ((modify >> 3) & 1)
                    {
                        event = "ɾ�����û�����";
                    }
                    else if ((modify >> 4) & 1)
                    {
                        event = "�޸���ʵʱ��";
                    }
                    else if ((modify >> 5) & 1)
                    {
                        event = "�޸��˿���׼����ʱ������";
                    }
                    else if ((modify >> 6) & 1)
                    {
                        event = "�޸��˽ڼ����б�";
                    }
                    else if ((modify >> 7) & 1)
                    {
                        event = "�޸��˺��⿪�����رգ������ÿ�����";
                    }
                }
                else if (remark == 0x08)//��Ч���û���ˢ����¼
                {
                    if(status != 0)
                    {
                        printf("�Ž�Э�����:remark=8!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    event = id + ":" + "��Ч���û�ˢ��";
                }
                else if (remark == 0x09)//�û�������Ч���ѹ�
                {
                    if(status != 0)
                    {
                        printf("�Ž�Э�����:remark=9!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    event = id + ":"  + "�û�������Ч���ѹ�";
                }
                else if (remark == 0x10)//��ǰʱ����û����޽���Ȩ��
                {
                    if(status != 0)
                    {
                        printf("�Ž�Э�����:remark=10!\n");
                        (*it)->value.quality	= util::RTData::kOutOfValRange;
                        (*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
                        continue;
                    }
                    event = id + ":" + "��ǰʱ����û����޽���Ȩ��";
                }
                // ��event����澯ģ��
                // TODO:
                //
                ae::Alarm alarm;
                alarm.ciid = dev_id_ + "." + (*it)->conf.id;
                alarm.desc = event;
                alarm.alarmid= util::IMUuid::createUuid();
                alarm.end_State = ae::alarm_continuous;
                alarm.begin_time = util::IMDateTime::currentDateTime().toUTC();
                alarm.level	= 1;
                alarm.dev_name = "dac";
                alarm.devid = dev_id_;

			    ae::creater::AlarmMgr::instance()->post(&alarm);

            }
			else
			{
				if("2" == (*it)->conf.get_param2)
				{
					unsigned char val = 0;
					if(!parseByteVal(recv_buf, atoi((*it)->conf.get_param1.c_str()), atoi((*it)->conf.get_param2.c_str()), val))
					{
						(*it)->value.quality	= util::RTData::kConfigError;
						(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
						continue;
					}

					(*it)->value.pv			= util::IMVariant((unsigned short)val);
				}
				else if("4" == (*it)->conf.get_param2)
				{
					unsigned short val = 0;
					if(!parseShortVal(recv_buf, atoi((*it)->conf.get_param1.c_str()), atoi((*it)->conf.get_param2.c_str()), val))
					{
						(*it)->value.quality	= util::RTData::kConfigError;
						(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
						continue;
					}

					(*it)->value.pv			= util::IMVariant(val);

				}
				else if("8" == (*it)->conf.get_param2)
				{
					int val = 0;
					if(!parseIntVal(recv_buf, atoi((*it)->conf.get_param1.c_str()), atoi((*it)->conf.get_param2.c_str()), val))
					{
						(*it)->value.quality	= util::RTData::kConfigError;
						(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
						continue;
					}

					(*it)->value.pv			= util::IMVariant(val);

				}
			}

			(*it)->value.quality	= util::RTData::kOk;
			(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();



		}
		else if(TagValue::kEnumStatus == (*it)->conf.data_type)
		{
			unsigned char val = 0;
			if(!parseEnumVal(recv_buf, atoi((*it)->conf.get_param1.c_str()),(*it)->conf.get_param2, val))
			{
				(*it)->value.quality	= util::RTData::kConfigError;
				(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
				continue;
			}

			(*it)->value.pv			= util::IMVariant((unsigned short)val);
			(*it)->value.quality	= util::RTData::kOk;
			(*it)->value.timestamp	= util::IMDateTime::currentDateTime().toUTC();
		}
	}

	return util::RTData::kOk;
}

int Device::set(const CmdFeature& cmd_feature, const Tag::Conf& tag_conf, const libistring& tag_value_pv)
{
	if(!serial->isOpen())
	{
		if(!serial->reOpen())
		{
			printf("Reopen COMM failed..(func:set)\n");
			return -1;
		}
	}

	int veri_pos = tag_conf.set_param3.find("pw");
	if(-1 != veri_pos)// need verify
    {
        CmdFeature cmd_verify;
        cmd_verify.addr = cmd_feature.addr;
        cmd_verify.cmd = pw_cmd_;
        int re = sendDev(cmd_verify);
        if(util::RTData::kOk != re)
        {
            printf("sendDev failed..(func:set)\n");
            serial->close();
            return util::RTData::kCmdRespError;
        }
        util::IMBuffer<2048> recv_buf;
        if(recvDev(recv_buf))
        {
            serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // �����Ч�Ĵ��ڻ�������
            printf("recvDev failed..(recvDev:%s, func:set)\n",recv_buf.ptr(0));
            return util::RTData::kCmdRespError;
        }

        // ����BTN
        util::IMBuffer<1> rtn;
        rtn.fromHex(recv_buf.ptr(7), 2);
        unsigned short RTN = rtn[0];
        if (RTN != 0)
        {
            printf("Set failed..(BTN:%d)\n",RTN);
            return util::RTData::kCmdRespError;
        }

       // return util::RTData::kOk;
    }

	if(TagValue::kDigit == tag_conf.data_type)
	{
		im_string key = "K" + tag_value_pv + "-";
		int pos = tag_conf.set_param1.find(key);
		if(-1 != pos)
		{
			int dataset_len = atoi(tag_conf.set_param2.c_str());
			if(dataset_len < 0 || dataset_len + pos >= (int)tag_conf.set_param1.size())
			{
				return util::RTData::kConfigError;
			}
			im_string data_set = tag_conf.set_param1.substr(pos+3, dataset_len);
			if(sendDev(cmd_feature, data_set.c_str()))
			{
				util::IMBuffer<2048> recv_buf;
				if(recvDev(recv_buf))
				{
					serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // �����Ч�Ĵ��ڻ�������

					printf("recvDev failed..(recvDev:%s, func:set)\n",recv_buf.ptr(0));
					return util::RTData::kCmdRespError;
				}

				return util::RTData::kOk;
			}
		}
		else
        {
            int re = sendDev(cmd_feature);
            if(util::RTData::kOk != re)
            {
                printf("sendDev failed..(func:set)\n");
                serial->close();
                return util::RTData::kCmdRespError;
            }
            util::IMBuffer<2048> recv_buf;
            if(recvDev(recv_buf))
            {
                serial->read(recv_buf.ptr(0, 2048), 2048, 1000); // �����Ч�Ĵ��ڻ�������
                printf("recvDev failed..(recvDev:%s, func:set)\n",recv_buf.ptr(0));
                return util::RTData::kCmdRespError;
            }

            // ����BTN
            util::IMBuffer<1> btn;
            btn.fromHex(recv_buf.ptr(7), 2);
            unsigned short BTN = btn[0];
            if (BTN != 0)
            {
                printf("Set failed..(BTN:%d)\n",BTN);
                return util::RTData::kCmdRespError;
            }
            return util::RTData::kOk;

        }
	}
	else if(TagValue::kAnalog == tag_conf.data_type)
	{

	}

	return util::RTData::kConfigError;
}



unsigned short makeDataLen( int nLen )
{
	unsigned short wSum = (-(nLen + (nLen>>4) + (nLen>>8) )) << 12;

	return (unsigned short)( (nLen&0x0FFF) | wSum );
}

/*
unsigned short makeDataLen( int nLen )
{
	unsigned short wSum = (nLen&0x0000000F) + ((nLen>>4)&0x0000000F) + ((nLen>>8)&0x0000000F) ;

	wSum = wSum%16;
	wSum = ~wSum + 1;

	return wSum;
}*/

unsigned short datalen2LenID(int lenth)
{
	return (lenth&0x0FFF);
}

unsigned short checkSum(const char *frame , const int len)
{
	unsigned short cs = 0;
	for(int i=0; i<len; i++)        // �����ۼӺͣ�������ͷ�ַ�
	{
		cs += *(frame + i);
	}

	return -cs;
}

int Device::sendDev(const CmdFeature& cmd_feature, const char* data_info)
{
	usleep(500000);
	int pos = 0;
	char ver_addr_cid[9] = {0};
	memcpy(ver_addr_cid, cmd_feature.cmd.c_str(), 8);
	char real_addr[3] = {0};
	im_sprintf(real_addr,3, "%02X", atoi(cmd_feature.addr.c_str()));
	ver_addr_cid[2]	= real_addr[0];
	ver_addr_cid[3]	= real_addr[1];

	char send_buf[33] = {0};
	if(0 == data_info)
	{
		int data_info_pos = cmd_feature.cmd.find("_");
		if(std::string::npos == (unsigned int)data_info_pos)
		{
		   // printf("addr:%s\ncmd_feature:%s,%s\n",cmd_feature.addr.c_str(),cmd_feature.cmd.c_str(),ver_addr_cid);

			unsigned short lenid = makeDataLen(0);
			printf("lenid:%d\n",lenid);
			im_sprintf(send_buf, 14, "~%s%04X\0",ver_addr_cid, lenid);
			pos += 13;   // 1�ֽ�ͷ + 8�ֽ�ver_addr_cid + 4�ֽ�lenth
		}
		else
		{
			im_string strinfo =  cmd_feature.cmd.substr(data_info_pos+1);
			unsigned short info_len = strinfo.size();
			unsigned short lenid = makeDataLen(info_len);
			im_sprintf(send_buf, 14+info_len, "~%s%04X%s\0",ver_addr_cid, lenid, strinfo.c_str());
			pos += 13 + info_len;
		}

	}
	else
	{
		unsigned short info_len = strlen(data_info);
		unsigned short lenid = makeDataLen(info_len);
		im_sprintf(send_buf, 14+info_len, "~%s%04X%s\0",ver_addr_cid, lenid, data_info);
		pos += 13 + info_len;
	}
	unsigned short cs = checkSum(&send_buf[1], pos -1);
	char cs_tail[5] = {0};
 	im_sprintf(cs_tail, 5,"%04X",cs);
	memcpy(send_buf+pos, cs_tail, 4);
	pos += 4;
	send_buf[pos++] = 0x0D;

	if(pos != serial->write(send_buf, pos))
	{
		printf("Send COMM Data failed(%s)\n", send_buf);
		return util::RTData::kCmdNoResp;
	}

	return util::RTData::kOk;
}


int Device::recvDev(util::IMBuffer<2048>& recv_buf)
{
	usleep(100000);
	// 1.����Э��ͷ��13���ֽ�
	int head_len = 13;
	int read_len = serial->read(recv_buf.ptr(0, head_len), head_len, 10000);
	if(0 == read_len)
	{
		printf("Command no respond.\n");
		return util::RTData::kCmdNoResp;
	}
	else if(read_len != head_len)
	{
		printf("Recv HeadLen Failed (head_len:%d, read_len:%d\n)", head_len, read_len);
		return util::RTData::kCmdRespError;
	}

	// 2.����LENID
	util::IMBuffer<2> str_lenth;
	str_lenth.fromHex(recv_buf.ptr(head_len-4), 4);
	unsigned short lenid = (str_lenth[0] << 8) + str_lenth[1];
	lenid = datalen2LenID(lenid);

	usleep(100000);
	// 3.��������
	int protocol_len = lenid + 5; // 4�ֽ�У��λ + 1�ֽڽ�����
	read_len = serial->read(recv_buf.ptr(head_len, protocol_len), protocol_len, 10000);
	if(read_len != protocol_len)
	{
		printf("Recv Body Failed (body_len:%d, read_len:%d\n)", protocol_len, read_len);
		return util::RTData::kCmdRespError;
	}

	// 4.У����֤
	unsigned short cs = checkSum(recv_buf.ptr(1), protocol_len + head_len -2 -4);
	util::IMBuffer<2> recv_str_cs;
	recv_str_cs.fromHex(recv_buf.ptr(recv_buf.size()-5), 4);
	unsigned short recv_cs = (recv_str_cs[0] << 8) + recv_str_cs[1];
	if(cs != recv_cs)
	{
		printf("CheckSum Failed (recv_buf:%s\n)", recv_buf.ptr(0));
		return util::RTData::kCmdRespError;
	}

	return util::RTData::kOk;

}

bool Device::parseByteVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, unsigned char& val)
{
	int start_base = 13;
	if(pos + val_len + start_base > recv_buf.size() || val_len != 2 )
	{
		return false;
	}

	util::IMBuffer<1> data;
	data.fromHex(recv_buf.ptr(start_base+pos), val_len);
	val = data[0];

	return true;
}
bool Device::parseShortVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, unsigned short& val)
{
	int start_base = 13;
	if(pos + val_len + start_base > recv_buf.size() || val_len != 4 )
	{
		return false;
	}

	util::IMBuffer<2> data;
	data.fromHex(recv_buf.ptr(start_base+pos), val_len);
	val = (data[0] << 8) + data[1];

	return true;
}
bool Device::parseIntVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, int& val)
{
	int start_base = 13;
	if(pos + val_len + start_base > recv_buf.size() || val_len != 8 )
	{
		return false;
	}

	util::IMBuffer<4> data;
	data.fromHex(recv_buf.ptr(start_base+pos), val_len);
	val = (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3];

	return true;
}
bool Device::parseFloatVal(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, float& val)
{
	int start_base = 13;
	if(pos + val_len + start_base > recv_buf.size() || val_len !=8 )
	{
		return false;
	}

	util::IMBuffer<4> data;
	data.fromHex(recv_buf.ptr(start_base+pos), val_len);

	memcpy(&val, &data[0], 4);

	return true;
}


bool Device::parseDigitVal(const util::IMBuffer<2048>& recv_buf, int pos, const libistring& param2, bool& val)
{
	int start_base = 13;

	if(pos + start_base > recv_buf.size())
	{
		return false;
	}

	util::IMBuffer<1> data;
	data.fromHex(recv_buf.ptr(start_base+pos), 2);
	unsigned char hex =  data[0];

	int flag = param2.find(".");
	if(-1 != flag)		// ȡλ
	{
		int bit = atoi(param2.substr(flag+1).c_str());
		val	= (hex >> bit) & 1;

		return true;
	}

	flag = param2.find("?");
	if(-1 != flag) // ȡֵ�Ƿ����
	{
		im_string condi = param2.substr(flag+1);
		if(-1 != condi.find(","))
		{
			val = false;
			std::vector<im_string> vec_val = util::spliteString(condi, ',');
			std::vector<im_string>::const_iterator it = vec_val.begin();
			for (it = vec_val.begin(); it != vec_val.end(); it++)
			{
				if(atoi((*it).c_str()) == hex)
				{
					val = true;
					break;
				}
			}

		}
		else
		{
			val = (atoi(condi.c_str()) == hex)?true:false;
		}

		return true;

	}


	//ֱ��ȡֵ
	val = hex>0?true:false;
	return true;
}

 bool Device::parseEnumVal(const util::IMBuffer<2048>& recv_buf, int pos, const libistring& param2, unsigned char& val)
 {
	 int start_base = 13;

	 if(pos + start_base > recv_buf.size())
	 {
		 return false;
	 }

	 util::IMBuffer<1> data;
	 data.fromHex(recv_buf.ptr(start_base+pos), 2);
	 val =  data[0];

	 return true;
 }

bool Device::parseString(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, im_string& val)
{
    int start_base = 13;

    if(start_base + pos + val_len > recv_buf.size())
	{
		return false;
	}
	char buff[2048] = {0};
    for(int i=0;i<val_len/2;i++)
    {
        util::IMBuffer<1> data;
        data.fromHex(recv_buf.ptr(start_base+pos+i*2), 2);
        sprintf(buff+i*2,"%02x",data[0]);
    }
    val = buff;

    return true;
}

Time metis_strptime(const char *str_time)
{
	struct tm stm;
	strptime(str_time,"%Y%m%d%H%M%S",&stm);
	Time t= mktime(&stm);
	return t;
}

 bool Device::parseBCDTime(const util::IMBuffer<2048>& recv_buf, int pos, int val_len, Time& val)
 {
    int start_base = 13;

    if(pos + val_len + start_base > recv_buf.size() || val_len !=7 )
	{
		return false;
	}
	im_string val_time;
	if (!parseString(recv_buf,pos,val_len,val_time))
    {
        return false;
    }
	val = metis_strptime(val_time.c_str());
    return true;
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



#include "basetype.h"
#include "errlog.h"
#include "PPCS_API.h"
#include "vavahal.h"
#include "vavaserver.h"
#include "watchdog.h"
#include "aesencrypt.h"
#include "record.h"

#define NO_STREAM_CHECK_TIME	(25*5) //5秒

VAVA_Rec_Manage g_manaulrec[MAX_CHANNEL_NUM];
VAVA_Rec_Manage g_alarmrec[MAX_CHANNEL_NUM];
VAVA_Rec_Manage g_fulltimerec[MAX_CHANNEL_NUM];
VAVA_Rec_Manage g_cloudrec[MAX_CHANNEL_NUM];

#if !defined(mbytes_write_int_big_endian_4b)
#define mbytes_write_int_big_endian_4b(_buf, _i_value) \
    do{\
        ((unsigned char*)(_buf))[0] = (unsigned char)(((_i_value) >> 24) & 0xff);\
        ((unsigned char*)(_buf))[1] = (unsigned char)(((_i_value) >> 16) & 0xff);\
        ((unsigned char*)(_buf))[2] = (unsigned char)(((_i_value) >> 8) & 0xff);\
        ((unsigned char*)(_buf))[3] = (unsigned char)((_i_value) & 0xff);\
    }while(0)
#endif

#ifdef CLOUDE_SUPPORT
static int getspsandpps(unsigned char *buff, unsigned char *spsbuff, unsigned char *ppsbuff, int *spssize, int *ppssize)
{
	int i;
	char tmpstr[4];
	char tmpbuff[30];

	for(i = 0; i < 100; i++)
	{
		if(buff[i] == 0 && buff[i + 1] == 0 && buff[i + 2] == 0 && buff[i + 3] == 1 && (buff[i + 4] & 0x1F) == 8)
		{
			*spssize = i - 4;
			memcpy(spsbuff, buff + 4, *spssize);
			break;
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: spssize = %d\n", FUN, LINE, *spssize);

	memset(tmpbuff, 0, 30);
	strcpy(tmpbuff, "[ ");
	for(i = 0; i < *spssize; i++)
	{
		memset(tmpstr, 0, 4);
		sprintf(tmpstr, "%x ", spsbuff[i]);
		strcat(tmpbuff, tmpstr);
	}
	strcat(tmpbuff, "]");

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);

	for(i = *spssize + 4; i < 100; i++)
	{
		if(buff[i] == 0 && buff[i + 1] == 0 && buff[i + 2] == 0 && buff[i + 3] == 1 && (buff[i + 4] & 0x1F) == 6)
		{
			*ppssize = i - *spssize - 8;
			memcpy(ppsbuff, buff + *spssize + 8, *ppssize);
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ppssize = %d\n", FUN, LINE, *ppssize);

	memset(tmpbuff, 0, 30);
	strcpy(tmpbuff, "[ ");
	for(i = 0; i < *ppssize; i++)
	{
		memset(tmpstr, 0, 4);
		sprintf(tmpstr, "%x ", ppsbuff[i]);
		strcat(tmpbuff, tmpstr);
	}
	strcat(tmpbuff, "]");

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);

	return 0;
}
#endif

void *Rec_FullTime_pth(void *data)
{
	int channel = *(int *)data;	

	int ret;
	int av_read;
	
	int offset;
	int size;
	int type;
	unsigned long long ntsamp;

	int totolsize = 0;
	int totoltime = 0;
	int totolframe = 0;

	int count = -1;

	struct timeval t_start;
	struct timeval t_current;

	//时间戳(增量时间)
	unsigned long long save = 0;
	unsigned long long increment;

	VAVA_RecHead av_head;
	VAVA_RecInfo recinfo;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);
		
		Err_Log("fulltime record channel err");
		return NULL;
	}

	while(g_running)
	{
		if(g_pair[channel].nstat == 0 || g_gatewayinfo.sdstatus != VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.totol < 100
			|| g_gatewayinfo.format_flag == 1 || g_fulltimerec[channel].ctrl == 0 || g_alarmrec[channel].ctrl == 1
			|| g_manaulrec[channel].ctrl == 1 || g_fulltimerec[channel].status == VAVA_REC_STATUS_STOP 
			|| g_camerainfo[channel].first_flag == 1 || g_camerainfo_dynamic[channel].online == 0)
		{
			sleep(1);
			continue;
		}

		//创建文件
		recinfo.tag = 0;
		recinfo.v_encode = VAVA_ENCODE_H264;
		recinfo.a_encode = g_camerainfo[channel].audiocodec;
		recinfo.res = g_camerainfo[channel].m_res;
		recinfo.fps = g_camerainfo[channel].m_fps;
		recinfo.vframe = 0;
		recinfo.size = 0;
		recinfo.time = 0;

		totolsize = 0;
		totoltime = 0;
		totolframe = 0;
		
		ret = Rec_Create_Fulltime_Recfile(channel, &recinfo);
		if(ret != 0)
		{
			sleep(3);
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: FullTime Record start [%d] =========\n", FUN, LINE, channel);

		av_read = g_avmemchace[channel].mv_write;
		gettimeofday(&t_start, NULL);

		while(g_running && g_pair[channel].nstat == 1 && g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD
			  && g_gatewayinfo.totol > 100 && g_gatewayinfo.format_flag == 0 && g_fulltimerec[channel].ctrl == 1 
			  && g_alarmrec[channel].ctrl == 0 && g_manaulrec[channel].ctrl == 0 && g_camerainfo[channel].first_flag == 0
			  && g_camerainfo_dynamic[channel].online == 1)
		{
			if(count >= 0)
			{
				//每20帧检测一次
				count++;
				if(count >= 20)
				{
					count = 0;
					gettimeofday(&t_current, NULL);

					if(t_current.tv_sec - t_start.tv_sec >= 300)
					{
						//超过5分钟自动生成新文件
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] fulltime more than 300 sec\n", FUN, LINE, channel);
						break;
					}
				}
			}

			if(av_read == -1)
			{
				av_read = g_avmemchace[channel].mv_write;
				if(av_read == -1)
				{
					usleep(40000);
					continue;
				}
			}
			
			if(av_read == g_avmemchace[channel].mv_write)
			{
				usleep(40000);
				continue;
			}

			offset = g_avmemchace[channel].mvlists[av_read].offset;
			size = g_avmemchace[channel].mvlists[av_read].size;
			type = g_avmemchace[channel].mvlists[av_read].ftype; 
			ntsamp = g_avmemchace[channel].mvlists[av_read].ntsamp;

			if(size <= 0)
			{
				av_read++;
				if(av_read >= MEM_MAIN_QUEUE_NUM)
				{
					av_read = 0;
				}
				
				continue;
			}

			if(g_fulltimerec[channel].start == 0)
			{
				if(type == 1)
				{
					gettimeofday(&t_start, NULL);
					g_fulltimerec[channel].start = 1;

					save = ntsamp;
					count = 0;
				}
			}

			if(g_fulltimerec[channel].start == 1)
			{
				//写入数据
				memset(&av_head, 0, sizeof(VAVA_RecHead));
				av_head.tag = VAVA_REC_HEAD;
				av_head.size = size;
				av_head.type = type;

				//统计帧数
				if(type == 0 || type == 1)
				{
					totolframe++;
				}

				//增量时间戳
				increment = ntsamp - save;
				if(increment <= 0)
				{
					av_head.time_sec = 0;
					av_head.time_usec = 0;
				}
				else
				{
					av_head.time_sec = increment / 1000;
					av_head.time_usec = increment % 1000;
				}

				if(fwrite(&av_head, sizeof(VAVA_RecHead), 1, g_fulltimerec[channel].fd) != 1)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [channel-%d] write record head err\n", FUN, LINE, channel);
					break;
				}
				
				if(fwrite(g_avmemchace[channel].pmvMemBegin + offset, size, 1, g_fulltimerec[channel].fd) != 1)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [channel-%d] write record data err\n", FUN, LINE, channel);
					break;
				}
				
				totolsize += size;
			}

			av_read++;
			if(av_read >= MEM_MAIN_QUEUE_NUM)
			{
				av_read = 0;
			}
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] totolsize = %d\n", FUN, LINE, channel, totolsize);

		if(totolsize <= 0)
		{
			Rec_Close_Fulltime_Recfile(channel, NULL, 0);
		}
		else
		{
			//统计时长
			totoltime = totolframe / g_camerainfo[channel].m_fps;
			if(totolframe % g_camerainfo[channel].m_fps != 0)
			{
				totoltime += 1;
			}
			
			//统计数据
			recinfo.tag = 1;
			recinfo.v_encode = g_camerainfo[channel].videocodec;
			recinfo.a_encode = g_camerainfo[channel].audiocodec;
			recinfo.res = g_camerainfo[channel].m_res;
			recinfo.fps = g_camerainfo[channel].m_fps;
			recinfo.vframe = totolframe;
			recinfo.size = totolsize;
			recinfo.time = totoltime; 

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] totolframe = %d, fps = %d\n", 
			                                FUN, LINE, channel, totolframe, g_camerainfo[channel].m_fps);

			Rec_Close_Fulltime_Recfile(channel, &recinfo, totoltime);
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] tFullTime Record Stop =========\n", FUN, LINE, channel);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

void *Rec_Manaul_pth(void *data)
{
	int channel = *(int *)data;

	int ret;
	int av_read;

	int totolsize = 0;
	int totoltime = 0;
	int totolframe = 0;

	int count = -1;
	int endtype = VAVA_REC_STOP_NAMAL;

	//流信息
	int offset;
	int size;
	int type;
	int fps;
	unsigned long long ntsamp;

	//开始和结束时间戳(用于计算录像时间)
	int start_ntsamp;
	int end_ntsamp;
	int save_ntsamp;
	
	//时间戳(增量时间)
	unsigned long long save = 0;
	unsigned long long increment;

	struct timeval t_current;
	struct timeval t_start;

	VAVA_RecHead av_head;
	VAVA_RecInfo recinfo;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);
		
		Err_Log("manaul record channel err");
		return NULL;
	}

	while(g_running)
	{
		if(g_pair[channel].nstat == 0 || g_gatewayinfo.sdstatus != VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.totol < 100 
			|| g_gatewayinfo.format_flag == 1 || g_manaulrec[channel].ctrl == 0 
			|| g_manaulrec[channel].status == VAVA_REC_STATUS_STOP || g_camerainfo[channel].first_flag == 1
			|| g_camerainfo_dynamic[channel].online == 0)
		{
			usleep(50000);
			continue;
		}

		//创建文件
		recinfo.tag = 0;
		recinfo.v_encode = VAVA_ENCODE_H264;
		recinfo.a_encode = g_camerainfo[channel].audiocodec;
		recinfo.res = g_camerainfo[channel].m_res;
		recinfo.fps = g_camerainfo[channel].m_fps;
		recinfo.encrypt = VAVA_REC_ENCRYPT_AES;
		recinfo.vframe = 0;
		recinfo.size = 0;
		recinfo.time = 0;

		totolsize = 0;
		totoltime = 0;
		totolframe = 0;

		ret = Rec_Cteate_Manual_Recfile(channel, &recinfo);
		if(ret != 0)
		{
			VAVAHAL_CheckTFRWStatus();
			
			sleep(3);
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d], Manual Record start ========= session - %d\n", 
		                                FUN, LINE, channel, g_manaulrec[channel].session);

		av_read = g_avmemchace[channel].mv_write;

		start_ntsamp = -1;
		end_ntsamp = -1;
		save_ntsamp = -1;

		while(g_running && g_pair[channel].nstat == 1 && g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD 
			  && g_gatewayinfo.totol > 100 && g_gatewayinfo.format_flag == 0 && g_manaulrec[channel].ctrl == 1 
			  && g_camerainfo[channel].first_flag == 0 && g_camerainfo_dynamic[channel].online == 1)
		{
			if(count >= 0)
			{
				//每20帧检测一次
				count++;
				if(count >= 20)
				{
					count = 0;
					gettimeofday(&t_current, NULL);

					if(t_current.tv_sec - t_start.tv_sec >= (300 + 60) && g_manaulrec[channel].start == 1)
					{
						//超过5分钟自动关闭文件
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d], manaul more than 300 sec\n", FUN, LINE, channel);
						
						endtype = VAVA_REC_STOP_TIMEOUT;
						break;
					}
				}
			}

			if(av_read == -1)
			{
				av_read = g_avmemchace[channel].mv_write;
				if(av_read == -1)
				{
					usleep(40000);
					continue;
				}
			}
			
			if(av_read == g_avmemchace[channel].mv_write)
			{
				usleep(40000);
				continue;
			}

			offset = g_avmemchace[channel].mvlists[av_read].offset;
			size = g_avmemchace[channel].mvlists[av_read].size;
			type = g_avmemchace[channel].mvlists[av_read].ftype; 
			fps = g_avmemchace[channel].mvlists[av_read].fps;
			ntsamp = g_avmemchace[channel].mvlists[av_read].ntsamp;

			if(size <= 0)
			{
				av_read++;
				if(av_read >= MEM_MAIN_QUEUE_NUM)
				{
					av_read = 0;
				}
				
				continue;
			}

			if(g_manaulrec[channel].start == 0)
			{
				if(type == 1)
				{
					gettimeofday(&t_start, NULL);
					g_manaulrec[channel].start = 1;
					g_manaul_ntsamp[channel] = ntsamp / 1000;

					save = ntsamp;
					count = 0;

					//开始时间
					start_ntsamp = (int)(ntsamp / 1000);
					save_ntsamp = (int)(ntsamp / 1000);
					end_ntsamp = (int)(ntsamp / 1000);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d], stream rec start [%d]\n", 
						                            FUN, LINE, channel, start_ntsamp);
				}
			}

			if(g_manaulrec[channel].start == 1)
			{
				//写入数据
				memset(&av_head, 0, sizeof(VAVA_RecHead));
				av_head.tag = VAVA_REC_HEAD;
				av_head.size = size;
				av_head.type = type;
				av_head.fps = fps;

				//统计帧数
				if(type == 0 || type == 1)
				{
					totolframe++;

					//结束时间戳实时更新
					save_ntsamp = end_ntsamp;
					end_ntsamp = (int)(ntsamp / 1000);
				}

				//增加录像时长判断
				if(end_ntsamp - start_ntsamp >= g_rectime.manaua_time + 1)
				{
					break;
				}

				//增量时间戳
				increment = ntsamp - save;
				if(increment <= 0)
				{
					av_head.time_sec = 0;
					av_head.time_usec = 0;
				}
				else
				{
					av_head.time_sec = increment / 1000;
					av_head.time_usec = increment % 1000;
				}

				if(fwrite(&av_head, sizeof(VAVA_RecHead), 1, g_manaulrec[channel].fd) != 1)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [channel-%d], write record head err\n", FUN, LINE, channel);
					break;
				}

				if(type == 1)
				{
					VAVA_Aes_Encrypt(g_avmemchace[channel].pmvMemBegin + offset, g_avmemchace[channel].pmvMemBegin + offset, size);
				}
				else if(type == 8)
				{
					unsigned char *p = (unsigned char *)(g_avmemchace[channel].pmvMemBegin + offset);
					p[0] = 0xBB;
					VAVA_Aes_Encrypt(g_avmemchace[channel].pmvMemBegin + offset + 1, g_avmemchace[channel].pmvMemBegin + offset + 1, size - 1);
				}
				
				if(fwrite(g_avmemchace[channel].pmvMemBegin + offset, size, 1, g_manaulrec[channel].fd) != 1)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [channel-%d], write record data err\n", FUN, LINE, channel);
					break;
				}
				
				totolsize += size;
			}

			av_read++;
			if(av_read >= MEM_MAIN_QUEUE_NUM)
			{
				av_read = 0;
			}
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d], stream rec end [%d], session - %d\n", 
		                                FUN, LINE, channel, save_ntsamp, g_manaulrec[channel].session);

		if(totolsize <= 0)
		{
			Rec_Close_Manual_Recfile(channel, NULL, 0);
		}
		else
		{
			#if 1 //使用时间戳计算方式
			totoltime = save_ntsamp - start_ntsamp;

			if(totoltime < 0)
			{
				Rec_Close_Manual_Recfile(channel, NULL, 0);
			}
			else
			{
				if(totoltime == 0)
				{
					totoltime = 1;
				}
				#else
				//统计时长
				totoltime = totolframe / g_camerainfo[channel].m_fps;
				if(totolframe % g_camerainfo[channel].m_fps != 0)
				{
					totoltime += 1;
				}
				#endif
				
				//统计数据
				recinfo.tag = 1;
				recinfo.v_encode = g_camerainfo[channel].videocodec;
				recinfo.a_encode = g_camerainfo[channel].audiocodec;
				recinfo.res = g_camerainfo[channel].m_res;
				recinfo.fps = g_camerainfo[channel].m_fps;
				recinfo.encrypt = VAVA_REC_ENCRYPT_AES;
				recinfo.vframe = totolframe;
				recinfo.size = totolsize;
				recinfo.time = totoltime;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d], totolframe = %d, alarm time = %d\n", 
		                                        FUN, LINE, channel, totolframe, totoltime);

				Rec_Close_Manual_Recfile(channel, &recinfo, totoltime);
			}
		}

		VAVAHAL_SendRecStop(g_manaulrec[channel].session, channel, endtype);

		endtype = VAVA_REC_STOP_NAMAL;
		
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d], Manual Record Stop =========\n", FUN, LINE, channel);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

void *Rec_Alarm_pth(void * data)
{
	int channel = *(int *)data;

	int ret;
	int av_read;

	int cmdsock;
	int recordstart = 1;

	int totolsize = 0;
	int totoltime = 0;
	int totolframe = 0;

	VAVA_RecHead av_head;
	VAVA_RecInfo recinfo;

	//流信息
	int offset;
	int size;
	int type;
	int fps;
	unsigned long long ntsamp;

	//开始和结束时间戳(用于计算录像时间)
	int start_ntsamp;
	int end_ntsamp;
	int save_ntsamp;

#ifdef ALARM_PHOTO
	//图片时间戳
	int photontsamp;

	int savephoto = 0;
	int saveflag = 0;
	
	char tmpdir[9];
	char tmpfile[11];
#endif

	struct timeval t_out_start;
	struct timeval t_out_current;

	unsigned long long save = 0;
	unsigned long long increment;

	int nodatacheck = 1;
	int pushflag = 0;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);
		
		Err_Log("record channel err");
		return NULL;
	}

	while(g_running)
	{
		if(g_pair[channel].nstat == 0 || g_gatewayinfo.sdstatus != VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.totol <= 100 
			|| g_gatewayinfo.format_flag == 1 || g_alarmrec[channel].ctrl == 0 
			|| g_alarmrec[channel].status == VAVA_REC_STATUS_STOP || g_camerainfo[channel].first_flag == 1
			|| g_camerainfo_dynamic[channel].online == 0)
		{
			g_camerainfo[channel].alarm_flag = 0;
			
			usleep(50000);
			continue;
		}

		#if 0
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [WAKE-UP][TF-Rec] channel - %d\n", FUN, LINE, channel);

		ret = VAVAHAL_WakeupCamera_Ex(channel, WAKE_UP_MODE_PIR);
		if(ret != VAVA_ERR_CODE_SUCCESS)
		{
			//唤醒失败 停止本次录像
			g_alarmrec[channel].ctrl = 0;
			continue;
		}
		#endif

		g_camerainfo[channel].alarm_flag = 1;
		g_md_result[channel] = 0;

		if(g_cloudflag[channel] == 1)
		{
			VAVAHAL_StartAlarmCloud(channel);
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *** channel - %d, alarm = %d, cloud = %d ***\n", 
		                                FUN, LINE, channel, g_camerainfo[channel].alarm_flag, g_cloudflag[channel]);

		//创建文件
		recinfo.tag = 0;
		recinfo.v_encode = VAVA_ENCODE_H264;
		recinfo.a_encode = g_camerainfo[channel].audiocodec;
		recinfo.res = g_camerainfo[channel].m_res;
		recinfo.fps = g_camerainfo[channel].m_fps;
		recinfo.encrypt = VAVA_REC_ENCRYPT_AES;
		recinfo.vframe = 0;
		recinfo.size = 0;
		recinfo.time = 0;

		totolsize = 0;
		totoltime = 0;
		totolframe = 0;

		ret = Rec_Cteate_Alarm_Recfile(channel, &recinfo);
		if(ret != 0)
		{
			VAVAHAL_CheckTFRWStatus();
			
			sleep(3);
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] Alarm Record start =========\n", FUN, LINE, channel);

		gettimeofday(&t_out_start, NULL);
		av_read = g_avmemchace[channel].mv_write;

		start_ntsamp = -1;
		end_ntsamp = -1;
		save_ntsamp = -1;

		while(g_running && g_pair[channel].nstat == 1 && g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD
			  && g_gatewayinfo.totol > 100 && g_gatewayinfo.format_flag == 0 && g_alarmrec[channel].ctrl == 1 
			  && g_camerainfo[channel].first_flag == 0 && g_camerainfo_dynamic[channel].online == 1)
		{
			if(recordstart == 1)
			{
				cmdsock = VAVAHAL_ReadSocketId(0, channel);
				if(cmdsock != -1)
				{
					VAVAHAL_InsertCmdList(channel, -1, VAVA_CMD_START_REC, NULL, 0);
					recordstart = 0;
				}
			}
			
			if(nodatacheck == 1)
			{
				gettimeofday(&t_out_current, NULL);
				if(t_out_current.tv_sec - t_out_start.tv_sec >= 20)
				{
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] More than 20 sec not have stream\n", FUN, LINE, channel);
					break;
				}
			}
			else
			{
				gettimeofday(&t_out_current, NULL);
				if(t_out_current.tv_sec - t_out_start.tv_sec >= g_rectime.alarm_time + 10)
				{
					//录像时长达到设置时间
					break;
				}
			}

			if(av_read == -1)
			{
				av_read = g_avmemchace[channel].mv_write;
				if(av_read == -1)
				{
					usleep(40000);
					continue;
				}
			}
			
			if(av_read == g_avmemchace[channel].mv_write)
			{
				usleep(40000);
				continue;
			}

			offset = g_avmemchace[channel].mvlists[av_read].offset;
			size = g_avmemchace[channel].mvlists[av_read].size;
			type = g_avmemchace[channel].mvlists[av_read].ftype; 
			fps = g_avmemchace[channel].mvlists[av_read].fps;
			ntsamp = g_avmemchace[channel].mvlists[av_read].ntsamp;

			if(size <= 0)
			{
				av_read++;
				if(av_read >= MEM_MAIN_QUEUE_NUM)
				{
					av_read = 0;
				}
				
				continue;
			}

			if(g_alarmrec[channel].start == 0)
			{
				if(type == 1)
				{
					gettimeofday(&t_out_start, NULL);
					g_alarmrec[channel].start = 1;

					save = ntsamp;
					nodatacheck = 0;

				#ifdef ALARM_PHOTO
					savephoto = 1;
				#endif

					//开始时间
					start_ntsamp = (int)(ntsamp / 1000);
					save_ntsamp = (int)(ntsamp / 1000);
					end_ntsamp = (int)(ntsamp / 1000);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] stream rec start [%d]\n", 
					                                FUN, LINE, channel, start_ntsamp);

				#ifdef ALARM_PHOTO
					//第一张图片保存
					saveflag = 0;
					memset(tmpdir, 0, 9);
					memset(tmpfile, 0, 11);
				#endif
				}
			}

			if(g_alarmrec[channel].start == 1)
			{
				#ifdef ALARM_PHOTO
				if((g_md_result[channel] == 1 || savephoto == 1) && type == 1)
				#else
				if(g_md_result[channel] == 1 && type == 1)
				#endif
				{
					#ifdef ALARM_PHOTO
					if(g_md_result[channel] == 1)
					{
						g_md_result[channel] = 0;
						pushflag = 1;
						savephoto = 0;

						//保存图片
						photontsamp = (int)(ntsamp / 1000);

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] modetect photo ntsamp = [%llu, %d]\n", 
					                                    FUN, LINE, channel, ntsamp, photontsamp);

						if(saveflag == 1)
						{
							VAVAHAL_SaveAlarmPhoto(channel, offset, size, photontsamp, 1, tmpdir, tmpfile);
							saveflag = 0;
						}
						else
						{
							VAVAHAL_SaveAlarmPhoto(channel, offset, size, photontsamp, 1, NULL, NULL);	
						}
					}
					else
					{
						if(saveflag == 1)
						{
							//清除第一张图片
							VAVAHAL_DelRecFile(tmpdir, tmpfile);
							saveflag = 0;
						}
					}

					//抓取第一张图片
					if(savephoto == 1)
					{
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] alarm first photo ntsamp = %llu\n", 
					                                    FUN, LINE, channel, ntsamp);

						//保存图片 不进入媒体库
						photontsamp = (int)(ntsamp / 1000);
						VAVAHAL_SaveAlarmPhoto(channel, offset, size, photontsamp, 0, NULL, NULL);

						VAVAHAL_SavePhotoInfo(channel, photontsamp, &saveflag, tmpdir, tmpfile);
						savephoto = 0;
					}

					#else

					//保存一张图片到固定位置
					VAVAHAL_SavePhoto(channel, offset, size);
					
					if(g_md_result[channel] == 1)
					{
						g_md_result[channel] = 0;
						pushflag = 1;
					}
					
					#endif
				}

				//统计帧数
				if(type == 0 || type == 1)
				{
					totolframe++;

					//结束时间戳实时更新
					save_ntsamp = end_ntsamp;
					end_ntsamp = (int)(ntsamp / 1000);
				}

				//增加录像时长判断
				if(end_ntsamp - start_ntsamp >= g_rectime.alarm_time + 1)
				{
					break;
				}

				//写入数据
				memset(&av_head, 0, sizeof(VAVA_RecHead));
				av_head.tag = VAVA_REC_HEAD;
				av_head.size = size;
				av_head.type = type;
				av_head.fps = fps;

				//增量时间戳
				increment = ntsamp - save;
				if(increment <= 0)
				{
					av_head.time_sec = 0;
					av_head.time_usec = 0;
				}
				else
				{
					av_head.time_sec = increment / 1000;
					av_head.time_usec = increment % 1000;
				}

				if(fwrite(&av_head, sizeof(VAVA_RecHead), 1, g_alarmrec[channel].fd) != 1)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [channel-%d] write record head err\n", FUN, LINE, channel);
					break;
				}

				if(type == 1)
				{
					VAVA_Aes_Encrypt(g_avmemchace[channel].pmvMemBegin + offset, g_avmemchace[channel].pmvMemBegin + offset, size);
				}
				else if(type == 8)
				{
					unsigned char *p = (unsigned char *)(g_avmemchace[channel].pmvMemBegin + offset);
					p[0] = 0xBB;
					VAVA_Aes_Encrypt(g_avmemchace[channel].pmvMemBegin + offset + 1, g_avmemchace[channel].pmvMemBegin + offset + 1, size - 1);
				}
				
				if(fwrite(g_avmemchace[channel].pmvMemBegin + offset, size, 1, g_alarmrec[channel].fd) != 1)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [channel-%d] write record data err\n", FUN, LINE, channel);
					break;
				}
				
				totolsize += size;
			}

			av_read++;
			if(av_read >= MEM_MAIN_QUEUE_NUM)
			{
				av_read = 0;
			}
		}

		//修复移动侦测结束后把手动录像流停止的问题
		if(g_manaulrec[channel].ctrl == 0 && g_cloudrec[channel].ctrl == 0)
		{
			VAVAHAL_InsertCmdList(channel, -1, VAVA_CMD_STOP_REC, NULL, 0);
		}
		recordstart = 1;

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] stream rec end [%d]\n", FUN, LINE, channel, save_ntsamp);
		
		if(totolsize <= 0)
		{
			Rec_Close_Alarm_Recfile(channel, NULL, 0, 0, 0);
		}
		else
		{
			//使用时间戳计算方式
			totoltime = save_ntsamp - start_ntsamp;
			
			if(pushflag == 0 || totoltime < 0)
			{
				Rec_Close_Alarm_Recfile(channel, NULL, 0, 0, 0);
			}
			else
			{
				if(totoltime == 0)
				{
					totoltime = 1;
				}
				
				//统计数据
				recinfo.tag = 1;
				recinfo.v_encode = g_camerainfo[channel].videocodec;
				recinfo.a_encode = g_camerainfo[channel].audiocodec;
				recinfo.res = g_camerainfo[channel].m_res;
				recinfo.fps = g_camerainfo[channel].m_fps;
				recinfo.encrypt = VAVA_REC_ENCRYPT_AES;
				recinfo.vframe = totolframe;
				recinfo.size = totolsize;
				recinfo.time = totoltime;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] totolframe = %d, alarm time = %d\n", 
				                                FUN, LINE, channel, totolframe, totoltime);

				Rec_Close_Alarm_Recfile(channel, &recinfo, totoltime, pushflag, start_ntsamp);
			}
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] Alarm Record Stop =========\n", FUN, LINE, channel);

		g_camerainfo[channel].alarm_flag = 0;
		pushflag = 0;
		
		VAVAHAL_SleepCamera_Ex(channel);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

#ifdef CLOUDE_SUPPORT
void *Rec_Cloud_pth(void *data)
{
	int channel = *(int *)data;

	int i;
	//int ret;
	int av_read;
	int av_write;
	int av_num;

	int cmdsock;
	int recordstart = 1;

	//流信息
	int offset;
	int size;
	int type;
	int timeout;
	unsigned long long ntsamp;

	//开始和结束时间戳(用于计算录像时间)
	int start_ntsamp;
	int end_ntsamp;
	int save_ntsamp;

	struct timeval t_out_start;
	struct timeval t_out_current;

	//测试
	struct timeval t_1;
	struct timeval t_2;
	
	int nodatacheck = 1;
	int ntp;
	int audioemptpack = 0;
	int lastpackflag = 0;
	int pushflag = 0;
	
	unsigned char *readbuff = NULL;

	unsigned char spsbuff[32];
	unsigned char ppsbuff[32];
	unsigned char spsppsbuff[64];
	int spssize = 0;
	int ppssize = 0;
	int spsppssize = 0;
	unsigned char *s = NULL;
	
	struct mps_clds_dev_sample clds_sample;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);
		
		Err_Log("cloudrec channel err");
		return NULL;
	}

	readbuff = g_avcloudbuff[channel].buff;
	memset(readbuff, 0, MEM_AV_CLOUD_SIZE);

	memset(spsbuff, 0, 32);
	memset(ppsbuff, 0, 32);
	memset(spsppsbuff, 0, 64);

	while(g_running)
	{
		if(g_pair[channel].nstat == 0 || g_cloudflag[channel] == 0 || g_cloudrec[channel].ctrl == 0
			|| g_cloudrec[channel].status == VAVA_REC_STATUS_STOP || g_camerainfo_dynamic[channel].online == 0)
		{
			g_camerainfo[channel].cloud_flag = 0;
			
			usleep(50000);
			continue;
		}

		#if 0
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [WAKE-UP][Cloud] channel - %d\n", FUN, LINE, channel);

		ret = VAVAHAL_WakeupCamera_Ex(channel, WAKE_UP_MODE_PIR);
		if(ret != VAVA_ERR_CODE_SUCCESS)
		{
			//唤醒失败 停止本次云存储
			g_cloudrec[channel].ctrl = 0;
			continue;
		}
		#endif

		g_camerainfo[channel].cloud_flag = 1;

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] Cloud Record start =========\n", FUN, LINE, channel);

		gettimeofday(&t_out_start, NULL);
		t_1.tv_sec = t_out_start.tv_sec;
		av_read = g_avmemchace[channel].mv_write;

#ifdef FRAME_COUNT
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] ===> [%d] [%d]\n", 
			                            FUN, LINE, channel, g_avmemchace[channel].mvlists[av_read].count, 
			                            g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].count);
#endif

		start_ntsamp = -1;
		end_ntsamp = -1;
		save_ntsamp = -1;

		while(g_running && g_pair[channel].nstat == 1 && g_cloudflag[channel] == 1 && g_cloudrec[channel].ctrl == 1
			  && g_camerainfo_dynamic[channel].online == 1)
		{
			if(recordstart == 1 && g_camerainfo[channel].alarm_flag == 0)
			{
				cmdsock = VAVAHAL_ReadSocketId(0, channel);
				if(cmdsock != -1)
				{
					VAVAHAL_InsertCmdList(channel, -1, VAVA_CMD_START_REC, NULL, 0);
					recordstart = 0;
				}
			}
			else
			{
				recordstart = 0;
			}
			
			if(nodatacheck == 1)
			{
				gettimeofday(&t_out_current, NULL);
				if(t_out_current.tv_sec - t_out_start.tv_sec >= 20)
				{
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] More than 20 sec not have stream\n", FUN, LINE, channel);
					break;
				}
			}
			else
			{
				gettimeofday(&t_out_current, NULL);
				if(t_out_current.tv_sec - t_out_start.tv_sec >= g_rectime.alarm_time + 10)
				{
					//录像时长达到设置时间
					break;
				}
			}

			if(av_read == -1)
			{
				av_read = g_avmemchace[channel].mv_write;
				if(av_read == -1)
				{
					usleep(40000);
					continue;
				}
			}
			
			if(av_read == g_avmemchace[channel].mv_write)
			{
				usleep(40000);
				continue;
			}

#ifdef FRAME_COUNT
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] ===> [%d] [%d]\n", 
				                            FUN, LINE, channel, g_avmemchace[channel].mvlists[av_read].count,
				                            g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].count);
#endif

			offset = g_avmemchace[channel].mvlists[av_read].offset;
			size = g_avmemchace[channel].mvlists[av_read].size;
			type = g_avmemchace[channel].mvlists[av_read].ftype; 
			ntsamp = g_avmemchace[channel].mvlists[av_read].ntsamp;

			//上传UTC时间
			ntp = VAVAHAL_GetNtp();
			ntsamp = ntsamp - (ntp * 3600 * 1000);

			if(size <= 0)
			{
				av_read++;
				if(av_read >= MEM_MAIN_QUEUE_NUM)
				{
					av_read = 0;
				}
				
				continue;
			}

			if(g_cloudrec[channel].start == 0)
			{
				if(type == 1)
				{
					memcpy(readbuff, g_avmemchace[channel].pmvMemBegin + offset, size);
					VAVA_Aes_Check_Video(readbuff, size);
					
					memset(spsbuff, 0, 32);
					memset(ppsbuff, 0, 32);
					getspsandpps(readbuff, spsbuff, ppsbuff, &spssize, &ppssize);

					memset(spsppsbuff, 0, 64);
					s = spsppsbuff;
					
					mbytes_write_int_big_endian_4b(s, spssize);
					memcpy(s + 4, spsbuff, spssize);
	    			s += 4 + spssize;

					mbytes_write_int_big_endian_4b(s, ppssize);
				    memcpy(s + 4, ppsbuff, ppssize);
				    s += 4 + ppssize;

	  				spsppssize = s - spsppsbuff;

					gettimeofday(&t_out_start, NULL);
					g_cloudrec[channel].start = 1;

					nodatacheck = 0;

					//开始时间
					start_ntsamp = (int)(ntsamp / 1000);

					if(g_cloudlink[channel] == 0)
					{
						//发送SPS PPS
						memset(&clds_sample, 0, sizeof(struct mps_clds_dev_sample));
						clds_sample.ab_time = ntsamp;
			            clds_sample.data = spsppsbuff;
			            clds_sample.len = spsppssize;
			            clds_sample.flag.is_video = 1;
			            clds_sample.flag.is_key_sample = 0;
			            mps_clds_dev_channel_sample_write(clds_chl[channel], &clds_sample);
					}

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] stream rec start [%d]\n", 
						                            FUN, LINE, channel, start_ntsamp);
				}
			}
			else if(g_cloudrec[channel].start == 2)
			{
				if(type == 1)
				{
					g_cloudrec[channel].start = 1;
				}
			}

			if(g_cloudrec[channel].start == 1)
			{
				if(g_cloudsend[channel] == 0 || g_cloudlink[channel] == 0)  //等待link和send
				{
					while(g_running && g_cloudrec[channel].ctrl)
					{
						av_num = 0;
						av_write = g_avmemchace[channel].mv_write;

						if(av_read < av_write)
						{
							av_num = MEM_MAIN_QUEUE_NUM - av_write + av_read;
						}
						else if(av_read > av_write)
						{
							av_num = av_read - av_write;
						}
						else
						{
							av_num = MEM_MAIN_QUEUE_NUM;
						}

						if(av_num < 50)
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] [Need Move] av_num = %d, av_read = %d\n", 
							                                FUN, LINE, channel, av_num, av_read);
							
							while(1)
							{
								av_read++;
								if(av_read >= MEM_MAIN_QUEUE_NUM)
								{
									av_read = 0;
								}

								if(av_read == av_write || g_avmemchace[channel].mvlists[av_read].ftype == 1)
								{
									break;
								}
							}

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] [Move end] av_read = %d\n", 
								                            FUN, LINE, channel, av_read);
							
							offset = g_avmemchace[channel].mvlists[av_read].offset;
							size = g_avmemchace[channel].mvlists[av_read].size;
							type = g_avmemchace[channel].mvlists[av_read].ftype; 
							ntsamp = g_avmemchace[channel].mvlists[av_read].ntsamp;

							//上传UTC时间
							ntp = VAVAHAL_GetNtp();
							ntsamp = ntsamp - (ntp * 3600 * 1000);

							start_ntsamp = (int)(ntsamp / 1000);
						}
						
						usleep(100000);

						gettimeofday(&t_2, NULL);
						if(t_2.tv_sec - t_1.tv_sec >= 8)
						{

							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [channel-%d] Not Get link event[%d] or Md Result[%d](8s)\n", 
							                                 FUN, LINE, channel, g_cloudlink[channel], g_cloudsend[channel]);
							break;
						}

						if(g_cloudsend[channel] == 1 && g_cloudlink[channel] == 1)
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] Cloud Start Wait Time = %d s\n", 
							                                FUN, LINE, channel, (int)(t_2.tv_sec - t_1.tv_sec));

							lastpackflag = 1;
							break;
						}
					}

					if(t_2.tv_sec - t_1.tv_sec >= 8 || g_cloudrec[channel].ctrl == 0)
					{
						//结束本次录像
						break;
					}
				}
				
				timeout = 0;
				while(g_cloudctrl[channel] == 1) //暂停
				{
					if(timeout++ >= 15) //3秒超时
					{
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] [WARING] cloud update timeout(3s)\n", 
							                            FUN, LINE, channel);
						break;
					}
						
					usleep(200000);	
				}

				if(timeout >= 15)
				{
					g_cloudrec[channel].start = 2;
				}
				else
				{
					//统计帧数
					if(type == 0 || type == 1)
					{
						//结束时间戳实时更新
						save_ntsamp = end_ntsamp;
						end_ntsamp = (int)(ntsamp / 1000);
					}

					//增加录像时长判断
					if(end_ntsamp - start_ntsamp >= g_rectime.alarm_time + 1)
					{
						break;
					}

					memset(readbuff, 0, MEM_AV_CLOUD_SIZE);
					memcpy(readbuff, g_avmemchace[channel].pmvMemBegin + offset, size);

					if(type == 1)
					{
						VAVA_Aes_Check_Video(readbuff, size);
					}
					else if(type == 8)
					{
						VAVA_Aes_Check_Audio(readbuff, size);
					}

					//发送数据
					if(type == 0) 
					{
						//P帧
						readbuff[0] = (size - 4) >> 24;  
						readbuff[1] = (size - 4) >> 16;  
						readbuff[2] = (size - 4) >> 8;  
						readbuff[3] = (size - 4) & 0xff; 

						memset((char *)&clds_sample, 0, sizeof(clds_sample));

						clds_sample.ab_time = ntsamp;
				        clds_sample.data = readbuff;
				        clds_sample.len = size;
				        clds_sample.flag.is_video = 1;
				        clds_sample.flag.is_key_sample = 0;
						mps_clds_dev_channel_sample_write(clds_chl[channel], &clds_sample);
					}
					else if(type == 1)
					{
						pushflag = 1;
						
						//I帧
						memset(&clds_sample, 0, sizeof(struct mps_clds_dev_sample));

						clds_sample.ab_time = ntsamp;
			            clds_sample.data = spsppsbuff;
			            clds_sample.len = spsppssize;
			            clds_sample.flag.is_video = 1;
			            clds_sample.flag.is_key_sample = 0;
			            mps_clds_dev_channel_sample_write(clds_chl[channel], &clds_sample);

						for(i = 0; i < 100; i++)
						{
							if(readbuff[i] == 0 && readbuff[i + 1] == 0 && readbuff[i + 2] == 0 && readbuff[i + 3] == 1 && readbuff[i + 4] == 0x65)
							{
								break;
							}
						}

						readbuff[i] = (size - 4 - i) >> 24;  
						readbuff[i + 1] = (size - 4 - i) >> 16;  
						readbuff[i + 2] = (size - 4 - i) >> 8;  
						readbuff[i + 3] = (size - 4 - i) & 0xff; 

						memset ((char *)&clds_sample, 0, sizeof (clds_sample));

						clds_sample.ab_time = ntsamp;
				        clds_sample.data = readbuff + i;
				        clds_sample.len = size - i;
				        clds_sample.flag.is_video = 1;
				        clds_sample.flag.is_key_sample = 1;
						mps_clds_dev_channel_sample_write(clds_chl[channel], &clds_sample);

						//发送音频空包
						if(audioemptpack == 0)
						{
							audioemptpack = 1;

							memset(readbuff, 0, MEM_AV_CLOUD_SIZE);

							//ADTS头
							readbuff[0] = 0xff;
							readbuff[1] = 0xf1;
							readbuff[2] = 0x6c;
							readbuff[3] = 0x40;
							readbuff[4] = 0x17;
							readbuff[5] = 0xdf;
							readbuff[6] = 0xfc;
							
							memset ((char *)&clds_sample, 0, sizeof(clds_sample));
					        clds_sample.ab_time = ntsamp;
					        clds_sample.data = readbuff;
					        clds_sample.len = 7;
					        clds_sample.flag.is_video = 0;
					        clds_sample.flag.is_key_sample = 0;
							mps_clds_dev_channel_sample_write(clds_chl[channel], &clds_sample);

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] --------- send audio adts head ---------\n", 
							                                FUN, LINE, channel);
						}
					}
					else
					{
						//音频
						memset ((char *)&clds_sample, 0, sizeof(clds_sample));
				        clds_sample.ab_time = ntsamp;
				        clds_sample.data = readbuff;
				        clds_sample.len = size;
				        clds_sample.flag.is_video = 0;
				        clds_sample.flag.is_key_sample = 0;
						mps_clds_dev_channel_sample_write(clds_chl[channel], &clds_sample);
					}
				}
			}

			av_read++;
			if(av_read >= MEM_MAIN_QUEUE_NUM)
			{
				av_read = 0;
			}

			usleep(30000);
		}

		if(lastpackflag == 1)
		{
			lastpackflag = 0;
			
			//发送尾包 发送SPS PPS + 结尾标识
			memset(&clds_sample, 0, sizeof(struct mps_clds_dev_sample));
			clds_sample.ab_time = save_ntsamp;
	        clds_sample.data = spsppsbuff;
	        clds_sample.len = spsppssize;
	        clds_sample.flag.is_video = 1;
	        clds_sample.flag.is_key_sample = 0;
			clds_sample.flag.is_end = 1;
	        mps_clds_dev_channel_sample_write(clds_chl[channel], &clds_sample);
		}

		//修复移动侦测结束后把手动录像流停止的问题
		if(g_manaulrec[channel].ctrl == 0 &&  g_alarmrec[channel].ctrl == 0)
		{
			VAVAHAL_InsertCmdList(channel, -1, VAVA_CMD_STOP_REC, NULL, 0);
		}
		
		recordstart = 1;
		audioemptpack = 0;

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] stream rec end [%d]\n", FUN, LINE, channel, save_ntsamp);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] Cloud Record Stop =========\n", FUN, LINE, channel);

		VAVAHAL_StopAlarmCloud(channel);

		if(pushflag == 1)
		{
			VAVAHAL_CloudPush(channel, start_ntsamp);
			pushflag = 0;
		}
		
		g_camerainfo[channel].cloud_flag = 0;
		
		VAVAHAL_SleepCamera_Ex(channel);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}
#endif

int Rec_CheckDir(char *dirname)
{
	DIR *dirp = NULL;
	FILE *fd = NULL;
	int ret;
	char str[64];
	char cmd[128];

	static int errcount = 0;
	
	VAVA_Idx_Head idxhead;

	//检测文件夹是否存在
	memset(str, 0, 64);
	sprintf(str, "/mnt/sd0/%s/%s", g_gatewayinfo.sn, dirname);

	dirp = opendir(str);
	if(dirp == NULL)
	{
		//创建文件夹
		memset(cmd, 0, 128);
		sprintf(cmd, "/bin/mkdir -p /mnt/sd0/%s/%s", g_gatewayinfo.sn, dirname);
		VAVAHAL_SystemCmd(cmd);

		ret = Rec_InserDirToIdx(dirname);
		if(ret == 0)
		{
			//创建索引文件
			memset(str, 0, 64);
			sprintf(str, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, RECIDX_FILE);
			fd = fopen(str, "wb");
			if(fd != NULL)
			{
				memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
				idxhead.tag = VAVA_IDX_HEAD;
				idxhead.totol = 0;
				idxhead.first = 0;

				fwrite(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
				fflush(fd);
				fclose(fd);

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ------ ***** [%s] index create success\n", FUN, LINE, str);
				
				errcount = 0;
			}
		}
		else
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: Rec_InserDirToIdx err\n", FUN, LINE);

			errcount++;
			if(errcount <= 3)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: try to remount tfcard\n", FUN, LINE);
				
				//尝试修复
				VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");
				g_gatewayinfo.sdstatus = VAVA_SD_STATUS_NOCRAD;
				g_gatewayinfo.format_flag = 0;
			}
			else
			{
				errcount = 4;
				g_gatewayinfo.sdstatus = VAVA_SD_STATUS_BADCARD;
			}
			
			return -1;
		}
	}
	else
	{
		closedir(dirp);
	}

	return 0;
}

int Rec_Create_Fulltime_Recfile(int channel, VAVA_RecInfo *recinfo)
{
	int ret;
	time_t time_manaul;
    struct tm* time_info;

	char filepath[128];

	if(g_fulltimerec[channel].status >= VAVA_REC_STATUS_RECODING)
	{
		return -1;
	}

	g_fulltimerec[channel].status = VAVA_REC_STATUS_RECODING;

	time(&time_manaul);
	time_info = localtime(&time_manaul);

	memset(g_fulltimerec[channel].dirname, 0, 9);
	sprintf(g_fulltimerec[channel].dirname, "%d%02d%02d", 1900 + time_info->tm_year, 
		                                                  time_info->tm_mon + 1,
		                                                  time_info->tm_mday);

	ret = Rec_CheckDir(g_fulltimerec[channel].dirname);
	if(ret != 0)
	{
		g_fulltimerec[channel].status = VAVA_REC_STATUS_IDLE;
		g_fulltimerec[channel].ctrl = 0;
		g_fulltimerec[channel].start = 0;
		memset(g_fulltimerec[channel].dirname, 0, 9);
		memset(g_fulltimerec[channel].filename, 0, 11);

		return -1;
	}

	memset(g_fulltimerec[channel].filename, 0, 11);
	sprintf(g_fulltimerec[channel].filename, "%02d%02d%02d_%d_%d", time_info->tm_hour,
		                                                           time_info->tm_min, 
		                                                           time_info->tm_sec, 
		                                                           VAVA_RECFILE_TYPE_FULLTIME, 
		                                                           channel);

	memset(filepath, 0, 128);
	sprintf(filepath, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, g_fulltimerec[channel].dirname, g_fulltimerec[channel].filename);

	g_fulltimerec[channel].fd = fopen(filepath, "wb");
	if(g_fulltimerec[channel].fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: open record file fail\n", FUN, LINE);

		g_fulltimerec[channel].status = VAVA_REC_STATUS_IDLE;
		g_fulltimerec[channel].ctrl = 0;
		g_fulltimerec[channel].start = 0;
		memset(g_fulltimerec[channel].dirname, 0, 9);
		memset(g_fulltimerec[channel].filename, 0, 11);
		
		return -1;
	}

	fwrite(recinfo, sizeof(VAVA_RecInfo), 1, g_fulltimerec[channel].fd);
	return 0;
}

int Rec_Close_Fulltime_Recfile(int channel, VAVA_RecInfo *recinfo, int totoltime)
{
	char tmpstr[128];
	
	if(totoltime == 0 && recinfo == NULL)
	{
		fclose(g_fulltimerec[channel].fd);
		g_fulltimerec[channel].fd = NULL;
	
		memset(tmpstr, 0, 128);
		sprintf(tmpstr, "/bin/rm -f /mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, g_fulltimerec[channel].dirname, g_fulltimerec[channel].filename);
		VAVAHAL_SystemCmd(tmpstr);

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [/mnt/sd0/%s/%s/%s] not have rec data, del it\n",
		                                FUN, LINE, g_gatewayinfo.sn, g_fulltimerec[channel].dirname, g_alarmrec[channel].filename);
	}
	else
	{
		fseek(g_fulltimerec[channel].fd, 0, SEEK_SET);
		fwrite(recinfo, sizeof(VAVA_RecInfo), 1, g_fulltimerec[channel].fd);

		fflush(g_fulltimerec[channel].fd);
		fclose(g_fulltimerec[channel].fd);
		g_fulltimerec[channel].fd = NULL;
		
		Rec_InserFileToIdx(channel, g_fulltimerec[channel].dirname, g_fulltimerec[channel].filename, VAVA_RECFILE_TYPE_FULLTIME, totoltime);
	}

	if(g_fulltimerec[channel].status <= VAVA_REC_STATUS_RECODING)
	{
		g_fulltimerec[channel].status = VAVA_REC_STATUS_IDLE;
	}
	
	//g_fulltimerec[channel].ctrl = 0; //需要循环录
	g_fulltimerec[channel].start = 0;
	memset(g_fulltimerec[channel].dirname, 0, 9);
	memset(g_fulltimerec[channel].filename, 0, 11);

	return 0;
}

int Rec_Cteate_Manual_Recfile(int channel, VAVA_RecInfo *recinfo)
{
	int ret;
	time_t time_manaul;
    struct tm* time_info;

	char filepath[128];

	if(g_manaulrec[channel].status >= VAVA_REC_STATUS_RECODING)
	{
		return -1;
	}

	g_manaulrec[channel].status = VAVA_REC_STATUS_RECODING;

	time(&time_manaul);
	time_info = localtime(&time_manaul);

	memset(g_manaulrec[channel].dirname, 0, 9);
	sprintf(g_manaulrec[channel].dirname, "%d%02d%02d", 1900 + time_info->tm_year, 
		                                                  time_info->tm_mon + 1,
		                                                  time_info->tm_mday);

	ret = Rec_CheckDir(g_manaulrec[channel].dirname);
	if(ret != 0)
	{
		g_manaulrec[channel].status = VAVA_REC_STATUS_IDLE;
		g_manaulrec[channel].ctrl = 0;
		g_manaulrec[channel].start = 0;
		memset(g_manaulrec[channel].dirname, 0, 9);
		memset(g_manaulrec[channel].filename, 0, 11);

		return -1;
	}

	memset(g_manaulrec[channel].filename, 0, 11);
	sprintf(g_manaulrec[channel].filename, "%02d%02d%02d_%d_%d", time_info->tm_hour,
		                                                         time_info->tm_min, 
		                                                         time_info->tm_sec, 
		                                                         VAVA_RECFILE_TYPE_MANAUL, 
		                                                         channel);

	memset(filepath, 0, 128);
	sprintf(filepath, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, g_manaulrec[channel].dirname, g_manaulrec[channel].filename);

	g_manaulrec[channel].fd = fopen(filepath, "wb");
	if(g_manaulrec[channel].fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open record file fail, channel = %d\n", FUN, LINE, channel);

		g_manaulrec[channel].status = VAVA_REC_STATUS_IDLE;
		g_manaulrec[channel].ctrl = 0;
		g_manaulrec[channel].start = 0;
		memset(g_manaulrec[channel].dirname, 0, 9);
		memset(g_manaulrec[channel].filename, 0, 11);
		
		return -1;
	}

	fwrite(recinfo, sizeof(VAVA_RecInfo), 1, g_manaulrec[channel].fd);
	return 0;
}

int Rec_Close_Manual_Recfile(int channel, VAVA_RecInfo *recinfo, int totoltime)
{
	char tmpstr[128];
	
	if(totoltime == 0 && recinfo == NULL)
	{
		fclose(g_manaulrec[channel].fd);
		g_manaulrec[channel].fd = NULL;
	
		memset(tmpstr, 0, 128);
		sprintf(tmpstr, "/bin/rm -f /mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, g_manaulrec[channel].dirname, g_manaulrec[channel].filename);
		VAVAHAL_SystemCmd(tmpstr);

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [/mnt/sd0/%s/%s/%s] not have rec data, del it\n", 
		                                FUN, LINE, g_gatewayinfo.sn, g_manaulrec[channel].dirname, g_alarmrec[channel].filename);
	}
	else
	{
		fseek(g_manaulrec[channel].fd, 0, SEEK_SET);
		fwrite(recinfo, sizeof(VAVA_RecInfo), 1, g_manaulrec[channel].fd);

		fflush(g_manaulrec[channel].fd);
		fclose(g_manaulrec[channel].fd);
		g_manaulrec[channel].fd = NULL;
		
		Rec_InserFileToIdx(channel, g_manaulrec[channel].dirname, g_manaulrec[channel].filename, VAVA_RECFILE_TYPE_MANAUL, totoltime);
	}

	if(g_manaulrec[channel].status <= VAVA_REC_STATUS_RECODING)
	{
		g_manaulrec[channel].status = VAVA_REC_STATUS_IDLE;
	}
	
	g_manaulrec[channel].ctrl = 0;
	g_manaulrec[channel].start = 0;
	memset(g_manaulrec[channel].dirname, 0, 9);
	memset(g_manaulrec[channel].filename, 0, 11);

	return 0;
}

int Rec_Cteate_Alarm_Recfile(int channel, VAVA_RecInfo *recinfo)
{
	int ret;
	time_t time_manaul;
    struct tm* time_info;

	char filepath[128];

	if(g_alarmrec[channel].status >= VAVA_REC_STATUS_RECODING)
	{
		return -1;
	}

	g_alarmrec[channel].status = VAVA_REC_STATUS_RECODING;

	time(&time_manaul);
	time_info = localtime(&time_manaul);

	memset(g_alarmrec[channel].dirname, 0, 9);
	sprintf(g_alarmrec[channel].dirname, "%d%02d%02d", 1900 + time_info->tm_year, 
		                                                  time_info->tm_mon + 1,
		                                                  time_info->tm_mday);

	ret = Rec_CheckDir(g_alarmrec[channel].dirname);
	if(ret != 0)
	{
		g_alarmrec[channel].status = VAVA_REC_STATUS_IDLE;
		g_alarmrec[channel].ctrl = 0;
		g_alarmrec[channel].start = 0;
		memset(g_alarmrec[channel].dirname, 0, 9);
		memset(g_alarmrec[channel].filename, 0, 11);

		return -1;
	}

	memset(g_alarmrec[channel].filename, 0, 11);
	sprintf(g_alarmrec[channel].filename, "%02d%02d%02d_%d_%d", time_info->tm_hour,
		                                                        time_info->tm_min, 
		                                                        time_info->tm_sec, 
		                                                        VAVA_RECFILE_TYPE_ALARM, 
		                                                        channel);

	memset(filepath, 0, 128);
	sprintf(filepath, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, g_alarmrec[channel].dirname, g_alarmrec[channel].filename);

	g_alarmrec[channel].fd = fopen(filepath, "wb");
	if(g_alarmrec[channel].fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open record file fail\n", FUN, LINE);

		g_alarmrec[channel].status = VAVA_REC_STATUS_IDLE;
		g_alarmrec[channel].ctrl = 0;
		g_alarmrec[channel].start = 0;
		memset(g_alarmrec[channel].dirname, 0, 9);
		memset(g_alarmrec[channel].filename, 0, 11);
		
		return -1;
	}

	fwrite(recinfo, sizeof(VAVA_RecInfo), 1, g_alarmrec[channel].fd);
	return 0;
}

int Rec_Close_Alarm_Recfile(int channel, VAVA_RecInfo *recinfo, int totoltime, int pushflag, int ntsamp)
{
	char tmpstr[128];

	if(totoltime == 0 && recinfo == NULL)
	{
		fclose(g_alarmrec[channel].fd);
		g_alarmrec[channel].fd = NULL;

		memset(tmpstr, 0, 128);
		sprintf(tmpstr, "/bin/rm -f /mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, g_alarmrec[channel].dirname, g_alarmrec[channel].filename);
		VAVAHAL_SystemCmd(tmpstr);

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [/mnt/sd0/%s/%s/%s] not have rec data, del it\n", 
		                                FUN, LINE, g_gatewayinfo.sn, g_alarmrec[channel].dirname, g_alarmrec[channel].filename);

		if(pushflag)
		{
			VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_TEXT, "", "", "", 0, ntsamp);
		}
	}
	else
	{
		fseek(g_alarmrec[channel].fd, 0, SEEK_SET);
		fwrite(recinfo, sizeof(VAVA_RecInfo), 1, g_alarmrec[channel].fd);

		fflush(g_alarmrec[channel].fd);
		fclose(g_alarmrec[channel].fd);
		g_alarmrec[channel].fd = NULL;
		
		Rec_InserFileToIdx(channel, g_alarmrec[channel].dirname, g_alarmrec[channel].filename, VAVA_RECFILE_TYPE_ALARM, totoltime);

		if(pushflag)
		{
		#ifdef ALARM_PHOTO_IOT
			//推送图片
			VAVASERVER_PushPhoto(channel);
		#endif
			
			VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_VIDEO, 
					               g_alarmrec[channel].dirname, g_alarmrec[channel].filename, "", totoltime, ntsamp);
		}
	}

	if(g_alarmrec[channel].status <= VAVA_REC_STATUS_RECODING)
	{
		g_alarmrec[channel].status = VAVA_REC_STATUS_IDLE;
	}
	g_alarmrec[channel].ctrl = 0;
	g_alarmrec[channel].start = 0;
	memset(g_alarmrec[channel].dirname, 0, 9);
	memset(g_alarmrec[channel].filename, 0, 11);

	return 0;
}

//录像日期索引文件格式及存储方式
//索引头 12字节 同步头 + 总个数 + 起始位置
//日期节点 12字节 随机码(1-100) + 日期名
//文件节点 16字节 随机码(1-100) + 类型 + 通道 + 文件名 + 录像时长
//随机码+0x1A存储
int Rec_InserDirToIdx(char *dirname)
{
	FILE *fd = NULL;
	int ret;
	VAVA_Idx_Head idxhead;
	VAVA_Idx_Date idxdate;
	char path[128];

	struct timeval randomtime;

	if(dirname == NULL)
	{
		return -1;
	}

	ret = VAVAHAL_Date_Verification(dirname);
	if(ret != 0)
	{
		return -1;
	}

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s", g_gatewayinfo.sn, RECIDX_FILE);

	pthread_mutex_lock(&mutex_idx_lock);

	fd = fopen(path, "r+");
	if(fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create date idx\n", FUN, LINE);

		fd = fopen(path, "wb");
		if(fd == NULL)
		{
			pthread_mutex_unlock(&mutex_idx_lock);
			return -1;
		}

		memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
		idxhead.tag = VAVA_IDX_HEAD;
		idxhead.totol = 1;
		idxhead.first = 0;

		memset(&idxdate, 0, sizeof(VAVA_Idx_Date));
		gettimeofday(&randomtime, NULL);
		srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
		idxdate.random = rand() % 100 + 1; //生成1-100随机数
		VAVAHAL_EncryptionDateStr(dirname, idxdate.dirname, idxdate.random);
		idxdate.random += 0x1A;

		fwrite(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
		fwrite(&idxdate, sizeof(VAVA_Idx_Date), 1, fd);

		fflush(fd);
		fclose(fd);

		pthread_mutex_unlock(&mutex_idx_lock);
		
		return 0;
	}

	memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
	fread(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
	if(idxhead.tag != VAVA_IDX_HEAD)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check head fail, tag = %x\n", FUN, LINE, idxhead.tag);
		
		Err_Log("recreate date idx");
		
		ret = fseek(fd, 0, SEEK_SET);
		if(ret == 0)
		{
			//重建索引文件
			memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
			idxhead.tag = VAVA_IDX_HEAD;
			idxhead.totol = 1;
			idxhead.first = 0;

			memset(&idxdate, 0, sizeof(VAVA_Idx_Date));
			gettimeofday(&randomtime, NULL);
			srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
			idxdate.random = rand() % 100 + 1; //生成1-100随机数
			VAVAHAL_EncryptionDateStr(dirname, idxdate.dirname, idxdate.random);
			idxdate.random += 0x1A;
			
			fwrite(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
			fwrite(&idxdate, sizeof(VAVA_Idx_Date), 1, fd);
		}	

		fflush(fd);
		fclose(fd);
		pthread_mutex_unlock(&mutex_idx_lock);

		return 0;
	}

	//先更新数据
	ret = fseek(fd, sizeof(VAVA_Idx_Head) + idxhead.totol * sizeof(VAVA_Idx_Date), SEEK_SET);
	if(ret == 0)
	{
		memset(&idxdate, 0, sizeof(VAVA_Idx_Date));
		gettimeofday(&randomtime, NULL);
		srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
		idxdate.random = rand() % 100 + 1; //生成1-100随机数
		VAVAHAL_EncryptionDateStr(dirname, idxdate.dirname, idxdate.random);
		idxdate.random += 0x1A;
		
		fwrite(&idxdate, sizeof(VAVA_Idx_Date), 1, fd);

		//更新头信息
		ret = fseek(fd, 0, SEEK_SET);
		if(ret == 0)
		{
			idxhead.totol += 1;
			fwrite(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
		}
	}

	fflush(fd);
	fclose(fd);
	
	pthread_mutex_unlock(&mutex_idx_lock);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: dirname - [%s], random = %d\n", FUN, LINE, dirname, idxdate.random);
	
	return 0;
}

int Rec_InserFileToIdx(int channel, char *dirname, char *filename, int rectype, int rectime)
{
	FILE *fd = NULL;
	//int i;
	int ret;
	VAVA_Idx_Head idxhead;
	VAVA_Idx_File idxfile;
	char path[128];

	struct timeval randomtime;

	if(dirname == NULL || filename == NULL)
	{
		return -1;
	}

	ret = VAVAHAL_Date_Verification(dirname);
	if(ret != 0)
	{
		return -1;
	}

	ret = VAVAHAL_RecFile_Verification(filename);
	if(ret != 0)
	{
		return -1;
	}

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, RECIDX_FILE);

	pthread_mutex_lock(&mutex_idx_lock);

	fd = fopen(path, "r+");
	if(fd == NULL)
	{
		//创建索引
		fd = fopen(path, "wb");
		if(fd == NULL)
		{
			pthread_mutex_unlock(&mutex_idx_lock);
			return -1;
		}

		memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
		idxhead.tag = VAVA_IDX_HEAD;
		idxhead.totol = 1;
		idxhead.first = 0;
		
		memset(&idxfile, 0, sizeof(VAVA_Idx_File));
		gettimeofday(&randomtime, NULL);
		srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
		idxfile.random = rand() % 100 + 1; //生成1-100随机数
		idxfile.type = rectype;
		idxfile.channel = channel;
		VAVAHAL_EncryptionFileStr(filename, idxfile.filename, idxfile.random);
		idxfile.random += 0x1A;
		idxfile.rectime = rectime;

		fwrite(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
		fwrite(&idxfile, sizeof(VAVA_Idx_File), 1, fd);

		fflush(fd);
		fclose(fd);

		pthread_mutex_unlock(&mutex_idx_lock);
		return 0;
	}

	memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
	fread(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);

	if(idxhead.tag != VAVA_IDX_HEAD)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check head fail, tag = %x\n", FUN, LINE, idxhead.tag);
		
		//重建索引文件
		ret = fseek(fd, 0, SEEK_SET);
		if(ret == 0)
		{
			memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
			idxhead.tag = VAVA_IDX_HEAD;
			idxhead.totol = 1;
			idxhead.first = 0;

			memset(&idxfile, 0, sizeof(VAVA_Idx_File));
			gettimeofday(&randomtime, NULL);
			srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
			idxfile.random = rand() % 100 + 1; //生成1-100随机数
			idxfile.type = rectype;
			idxfile.channel = channel;
			VAVAHAL_EncryptionFileStr(filename, idxfile.filename, idxfile.random);
			idxfile.random += 0x1A;
			idxfile.rectime = rectime;

			fwrite(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
			fwrite(&idxfile, sizeof(VAVA_Idx_File), 1, fd);
		}
		
		fclose(fd);
		pthread_mutex_unlock(&mutex_idx_lock);

		Err_Log("recreate file idx");
		return 0;
	}

	//优先更新数据
	ret = fseek(fd, sizeof(VAVA_Idx_Head) + idxhead.totol * sizeof(VAVA_Idx_File), SEEK_SET);
	if(ret == 0)
	{
		memset(&idxfile, 0, sizeof(VAVA_Idx_File));
		gettimeofday(&randomtime, NULL);
		srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
		idxfile.random = rand() % 100 + 1; //生成1-100随机数
		idxfile.type = rectype;
		idxfile.channel = channel;
		VAVAHAL_EncryptionFileStr(filename, idxfile.filename, idxfile.random);
		idxfile.random += 0x1A;
		idxfile.rectime = rectime;

		fwrite(&idxfile, sizeof(VAVA_Idx_File), 1, fd);

		//更新头信息
		ret = fseek(fd, 0, SEEK_SET);
		if(ret == 0)
		{
			idxhead.totol += 1;
			fwrite(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
		}
	}

	fflush(fd);
	fclose(fd);
	pthread_mutex_unlock(&mutex_idx_lock);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: dirname - [%s], filename - [%s], random = %d\n", 
	                                FUN, LINE, dirname, filename, idxfile.random);
	return 0;
}

int Rec_DelFirstRecDir()
{
	FILE *fd = NULL;
	char dirname[9];
	char path[128];
	int ret;
	char *datelist = NULL;
	VAVA_Idx_Head idxhead;
	VAVA_Idx_Date idxdate;
#if 0
	VAVA_RecDel recdel;
#endif

	pthread_mutex_lock(&mutex_idx_lock);

	//取得第一个文件夹
	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s", g_gatewayinfo.sn, RECIDX_FILE);
	fd = fopen(path, "r");
	if(fd == NULL)
	{
		pthread_mutex_unlock(&mutex_idx_lock);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open dateidx fail, sdstatus = %d\n", FUN, LINE, g_gatewayinfo.sdstatus);
		return -1;
	}

	memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
	fread(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
	if(idxhead.tag != VAVA_IDX_HEAD)
	{
		fclose(fd);
		pthread_mutex_unlock(&mutex_idx_lock);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check date idx tag fail, tag = %x\n", FUN, LINE,idxhead.tag);
		return -1;
	}
	
	ret = fseek(fd, sizeof(VAVA_Idx_Head) + idxhead.first * sizeof(VAVA_Idx_Date), SEEK_SET);
	if(ret != 0)
	{
		fclose(fd);
		pthread_mutex_unlock(&mutex_idx_lock);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: fseek fail\n", FUN, LINE);
		return -1;
	}

	while(g_running)
	{
		if(feof(fd))
		{
			fclose(fd);
			pthread_mutex_unlock(&mutex_idx_lock);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get dirname fail\n", FUN, LINE);
			return -1;
		}
		
		memset(&idxdate, 0, sizeof(VAVA_Idx_Date));
		ret = fread(&idxdate, sizeof(VAVA_Idx_Date), 1, fd);
		if(ret <= 0)
		{
			fclose(fd);
			pthread_mutex_unlock(&mutex_idx_lock);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get dirname fail, ret = %d\n", FUN, LINE, ret);
			return -1;
		}

		break;
	}

	fclose(fd);
	fd = NULL;

	VAVAHAL_ParseDateStr(idxdate.dirname, dirname, idxdate.random);
	ret = VAVAHAL_Date_Verification(dirname);
	if(ret != 0)
	{
		pthread_mutex_unlock(&mutex_idx_lock);
		return -1;
	}

#if 0
	ret = VAVAHAL_Date_CurrentCheck(dirname);
	if(ret == 0)
	{
		//当天文件夹 同步删除
		VAVAHAL_DelRecDir(dirname);
	}
	else
	{
		//非当天文件夹异步删除
		//删除整个文件夹 释放空间 耗时动作添加到队列
		memset(&recdel, 0, sizeof(VAVA_RecDel));
		strcpy(recdel.dirname, dirname);
		recdel.type = VAVA_RECFILE_DEL_ALLDIR;
		VAVAHAL_InsertCmdList(0, -1, VAVA_CMD_RECORD_DEL, &recdel, sizeof(VAVA_RecDel));
	}
#else
	//异步删除导致数据未删除但索引删除了
	//改为同步删除
	VAVAHAL_DelRecDir(dirname);
#endif

	datelist = Rec_ImportDateIdx();
	if(datelist != NULL)
	{
		Rec_DelRecDate(datelist, dirname);
		Rec_ReleaseDateIdx(datelist);

		free(datelist);
		datelist = NULL;
	}
	
	pthread_mutex_unlock(&mutex_idx_lock);
	return 0;
}

char *Rec_ImportDateIdx()
{
	char *datelist = NULL;
	FILE *fd = NULL;
	char path[128];
	int ret;
	int totol;
	int current;
	VAVA_Idx_Head idxhead;
	VAVA_Idx_Date idxdate;

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s", g_gatewayinfo.sn, RECIDX_FILE);

	fd = fopen(path, "r");
	if(fd == NULL)
	{
		return NULL;
	}

	memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
	fread(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
	if(idxhead.tag != VAVA_IDX_HEAD)
	{
		fclose(fd);
		return NULL;
	}

	datelist = malloc(sizeof(VAVA_Idx_Head) + idxhead.totol * sizeof(VAVA_Idx_Date));
	if(datelist == NULL)
	{
		fclose(fd);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc datelist fail\n", FUN, LINE);
		
		Err_Log("malloc datelist fail");
		g_running = 0;
		return NULL;
	}

	totol = idxhead.totol - idxhead.first;
	current = 0;

	ret = fseek(fd, sizeof(VAVA_Idx_Head) + idxhead.first * sizeof(VAVA_Idx_Date), SEEK_SET);
	if(ret != 0)
	{
		fclose(fd);
		return NULL;
	}

	while(g_running)
	{
		if(feof(fd))
		{
			break;
		}

		if(current == totol)
		{
			break;
		}

		memset(&idxdate, 0, sizeof(VAVA_Idx_Date));
		ret = fread(&idxdate, sizeof(VAVA_Idx_Date), 1, fd);
		if(ret <= 0)
		{
			break;
		}

		memcpy(datelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_Date), &idxdate, sizeof(VAVA_Idx_Date));
		current += 1;
	}

	fclose(fd);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol = %d, current = %d\n", FUN, LINE, totol, current);

	idxhead.tag = VAVA_IDX_HEAD;
	idxhead.totol = current;
	idxhead.first = 0;

	memcpy(datelist, &idxhead, sizeof(VAVA_Idx_Head));

	return datelist;
}

int Rec_DelRecDate(char *datelist, char *dirname)
{
	int ret;
	int current;
	char tmpdirname[9];
	VAVA_Idx_Head *idxhead;
	VAVA_Idx_Date *idxdate;

	idxhead = (VAVA_Idx_Head *)datelist;
	for(current = 0; current < idxhead->totol; current++)
	{
		idxdate = (VAVA_Idx_Date *)(datelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_Date));
		VAVAHAL_ParseDateStr(idxdate->dirname, tmpdirname, idxdate->random);
		ret = VAVAHAL_Date_Verification(tmpdirname);
		if(ret != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: verification date fail\n", FUN, LINE);
			
			if(current < idxhead->totol - 1)
			{
				memcpy(datelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_Date),
				       datelist + sizeof(VAVA_Idx_Head) + (current + 1) * sizeof(VAVA_Idx_Date),
				       (idxhead->totol - 1 - current) * sizeof(VAVA_Idx_Date));
			}

			idxhead->totol -= 1;
			continue;
		}
		
		if(strcmp(tmpdirname, dirname) == 0)
		{
			break;
		}
	}

	if(current >= idxhead->totol)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: No find this date [%s]\n", FUN, LINE, dirname);
		return -1;
	}
	else
	{
		if(current < idxhead->totol - 1)
		{
			memcpy(datelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_Date),
				   datelist + sizeof(VAVA_Idx_Head) + (current + 1) * sizeof(VAVA_Idx_Date),
				   (idxhead->totol - 1 - current) * sizeof(VAVA_Idx_Date));
		}


		idxhead->totol -= 1;
	}

	return 0;
}

int Rec_ReleaseDateIdx(char *datelist)
{
	FILE *fd = NULL;
	char path[128];
	VAVA_Idx_Head *idxhead;

	idxhead = (VAVA_Idx_Head *)datelist;

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s", g_gatewayinfo.sn, RECIDX_FILE);
	fd = fopen(path, "wb");
	if(fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open date idx fail\n", FUN, LINE);
		
		Err_Log("open dateidx fail");
		return 0;
	}

	fwrite(datelist, sizeof(VAVA_Idx_Head) + idxhead->totol * sizeof(VAVA_Idx_Date), 1, fd);

	fflush(fd);
	fclose(fd);

	return 0;
}

char *Rec_ImportFileIdx(char *dirname)
{
	char *filelist = NULL;
	FILE *fd = NULL;
	char path[128];
	int ret;
	int totol;
	int current;
	VAVA_Idx_Head idxhead;
	VAVA_Idx_File idxfile;

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, RECIDX_FILE);

	fd = fopen(path, "r");
	if(fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open %s fail\n", FUN, LINE, path);
		return NULL;
	}

	memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
	fread(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
	if(idxhead.tag != VAVA_IDX_HEAD)
	{
		fclose(fd);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check idx head fail\n", FUN, LINE);
		return NULL;
	}

	filelist = malloc(sizeof(VAVA_Idx_Head) + idxhead.totol * sizeof(VAVA_Idx_File));
	if(filelist == NULL)
	{
		fclose(fd);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc filelist fail\n", FUN, LINE);
		Err_Log("malloc filelist fail");
		g_running = 0;
		return NULL;
	}

	totol = idxhead.totol - idxhead.first;
	current = 0;

	if(idxhead.first != 0)
	{
		ret = fseek(fd, sizeof(VAVA_Idx_Head) + idxhead.first * sizeof(VAVA_Idx_File), SEEK_SET);
		if(ret != 0)
		{
			fclose(fd);
			
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: fseek fail\n", FUN, LINE);
			return NULL;
		}
	}

	while(g_running)
	{
		if(feof(fd))
		{
			break;
		}

		if(current == totol)
		{
			break;
		}

		memset(&idxfile, 0, sizeof(VAVA_Idx_File));
		ret = fread(&idxfile, sizeof(VAVA_Idx_File), 1, fd);
		if(ret <= 0)
		{
			break;
		}

		memcpy(filelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_File), &idxfile, sizeof(VAVA_Idx_File));
		current += 1;
	}

	fclose(fd);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol = %d, current = %d\n", FUN, LINE, totol, current);

	idxhead.tag = VAVA_IDX_HEAD;
	idxhead.totol = current;
	idxhead.first = 0;

	memcpy(filelist, &idxhead, sizeof(VAVA_Idx_Head));

	return filelist;
}

int Rec_DelRecFile(char *filelist, char *filename)
{
	int ret;
	int current;
	char tmpfilename[11];
	VAVA_Idx_Head *idxhead;
	VAVA_Idx_File *idxfile;

	idxhead = (VAVA_Idx_Head *)filelist;
	for(current = 0; current < idxhead->totol; current++)
	{
		idxfile = (VAVA_Idx_File *)(filelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_File));
		VAVAHAL_ParseFileStr(idxfile->filename, tmpfilename, idxfile->random);
		ret = VAVAHAL_RecFile_Verification(tmpfilename);
		if(ret != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: verification file fail\n", FUN, LINE);
			
			if(current < idxhead->totol - 1)
			{
				memcpy(filelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_File),
				       filelist + sizeof(VAVA_Idx_Head) + (current + 1) * sizeof(VAVA_Idx_File),
				       (idxhead->totol - 1 - current) * sizeof(VAVA_Idx_File));
			}

			idxhead->totol -= 1;
			continue;
		}
		
		if(strcmp(tmpfilename, filename) == 0)
		{
			break;
		}
	}

	if(current >= idxhead->totol)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: No find this file [%s]\n", FUN, LINE, filename);
		return -1;
	}
	else
	{
		if(current < idxhead->totol - 1)
		{
			memcpy(filelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_File),
				   filelist + sizeof(VAVA_Idx_Head) + (current + 1) * sizeof(VAVA_Idx_File),
				   (idxhead->totol - 1 - current) * sizeof(VAVA_Idx_File));
		}


		idxhead->totol -= 1;
	}

	return 0;
}

int Rec_ReleaseFileIdx(char *filelist, char *dirname)
{
	FILE *fd = NULL;
	char path[128];
	char *datelist = NULL;
	VAVA_Idx_Head *idxhead;

	idxhead = (VAVA_Idx_Head *)filelist;

	if(idxhead->totol == 0)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Dir no recfile, remove it[%s]\n", FUN, LINE, dirname);
		
		//删除文件夹
		memset(path, 0, 128);
		sprintf(path, "/bin/rm -rf /mnt/sd0/%s/%s", g_gatewayinfo.sn, dirname);
		VAVAHAL_SystemCmd(path);

		datelist = Rec_ImportDateIdx();
		if(datelist != NULL)
		{
			Rec_DelRecDate(datelist, dirname);
			Rec_ReleaseDateIdx(datelist);
		}
		
		return 0;
	}

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, RECIDX_FILE);
	fd = fopen(path, "wb");
	if(fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open date idx fail\n", FUN, LINE);
		
		Err_Log("open dateidx fail");
		return 0;
	}

	fwrite(filelist, sizeof(VAVA_Idx_Head) + idxhead->totol * sizeof(VAVA_Idx_File), 1, fd);

	fflush(fd);
	fclose(fd);

	return 0;
}

void *Rec_PlayBack(void *data)
{
	int ret;
	int channel;
	unsigned int checksize;
	unsigned int framenum = 0;
	int start = 0;
	char *readbuff = NULL;
	VAVA_RecHead rechead;
	VAVA_AV_HEAD avhead;
	VAVA_CMD_HEAD vavahead;
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;
	char sendbuff[1024];
	int buffsize = MEM_AV_PLAY_SIZE;
	int sendsize;
	int totolsize;

	channel = *(int *)data;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);
		
		Err_Log("recplay channel err");
		g_running = 0;
		return NULL;
	}

	readbuff = (char *)g_avplaybuff.buff;
	memset(readbuff, 0, buffsize);

	totolsize = 0;

	while(g_running)
	{
		if(feof(g_recplay[channel].fd))
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --------- READ END ---------\n", FUN, LINE);
			break;
		}

		if(g_recplay[channel].ctrl == VAVA_RECFILE_PLAY_CTRL_PAUSE)
		{
			usleep(100000);
			continue;
		}
		else if(g_recplay[channel].ctrl == VAVA_RECFILE_PLAY_CTRL_STOP)
		{
			break;
		}

		ret = PPCS_Check_Buffer(g_recplay[channel].sesssion, P2P_CHANNEL_RECAV, &checksize, NULL);
		if(ret != ERROR_PPCS_SUCCESSFUL)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: PPCS_Check_Buffer fail, ret = %d\n", FUN, LINE, ret);
			
			usleep(100000);
			continue;
		}

		if(checksize >= SYP2P_VIDEO_LOSTP_SIZE)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: wait app read data, buffsize = %d\n", FUN, LINE, checksize);
			
			sleep(1);
			continue;
		}

		memset(&rechead, 0, sizeof(VAVA_RecHead));
		ret = fread(&rechead, sizeof(VAVA_RecHead), 1, g_recplay[channel].fd);
		if(ret <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: read head fail\n", FUN, LINE);
			break;
		}

		if(rechead.tag != VAVA_REC_HEAD)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check head fail, head = %x\n", FUN, LINE, rechead.tag);
			break;
		}

		if(rechead.size >= MEM_AV_PLAY_SIZE)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check size too big, size = %d\n", FUN, LINE, rechead.size);
			break;
		}

		memset(readbuff, 0, buffsize);
		ret = fread(readbuff, rechead.size, 1, g_recplay[channel].fd);
		if(ret <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: read data fail\n", FUN, LINE);
			break;
		}

		if(g_recplay[channel].encrypt == VAVA_REC_ENCRYPT_AES)
		{
			if(rechead.type == 1)
			{
				VAVA_Aes_Decrypt(readbuff, readbuff, rechead.size);
			}
			else if(rechead.type == 8)
			{
				VAVA_Aes_Decrypt(readbuff + 1, readbuff + 1, rechead.size);
				readbuff[0] = 0xFF;
			}
		}

		switch(rechead.type)
		{
			case 0: //P FRAME
				if(start == 1)
				{
					memset(&avhead, 0, sizeof(VAVA_AV_HEAD));
					avhead.tag = VAVA_TAG_AP_VIDEO;
					avhead.encodetype = g_recplay[channel].v_encode;
					avhead.frametype = rechead.type;
					avhead.framerate = rechead.fps;
					avhead.res = g_recplay[channel].res;
					avhead.size = rechead.size;
					avhead.ntsamp_sec = rechead.time_sec;
					avhead.ntsamp_usec = rechead.time_usec;
					avhead.framenum = framenum++;

					ret = PPCS_Write(g_recplay[channel].sesssion, P2P_CHANNEL_RECAV, (char *)&avhead, sizeof(VAVA_AV_HEAD));
					if(ret != sizeof(VAVA_AV_HEAD))
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: send P head err\n", FUN, LINE);
					}

				 	ret = PPCS_Write(g_recplay[channel].sesssion, P2P_CHANNEL_RECAV, readbuff, rechead.size);
					if(ret != rechead.size)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: send P data err\n", FUN, LINE);
					}
					else
					{
						totolsize += rechead.size;
					}
				}
				break;
			case 1: //I FRAME
				memset(&avhead, 0, sizeof(VAVA_AV_HEAD));
				avhead.tag = VAVA_TAG_AP_VIDEO;
				avhead.encodetype = g_recplay[channel].v_encode;
				avhead.frametype = rechead.type;
				avhead.framerate = rechead.fps;
				avhead.res = g_recplay[channel].res;
				avhead.size = rechead.size;
				avhead.ntsamp_sec = rechead.time_sec;
				avhead.ntsamp_usec = rechead.time_usec;
				avhead.framenum = framenum++;

				ret = PPCS_Write(g_recplay[channel].sesssion, P2P_CHANNEL_RECAV, (char *)&avhead, sizeof(VAVA_AV_HEAD));
				if(ret != sizeof(VAVA_AV_HEAD))
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: send I head err\n", FUN, LINE);
				}
				
				ret = PPCS_Write(g_recplay[channel].sesssion, P2P_CHANNEL_RECAV, readbuff, rechead.size);
				if(ret != rechead.size)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: send I data err\n", FUN, LINE);
				}
				else
				{
					totolsize += rechead.size;
				}
				start = 1;
				break;
			case 8: //AUDIO
				if(start == 1)
				{
					memset(&avhead, 0, sizeof(VAVA_AV_HEAD));
					avhead.tag = VAVA_TAG_AP_AUDIO;
					avhead.encodetype = g_recplay[channel].a_encode;
					avhead.frametype = rechead.type;
					avhead.framerate = rechead.fps;
					avhead.size = rechead.size;
					avhead.ntsamp_sec = rechead.time_sec;
					avhead.ntsamp_usec = rechead.time_usec;
					avhead.framenum = framenum++;

					ret = PPCS_Write(g_recplay[channel].sesssion, P2P_CHANNEL_RECAV, (char *)&avhead, sizeof(VAVA_AV_HEAD));
					if(ret != sizeof(VAVA_AV_HEAD))
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: send A head err\n", FUN, LINE);
					}
					
					ret = PPCS_Write(g_recplay[channel].sesssion, P2P_CHANNEL_RECAV, readbuff, rechead.size);
					if(ret != rechead.size)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: send A data err\n", FUN, LINE);
					}
					else
					{
						totolsize += rechead.size;
					}
				}
				break;
			default:
				break;
		}

		#if 0
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: frame type - %d, sec = %d, usec = %d\n", 
		                                FUN, LINE, rechead.type, rechead.time_sec, rechead.time_usec);
		#endif

		if(rechead.type == 8)
		{
			continue;
		}
		
		if(g_recplay[channel].type == VAVA_RECFILE_TRANSPORT_NORMA)
		{
			usleep(30000);
		}
		else
		{
			usleep(5000);
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totolsize = %d\n", FUN, LINE, totolsize);

	//发送完成确认包
	memset(&avhead, 0, sizeof(VAVA_AV_HEAD));
	avhead.tag = VAVA_TAG_AP_VIDEO;
	avhead.encodetype = 0;
	avhead.frametype = 0;
	avhead.framerate = 0;
	avhead.res = 0;
	avhead.size = 0;
	avhead.ntsamp_sec = 0;
	avhead.ntsamp_usec = 0;
	avhead.framenum = 0;
	PPCS_Write(g_recplay[channel].sesssion, P2P_CHANNEL_RECAV, (char *)&avhead, sizeof(VAVA_AV_HEAD));

	//发送完成指令
	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		
		return NULL;
	}

	cJSON_AddStringToObject(pJsonRoot, "dirname", g_recplay[channel].dirname);
	cJSON_AddStringToObject(pJsonRoot, "filename", g_recplay[channel].filename);
	cJSON_AddNumberToObject(pJsonRoot, "size", totolsize);

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	
	memset(&vavahead, 0, sizeof(VAVA_CMD_HEAD));
	vavahead.sync_code = VAVA_TAG_APP_CMD;
	vavahead.cmd_code = VAVA_CMD_PLAYBACK_END;
	vavahead.cmd_length = strlen(pstr);

	memset(sendbuff, 0, 1024);
	memcpy(sendbuff, &vavahead, sizeof(VAVA_CMD_HEAD));
	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(g_recplay[channel].sesssion, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(g_recplay[channel].sesssion, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	pthread_mutex_lock(&mutex_recplay_lock);

	fclose(g_recplay[channel].fd);
	g_recplay[channel].fd = NULL;
	g_recplay[channel].sesssion = -1;
	g_recplay[channel].token = 0;
	g_recplay[channel].type = -1;
	g_recplay[channel].ctrl = 0;
	g_recplay[channel].v_encode = -1;
	g_recplay[channel].a_encode = -1;
	g_recplay[channel].fps = 0;
	g_recplay[channel].res = -1;
	g_recplay[channel].encrypt = VAVA_REC_ENCRYPT_FREE;
	memset(g_recplay[channel].dirname, 0, 9);
	memset(g_recplay[channel].filename, 0, 11);

	pthread_mutex_unlock(&mutex_recplay_lock);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Exit channel = %d\n", FUN, LINE, channel);

	return NULL;
}


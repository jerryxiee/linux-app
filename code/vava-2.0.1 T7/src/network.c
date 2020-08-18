#include "basetype.h"
#include "vavahal.h"
#include "record.h"
#include "network.h"
#include "sdcheck.h"
#include "mp4v2.h"  
#include "watchdog.h"
#include "aesencrypt.h"

//#define DURATION_OFFSET		45000	//AAC前半秒编码数据为0补偿值
//#define OFFSET_VAL			3000	//MP410帧转15帧被偿值 (90000 / 10 - 90000 / 15)

typedef struct Joseph_Mp4_Config
{
	MP4FileHandle hFile;			//mp4文件描述符
	MP4TrackId video;				//视频轨道标志符
	MP4TrackId audio;				//音频轨道标志符
	int m_vFrameDur;				//帧间隔时间
	unsigned int timeScale;			//视频每秒的ticks数,如90000
	unsigned int fps;				//视频帧率
	unsigned short width;			//视频宽
	unsigned short height;			//视频高
	unsigned long long int timestamp;
	unsigned long long int mtimestamp;
}VAVA_MP4config;

static unsigned char g_nassync_flag;
volatile unsigned char g_nas_net_check = 0;

void *FTP_Manage_pth(void *data)
{
	int i;
	int ftpflag = 0; //0 off  1 open
	int checkflag = 0;
	int checknum = 0;
	
	while(g_running)
	{
		if(g_gatewayinfo.sdstatus != VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.format_flag == 1)
		{
			if(ftpflag == 1)
			{
				VAVAHAL_SystemCmd("/etc/init.d/vsftpd stop");
				ftpflag = 0;
				checknum = 0;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========  FTP CLOSE ========\n", FUN, LINE);
			}

			sleep(5);
			continue;
		}
		
		checkflag = 0;
		
		for(i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			if(g_pair[i].nstat == 0)
			{
				continue;
			}

			if(g_alarmrec[i].ctrl == 1 || g_manaulrec[i].ctrl == 1)
			{
				checkflag = 1;
				break;
			}
		}

		if(checkflag == 0)
		{
			if(ftpflag == 0)
			{
				checknum++; //(5秒一次)
				if(checknum >= 6)
				{
					VAVAHAL_SystemCmd("/etc/init.d/vsftpd start");
					ftpflag = 1;

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========  FTP OPEN ========\n", FUN, LINE);
				}
			}
		}
		else
		{
			if(ftpflag == 1)
			{
				VAVAHAL_SystemCmd("/etc/init.d/vsftpd stop");
				ftpflag = 0;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========  FTP CLOSE ========\n", FUN, LINE);
			}

			checknum = 0;
		}

		sleep(5);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

int Nas_GetCapacity(char *path)
{
	struct statfs statFS; //系统stat的结构体
	unsigned long long usedBytes = 0;
	unsigned long long freeBytes = 0;
	unsigned long long totalBytes = 0;

	if(statfs(path, &statFS) == -1)
	{   
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Nas_GetCapacity: statfs failed for path->[%s]\n", FUN, LINE, path);
		return -1;   
	} 

	totalBytes = (unsigned long long)statFS.f_blocks * (unsigned long long)statFS.f_frsize; //详细的分区总容量， 以字节为单位
	freeBytes = (unsigned long long)statFS.f_bfree * (unsigned long long)statFS.f_frsize;   //详细的剩余空间容量，以字节为单位
	usedBytes = (unsigned long long)(totalBytes - freeBytes);    

	//换算成MB
    g_gatewayinfo.nas_totol = totalBytes / 1024 / 1024;
    g_gatewayinfo.nas_used = usedBytes / 1024 / 1024;
    g_gatewayinfo.nas_free =  freeBytes / 1024 / 1024;

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol - %d MB, used - %d MB, free - %d MB\n", 
		                            FUN, LINE, g_gatewayinfo.nas_totol, g_gatewayinfo.nas_used, g_gatewayinfo.nas_free);
	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);

	return 0;
}

void *Nas_Manage_pth(void *data)
{
	int ret;
	int mountflag = 0;		//挂载状态
	int nasspace = 0;
	//int pingtime = 0;
	char cmd[256];

	//延时启动(2分钟)
	int starttime = 120;

	FILE *fd = NULL;

	while(g_running)
	{
		if(starttime > 0)
		{
			starttime--;
			sleep(1);
			continue;
		}
		
		if(g_nas_config.ctrl == 0 || g_nas_net_check == 0)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [##]-> NasOFF: mountflag = %d, syncflag = %d\n", FUN, LINE, mountflag, g_nassync_flag);

			g_nas_status = VAVA_NAS_STATUS_IDLE;
			g_nassync_flag = 0; //停止同步

			if(mountflag == 1)
			{
				sleep(1);
				
				VAVAHAL_SystemCmd("/bin/umount -l /mnt/nas");
				VAVAHAL_SystemCmd("/bin/rm -rf /mnt/nas");
				
				mountflag = 0;
			}
	
			sleep(10);
			continue;
		}

		if(g_nas_change == 1)
		{
			g_nas_change = 0;
			
			g_nassync_flag = 0; //停止同步

			if(mountflag == 1)
			{
				sleep(1);
				
				VAVAHAL_SystemCmd("/bin/umount -l /mnt/nas");
				VAVAHAL_SystemCmd("/bin/rm -rf /mnt/nas");
				
				mountflag = 0;

				sleep(5);
			}
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [##]-> NasON: mountflag = %d, syncflag = %d\n", FUN, LINE, mountflag, g_nassync_flag);

		//检测NAS参数是否正确
		if(mountflag == 0)
		{
			if(VAVAHAL_CheckIP(g_nas_config.ip) != 0 || VAVAHAL_CheckLinuxPath(g_nas_config.path) != 0)
			{
				g_nas_status = VAVA_NAS_STATUS_PARAM_FAIL;
				g_nassync_flag = 0; //停止同步
				
				sleep(10);
				continue;
			}
		}

		//检测服务器连接状态
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: PING Nas server...\n", FUN, LINE);
		
		ret = VAVAHAL_PingIPStatus(g_nas_config.ip);
		if(ret != 0)
		{
			g_nas_status = VAVA_NAS_STATUS_CONNECT_FAIL;
			g_nassync_flag = 0; //停止同步

			if(mountflag == 1)
			{
				VAVAHAL_SystemCmd("/bin/umount -l /mnt/nas");
				VAVAHAL_SystemCmd("/bin/rm -rf /mnt/nas");
				mountflag = 0;
			}

			//ngtime = 0;
			sleep(10);
			continue;
		}

		//检测TF卡状态
		if(g_gatewayinfo.sdstatus != VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.format_flag == 1 || g_gatewayinfo.totol < 100)
		{
			g_nas_status = VAVA_NAS_STATUS_SD_FAIL;
			g_nassync_flag = 0; //停止同步

			if(mountflag == 1)
			{
				VAVAHAL_SystemCmd("/bin/umount -l /mnt/nas");
				VAVAHAL_SystemCmd("/bin/rm -rf /mnt/nas");
				mountflag = 0;
			}

			sleep(10);
			continue;
		}

		//检测NAS挂载情况
		if(mountflag == 0)
		{
			VAVAHAL_SystemCmd("/bin/umount -l /mnt/nas");
			sleep(1);

			VAVAHAL_SystemCmd("/bin/mkdir -p /mnt/nas");
			memset(cmd, 0, 256);
			sprintf(cmd, "/bin/mount -t nfs -v -o rw,intr,soft,timeo=1,retry=1 %s:%s  /mnt/nas -o nolock", g_nas_config.ip, g_nas_config.path);
			ret = VAVAHAL_SystemCmd(cmd);
			if(ret != 0)
			{
				g_nas_status = VAVA_NAS_STATUS_MOUNT_FAIL;
				sleep(10);
				continue;
			}

			sleep(1);
			
			//检测是否可写入
			fd = fopen("/mnt/nas/test.file", "wb");
			if(fd == NULL)
			{
				VAVAHAL_SystemCmd("/bin/umount -l /mnt/nas");
				sleep(1);
				
				g_nas_status = VAVA_NAS_STATUS_NOWRITE;
				sleep(10);
				continue;
			}
			else
			{
				fclose(fd);
				fd = NULL;

				//删除测试文件
				VAVAHAL_SystemCmd("/bin/rm -f /mnt/nas/test.file");
			}

			//获取空间大小
			Nas_GetCapacity("/mnt/nas");
			if(g_gatewayinfo.nas_free <= 500)
			//if(g_gatewayinfo.nas_free <= 1868000)
			{
				g_nas_status = VAVA_NAS_STATUS_LACKOF_SPACE;
				g_nassync_flag = 0;

				sleep(10);
				continue;
			}
			
			g_nas_status = VAVA_NAS_STATUS_SYNC;
			g_nassync_flag = 1;
			
			mountflag = 1;
		}
		else
		{
			if(nasspace++ >= 180)
			{
				nasspace = 0;
				
				Nas_GetCapacity("/mnt/nas");

				if(g_gatewayinfo.nas_free <= 500)
				{
					g_nas_status = VAVA_NAS_STATUS_LACKOF_SPACE;
					g_nassync_flag = 0;
				}
				else
				{
					g_nas_status = VAVA_NAS_STATUS_SYNC;
					g_nassync_flag = 1;
				}
			}
		}

		sleep(10);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

int Nas_GetSpsPps(unsigned char *buff, unsigned char *spsbuff, unsigned char *ppsbuff, 
	              unsigned char *setbuff, int *spssize, int *ppssize, int *setsize)
{
	int i;

	for(i = 0; i < 100; i++)
	{
		if(buff[i] == 0 && buff[i + 1] == 0 && buff[i + 2] == 0 && buff[i + 3] == 1 && buff[i + 4] == 0x68)	
		{
			*spssize = i - 4;
			memcpy(spsbuff, buff + 4, *spssize);
		}
	}

	for(i = *spssize + 4; i < 100; i++)
	{
		if(buff[i] == 0 && buff[i + 1] == 0 && buff[i + 2] == 0 && buff[i + 3] == 1 && buff[i + 4] == 0x6)
		{
			*ppssize = i - *spssize - 8;
			memcpy(ppsbuff, buff + *spssize + 8, *ppssize);
		}
	}

	for(i = *ppssize + 8; i < 100; i++)
	{
		if(buff[i] == 0 && buff[i + 1] == 0 && buff[i + 2] == 0 && buff[i + 3] == 1 && buff[i + 4] == 0x65)
		{
			*setsize = i - *ppssize - *spssize - 12;
			memcpy(setbuff, buff + *spssize + *ppssize + 12, *setsize);
		}
	}

	return 0;
}

int Nas_RecFile_Verification(char *recfilename)
{
	int ret;
	char checktime[7];
	char ch_str[2];
	char type_str[2];
	int channel;
	int type;
	
	
	if(recfilename == NULL)
	{
		return -1;
	}

	if(strlen(recfilename) < 10)
	{
		return -1;
	}

	if(recfilename[6] != '_' || recfilename[8] != '_')
	{
		return -1;
	}

	memset(checktime, 0, 7);
	memcpy(checktime, recfilename, 6);

	ret = VAVAHAL_Time_Verification(checktime);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check time fail, time = [%s]\n", FUN, LINE, checktime);
		return -1;
	}

	memset(type_str, 0, 2);
	memset(ch_str, 0, 2);
	memcpy(type_str, recfilename + 7, 1);
	memcpy(ch_str, recfilename + 9, 1);
	channel = atoi(ch_str);
	type = atoi(type_str);

	if(channel < 0 || channel > MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check channel fail, channel = %d\n", FUN, LINE, channel);
		return -1;
	}

	if(type != VAVA_RECFILE_TYPE_FULLTIME && type != VAVA_RECFILE_TYPE_ALARM && type != VAVA_RECFILE_TYPE_MANAUL)
	{
		return -1;
	}
	
	return 0;
}

int Nas_CloseMp4Encoder(void *handle)
{
	VAVA_MP4config *mp4_config = (VAVA_MP4config*)handle;

	if(mp4_config->hFile)
	{  
		MP4Close(mp4_config->hFile, 0);  
		mp4_config->hFile = NULL;  
	}

	return 0;
}

int Nas_stop()
{
	g_nassync_flag = 0;
	g_nas_config.ctrl = 0;

	return 0;
}

int Nas_SyncFile(char *dirname, char *filename, char *path)
{
	FILE *fd = NULL;

	VAVA_RecInfo recinfo;
	VAVA_RecHead rechead;
	char srcpath[128];
	
	VAVA_MP4config *mp4_config = NULL;

	int i;
	int ret;
	int result = 0;
	int first = 0;
	int spssize;
	int ppssize;
	int setsize;
	unsigned char spsbuff[32];
	unsigned char ppsbuff[32];
	unsigned char setbuff[32];
	unsigned char *readbuff = NULL;

	long long video_time = -1;
	//int ntsamp_offset = 0;
	
	if(dirname == NULL || filename == NULL)
	{
		return 0;
	}

	readbuff = g_avnasbuff.buff;
	memset(readbuff, 0, MEM_AV_NAS_SIZE);

	memset(srcpath, 0, 128);
	sprintf(srcpath, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, filename);
	fd = fopen(srcpath, "r");
	if(fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open file [%s] fail\n", FUN, LINE, srcpath);
		return 0;
	}

	ret = fread(&recinfo, sizeof(VAVA_RecInfo), 1, fd);
	if(ret <= 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: Red Recfile info fail\n", FUN, LINE);
		
		fclose(fd);
		return 0;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------- NAS SYNC FILE START -------------\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:          Filesize - %d, [fps - %d]             \n", FUN, LINE, recinfo.size, recinfo.fps);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -----------------------------------------------\n", FUN, LINE);

	/*init mp4*/ 
	mp4_config = (VAVA_MP4config *)Nas_InitMp4Encoder(path, recinfo.fps);
	if(mp4_config == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init mp4 failed\n", FUN, LINE);
		
		fclose(fd);
		return 0;
	}	

	while(g_running)
	{	
		if(g_nassync_flag == 0 || g_nas_net_check == 0)
		{
			result = 2;
			break;
		}
		
		if(feof(fd))
		{
			break;
		}

		ret = fread(&rechead, sizeof(VAVA_RecHead), 1, fd);
		if(ret <= 0)
		{
			break;
		}

		if(rechead.size >= MEM_AV_NAS_SIZE)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check size too big, size = %d\n", FUN, LINE, rechead.size);
			break;
		}

		memset(readbuff, 0, MEM_AV_NAS_SIZE);
		ret = fread(readbuff, rechead.size, 1, fd);
		if(ret <= 0)
		{
			break;
		}

		if((rechead.type == 1 || rechead.type == 8) && recinfo.encrypt == VAVA_REC_ENCRYPT_AES)
		{
			VAVA_Aes_Decrypt((char *)readbuff, (char *)readbuff, rechead.size);
		}

		if(first == 0)
		{
			memset(spsbuff, 0, 32);
			memset(ppsbuff, 0, 32);
			Nas_GetSpsPps(readbuff, spsbuff, ppsbuff, setbuff, &spssize, &ppssize, &setsize);

			#if 0
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get spssize = %d, ppssize = %d, setsize = %d\n", 
					                        FUN, LINE, spssize, ppssize, setsize);
			#endif

			mp4_config->video = MP4AddH264VideoTrack(mp4_config->hFile,   
					                                 mp4_config->timeScale,  //timeScale
					                                (mp4_config->timeScale / mp4_config->fps),  //sampleDuration    timeScale/fps
													 mp4_config->width,     // width  
													 mp4_config->height,    // height  
													 spsbuff[1], 			// sps[1] AVCProfileIndication  
													 spsbuff[2], 			// sps[2] profile_compat  
													 spsbuff[3], 			// sps[3] AVCLevelIndication  
													 3);         

			//设置sps和pps  
		    MP4AddH264SequenceParameterSet(mp4_config->hFile, mp4_config->video, spsbuff, spssize);  
		    MP4AddH264PictureParameterSet(mp4_config->hFile, mp4_config->video, ppsbuff, ppssize);  
			MP4AddH264PictureParameterSet(mp4_config->hFile, mp4_config->video, setbuff, setsize); 
			mp4_config->timestamp = 0;
			first = 1;
		}

		result = 0;

		switch(rechead.type)
		{
			case 0:
				readbuff[0] = (rechead.size - 4) >> 24;  
				readbuff[1] = (rechead.size - 4) >> 16;  
				readbuff[2] = (rechead.size - 4) >> 8;  
				readbuff[3] = (rechead.size - 4) & 0xff; 

				video_time = (90000 / rechead.fps);

				/*write video P frame to mp4*/
				if(!MP4WriteSample(mp4_config->hFile, mp4_config->video, readbuff, rechead.size, video_time, 0, 0))
				{  
					result = 1;  
				}
				break;
			case 1:
				//只能填写实际帧数据
				for(i = 0; i < 100; i++)
				{
					if(readbuff[i] == 0 && readbuff[i + 1] == 0 && readbuff[i + 2] == 0 && readbuff[i + 3] == 1 && readbuff[i + 4] == 0x65)
					{
						break;
					}
				}
				
				readbuff[i] = (rechead.size - 4 - i) >> 24;  
				readbuff[i + 1] = (rechead.size - 4 - i) >> 16;  
				readbuff[i + 2] = (rechead.size - 4 - i) >> 8;  
				readbuff[i + 3] = (rechead.size - 4 - i) & 0xff; 

				video_time = (90000 / rechead.fps);
				
				/*write video I frame to mp4*/
				if(!MP4WriteSample(mp4_config->hFile, mp4_config->video, readbuff + i, rechead.size - i, video_time, 0, 1))
				{  
					result = 1;  
				}
				break;
			case 8:
				//音频 
				if(!MP4WriteSample(mp4_config->hFile, mp4_config->audio, readbuff + 7, rechead.size - 7, MP4_INVALID_DURATION, 0, 1))
				{
					result = 1;
				}
				break;
			default:
				break;
		}

		if(result != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: Write frame [%d] sample fail\n", FUN, LINE, rechead.type);
			break;
		}

		usleep(50000);
	}

	fclose(fd);

	if(result != 2)
	{
		Nas_CloseMp4Encoder((void *)mp4_config);
	}

	free(mp4_config);
	mp4_config = NULL;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------- NAS SYNC FILE END -------------\n", FUN, LINE);

	return result;
}

void *Nas_InitMp4Encoder(char *mp4path, int fps)
{
	VAVA_MP4config *mp4_config = NULL;

	unsigned char aacConfig[2] = {0x15, 0x88};  // 0001 0101 1000 1000
	
	mp4_config = (VAVA_MP4config *)malloc(sizeof(VAVA_MP4config));

	mp4_config->m_vFrameDur = 0;
	mp4_config->video = MP4_INVALID_TRACK_ID;	
	mp4_config->audio = MP4_INVALID_TRACK_ID;
	mp4_config->hFile = NULL;
	mp4_config->timeScale = 90000;	
	mp4_config->fps = fps; 
	mp4_config->width = 1920;          
	mp4_config->height = 1080; 

	/*file handle*/
	mp4_config->hFile = MP4Create(mp4path, 0);
	if(mp4_config->hFile == MP4_INVALID_FILE_HANDLE)
	{
		free(mp4_config);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open outfile [%s] fialed\n", FUN, LINE, mp4path);
        return NULL;
    }

	MP4SetTimeScale(mp4_config->hFile, mp4_config->timeScale);  //timeScale

	/*audio track*/
	mp4_config->audio = MP4AddAudioTrack(mp4_config->hFile, 8000, 1024, MP4_MPEG4_AUDIO_TYPE);
	if(mp4_config->audio == MP4_INVALID_TRACK_ID)
    {
		free(mp4_config);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: add audio track failed\n", FUN, LINE);
        return NULL;
    }

	MP4SetAudioProfileLevel(mp4_config->hFile, 0x2);
	MP4SetTrackESConfiguration(mp4_config->hFile, mp4_config->audio, aacConfig, 2);

	return (void *)mp4_config;
}

void *Nas_Sync_pth(void *data)
{
	int ret;
	int flag = 0; 
	int datenum;
	int filenum;
	int timeflag = 0;

	FILE *tmpfd = NULL;
	DIR *dir = NULL;
	char *datelist = NULL;
	char *filelist = NULL;

	char dirname[9];
	char filename[11];
	char path[128];
	char cmd[128];

	VAVA_Idx_Head *datehead;
	VAVA_Idx_Head *filehead;
	VAVA_Idx_Date *idxdate;
	VAVA_Idx_File *idxfile;

	struct timeval t_lasttime;	//上次同步时间
	struct timeval t_currtime;	

	while(g_running)
	{
		if(g_nassync_flag == 0 || g_nas_net_check == 0)
		{
			if(datelist != NULL)
			{
				free(datelist);
				datelist = NULL;
			}

			if(filelist != NULL)
			{
				free(filelist);
				filelist = NULL;
			}

			sleep(5);
			continue;
		}

		if(timeflag == 1)
		{
			gettimeofday(&t_currtime, NULL);

			//4小时同步一次
			if(t_currtime.tv_sec - t_lasttime.tv_sec >= 14400)
			{
				timeflag = 0;
			}

			sleep(10);
			continue;
		}

		if(flag == 0)
		{
			//获取日期列表
			pthread_mutex_lock(&mutex_idx_lock);
			datelist = Rec_ImportDateIdx();
			pthread_mutex_unlock(&mutex_idx_lock);

			if(datelist == NULL)
			{
				sleep(10);
				continue;
			}

			flag = 1;

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$$ Nas_Sync_pth: Beging Sync\n", FUN, LINE);

			memset(path, 0, 128);
			sprintf(path, "/mnt/nas/%s", g_gatewayinfo.sn);
			dir = opendir(path);
			if(dir == NULL)
			{
				memset(path, 0, 128);
				sprintf(path, "/bin/mkdir -p /mnt/nas/%s", g_gatewayinfo.sn);
				VAVAHAL_SystemCmd(path);
			}
			else
			{
				closedir(dir);
			}
		}
		else
		{
			datehead = (VAVA_Idx_Head *)datelist;
			if(datehead->tag != VAVA_IDX_HEAD)
			{
				free(datelist);
				datelist = NULL;
				flag = 0; 
				
				sleep(10);
				continue;
			}

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol = %d, first = %d\n", FUN, LINE, datehead->totol, datehead->first);
			
			for(datenum = 0; datenum < datehead->totol; datenum++)
			{
				if(g_nassync_flag == 0 || g_nas_net_check == 0)
				{
					break;
				}

				memset(dirname, 0, 9);
				idxdate = (VAVA_Idx_Date *)(datelist + sizeof(VAVA_Idx_Head) + datenum * sizeof(VAVA_Idx_Date));
				VAVAHAL_ParseDateStr(idxdate->dirname, dirname, idxdate->random);

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: random = %d, [%x %x %x %x %x %x %x %x] dirname = [%s]\n", 
					                            FUN, LINE, idxdate->random - 0x1a, idxdate->dirname[0], idxdate->dirname[1],
					                            idxdate->dirname[2], idxdate->dirname[3], idxdate->dirname[4],
					                            idxdate->dirname[5], idxdate->dirname[6], idxdate->dirname[7], dirname);
				
				ret = VAVAHAL_Date_Verification(dirname);
				if(ret == 0)
				{
					//检测目标文件夹
					ret = VAVAHAL_CheckNasDir(dirname);
					if(ret == 0)
					{
						//获取文件列表
						pthread_mutex_lock(&mutex_idx_lock);
						filelist = Rec_ImportFileIdx(dirname);
						pthread_mutex_unlock(&mutex_idx_lock);

						if(filelist == NULL)
						{
							usleep(200000);
							continue;
						}

						filehead = (VAVA_Idx_Head *)filelist;
						if(filehead->tag != VAVA_IDX_HEAD)
						{
							free(filelist);
							filelist = NULL;
							usleep(200000);
							continue;
						}

						for(filenum = 0; filenum < filehead->totol; filenum++)
						{
							if(g_nassync_flag == 0 || g_nas_net_check == 0)
							{
								break;
							}

							memset(filename, 0, 11);
							idxfile = (VAVA_Idx_File *)(filelist + sizeof(VAVA_Idx_Head) + filenum * sizeof(VAVA_Idx_File));
							VAVAHAL_ParseFileStr(idxfile->filename, filename, idxfile->random);
							
							//ret = VAVAHAL_RecFile_Verification(filename);
							ret = Nas_RecFile_Verification(filename);
							if(ret == 0)
							{
								memset(path, 0, 128);
								ret = VAVAHAL_BuildNasPath(path, dirname, filename);
								if(ret == 0)
								{
									tmpfd = fopen(path, "r");
									if(tmpfd == NULL)
									{
										#if 0
										while(g_running && g_nassync_flag)
										{
											ret = VAVAHAL_CheckSysIdle();
											if(ret == 0)
											{
												break;
											}

											sleep(1);
											continue;
										}
										#endif

										if(g_nassync_flag == 0 || g_nas_net_check == 0)
										{
											break;
										}

										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Nas sync: [%s/%s] ---> [%s/%s.mp4]\n", 
					                                                    FUN, LINE, dirname, filename, dirname, filename);
										
										//未发现目标 同步该文件
										ret = Nas_SyncFile(dirname, filename, path);
										if(ret == 0)
										{
											sleep(10);
										}
										else if(ret == 2)
										{
											memset(cmd, 0, 128);
											sprintf(cmd, "/bin/rm -f %s", path);
											VAVAHAL_SystemCmd(cmd);
											continue;
										}

										//sleep(1);
									}
									else
									{
										//已存在 不需要同步
										fclose(tmpfd);
									}
								}
							}

							usleep(30000);
						}

						if(filenum == filehead->totol)
						{
							//文件夹同步完成
							VAVAHAL_UpdateNasDir(dirname);
						}

						if(filelist != NULL)
						{
							free(filelist);
							filelist = NULL;
						}
					}
				}

				usleep(200000);
			}

			if(datenum == datehead->totol)
			{
				//完全同步
				timeflag = 1;

				//存储上次同步时间
				gettimeofday(&t_lasttime, NULL);
			}

			if(datelist != NULL)
			{
				free(datelist);
				datelist = NULL;
			}

			flag = 0;
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}


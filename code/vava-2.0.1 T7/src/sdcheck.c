#include "basetype.h"
#include "errlog.h"
#include "record.h"
#include "vavahal.h"
#include "watchdog.h"
#include "sdcheck.h"

#define DIR_DATA_TYPE_DIR				4  //文件夹
#define DIR_DATA_TYPE_FILE				8  //文件
#define DIR_HEAD_IDX_SIZE				50000
#define DIR_DATE_IDX_SIZE				200000

int SD_CheckDev(char *devname)
{
	FILE *fd = NULL;
	char buff_1[32];
	char buff_2[32];

	//获取磁盘信息
	VAVAHAL_SystemCmd("cat /proc/partitions |  grep -r \"mmcblk\" | tail -n 1 |  awk -F' ' '{ print $4}' > /tmp/sd");
	usleep(100000);

	fd = fopen("/tmp/sd", "r");
	if(fd == NULL)
	{
		return -1;
	}

	memset(buff_1, 0, 32);
	memset(buff_2, 0, 32);

	//取两次 有些SD卡插上去名字不一样
	//-------------- 1 --------------
	fgets(buff_1, 32, fd);
	buff_1[strlen(buff_1) - 1] = '\0';
	//-------------- 2 --------------
	fgets(buff_2, 32, fd);
	buff_2[strlen(buff_2) - 1] = '\0';
	fclose(fd);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: buff_1 = %s\n", FUN, LINE, buff_1);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: buff_2 = %s\n", FUN, LINE, buff_2);

	if(buff_1[0] == '\0' && buff_1[0] == '\0')
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: No find dev\n", FUN, LINE);
		return -1;
	}

	memset(devname, 0, 32);
	
	if(buff_2[0] != '\0')
	{
		strcpy(devname, buff_2);
	}
	else
	{
		strcpy(devname, buff_1);
	}

	return 0;
}

static int SD_MountDev(char *devname)
{
	int ret;
	char syscmd[256];
	
	VAVAHAL_SystemCmd("/bin/mkdir -p /mnt/sd0");
	VAVAHAL_SystemCmd("sync");
	sleep(1);

	memset(syscmd, 0, 256);
	sprintf(syscmd, "/bin/mount -t vfat /dev/%s /mnt/sd0", devname);
	ret = VAVAHAL_SystemCmd(syscmd);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: mount sdcard fail\n", FUN, LINE);
		return -1;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: mount sdcard success\n", FUN, LINE);

	return 0;
}

static int SD_CheckMount()
{
	int ret;
	char m_type[256];

	ret = VAVAHAL_SystemCmd_Ex("mount | grep \"/mnt/sd0 type vfat\"", m_type, 256);
	if(ret == 0)
	{
		return 0;
	}

	return -1;
}

static int SD_CheckRecIDX()
{
	FILE *fd = NULL;
	char path[128];

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s", g_gatewayinfo.sn, RECIDX_FILE);

	fd = fopen(path, "r");
	if(fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check idx file fail\n", FUN, LINE);
		return -1;
	}
	fclose(fd);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: check idx file success\n", FUN, LINE);
	return 0;
}

static int SD_GetRecTime(char *path)
{
	FILE *fd = NULL;
	int ret;
	VAVA_RecInfo recinfo;

	fd = fopen(path, "r");
	if(fd == NULL)
	{
		return 0;
	}

	memset(&recinfo, 0, sizeof(VAVA_RecInfo));
	ret = fread(&recinfo, sizeof(VAVA_RecInfo), 1, fd);
	fclose(fd);

	if(ret <= 0 || recinfo.tag == 0)
	{
		return 0;
	}

	return recinfo.time;
}

static int SD_CreateListIDX(char *datename, char *idxlist)
{
	FILE *fd = NULL;
	char path[128];
	char buff[128];

	int ret;
	int type;
	int channel;
	int writesize = 0;
	
	VAVA_Idx_Head *idxhead;
	VAVA_Idx_File idxfile;
	
	struct timeval randomtime;

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, datename, RECIDX_FILE);
	fd = fopen(path, "r");
	if(fd != NULL)
	{
		fclose(fd);
		return 0;
	}

	memset(idxlist, 0, DIR_DATE_IDX_SIZE);
	idxhead = (VAVA_Idx_Head *)idxlist;
	idxhead->tag = VAVA_IDX_HEAD;
	idxhead->totol = 0;
	idxhead->first = 0;

	writesize += sizeof(VAVA_Idx_Head);

	memset(path, 0, 128);
	sprintf(path, "ls /mnt/sd0/%s/%s > /tmp/idxfile", g_gatewayinfo.sn, datename);
	VAVAHAL_SystemCmd(path);

	sleep(1);
	
	fd = fopen("/tmp/idxfile", "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(buff, 0, 128);
			if(fgets(buff, 128, fd) == NULL)
			{
				break;
			}
			buff[strlen(buff) - 1] = '\0';

			if(strlen(buff) != 10)
			{
				continue;
			}

			ret = VAVAHAL_RecFile_Verification_Ex(buff, &type, &channel);
			if(ret == 0)
			{
				memset(&idxfile, 0, sizeof(VAVA_Idx_File));
				gettimeofday(&randomtime, NULL);
				srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
				idxfile.random = rand() % 100 + 1; //生成1-100随机数
				idxfile.type = type;
				idxfile.channel = channel;
				VAVAHAL_EncryptionFileStr(buff, idxfile.filename, idxfile.random);
				idxfile.random += 0x1A;

				memset(path, 0, 128);
				sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, datename, buff);
				idxfile.rectime = SD_GetRecTime(path);

				memcpy(idxlist + writesize, &idxfile, sizeof(VAVA_Idx_File));
				writesize += sizeof(VAVA_Idx_File);
				idxhead->totol += 1;
			}
		}

		fclose(fd);
		fd = NULL;
	}

	if(idxhead->totol > 0)
	{
		memset(path, 0, 128);
		sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, datename, RECIDX_FILE);

		fd = fopen(path, "wb");
		if(fd == NULL)
		{
			return -1;
		}

		fwrite(idxlist, writesize, 1, fd);
		fflush(fd);
		fclose(fd);

		return 0;
	}

	return -1;
}

static int SD_CreateIDX()
{
	FILE *fd = NULL;
	char path[128];
	char cmd[128];
	char buff[128];
	int ret;
	int writesize = 0;
	
	VAVA_Idx_Head *idxhead;
	VAVA_Idx_Date idxdate;
	struct timeval randomtime;
	
	char *idxbuff = NULL;
	char *idxlist = NULL;
	
	idxbuff = malloc(DIR_HEAD_IDX_SIZE);
	if(idxbuff == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc idxbuff fail\n", FUN, LINE);
		
		Err_Log("malloc idxbuff fail");
		g_running = 0;
		return -1;
	}
	
	memset(idxbuff, 0, DIR_HEAD_IDX_SIZE);
	idxhead = (VAVA_Idx_Head *)idxbuff;
	idxhead->tag = VAVA_IDX_HEAD;
	idxhead->totol = 0;
	idxhead->first = 0;
	
	writesize += sizeof(VAVA_Idx_Head);

	idxlist = malloc(DIR_DATE_IDX_SIZE);
	if(idxlist == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc idxlist fail\n", FUN, LINE);
		
		Err_Log("malloc idxlist fail");
		free(idxbuff);
		g_running = 0;
		return -1;
	}

	memset(cmd, 0, 128);
	sprintf(cmd, "/bin/mkdir -p /mnt/sd0/%s", g_gatewayinfo.sn);
	VAVAHAL_SystemCmd(cmd);

	memset(path, 0, 128);
	sprintf(path, "ls /mnt/sd0/%s > /tmp/idxdate", g_gatewayinfo.sn);
	VAVAHAL_SystemCmd(path);

	sleep(1);

	fd = fopen("/tmp/idxdate", "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(buff, 0, 128);
			if(fgets(buff, 128, fd) == NULL)
			{
				break;
			}
			buff[strlen(buff) - 1] = '\0';
			
			if(strlen(buff) != 8)
			{
				continue;
			}

			ret = VAVAHAL_Date_Verification(buff);
			if(ret == 0)
			{
				if(atoi(buff) < 20180000)
				{
					memset(cmd, 0, 128);
					sprintf(cmd, "/bin/rm -rf /mnt/sd0/%s/%s", g_gatewayinfo.sn, buff);
					VAVAHAL_SystemCmd(cmd);
					continue;
				}

				ret = SD_CreateListIDX(buff, idxlist);
				if(ret == 0)
				{
					memset(&idxdate, 0, sizeof(VAVA_Idx_Date));
					gettimeofday(&randomtime, NULL);
					srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
					idxdate.random = rand() % 100 + 1; //生成1-100随机数
					VAVAHAL_EncryptionDateStr(buff, idxdate.dirname, idxdate.random);
					idxdate.random += 0x1A;

					memcpy(idxbuff + writesize, &idxdate, sizeof(VAVA_Idx_Date));
					writesize += sizeof(VAVA_Idx_Date);
					idxhead->totol += 1;
				}
			}
		}

		fclose(fd);
		fd = NULL;

		memset(path, 0, 128);
		sprintf(path, "/mnt/sd0/%s/%s", g_gatewayinfo.sn, RECIDX_FILE);

		fd = fopen(path, "wb");
		if(fd == NULL)
		{
			return -1;
		}

		fwrite(idxbuff, writesize, 1, fd);
		fflush(fd);
		fclose(fd);

		free(idxbuff);
		idxbuff = NULL;

		free(idxlist);
		idxlist = NULL;

		VAVAHAL_SystemCmd("/bin/rm -f /tmp/idxfile");
		VAVAHAL_SystemCmd("/bin/rm -f /tmp/idxdate");
	}

	return 0;
}

static int SD_CreateRecIDX()
{
	struct timeval t_start;
	struct timeval t_end;
	int usetime = 0;
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:             build IDX Start             \n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------\n", FUN, LINE);

	gettimeofday(&t_start, NULL);
	SD_CreateIDX();
	gettimeofday(&t_end, NULL);

	usetime = (int)(t_end.tv_sec - t_start.tv_sec);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:        Build End (usetime - %d s)       \n", FUN, LINE, usetime);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------\n", FUN, LINE);
	
	return 0;
}

static int SD_GetCapacity(char *path)
{
	struct statfs statFS; //系统stat的结构体
	unsigned long long usedBytes = 0;
	unsigned long long freeBytes = 0;
	unsigned long long totalBytes = 0;

	if(statfs(path, &statFS) == -1)
	{   
		//获取分区的状态 
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: statfs failed for path [%s]\n", FUN, LINE, path);
		return -1;   
	} 

	totalBytes = (unsigned long long)statFS.f_blocks * (unsigned long long)statFS.f_frsize; //详细的分区总容量， 以字节为单位
	freeBytes = (unsigned long long)statFS.f_bfree * (unsigned long long)statFS.f_frsize;   //详细的剩余空间容量，以字节为单位
	usedBytes = (unsigned long long)(totalBytes - freeBytes);    

	if(totalBytes >= 100000000)
	{
		//换算成MB
	    g_gatewayinfo.totol = totalBytes / 1024 / 1024;
	    g_gatewayinfo.used = usedBytes / 1024 / 1024;
	    g_gatewayinfo.free =  freeBytes / 1024 / 1024;

		VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol - %d MB, used - %d MB, free - %d MB\n", 
		                                FUN, LINE, g_gatewayinfo.totol, g_gatewayinfo.used, g_gatewayinfo.free);
		VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	}
	else
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Not need update, totol - %lld, used - %lld, free - %lld\n", 
		                                FUN, LINE, totalBytes, usedBytes, freeBytes);

		return -1;
	}

	return 0;
}

int SD_TFCheck(int *status, int *format)
{
	int ret;
	int tmpstatus = 0;
	int tmpformat = 0;
	char devname[32];

	//检测是否已挂载
	ret = SD_CheckMount();
	if(ret == 0)
	{
		//检测索引文件
		tmpstatus = 1;

		//直接重建一级索引
		ret = SD_CreateRecIDX();
		if(ret != 0)
		{
			tmpformat = 1;
		}

		#if 0
		//检测索引文件
		ret = SD_CheckRecIDX();
		if(ret != 0)
		{
			//重建索引
			ret = SD_CreateRecIDX();
			if(ret != 0)
			{
				tmpformat = 1;
			}
		}
		#endif

		*status = tmpstatus;
		*format = tmpformat;
	}
	else
	{
		//需要挂载
		ret = SD_CheckDev(devname);
		if(ret == 0)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Find SD dev\n", FUN, LINE);
			
			tmpstatus = 1;

			ret = SD_MountDev(devname);
			if(ret != 0)
			{
				tmpformat = 1;
			}
			else
			{
				//重建索引
				ret = SD_CreateRecIDX();
				if(ret != 0)
				{
					tmpformat = 1;
				}

				#if 0
				//检测索引文件
				ret = SD_CheckRecIDX();
				if(ret != 0)
				{
					//重建索引
					ret = SD_CreateRecIDX();
					if(ret != 0)
					{
						tmpformat = 1;
					}
				}
				#endif
			}

			*status = tmpstatus;
			*format = tmpformat;
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: status = %d, format = %d\n", FUN, LINE, tmpstatus, tmpformat);

	return 0;
}

void *SDPnp_pth(void *data)
{
	int i;
	int sockfd = -1;
	int len = 0;
	char recvbuff[4096];
	struct sockaddr_nl sa;
	struct iovec iov;
	struct msghdr msg;

	int tmpstatus = VAVA_SD_STATUS_NOCRAD;
	int tmpformat = 0;
	
	struct timeval t_nodev_1;
	struct timeval t_nodev_2;

	g_sdformating = 0;
	g_sdpoping = 0;
	tmpstatus = 0;
	tmpformat = 0;

#if 0
	VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");
	usleep(500000);
	VAVAHAL_SystemCmd("/bin/rm -rf /mnt/sd0");
	VAVAHAL_SystemCmd("sync");
	usleep(500000);
#endif

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----- start -----\n", FUN, LINE);

	//初次启动先检测一次TF卡情况
	gettimeofday(&t_nodev_1, NULL);
	SD_TFCheck(&tmpstatus, &tmpformat);
	gettimeofday(&t_nodev_2, NULL);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: SD_TFCheck used time - %d s\n", FUN, LINE, (int)(t_nodev_2.tv_sec - t_nodev_1.tv_sec));

	g_waitflag = 0;
	
	g_gatewayinfo.sdstatus = tmpstatus;
	g_gatewayinfo.format_flag = tmpformat;

	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD && g_gatewayinfo.format_flag == 0)
	{
		//开启录像功能
		VAVAHAL_OpenRec(1, 0);
	}

	memset(&sa, 0, sizeof(struct sockaddr_nl));
	sa.nl_family = AF_NETLINK;
	sa.nl_groups = NETLINK_KOBJECT_UEVENT;
	sa.nl_pid = 0; 

	iov.iov_base = (void *)recvbuff;
	iov.iov_len = 4096;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *)&sa;
	msg.msg_namelen = sizeof(sa);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	sockfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if(sockfd == -1)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: socket creat failed: %s\n", FUN, LINE, strerror(errno));
		Err_Log("SDsocket create fail");
		g_running = 0;
		return NULL;
	}

	if(bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: socket bind failed: %s\n", FUN, LINE, strerror(errno));
		Err_Log("SDsocket bind fail");
		g_running = 0;
		return NULL;
	}

	gettimeofday(&t_nodev_1, NULL);

	while(g_running)
	{	
		len = recvmsg(sockfd, &msg, MSG_DONTWAIT);
		if(len == 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: socket closed: %s\n", FUN, LINE, strerror(errno));	
			Err_Log("SDsocket bind fail");
			g_running = 0;
			return NULL;
		}
		else if(len >= 32)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --------------------------\n", FUN, LINE);
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, recvbuff);	
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --------------------------\n", FUN, LINE);
			
			if(strncmp(recvbuff, "add", strlen("add")) == 0 && strstr(recvbuff, "mmc") != NULL && g_update_sd_nochek == 0)
			{
				if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD || g_sdformating == 1)
				{				
					continue;
				}

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------- TF Cart Insert ----------\n", FUN, LINE);

				tmpstatus = 0;
				tmpformat = 0;

				SD_TFCheck(&tmpstatus, &tmpformat);
				
				//重新插卡后恢复检测
				g_sdpoping = 0;
				
				g_gatewayinfo.sdstatus = tmpstatus;
				g_gatewayinfo.format_flag = tmpformat;

				//开启录像功能
				VAVAHAL_OpenRec(1, 0);
			}
			else if(strncmp(recvbuff, "remove", strlen("remove")) == 0 && strstr(recvbuff, "mmc") != NULL && g_update_sd_nochek == 0)
			{
				if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_NOCRAD)
				{
					continue;
				}

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------- TF Cart Remove ----------\n", FUN, LINE);

				//关闭录像功能
				VAVAHAL_CloseRec(1, 0);
				
				for(i = 0; i < MAX_CHANNEL_NUM; i++)
				{
					//停止录像
					VAVAHAL_StopManualRec(i);
					VAVAHAL_StopFullTimeRec(i);
					VAVAHAL_StopAlarmRec(i);
				}

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: new remove messag...\n", FUN, LINE);
				
				VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");
				usleep(500000);
				VAVAHAL_SystemCmd("/bin/rm -rf /mnt/sd0");
				VAVAHAL_SystemCmd("sync");
					
				//初始化卡信息
				g_gatewayinfo.sdstatus = VAVA_SD_STATUS_NOCRAD;
				g_gatewayinfo.format_flag = 0;
				g_gatewayinfo.totol = 0;
				g_gatewayinfo.used = 0;
				g_gatewayinfo.free = 0;
			}

			continue;
		}

		sleep(1);

		gettimeofday(&t_nodev_2, NULL);

		//当无卡时每2分钟检测一次是否有卡插入
		if(t_nodev_2.tv_sec - t_nodev_1.tv_sec >= 120 && g_gatewayinfo.sdstatus == VAVA_SD_STATUS_NOCRAD
			&& g_sdformating == 0 && g_sdpoping == 0)
		{
			tmpstatus = 0;
			tmpformat = 0;

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: sdstatus - %d, sdformating - %d, sdpoping - %d\n", 
				                           FUN, LINE, g_gatewayinfo.sdstatus, g_sdformating, g_sdpoping);

			SD_TFCheck(&tmpstatus, &tmpformat);
			g_gatewayinfo.sdstatus = tmpstatus;
			g_gatewayinfo.format_flag = tmpformat;

			if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD && g_gatewayinfo.format_flag == 0)
			{
				//开启录像功能
				VAVAHAL_OpenRec(1, 0);
			}
			
			t_nodev_1.tv_sec = t_nodev_2.tv_sec;
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

void *SDSus_pth(void *data)
{
	int ret;
	int recflag;
	int checkflag;
	int tmpstatus;
	int tmpformat;
	struct timeval t_start;
	struct timeval t_current;

	g_sdformat = 0;
	checkflag = 0;
	recflag = 0;
	t_start.tv_sec = 0;

	int count = 0;
	
	while(g_running)
	{
		if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_NOCRAD || g_update_sd_nochek == 1)
		{
			//挂载成功后需要检测一次TF卡空间
			checkflag = 1;
			
			sleep(1);
			continue;	
		}
		else
		{
			if(g_gatewayinfo.format_flag == 1)
			{
				if(count++ >= 3)
				{
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: recheck sd format status\n", FUN, LINE);
					
					ret = SD_CheckMount();
					if(ret == 0)
					{
						ret = SD_CheckRecIDX();
						if(ret == 0)
						{
							g_gatewayinfo.format_flag = 0;
						}
					}

					count = 0;
				}

				sleep(1);
				continue;
			}
		}
	
		gettimeofday(&t_current, NULL);

		//每2分钟检测一次卡容量
		if(t_current.tv_sec - t_start.tv_sec >= 120 || g_sdformat == 1 || checkflag == 1)
		{
			sleep(4);
			
			if(g_sdformat == 1 || checkflag == 1)
			{	
				g_sdformat = 0;
				checkflag = 0;
				recflag = 1;
			}
			
			pthread_mutex_lock(&mutex_format_lock);

			ret = SD_GetCapacity("/mnt/sd0");
			if(ret == 0)
			{
				if(recflag == 1)
				{
					if(g_gatewayinfo.totol > 500)
					{
						//挂载完成后恢复录像
						recflag = 0;
						VAVAHAL_OpenRec(1, 0);
					}
					else 
					{
						VAVAHAL_CloseRec(1, 0);
						g_gatewayinfo.sdstatus = VAVA_SD_STATUS_FULL;
					}
				}

				if(g_gatewayinfo.free >= 200 && g_gatewayinfo.free <= 500) //保留500M可用空间
				{
					Rec_DelFirstRecDir();
				}
				else if(g_gatewayinfo.free < 200)
				{
					VAVAHAL_CloseRec(1, 0);
					g_gatewayinfo.sdstatus = VAVA_SD_STATUS_FULL;
				}
			}
			else
			{
				//开机概率出现挂载成功但实际未挂载上的问题
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: TF totol size less than 100 Mb, Need recheck\n", FUN, LINE);
					
				VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");
				
				tmpstatus = 0;
				tmpformat = 0;
				
				//重新检测一次TF卡
				SD_TFCheck(&tmpstatus, &tmpformat);
				g_gatewayinfo.sdstatus = tmpstatus;
				g_gatewayinfo.format_flag = tmpformat;

				if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD && g_gatewayinfo.format_flag == 0)
				{
					//开启录像功能
					VAVAHAL_OpenRec(1, 0);
				}

				pthread_mutex_unlock(&mutex_format_lock);
				continue;
			}
			
			t_start.tv_sec = t_current.tv_sec;
			pthread_mutex_unlock(&mutex_format_lock);
		}

		sleep(1);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}


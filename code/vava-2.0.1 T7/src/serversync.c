#include "basetype.h"
#include "vavahal.h"
#include "vavaserver.h"
#include "serversync.h"

int g_pair_sync_time = 0; 	//同步间隔(服务器->基站)
int g_status_sync_time = 0;	//同步间隔(同步基站和摄像机状态)
int g_status_sync_time_slow = 0;//同步间隔(同步基站和摄像机状态)
int g_bsattr_sync_flag = 0;		//同步标志(基站)
int g_cameraattr_sync_flag[MAX_CHANNEL_NUM];	//同步标志(摄像机)
VAVA_Pair_Sync g_pairsync[MAX_CHANNEL_NUM];

void *PairSync_pth(void *data)
{
	int ret;
	int time = 0;
	int channel;
	VAVA_ClearCamera cleardata;

	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		g_pairsync[channel].nstat = 0;
		g_pairsync[channel].type = 0;
		memset(g_pairsync[channel].sn, 0, 32);
	}

	time = 0;
	g_pair_sync_time = 10;
	
	while(g_running)
	{
		sleep(1);

		if(g_router_link_status != VAVA_NETWORK_LINKOK || g_pairmode == 1 || g_update.status != VAVA_UPDATE_IDLE)
		{
			continue;
		}

		if(g_pairmode == 1)
		{
			sleep(5);
			continue;
		}
		
		if(time++ < g_pair_sync_time)
		{
			continue;
		}

		time = 0;

		ret = VAVASERVER_GetToken();
		if(ret != 0)
		{
			continue;
		}

		ret = VAVASERVER_GetPairList();
		if(ret != 0)
		{
			continue;
		}

		pthread_mutex_lock(&mutex_pair_lock);

		for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
		{
			if(g_pair[channel].nstat == 0)
			{
				continue;
			}

			if(g_pairmode == 1)
			{
				break;
			}

			if(g_pairsync[channel].nstat == 0)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: - Remove By App - [channel - %d]\n", FUN, LINE, channel);
				
				//关闭录像功能
				VAVAHAL_CloseRec(0, channel);

				//关闭云存录像
				VAVAHAL_CloseCloudRec(0, channel);
				
				//停止录像
				VAVAHAL_StopManualRec(channel);
				VAVAHAL_StopFullTimeRec(channel);
				VAVAHAL_StopAlarmRec(channel);
				VAVAHAL_StopAlarmCloud(channel);

				g_cloudflag[channel] = 0;

				usleep(500000);

				//休眠摄像机
				VAVAHAL_SleepCamera(channel);

				//恢复参数
				VAVAHAL_ResetCamera(channel);

				memset(&cleardata, 0, sizeof(VAVA_ClearCamera));
				cleardata.addr = g_pair[channel].addr;

				//清除配对信息
				g_pair[channel].nstat = 0;
				g_pair[channel].ipaddr = 0xFFFFFFFF;
				g_pair[channel].addr = 0xFFFFFFFF;
				g_pair[channel].lock = 0;
				memset(g_pair[channel].mac, 0, 18);
				memset(g_pair[channel].id, 0, 32);

				//写入配置文件
				VAVAHAL_WritePairInfo();

				g_cloudrecheck[channel] = 1;

				//开启录像功能
				VAVAHAL_OpenRec(0, channel);

				VAVAHAL_InitCameraInfo(channel);

				VAVAHAL_InsertCmdList(channel, -1, VAVA_CMD_CLEARMATCH, &cleardata, sizeof(VAVA_ClearCamera));
			}
		}
		
		pthread_mutex_unlock(&mutex_pair_lock);
	}

	return NULL;
}

void *StatusSync_pth(void *data)
{
	int channel;
	int ret;
	int time;
	int sessionnum;

	sleep(10);
	
	g_status_sync_time = 10;
	g_status_sync_time_slow = 60;
	g_bsattr_sync_flag = 1;		
	
	time = 0;

	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		g_cameraattr_sync_flag[channel] = 1;
	}

	while(g_running)
	{
		sleep(1);

		if(g_bsattr_sync_flag == 1)
		{
			ret = VAVASERVER_GetToken();
			if(ret == 0)
			{
				ret = VAVASERVER_UpdateBsAttr();
				if(ret == 0)
				{
					g_bsattr_sync_flag = 0;
				}
			}
		}

		for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
		{
			if(g_pair[channel].nstat == 0)
			{
				continue;
			}
			
			if(g_cameraattr_sync_flag[channel] == 1 && g_camerainfo[channel].first_flag == 0)
			{
				ret = VAVASERVER_GetToken();
				if(ret == 0)
				{
					ret = VAVASERVER_UpdateCameraAttr(channel);
					if(ret == 0)
					{
						g_cameraattr_sync_flag[channel] = 0;
					}
				}
			}

		}
		
		VAVAHAL_GetSessionNum(&sessionnum);
		
		if(sessionnum <= 0)
		{
			if(time++ < g_status_sync_time_slow)
			{
				continue;
			}
		}
		else
		{
			if(time++ < g_status_sync_time)
			{
				continue;
			}
		}

		time = 0;

		ret = VAVASERVER_GetToken();
		if(ret != 0)
		{
			continue;
		}

		ret = VAVASERVER_UpdateStatus();
		if(ret != 0)
		{
			continue;
		}
	}

	return NULL;
}

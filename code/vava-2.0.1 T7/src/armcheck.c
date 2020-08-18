#include "basetype.h"
#include "rfserver.h"
#include "vavahal.h"
#include "armcheck.h"
#include "watchdog.h"

#define ARM_CLOSE				0
#define ARM_OPEN				1

void getdateinfo(char *date, int *weekday, int *currenttime)
{
	time_t tt;
	struct tm *timeinfo;

	time(&tt);
	timeinfo = localtime(&tt);

	sprintf(date, "%d%02d%02d", 1900 + timeinfo->tm_year, timeinfo->tm_mon + 1, timeinfo->tm_mday);
	*weekday = timeinfo->tm_wday;
	*currenttime = timeinfo->tm_hour * 3600 + timeinfo->tm_min * 60 + timeinfo->tm_sec;

	return;
}

void getdateinfo_ex(int *weekday, int *currenttime)
{
	time_t tt;
	struct tm *timeinfo;

	time(&tt);
	timeinfo = localtime(&tt);

	*weekday = timeinfo->tm_wday;
	*currenttime = timeinfo->tm_hour * 3600 + timeinfo->tm_min * 60 + timeinfo->tm_sec;

	return;
}

void print_arminfo(int flag, int channel)
{
	if(flag == ARM_CLOSE)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =====================================\n", FUN, LINE);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:         [ARM CLOSE] channel - %d \n", FUN, LINE, channel);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =====================================\n", FUN, LINE);
	}
	else if(flag == ARM_OPEN)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =====================================\n", FUN, LINE);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:         [ARM OPEN] channel - %d \n", FUN, LINE, channel);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =====================================\n", FUN, LINE);
	}

	return;
}

void *ArmCheck_v1_pth(void *data)
{
	int ret;
	int currentweek;
	int currenttime;
	int tmpweek;
	
	int channel;
	int armlist;

	int starttime;
	int endtime;
	int armflag;
	int printcount[MAX_CHANNEL_NUM];
	int tmpstatus[MAX_CHANNEL_NUM];

	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		printcount[channel] = 0;
		tmpstatus[channel] = -1;
	}
	
	while(g_running)
	{
		//获取当前星期和时间信息
		getdateinfo_ex(&currentweek, &currenttime);

		//按通道检测
		for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
		{
			//检测配对
			if(g_pair[channel].nstat == 0 || g_camerainfo_dynamic[channel].online == 0)
			{
				continue;
			}

			//检测摄像机是否准备就绪
			if(g_camerainfo[channel].first_flag == 1)
			{
				if(printcount[channel]++ >= 5)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: wait check, channel = %d, firstflag = %d\n", 
						                             FUN, LINE, channel, g_camerainfo[channel].first_flag);
					printcount[channel] = 0;
				}

				tmpstatus[channel] = -1;
				continue;
			}

			if(g_camera_arminfo_v1[channel].type == VAVA_ARMING_STATUS_DISABLE
				|| g_camera_arminfo_v1[channel].type == VAVA_ARMING_STATUS_ONTIME_OFF)
			{
				//全天撤防
				g_camera_arminfo_v1[channel].status = VAVA_ARMING_STATUS_DISABLE;

				if(tmpstatus[channel] != g_camera_arminfo_v1[channel].status)
				{
					//撤防
					ret = VAVAHAL_SetPirStatus(channel, VAVA_PIR_SENSITIVITY_OFF);
					if(ret == VAVA_ERR_CODE_SUCCESS)
					{
						tmpstatus[channel] = g_camera_arminfo_v1[channel].status;
						print_arminfo(ARM_CLOSE, channel);

						g_devinfo_update = 1;
					}
				}
			}
			else if(g_camera_arminfo_v1[channel].type == VAVA_ARMING_STATUS_ENABLE)
			{
				//全天布防
				g_camera_arminfo_v1[channel].status = VAVA_ARMING_STATUS_ENABLE;

				if(tmpstatus[channel] != g_camera_arminfo_v1[channel].status)
				{
					//布防
					ret = VAVAHAL_SetPirStatus(channel, g_pir_sensitivity[channel]);
					if(ret == VAVA_ERR_CODE_SUCCESS)
					{
						tmpstatus[channel] = g_camera_arminfo_v1[channel].status;
						print_arminfo(ARM_OPEN, channel);

						g_devinfo_update = 1;
					}
				}
			}
			else
			{
				armflag = 0;
				
				//定时布防 按条目检测
				for(armlist = 0; armlist < MAX_ARM_LIST_NUM; armlist++)
				{
					if(g_camera_arminfo_v1[channel].armdata[armlist].nstat == 0)
					{
						break;
					}

					//检查是否处于布防区间
					starttime = g_camera_arminfo_v1[channel].armdata[armlist].s_h * 3600
					            + g_camera_arminfo_v1[channel].armdata[armlist].s_m * 60
					            + g_camera_arminfo_v1[channel].armdata[armlist].s_s;
					endtime   =  g_camera_arminfo_v1[channel].armdata[armlist].e_h * 3600
					            + g_camera_arminfo_v1[channel].armdata[armlist].e_m * 60
					            + g_camera_arminfo_v1[channel].armdata[armlist].e_s;

					if(endtime >= starttime)
					{
						//检测星期
						if(g_camera_arminfo_v1[channel].armdata[armlist].weekday[currentweek] == 1)
						{
							if(currenttime > starttime && currenttime < endtime)
							{
								armflag = 1;
								break;
							}
						}
					}
					else
					{
						//存在跨天现象
						tmpweek = currentweek - 1;
						if(tmpweek < 0)
						{
							tmpweek = 6;
						}

						//检测星期
						if(g_camera_arminfo_v1[channel].armdata[armlist].weekday[currentweek] == 1)
						{
							if(currenttime > starttime && currenttime < 24 * 3600)
							{
								armflag = 1;
								break;
							}
						}
						else if(g_camera_arminfo_v1[channel].armdata[armlist].weekday[tmpweek] == 1)
						{
							if(currenttime > 0 && currenttime < endtime)
							{
								armflag = 1;
								break;
							}
						}
					}
				}

				if(armflag == 0) //处于撤防区间
				{
					g_camera_arminfo_v1[channel].status = VAVA_ARMING_STATUS_DISABLE;
				
					if(tmpstatus[channel] != g_camera_arminfo_v1[channel].status)
					{
						//撤防
						ret = VAVAHAL_SetPirStatus(channel, VAVA_PIR_SENSITIVITY_OFF);
						if(ret == VAVA_ERR_CODE_SUCCESS)
						{
							tmpstatus[channel] = g_camera_arminfo_v1[channel].status;
							print_arminfo(ARM_CLOSE, channel);

							g_devinfo_update = 1;
						}
					}
				}
				else  //处于布防区间
				{
					g_camera_arminfo_v1[channel].status = VAVA_ARMING_STATUS_ENABLE;

					if(tmpstatus[channel] != g_camera_arminfo_v1[channel].status)
					{
						//布防
						ret = VAVAHAL_SetPirStatus(channel, g_pir_sensitivity[channel]);
						if(ret == VAVA_ERR_CODE_SUCCESS)
						{
							tmpstatus[channel] = g_camera_arminfo_v1[channel].status;
							print_arminfo(ARM_OPEN, channel);

							g_devinfo_update = 1;
						}
					}
				}
			}
		}
		
		sleep(5);
	}
	
	//线程异常退出后由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}


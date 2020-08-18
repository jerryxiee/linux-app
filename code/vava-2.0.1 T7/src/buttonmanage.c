#include "basetype.h"
#include "errlog.h"
#include "vavahal.h"
#include "vavaserver.h"
#include "watchdog.h"
#include "buttonmanage.h"

//按键管理
int g_buttonfd = -1;				//句柄
int g_buttonkey = -1;				//键值
int g_buttiontime = 0;				//时长
int g_buttionflag = 0;				//标志位

volatile unsigned char g_apflag = 0;//配网模式

void signal_button(int signo)
{
	int flag;
	int val[2];
	static int buttonerr = 0; //按键失效检测
	
	if(g_buttonfd != -1)
	{
		read(g_buttonfd, val, sizeof(val));

		flag = 1;
		
		if(g_update.status == VAVA_UPDATE_IDLE)
		{
			flag = 0;
		}
		else
		{
			if(g_update.type == VAVA_UPDATE_TYPE_CAMERA && g_update.status == VAVA_UPDATE_UPGRADING)
			{
				flag = 0;
			}
		}
		
		if(flag == 1)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: g_buttonkey = %d, g_buttiontime = %d [Lock Key]\n", FUN, LINE, g_buttonkey, g_buttiontime);
		}
		else
		{
			g_buttonkey = val[1];
			g_buttiontime = val[0];
			g_buttionflag = 1;

			if(g_buttonkey < 0)
			{
				buttonerr++;
				if(buttonerr >= 5)
				{
					WatchDog_Stop(__FUNCTION__);
				}
			}
			else
			{
				buttonerr = 0;
			}

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: g_buttonkey = %d, g_buttiontime = %d, err = %d\n", FUN, LINE, g_buttonkey, g_buttiontime, buttonerr);
		}
	}

	return;
}

void *buttionmanage_pth(void *data)
{
	int flag;
	int fd;

	signal(SIGIO, signal_button);
	
	fd = open("/dev/my_drv", O_RDWR);
	if(fd < 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open /dev/my_drv err\n", FUN, LINE);

		Err_Log("open /dev/my_drv err");
		g_running = 0;
		return NULL;
	}

	fcntl(fd, F_SETOWN, getpid());
	flag = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flag|FASYNC);

	g_buttonfd = fd;

	while(g_running)
	{
		//按键处理
		if(g_buttionflag == 1 && g_buttonkey >= 0)
		{
			switch(g_buttonkey)
			{
				case VAVA_BUTTON_KEY_RESET:  //长按10秒恢复出厂设置
					if(g_addstation == 1)
					{
						break;
					}
					
					if(g_buttiontime >= 10)
					{
						//LED灯显示
						VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_FAST_FLASH);
						VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);

						VAVAHAL_PlayAudioFile("/tmp/sound/gateway_factory.opus");
						VAVAHAL_ResetGateWay();
						while(1)
						{
							sleep(1);
						}
					}
					break;
				case VAVA_BUTTON_KEY_SYNC:  //摄像机配对(无需要APP)
					if(g_addstation == 1)
					{
						break;
					}
					
					if(g_keyparing == 0 && g_pairmode == 0 && g_buttiontime >= 5)
					{
						g_keyparing = 1;
					}
					break;
				case VAVA_BUTION_DOUBLE:  //基站进入添加模式
					if(g_keyparing == 1)
					{
						break;
					}

					if(g_addstation == 0)
					{
						g_addstation = 1;
					}
					break;
				default:
					break;
			}

			g_buttionflag = 0;
		}

		sleep(1);
	}

	close(g_buttonfd);
	g_buttonfd = -1;

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}

//SYNC键配对
void *keysync_pth(void *data)
{
	int ret;
	int timeout;
	char sn[32];
	unsigned int addr;

	while(g_running)
	{
		if(g_keyparing == 0 || g_addstation == 1)
		{
			usleep(500000);
			continue;
		}

		//LED灯显示
		VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_FAST_FLASH);
		VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_SLAKE);

		timeout = 60;

		//查找是否可以配对
		while(g_running)
		{
			sleep(1);

			if(timeout % 10 == 0)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: @@ Key Pairing Search Camera @@ [timeout = %d]\n", FUN, LINE, timeout);
				
				memset(sn, 0, 32);
				ret = VAVAHAL_SearchList_FindOne(sn, &addr);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: @@ VAVAHAL_SearchList_FindOne ret = %d @@ [timeout = %d]\n", FUN, LINE, ret);
				if(ret == 0)
				{
					break;
				}
				else if(ret == -2)
				{
					//恢复灯状态
					VAVAHAL_ResetLedStatus();
					g_keyparing = 0;
					
					VAVAHAL_PlayAudioFile("/tmp/sound/need_app_pair.opus");
					break;
				}
			}

			if(timeout-- <= 0)
			{
				break;
			}
		}

		if(timeout <= 0)
		{
			//超时恢复灯状态
			VAVAHAL_ResetLedStatus();
			g_keyparing = 0;

			VAVAHAL_PlayAudioFile("/tmp/sound/sync_timeout.opus");
			continue;
		}

		if(g_keyparing == 0)
		{
			continue;
		}

		ret = VAVAHAL_PairCamera(NULL, -1, addr, sn, "", -1);
		if(ret != 0)
		{
			//恢复灯状态
			VAVAHAL_ResetLedStatus();
			g_keyparing = 0;

			if(ret == VAVA_ERR_CODE_PAIR_FAIL)
			{
				VAVAHAL_PlayAudioFile("/tmp/sound/sync_fail.opus");
			}
			else if(ret == VAVA_ERR_CODE_CHANNEL_FULL)
			{
				VAVAHAL_PlayAudioFile("/tmp/sound/channel_full.opus");
			}

			continue;
		}

		timeout = 60;
		while(g_running && g_keyparing)
		{
			sleep(1);

			if(timeout % 10 == 0)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: @@ Key Pairing Wait @@ [timeout = %d]\n", FUN, LINE, timeout);
			}

			if(timeout-- <= 0)
			{
				break;
			}
		}

		if(timeout <= 0)
		{
			//超时恢复灯状态
			VAVAHAL_ResetLedStatus();
			g_keyparing = 0;

			VAVAHAL_PlayAudioFile("/tmp/sound/sync_timeout.opus");
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}

#if 0
void *keysync_pth(void *data)
{
	int ret;
	int pairflag = 0;
	int timeout = 0;
	
	while(g_running)
	{
		if(g_keyparing == 0 || g_addstation == 1)
		{
			usleep(500000);
			continue;
		}

		//LED灯显示
		VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_FAST_FLASH);
		VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_SLAKE);

		timeout = 60;
		pairflag = 0;
			
		while(g_running)
		{
			if(g_keyparing == 0)
			{
				break;
			}

			if(pairflag == 1)
			{
				sleep(1);

				timeout--;
				if(timeout <= 0)
				{
					//超时恢复灯状态
					VAVAHAL_ResetLedStatus();
					g_keyparing = 0;

					VAVAHAL_PlayAudioFile("/tmp/sound/sync_timeout.opus");
					break;
				}

				if(timeout % 10 == 0)
				{
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: @@ PAIR WAIT @@ [timeout = %d]\n", FUN, LINE, timeout);
				}
				
				continue;
			}
			
			while(g_running)
			{
				sleep(1);

				timeout--;
				if(timeout <= 0)
				{
					//超时恢复灯状态
					VAVAHAL_ResetLedStatus();
					g_keyparing = 0;

					VAVAHAL_PlayAudioFile("/tmp/sound/sync_timeout.opus");
					break;
				}

				if(timeout % 10 == 0)
				{
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: @@ PAIR FOR NO APP @@ [timeout = %d]\n", FUN, LINE, timeout);

					ret = VAVAHAL_Pair_NoAPP();
					if(ret != 0)
					{
						if(ret == -2)
						{
							//恢复灯状态
							VAVAHAL_ResetLedStatus();
							g_keyparing = 0;
							
							VAVAHAL_PlayAudioFile("/tmp/sound/need_app_pair.opus");
						}
						
						break;
					}

					pairflag = 1;
					timeout = 30;
					break;
				}
			}
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: @@ PAIR FOR NO APP EXIT @@\n", FUN, LINE);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}
#endif

void *keyaddstation_pth(void *data)
{
	int ret;
	int timeout = 0;
	
	while(g_running)
	{
		if(g_keyparing == 1 || g_addstation == 0)
		{
			usleep(500000);
			continue;
		}

		timeout = 60;

		//与服务器验证
		ret = VAVASERVER_GetToken();
		if(ret != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get token fail\n", FUN, LINE);
			g_addstation = 0;
			continue;
		}

		ret = VAVASERVER_AddStationOn(&timeout);
		if(ret != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: add station on status fail\n", FUN, LINE);
			g_addstation = 0;
			continue;
		}

		//LED灯显示
		VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_FAST_FLASH);
		VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);

		while(g_running)
		{
			if(g_addstation == 0)
			{
				break;
			}

			timeout--;
			if(timeout <= 0)
			{
				g_addstation = 0;
				break;
			}

			sleep(1);
		}

		//恢复灯状态
		VAVAHAL_ResetLedStatus();

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: @@ KEY ADD STATION EXIT @@\n", FUN, LINE);
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

void *buzzer_manage_pth(void *data)
{
	g_buzzer_flag = 0;
	g_buzzer_type = VAVA_BUZZER_TYPE_OPEN;
	int times;
	int count;
	int flag;

	while(g_running)
	{
		if(g_buzzer_flag == 0)
		{
			sleep(1);
			continue;
		}

		//响3秒停2秒 每组5秒 4组共20秒
		times = 0; //次数
		count = 0; //计时
		flag = 0;  //开关  0 未测试 -> 1 开启 -> 2关闭
		
		while(g_running)
		{
			if(g_buzzer_flag == 0)
			{
				VAVAHAL_BuzzerCtrl(VAVA_BUZZER_TYPE_CLOSE);
				break;
			}

			if(flag == 0)
			{
				VAVAHAL_BuzzerCtrl(g_buzzer_type);
				flag = 1;
				count = 4;
			}
			else if(flag == 1)
			{
				if(count-- >= 0)
				{
					sleep(1);
					continue;
				}

				VAVAHAL_BuzzerCtrl(VAVA_BUZZER_TYPE_CLOSE);
				flag = 2;
				count = 1;
			}
			else if(flag == 2)
			{
				if(count-- >= 0)
				{
					sleep(1);
					continue;
				}

				flag = 0;
				count = 0;

				if(times++ >= 3)
				{
					break;
				}

				continue;
			}
			
			sleep(1);
		}

		g_buzzer_flag = 0;
		g_devinfo_update = 1;
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}


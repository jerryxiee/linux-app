#include "basetype.h"
#include "errlog.h"
#include "vavahal.h"
#include "vavaserver.h"
#include "watchdog.h"
#include "cloud_fsk.h"

struct mps_clds_dev_channel *clds_chl[MAX_CHANNEL_NUM];
volatile unsigned char g_cloudflag[MAX_CHANNEL_NUM];
volatile unsigned char g_cloudctrl[MAX_CHANNEL_NUM];
volatile unsigned char g_cloudsend[MAX_CHANNEL_NUM];
volatile unsigned char g_cloudlink[MAX_CHANNEL_NUM];
volatile unsigned char g_cloudrecheck[MAX_CHANNEL_NUM];
volatile unsigned char g_cloudupcheck[MAX_CHANNEL_NUM];
volatile unsigned char g_clouddebugflag;

volatile unsigned char g_clouddebuglever = 0;

long chl_on_event (struct mps_clds_dev_channel *chl, struct len_str *evt_type, struct len_str *data, void *ref)
{
    int channel = *(int *)ref;

    if(!evt_type || !evt_type->data)
    {
        return 0;
    }

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [chl_on_event][channel - %d]: [%s]\n", FUN, LINE, channel, evt_type->data);
	
    if(strcmp(evt_type->data, "pause") == 0)
    {
        g_cloudctrl[channel] = 1;
    }
    else if(strcmp(evt_type->data, "play") == 0)
    {
        g_cloudctrl[channel] = 0;
    }
	else if(strcmp(evt_type->data, "link") == 0)
	{
		g_cloudlink[channel] = 1;
	}
	else if(strcmp(evt_type->data, "connect.close") == 0)
	{
		g_cloudlink[channel] = 0;
		g_cloudflag[channel] = 0;
		g_cloudctrl[channel] = 0;
		g_cloudrecheck[channel] = 1;

		g_clouddebugflag = 1;

		#if 0
		if(clds_chl[channel] != NULL)
		{
			mps_clds_dev_channel_destroy(clds_chl[channel]);
			clds_chl[channel] = NULL;
		}
		#endif

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------\n", FUN, LINE);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------- CLOUD DISLINK CHANNEL - %d -------\n", FUN, LINE, channel);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------\n", FUN, LINE);
	}
	
    return 0;
}

void *cloud_fsk_pth(void *data)
{
	int ret;
	int channel;
	int savedebuglever = 2;
	int waitcount = 0;
	char *fsktoken[4];

	int lasttime[MAX_CHANNEL_NUM];
	int pausecheck[MAX_CHANNEL_NUM];
	int pausetime[MAX_CHANNEL_NUM];
#ifdef DEVELOPMENT_MODE
	int cloudupctime[MAX_CHANNEL_NUM];
#endif
	struct timeval t_current;

	struct mps_clds_dev_mod *clds_mod = NULL;
	struct mps_clds_dev_create_param mod_create_param;
	struct mps_clds_dev_channel_create_param chl_create_param;

	//延时启动 避免与DHCP服务器冲突
	while(g_running)
	{
		//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: wait Internet...\n", FUN, LINE);
		
		sleep(1);
		
		if(g_router_link_status == VAVA_NETWORK_LINKOK)
		{
			break;
		}
	}

	memset(&mod_create_param, 0, sizeof(struct mps_clds_dev_create_param));
	clds_mod = mps_clds_dev_mod_create(&mod_create_param);
	if(clds_mod == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: mps_clds_dev_mod_create fail\n", FUN, LINE);

		Err_Log("init cloud fail");
		g_running = 0;
		return NULL;
	}

	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		clds_chl[channel] = NULL;
		g_cloudflag[channel] = 0;
		g_cloudctrl[channel] = 0;
		g_cloudsend[channel] = 0;
		g_cloudlink[channel] = 0;

		lasttime[channel] = 0;
		pausecheck[channel] = 0;
		pausetime[channel] = 0;
#ifdef DEVELOPMENT_MODE
		cloudupctime[channel] = 0;
#endif

		fsktoken[channel] = NULL;
		fsktoken[channel] = malloc(1024);
		if(fsktoken[channel] == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc fsktoken fail\n", FUN, LINE);

			Err_Log("fsktoken malloc fail");
			g_running = 0;
			return NULL;
		}
	}

	mps_clds_dev_mod_debug_dump_set(clds_mod, savedebuglever);
	
	while(g_running)
	{
		sleep(1);

		//升级过程释放云存资源
		if(g_update.status != VAVA_UPDATE_IDLE)
		{
			sleep(1);
			
			for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
			{
				if(clds_chl[channel] != NULL)
				{
					mps_clds_dev_channel_destroy(clds_chl[channel]);
					clds_chl[channel] = NULL;

					g_cloudlink[channel] = 0;
					g_cloudflag[channel] = 0;
					g_cloudctrl[channel] = 0;
					g_cloudrecheck[channel] = 1;
				}
			}

			continue;
		}

		if(savedebuglever != g_clouddebuglever)
		{
			savedebuglever = g_clouddebuglever;
			mps_clds_dev_mod_debug_dump_set(clds_mod, savedebuglever);
		}

		if(waitcount++ < 10)
		{
			continue;
		}
		else
		{
			waitcount = 0;
		}

		gettimeofday(&t_current, NULL);

		for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
		{
			if(g_pair[channel].nstat == 0)
			{
				if(clds_chl[channel] != NULL)
				{
					mps_clds_dev_channel_destroy(clds_chl[channel]);
					clds_chl[channel] = NULL;
				}
				
				continue;
			}

			#ifdef DEVELOPMENT_MODE
			if(g_cloudupcheck[channel] == 1)
			{
				cloudupctime[channel] = t_current.tv_sec;
				g_cloudupcheck[channel] = 0;

				g_cloudlink[channel] = 0;
				g_cloudflag[channel] = 0;
				g_cloudctrl[channel] = 0;
				g_cloudrecheck[channel] = 1;
			}
			else if(t_current.tv_sec - cloudupctime[channel] >= 60) //检测云存更新标记(每分钟检测一次)
			{
				cloudupctime[channel] = t_current.tv_sec;
				g_cloudupcheck[channel] = 0;
				
				ret = VAVASERVER_GetCloudUpFlag(channel);
				if(ret == 0)
				{
					g_cloudlink[channel] = 0;
					g_cloudflag[channel] = 0;
					g_cloudctrl[channel] = 0;
					g_cloudrecheck[channel] = 1;
				}
			}
			#endif

			if(g_cloudflag[channel] == 1)
			{
				if(pausecheck[channel] == 0)
				{
					if(g_cloudctrl[channel] == 1)
					{
						pausecheck[channel] = 1;
						pausetime[channel] = t_current.tv_sec;
					}
				}
				else
				{
					if(g_cloudctrl[channel] == 0)
					{
						pausecheck[channel] = 0;
						pausetime[channel] = 0;	
					}
					else
					{
						if(t_current.tv_sec - pausetime[channel] >= 60)
						{
							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: more then 60s not get play event, channel - %d\n", FUN, LINE, channel);
							
							g_cloudctrl[channel] = 0;

							pausecheck[channel] = 0;
							pausetime[channel] = 0;	
						}
					}
				}

				continue;
			}

			if(t_current.tv_sec - lasttime[channel] >= 1600 || g_cloudrecheck[channel] == 1)
			{
				lasttime[channel] = t_current.tv_sec;
				g_cloudrecheck[channel] = 0;
				
				if(clds_chl[channel] != NULL)
				{
					//savedebuglever = 2;
					//g_clouddebuglever = 2;
					//mps_clds_dev_mod_debug_dump_set(clds_mod, savedebuglever);
					
					mps_clds_dev_channel_destroy(clds_chl[channel]);
					clds_chl[channel] = NULL;
					sleep(1);
				}

				//检测是否支持云服务
				ret = VAVASERVER_GetToken();
				if(ret != 0)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get token fail, channel - %d\n", FUN, LINE, channel);
					
					g_cloudrecheck[channel] = 1;

					break;
				}

				memset(fsktoken[channel], 0, 1024);
				ret = VAVASERVER_GetCloudStatus(channel, fsktoken[channel]);
				if(ret != 0)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get cloudstatus fail, channel - %d\n", FUN, LINE, channel);
					
					if(ret == 1)
					{
						//token超时
						g_cloudrecheck[channel] = 1;
					}

					//未开通云存储
					continue;
				}

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel - %d, token = [%s]\n", FUN, LINE, channel, fsktoken[channel]);

				//开启云服务
				memset(&chl_create_param, 0, sizeof(struct mps_clds_dev_channel_create_param));

				//云存储模块可以创建多个通道，每个通道需要一个令牌（token）
			    chl_create_param.token.data = fsktoken[channel];
			    chl_create_param.token.len = strlen(fsktoken[channel]);
			    chl_create_param.chl_on_event = chl_on_event;
			    chl_create_param.refer = (void *)&g_channel[channel];

				clds_chl[channel] = mps_clds_dev_channel_create(clds_mod, &chl_create_param);
			    if(NULL == clds_chl[channel])
			    {
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: mps_clds_dev_channel_create fail\n", FUN, LINE);
					
					g_cloudrecheck[channel] = 1;
					continue;
			    }

				g_cloudflag[channel] = 1;
				g_cloudctrl[channel] = 0;

				VAVAHAL_OpenCloudRec(0, channel);

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------- CLOUD SUPPORT CHANNEL - %d -------\n", FUN, LINE, channel);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------\n", FUN, LINE);
			}
		}
	}

	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		if(fsktoken[channel] != NULL)
		{
			free(fsktoken[channel]);
			fsktoken[channel] = NULL;
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}

void *cloud_fsk_debug(void *data)
{
	int count = 0;
	g_clouddebugflag = 0;
	
	while(g_running)
	{
		sleep(1);

		if(g_clouddebugflag == 1)
		{
			g_clouddebuglever = 6;
			g_clouddebugflag = 0;
		}

		if(g_clouddebuglever == 6)
		{
			count++;
			if(count >= 120)
			{
				count = 0;
				g_clouddebuglever = 2;
			}
		}
	}

	return NULL;
}


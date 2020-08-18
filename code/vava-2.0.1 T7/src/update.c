#include "basetype.h"
#include "crc32.h"
#include "errlog.h"
#include "vavahal.h"
#include "vavaserver.h"
#include "network.h"
#include "networkcheck.h"
#include "watchdog.h"
#include "update.h"

#define CAMERA_UPGRATE_SEND_SIZE		10000

static VAVA_OtaInfo g_otainfo;
static unsigned char g_uptimeout;		//超时标记

VAVA_OtaHead *g_otahead;
VAVA_FwHead *g_fw1; //升级类型为基站时有两个 升级类型为摄像机时只有一个
VAVA_FwHead *g_fw2;

void *update_pth(void *data)
{
	int i;
	int ret;
	int errflag;
	int breakflag;
	int errnum;
	int cmdfd;
	int avfd;

	char *path = NULL;
	char cmd[128];

	while(g_running)
	{
		if(g_update.status == VAVA_UPDATE_START)
		{
			break;
		}

		sleep(1);
		continue;
	}

#ifdef NAS_NFS
	//关闭NAS
	Nas_stop();
#endif

	//关闭录像
	VAVAHAL_CloseRec(1, 0);

	//关闭云存录像
	VAVAHAL_CloseCloudRec(1, 0);

	//停止录像
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		VAVAHAL_StopManualRec(i);
		VAVAHAL_StopFullTimeRec(i);
		VAVAHAL_StopAlarmRec(i);
		VAVAHAL_StopAlarmCloud(i);
	}
	
	sleep(3);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: update begin, type = %d\n", FUN, LINE, g_update.type);

	//清理缓存
	sleep(1);
	VAVAHAL_SystemCmd("echo 1 > /proc/sys/vm/drop_caches");
	VAVAHAL_SystemCmd("echo 2 > /proc/sys/vm/drop_caches");
	VAVAHAL_SystemCmd("echo 3 > /proc/sys/vm/drop_caches");

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===> [ Close Watch Dog ] <===\n", FUN, LINE, g_update.type);
	
	//关闭看门狗
	VAVAHAL_SystemCmd("vava-dogctrl -e 0");

	VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
	
	while(g_running)
	{
		ret = update_init();
		if(ret != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: update_init fail\n", FUN, LINE);
			
			Err_Log("update curl init fail");
			break;
		}

		g_update.status = VAVA_UPDATE_LOADING;
		ret = VAVASERVER_DownLoad();
		if(ret != 0)
		{
			g_update.status = VAVA_UPDATE_LOAD_ERR;
			VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, g_update.loading, g_update.current);
			
			Err_Log("update curl perform fail");
			break;
		}

		g_update.status = VAVA_UPDATE_LOAD_FINISH;
		VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, g_update.loading, g_update.current);

		update_finish();

		//检测升级头
		if(g_otahead->tag != VAVA_TAG_OTA)
		{
			g_update.status = VAVA_UPDATE_UPFILE_NOSUPPORT;
			VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
			break;
		}

		//检查文件大小
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: check ota file size: writesize = %d, otapacksize = %d\n", 
		                                FUN, LINE, g_otainfo.fw1wsize + g_otainfo.fw2wsize, g_otahead->totolsize);
		
		if(g_otainfo.fw1wsize + g_otainfo.fw2wsize != g_otahead->totolsize)
		{
			g_update.status = VAVA_UPDATE_CRCERR;
			VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
			break;
		}

		if(g_otahead->otatype == VAVA_UPDATE_TYPE_GATEWAY)
		{
			//停止internet检测
			internetcheck_stop();

			#if 0
			VAVAHAL_StopAvServer();
			VAVAHAL_FreeAvMem();
			
			//清理缓存
			sleep(1);
			VAVAHAL_SystemCmd("echo 1 > /proc/sys/vm/drop_caches");
			VAVAHAL_SystemCmd("echo 2 > /proc/sys/vm/drop_caches");
			VAVAHAL_SystemCmd("echo 3 > /proc/sys/vm/drop_caches");
			#endif

			//校验升级包
			ret = update_checkpack(1);
			if(ret != 0)
			{
				g_update.status = VAVA_UPDATE_CRCERR;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
				break;
			}

			if(g_otahead->upnum == 2)
			{
				ret = update_checkpack(2);
				if(ret != 0)
				{
					g_update.status = VAVA_UPDATE_CRCERR;
					VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
					break;
				}
			}

			//移除TF卡
			g_sdpoping = 1;
			g_update_sd_nochek = 1; 
			VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");

			//通知APP基站升级中
			g_update.status = VAVA_UPDATE_UPGRADING;
			VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);

			g_uptimeout = 1;
			VAVAHAL_CreateUpgradingTimeout(g_otahead->upnum);

			//升级短波
			if(g_otainfo.rfflag == 1)
			{
				ret = VAVAHAL_UpgradeRf();
				if(ret != 0)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: update rf fail, ret = %d\n", FUN, LINE, ret);
					
					Err_Log("update rf fail");

					g_uptimeout = 0;
					sleep(2);

					g_update.status = VAVA_UPDATE_FAIL;
					VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 100, g_update.current);
					g_update_sd_nochek = 0; 
					break;
				}

				if(g_otahead->upnum == 1)
				{
					g_uptimeout = 0;
					sleep(2);
					
					g_update.status = VAVA_UPDATE_SUCCESS;
					VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 100, g_update.current);
				}
			}

			//升级基站
			if(g_otainfo.bsflag == 1)
			{
				if(g_fw1->fwtype == VAVA_UPDATE_TYPE_GATEWAY)
				{
					path = UPDATE_FILE1;
				}
				else if(g_fw2->fwtype == VAVA_UPDATE_TYPE_GATEWAY)
				{
					path = UPDATE_FILE2;
				}
				else
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: type check err\n", FUN, LINE);
				}

				if(path != NULL)
				{
					memset(cmd, 0, 128);
					sprintf(cmd, "/sbin/sysupgrade -n %s", path);
					ret = VAVAHAL_SystemCmd(cmd);
					if(ret == 0)
					{
						g_uptimeout = 0;
						sleep(2);
						
						g_update.status = VAVA_UPDATE_SUCCESS;
						VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 100, g_update.current);
					}
					else
					{
						g_uptimeout = 0;
						sleep(2);
						
						g_update.status = VAVA_UPDATE_FAIL;
						VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 100, g_update.current);
					}
				}
				else
				{
					ret = -1;
				}
			}

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =====================================================\n", FUN, LINE);
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:             Gateway Update Complate, ret = %d        \n", FUN, LINE, ret);
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =====================================================\n", FUN, LINE);

			sleep(5);
			VAVAHAL_SystemCmd("reboot");

			while(1)
			{
				sleep(5);
				VAVAHAL_SystemCmd("/bin/rm -f /tmp/update/*");
				sleep(5);
				VAVAHAL_SystemCmd("reboot");
			}

			return NULL;
		}
		else
		{
			//直接发送给摄像机
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: camera upgare file size = %d\n", FUN, LINE, g_otahead->totolsize);

			if(g_pair[g_update.current].nstat == 0)
			{
				g_update.status = VAVA_UPDATE_NOPAIR;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
				break;
			}

			if(g_camerainfo_dynamic[g_update.current].battery <= CAMERA_UPDATE_LOW_POWER)
			//	|| g_camerainfo_dynamic[g_update.current].voltage <= 7270)
			{
				g_update.status = VAVA_UPDATE_POWERLOW;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
				break;
			}

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: camera %d upgrate\n", FUN, LINE, g_update.current);

			//唤醒摄像机
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [WAKE-UP][update-camera] channel - %d\n", FUN, LINE, g_update.current);

			errnum = 0;
			while(g_running)
			{
				ret = VAVAHAL_WakeupCamera_Ex(g_update.current, WAKE_UP_MODE_APP);
				if(ret != 0)
				{
					if(errnum++ >= 2)
					{
						break;
					}
				}
				else
				{
					break;
				}
				
				sleep(2);
				continue;
			}

			if(errnum >= 2)
			{
				g_update.status = VAVA_UPDATE_REQ_FAIL;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
				break;
			}

			g_camerainfo[g_update.current].up_flag = 1;

			errflag = 0;
			cmdfd = -1;
			avfd = -1;
			
			while(g_running)
			{
				cmdfd = VAVAHAL_ReadSocketId(0, g_update.current);
				avfd = VAVAHAL_ReadSocketId(1, g_update.current);
				if(cmdfd < 0 || avfd < 0)
				{
					errflag++;
					if(errflag >= 10)
					{
						break;
					}
					
					sleep(1);
					continue;
				}
				
				break;
			}

			if(errflag >= 10)
			{
				g_camerainfo[g_update.current].up_flag = 0;
				VAVAHAL_SleepCamera_Ex(g_update.current);

				//升级超时(唤醒摄像机失败)
				g_update.status = VAVA_UPDATE_FAIL;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
				break;
			}
			
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmdfd = %d, avfd == %d\n", FUN, LINE, cmdfd, avfd);

			errflag = 0;
			breakflag = 0;

			while(g_running)
			{
				if(errflag++ >= 3)
				{
					//超时
					g_update.status = VAVA_UPDATE_RESP_TIMEOUT;
					VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get upgrate resp timeout, channel = %d\n", FUN, LINE, g_update.current);
					
					memset(cmd, 0, 128);
					sprintf(cmd, "get camera %d upgrate resp fail", g_update.current);
					Err_Log(cmd);

					g_camerainfo[g_update.current].up_flag = 0;
					VAVAHAL_SleepCamera_Ex(g_update.current);
					
					break;
				}
				
				g_update.wait = 6; //3秒超时

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: send upgrate req to camera, errflag = %d\n", FUN, LINE, errflag);

				//发送升级请求
				ret = VAVAHAL_UpdateCamera(cmdfd, VAVA_CONFIG_UPDATE_REQ, g_otahead->upnum, g_otahead->totolsize);
				if(ret != 0)
				{
					g_update.wait = 0;
					g_update.status = VAVA_UPDATE_REQ_FAIL;
					VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: send upgrate req fail, channel = %d\n", FUN, LINE, g_update.current);
					
					memset(cmd, 0, 128);
					sprintf(cmd, "send camera %d upgrate req fail", g_update.current);
					Err_Log(cmd);

					g_camerainfo[g_update.current].up_flag = 0;
					VAVAHAL_SleepCamera_Ex(g_update.current);

					breakflag = 1;
					break;
				}

				//等待摄像机回复升级确认
				ret = update_wait(&g_update.wait);
				if(ret != 0)
				{
					continue;
				}
				else
				{
					break;
				}
			}

			if(breakflag == 1)
			{
				breakflag = 0;
				break;
			}

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: send upgrate data to camera\n", FUN, LINE);

			ret = update_sendtocamera(cmdfd, avfd, g_otahead->totolsize);
			if(ret != 0)
			{
				g_camerainfo[g_update.current].up_flag = 0;
				VAVAHAL_SleepCamera_Ex(g_update.current);
				break;
			}

			g_update.wait = 20; // 10 秒超时

			//通知摄像机传输完成
			VAVAHAL_UpdateCamera(cmdfd, VAVA_CONFIG_UPDATE_TRANSEND, 0, 0);

			//等待摄像机校验结果
			ret = update_wait(&g_update.wait);
			if(ret != 0)
			{
				g_update.status = VAVA_UPDATE_CHECK_TIMEOUT;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get upgrate check result timeout, channel = %d\n", FUN, LINE, g_update.current);
				
				memset(cmd, 0, 128);
				sprintf(cmd, "get camera %d upgrate check timeout", g_update.current);
				Err_Log(cmd);

				g_camerainfo[g_update.current].up_flag = 0;
				VAVAHAL_SleepCamera_Ex(g_update.current);
				break;
			}

			//校验失败
			if(g_update.result == VAVA_CAMERA_UPDATE_CRCFAIL)
			{
				g_update.status = VAVA_UPDATE_CHECK_FAIL;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: upgrate check fail, channel = %d\n", FUN, LINE, g_update.current);
				
				memset(cmd, 0, 128);
				sprintf(cmd, "camera %d upgrate check fail", g_update.current);
				Err_Log(cmd);

				g_camerainfo[g_update.current].up_flag = 0;
				VAVAHAL_SleepCamera_Ex(g_update.current);
				break;
			}

			//通知APP摄像机升级中
			g_update.status = VAVA_UPDATE_UPGRADING;
			VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);

			g_uptimeout = 1;
			VAVAHAL_CreateUpgradingTimeout(g_otahead->upnum);

			g_update.result = -1;

			if(g_otahead->upnum == 1)
			{
				g_update.wait = 150; //1分15秒升级超时
			}
			else
			{
				g_update.wait = 300; //2分30秒升级超时
			}

			//等待摄像机返回升级结果
			ret = update_wait(&g_update.wait);
			if(ret != 0)
			{
				g_uptimeout = 0;
				sleep(2);
				
				//通知摄像机退出升级模式
				VAVAHAL_UpdateCamera(cmdfd, VAVA_CONFIG_UPDATE_TRANSFAIL, 0, 0);
				
				//通知APP升级超时
				g_update.status = VAVA_UPDATE_TIMEOUT;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 100, g_update.current);

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========================================\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:            Camera Update TimeOut        \n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========================================\n", FUN, LINE);

				g_camerainfo[g_update.current].up_flag = 0;
				VAVAHAL_SleepCamera_Ex(g_update.current);
				break;
			}

			g_uptimeout = 0;
			sleep(2);
				
			if(g_update.result == VAVA_CAMERA_UPDATE_SUCCESS)
			{
				//通知APP升级成功
				g_camerainfo[g_update.current].up_flag = 0;
				
				g_update.status = VAVA_UPDATE_SUCCESS;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 100, g_update.current);

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========================================\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:            Camera Update Success        \n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========================================\n", FUN, LINE);
			}
			else
			{
				//通知APP升级失败
				g_camerainfo[g_update.current].up_flag = 0;
				g_update.status = VAVA_UPDATE_FAIL;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 100, g_update.current);

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========================================\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:             Camera Update Fail          \n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========================================\n", FUN, LINE);
			}

			g_camerainfo[g_update.current].first_flag = 1;

			//由摄像机自动重启 以更新版本信息
			//VAVAHAL_SleepCamera_Ex(g_update.current);
		}

		break;
	}

	update_deinit();
	
	//恢复参数
	g_update.status = VAVA_UPDATE_IDLE;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Open Watch Dog\n", FUN, LINE);

	//开启看门狗
	VAVAHAL_SystemCmd("vava-dogctrl -e 1");

	//开启网络检测
	internetcheck_start();

	//开启录像
	VAVAHAL_OpenRec(1, 0);

	//开启云存录像
	VAVAHAL_OpenCloudRec(1, 0);

#ifdef NAS_NFS
	//重新加载NAS配置
	VAVAHAL_ReadNasConfig();
#endif

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Exit upgrate pth\n", FUN, LINE);
	
	return NULL;
}

void *UpgringTimeout_pth(void *data)
{
	int upnum = *(int *)data;
	int timeout = 150;
	int current = 0;
	int save = 0;
	int loading = 0;

	if(upnum == 1)
	{
		timeout = 75;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: upnum = %d, timeout = %d\n", FUN, LINE, upnum, timeout);

	while(g_running && g_uptimeout)
	{
		sleep(1);
		
		current++;
		if(current >= timeout)
		{
			break;
		}

		loading = (int)((float)current / (float)timeout * 100);
		if(loading - save >= 3)
		{
			if(loading <= 75)
			{
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, loading + 15, g_update.current);
			}
			else if(loading > 75 && loading <= 90)
			{
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 90, g_update.current);
			}
			else
			{
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, loading, g_update.current);
			}
			
			save = loading;
		}
	}

	return NULL;
}

int update_init()
{
	g_otainfo.init_flag = 0;
	
	g_otainfo.headsize = 0;
	g_otainfo.fw1infosize = 0;
	g_otainfo.fw2infosize = 0;
	
	g_otainfo.fw1wsize = 0;
	g_otainfo.fw2wsize = 0;

	g_otainfo.rfflag = 0;
	g_otainfo.bsflag = 0;
	
	memset(g_otainfo.headbuff, 0, OTA_HEAD_SIZE);
	memset(g_otainfo.fwinfo_1, 0, OTA_FWINFO_SIZE);
	memset(g_otainfo.fwinfo_2, 0, OTA_FWINFO_SIZE);

	VAVAHAL_SystemCmd("/bin/mkdir -p /tmp/update");

	g_otainfo.upfd1 = fopen(UPDATE_FILE1, "wb");
	if(g_otainfo.upfd1 == NULL)
	{
		return -1;
	}

	g_otainfo.upfd2 = fopen(UPDATE_FILE2, "wb");
	if(g_otainfo.upfd2 == NULL)
	{
		fclose(g_otainfo.upfd1);
		return -1;
	}

	g_otainfo.upmem = malloc(OTA_UPMEM_SIZE);
	if(g_otainfo.upmem == NULL)
	{
		fclose(g_otainfo.upfd1);
		fclose(g_otainfo.upfd2);
		
		return -1;
	}

	g_otainfo.memsize = 0;

	g_otainfo.init_flag = 1;
	
	return 0;
}

int update_deinit()
{
	g_otainfo.init_flag = 0;
	
	g_otainfo.headsize = 0;
	g_otainfo.fw1infosize = 0;
	g_otainfo.fw2infosize = 0;
	
	g_otainfo.fw1wsize = 0;
	g_otainfo.fw2wsize = 0;

	g_otainfo.rfflag = 0;
	g_otainfo.bsflag = 0;
	
	memset(g_otainfo.headbuff, 0, OTA_HEAD_SIZE);
	memset(g_otainfo.fwinfo_1, 0, OTA_FWINFO_SIZE);
	memset(g_otainfo.fwinfo_2, 0, OTA_FWINFO_SIZE);

	VAVAHAL_SystemCmd("/bin/rm -f /tmp/update/img1.bin");
	VAVAHAL_SystemCmd("/bin/rm -f /tmp/update/img2.bin");

	VAVAHAL_SystemCmd("/bin/rm -rf /tmp/update");
	VAVAHAL_SystemCmd("sync");

	if(g_otainfo.upfd1 != NULL)
	{
		fclose(g_otainfo.upfd1);
		g_otainfo.upfd1 = NULL;
	}

	if(g_otainfo.upfd2 != NULL)
	{
		fclose(g_otainfo.upfd2);
		g_otainfo.upfd2 = NULL;
	}

	if(g_otainfo.upmem != NULL)
	{
		free(g_otainfo.upmem);
		g_otainfo.upmem = NULL;
	}

	g_otainfo.memsize = 0;
	
	return 0;
}

int update_write(char *ptr, int size)
{
	int ret;
	int inputsize = size;
	int cpsize;
	int offset = 0;

	if(g_otainfo.init_flag == 0)
	{
		return -1;
	}
		
	while(inputsize > 0)
	{
		cpsize = OTA_UPMEM_SIZE - g_otainfo.memsize;
		if(inputsize >= cpsize)
		{
			memcpy(g_otainfo.upmem + g_otainfo.memsize, ptr, cpsize);
			g_otainfo.memsize += cpsize;
			inputsize -= cpsize;
		}
		else
		{
			memcpy(g_otainfo.upmem + g_otainfo.memsize, ptr, inputsize);
			g_otainfo.memsize += inputsize;
			inputsize = 0;
		}

		//处理数据
		offset = 0;
		while(g_otainfo.memsize > 0)
		{
			//检测OTA头
			if(g_otainfo.headsize < OTA_HEAD_SIZE)
			{
				cpsize = OTA_HEAD_SIZE - g_otainfo.headsize;
				if(g_otainfo.memsize >= cpsize)
				{
					memcpy(g_otainfo.headbuff + g_otainfo.headsize, g_otainfo.upmem + offset, cpsize);
					g_otainfo.headsize += cpsize;

					offset += cpsize;
					g_otainfo.memsize -= cpsize;

					g_otahead = (VAVA_OtaHead *)g_otainfo.headbuff;
				}
				else
				{
					cpsize = g_otainfo.memsize;
					memcpy(g_otainfo.headbuff + g_otainfo.headsize, g_otainfo.upmem + offset, cpsize);
					g_otainfo.headsize += cpsize;
					g_otainfo.memsize = 0;
				}

				continue;
			}

			if(g_otahead->otatype == VAVA_UPDATE_TYPE_GATEWAY)
			{
				//检测固件头1
				if(g_otainfo.fw1infosize < OTA_FWINFO_SIZE)
				{
					cpsize = OTA_FWINFO_SIZE - g_otainfo.fw1infosize;
					if(g_otainfo.memsize >= cpsize)
					{
						memcpy(g_otainfo.fwinfo_1 + g_otainfo.fw1infosize, g_otainfo.upmem + offset, cpsize);
						g_otainfo.fw1infosize += cpsize;

						offset += cpsize;
						g_otainfo.memsize -= cpsize;

						g_fw1 = (VAVA_FwHead *)g_otainfo.fwinfo_1;

						if(g_fw1->fwtype == VAVA_UPDATE_TYPE_RF)
						{
							g_otainfo.rfflag = 1;
						}
						else if(g_fw1->fwtype == VAVA_UPDATE_TYPE_GATEWAY)
						{
							g_otainfo.bsflag = 1;
						}

						g_otainfo.fw1wsize += sizeof(VAVA_FwHead);
					}
					else
					{
						cpsize = g_otainfo.memsize;
						memcpy(g_otainfo.fwinfo_1 + g_otainfo.fw1infosize, g_otainfo.upmem + offset, cpsize);
						g_otainfo.fw1infosize += cpsize;
						g_otainfo.memsize = 0;
					}
					
					continue;
				}

				//写入固件1
				if(g_otainfo.fw1wsize < g_fw1->filesize + sizeof(VAVA_FwHead))
				{
					cpsize = g_fw1->filesize + sizeof(VAVA_FwHead) - g_otainfo.fw1wsize;
					if(g_otainfo.memsize >= cpsize)
					{
						ret = fwrite(g_otainfo.upmem + offset, cpsize, 1, g_otainfo.upfd1);
						if(ret <= 0)
						{
							return -1;
						}
						
						g_otainfo.fw1wsize += cpsize;

						offset += cpsize;
						g_otainfo.memsize -= cpsize;
					}
					else
					{
						cpsize = g_otainfo.memsize;
						ret = fwrite(g_otainfo.upmem + offset, cpsize, 1, g_otainfo.upfd1);
						if(ret <= 0)
						{
							return -1;
						}

						g_otainfo.fw1wsize += cpsize;
						g_otainfo.memsize = 0;
					}
					
					continue;
				}

				//检测固件头2
				if(g_otainfo.fw2infosize < OTA_FWINFO_SIZE)
				{
					cpsize = OTA_FWINFO_SIZE - g_otainfo.fw2infosize;
					if(g_otainfo.memsize >= cpsize)
					{
						memcpy(g_otainfo.fwinfo_2 + g_otainfo.fw2infosize, g_otainfo.upmem + offset, cpsize);
						g_otainfo.fw2infosize += cpsize;

						offset += cpsize;
						g_otainfo.memsize -= cpsize;

						g_fw2 = (VAVA_FwHead *)g_otainfo.fwinfo_2;

						if(g_fw2->fwtype == VAVA_UPDATE_TYPE_RF)
						{
							g_otainfo.rfflag = 1;
						}
						else if(g_fw2->fwtype == VAVA_UPDATE_TYPE_GATEWAY)
						{
							g_otainfo.bsflag = 1;
						}

						g_otainfo.fw2wsize += sizeof(VAVA_FwHead);
					}
					else
					{
						cpsize = g_otainfo.memsize;
						memcpy(g_otainfo.fwinfo_2 + g_otainfo.fw2infosize, g_otainfo.upmem + offset, cpsize);
						g_otainfo.fw2infosize += cpsize;
						g_otainfo.memsize = 0;
					}

					continue;
				}

				//写入固件2
				if(g_otainfo.fw2wsize < g_fw2->filesize + sizeof(VAVA_FwHead))
				{
					cpsize = g_fw2->filesize + sizeof(VAVA_FwHead) - g_otainfo.fw2wsize;
					if(g_otainfo.memsize >= cpsize)
					{
						ret = fwrite(g_otainfo.upmem + offset, cpsize, 1, g_otainfo.upfd2);
						if(ret <= 0)
						{
							return -1;
						}
						
						g_otainfo.fw2wsize += cpsize;

						offset += cpsize;
						g_otainfo.memsize -= cpsize;
					}
					else
					{
						cpsize = g_otainfo.memsize;
						ret = fwrite(g_otainfo.upmem + offset, cpsize, 1, g_otainfo.upfd2);
						if(ret <= 0)
						{
							return -1;
						}

						g_otainfo.fw2wsize += cpsize;
						g_otainfo.memsize = 0;
					}

					continue;
				}
			}
			else if(g_otahead->otatype == VAVA_UPDATE_TYPE_CAMERA)
			{
				if(g_otainfo.fw1wsize < g_otahead->totolsize)
				{
					cpsize = g_otahead->totolsize - g_otainfo.fw1wsize;
					if(g_otainfo.memsize >= cpsize)
					{
						ret = fwrite(g_otainfo.upmem + offset, cpsize, 1, g_otainfo.upfd1);
						if(ret <= 0)
						{
							return -1;
						}

						g_otainfo.fw1wsize += cpsize;

						offset += cpsize;
						g_otainfo.memsize -= cpsize;
					}
					else
					{
						cpsize = g_otainfo.memsize;
						ret = fwrite(g_otainfo.upmem + offset, cpsize, 1, g_otainfo.upfd1);
						if(ret <= 0)
						{
							return -1;
						}

						g_otainfo.fw1wsize += cpsize;
						g_otainfo.memsize = 0;
					}
				}
			}
		}
	}

	return 0;
}

int update_finish()
{
	if(g_otainfo.upfd1 != NULL)
	{
		fclose(g_otainfo.upfd1);
		g_otainfo.upfd1 = NULL;
	}

	if(g_otainfo.upfd2 != NULL)
	{
		fclose(g_otainfo.upfd2);
		g_otainfo.upfd2 = NULL;
	}

	return 0;
}

int update_checkpack(int fwnum)
{
	FILE *fd = NULL;
	char *path = NULL;
	char *readbuff = NULL;

	int ret;
	int writesize;
	unsigned int crc32;

	VAVA_FwHead *fwhead = NULL;

	if(fwnum == 1)
	{
		fwhead = (VAVA_FwHead *)g_otainfo.fwinfo_1;
		path = UPDATE_FILE1;
	}
	else if(fwnum == 2)
	{
		fwhead = (VAVA_FwHead *)g_otainfo.fwinfo_2;
		path = UPDATE_FILE2;
	}

	fd = fopen(path, "rb");
	if(fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open update file fail, fwnum = %d\n", FUN, LINE, fwnum);
		return -1;
	}

	readbuff = malloc(CAMERA_UPGRATE_SEND_SIZE);
	if(readbuff == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc readbuff fail\n", FUN, LINE);
		
		fclose(fd);
		return -1;
	}

	writesize = fwhead->filesize;
	CRC32_OTA_Init();

	while(g_running)
	{
		if(writesize <= 0)
		{
			break;
		}
		
		memset(readbuff, 0, CAMERA_UPGRATE_SEND_SIZE);

		if(writesize >= CAMERA_UPGRATE_SEND_SIZE)
		{
			ret = fread(readbuff, CAMERA_UPGRATE_SEND_SIZE, 1, fd);
			if(ret <= 0)
			{
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read fwdata fail, fwnum = %d\n", FUN, LINE, fwnum);
				
				free(readbuff);
				fclose(fd);
				return -1;
			}

			CRC32_OTA_Calculate((unsigned char *)readbuff, CAMERA_UPGRATE_SEND_SIZE);
			writesize -= CAMERA_UPGRATE_SEND_SIZE;
		}
		else
		{
			ret = fread(readbuff, writesize, 1, fd);
			if(ret <= 0)
			{
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read fwdata fail, fwnum = %d\n", FUN, LINE, fwnum);
				
				free(readbuff);
				fclose(fd);
				return -1;
			}

			CRC32_OTA_Calculate((unsigned char *)readbuff, writesize);
			writesize = 0; 
		}
	}

	//包校验
	crc32 = CRC32_OTA_GetResult();
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: fwnum = %d type = %d, packcrc = %x, buildcrc = %x\n", 
	                                FUN, LINE, fwnum, fwhead->fwtype, fwhead->crc32, crc32);
	
	if(crc32 != (unsigned int)fwhead->crc32)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: crc check fail, fwnum = %d\n", FUN, LINE, fwnum);
		
		free(readbuff);
		fclose(fd);
		return -1;
	}

	free(readbuff);
	fclose(fd);

	return 0;
}

int update_sendtocamera(int cmdfd, int avfd, int filesize)
{
	FILE *fd = NULL;
	char *sendbuff = NULL;

	int ret;
	int readsize;
	int sendsize;
	int readcount;
	int errcount;
	int sendtimeout;

	int tmpavfd;

	fd_set rdWd;
	struct timeval timeout;

	struct timeval t_start;
	struct timeval t_current;

	VAVA_Avhead *uphead;

	tmpavfd = VAVAHAL_ReadSocketId(1, g_update.current);

	fd = fopen(UPDATE_FILE1, "r");
	if(fd == NULL)
	{
		g_update.status = VAVA_UPDATE_FILE_OPEN_FAIL;
		VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open camera update file fail\n", FUN, LINE);
		
		Err_Log("camera upgratefile open fail");
		return -1;
	}

	sendbuff = malloc(CAMERA_UPGRATE_SEND_SIZE + sizeof(VAVA_Avhead));
	if(sendbuff == NULL)
	{
		fclose(fd);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc upgrate buff fail\n", FUN, LINE);
		
		Err_Log("malloc upgrate buff fail");
		g_running = 0;
		return -1;
	}

	//开始发送升级数据
	readsize = 0;
	sendsize = 0;
	readcount = 0;
	errcount = 0;

	//通知APP开始发送数据到摄像机
	g_update.status = VAVA_UPDATE_TRANSMITTING;
	g_update.loading = 0;
	VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, g_update.loading, g_update.current);

	gettimeofday(&t_start, NULL);
	sendtimeout = 0;

	while(g_running)
	{
		tmpavfd = VAVAHAL_ReadSocketId(1, g_update.current);
		if(tmpavfd == -1)
		{
			errcount++;
			if(errcount >= 3)
			{
				break;
			}
			
			sleep(1);
			continue;
		}
		
		memset(sendbuff, 0, CAMERA_UPGRATE_SEND_SIZE + sizeof(VAVA_Avhead));
		readsize = fread(sendbuff + sizeof(VAVA_Avhead), 1, CAMERA_UPGRATE_SEND_SIZE, fd);
		if(readsize <= 0)
		{
			break;
		}

		uphead = (VAVA_Avhead *)sendbuff;
		uphead->tag = VAVA_TAG_CAMERA_AV;
		uphead->streamtype = VAVA_STREAM_TYPE_UPGRATE;
		uphead->encodetype = 0;
		uphead->frametype = 0;
		uphead->res = filesize;
		uphead->size = readsize;
		uphead->ntsamp = 0;

		sendsize += readsize;

		gettimeofday(&t_current, NULL);
		if(t_current.tv_sec - t_start.tv_sec > 120)
		{
			break;
		}

		FD_ZERO(&rdWd);
		FD_SET(tmpavfd, &rdWd);

		timeout.tv_sec = 0;				
		timeout.tv_usec = 500000;

		ret = select(tmpavfd + 1, NULL, &rdWd, NULL, &timeout);
		if(ret < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err, ret = %d\n", FUN, LINE, ret);
			break;
		}
		else if(ret == 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select timeout\n", FUN, LINE);

			VAVAHAL_WriteSocketId(1, g_update.current, -1);
			sleep(1);

			//tmpavfd = VAVAHAL_ReadSocketId(1, g_update.current);
			sendtimeout++;
			if(sendtimeout >= 10)
			{
				break;
			}
			
			continue;
		}
		else
		{
			if(FD_ISSET(tmpavfd, &rdWd))
			{
				sendtimeout = 0;
				
				ret = send(tmpavfd, sendbuff, sizeof(VAVA_Avhead) + readsize, 0);
				if(ret <= 0)
				{
					VAVAHAL_WriteSocketId(1, g_update.current, -1);
					break;
				}

				if(ret != sizeof(VAVA_Avhead) + readsize)
				{
					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: send upgrate data fail, ret = %d, sendsize = %d\n", 
					                              FUN, LINE, ret, sizeof(VAVA_Avhead) + readsize);
					break;
				}
				
				g_update.loading = (int)((float)sendsize / filesize * 100);
				if(readcount++ >= 50)
				{
					//通知APP发送进度
					VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, g_update.loading, g_update.current);
					readcount = 0;
				}
			}
		}
	}
	 
	fclose(fd);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Send last flag package\n", FUN, LINE);

	//增加尾包
	memset(sendbuff, 0, CAMERA_UPGRATE_SEND_SIZE + sizeof(VAVA_Avhead));
	uphead = (VAVA_Avhead *)sendbuff;
	uphead->tag = VAVA_TAG_CAMERA_AV;
	uphead->streamtype = VAVA_STREAM_TYPE_UPGRATE;
	uphead->encodetype = 0;
	uphead->frametype = 0;
	uphead->res = filesize;
	uphead->size = 0;
	uphead->ntsamp = 0;
	send(tmpavfd, sendbuff, sizeof(VAVA_Avhead), 0);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: camera upgrate data send end, sendsize = %d, filesize = %d\n", 
	                                FUN, LINE, sendsize, filesize);

	if(sendsize != filesize) //升级数据大小不一致
	{
		//通知摄像机退出升级模式
		VAVAHAL_UpdateCamera(cmdfd, VAVA_CONFIG_UPDATE_TRANSFAIL, 0, 0);

		//通知APP摄像机传输失败
		g_update.status = VAVA_UPDATE_TRANS_FIAL;
		VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, g_update.loading, g_update.current);

		free(sendbuff);
		sendbuff = NULL;

		return -1;
	}

	free(sendbuff);
	sendbuff = NULL;

	return 0;
}

int update_wait(int *wait)
{	
	while(g_running)
	{
		if(g_running == 0)
		{
			break;
		}

		*wait -= 1;

		if(*wait == 0)
		{
			break;
		}

		if(*wait < 0)
		{
			return 0;
		}

		usleep(500000);
	}

	return -1;
}

int update_sendto_tool(int sockfd, struct sockaddr_in cliaddr, int cmd, int val)
{
	VAVA_ToolUpdataHead tuhead;

	memset(&tuhead, 0, sizeof(VAVA_ToolUpdataHead));
	tuhead.tag = 0xEB0000CC;
	tuhead.cmd = cmd;
	tuhead.crc = val;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmd = %d, val = %d\n", FUN, LINE, cmd, val);

	sendto(sockfd, &tuhead, sizeof(VAVA_ToolUpdataHead), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));

	return 0;
}


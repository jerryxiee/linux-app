#include "basetype.h"
#include "errlog.h"
#include "PPCS_API.h"
#include "avserver.h"
#include "quelist.h"
#include "crc32.h"
#include "update.h"
#include "record.h"
#include "eeprom.h"
#include "camerapair.h"
#include "rfserver.h"
#include "sdcheck.h"
#include "vavahal.h"
#include "network.h"
#include "cc1310upgrade.h"
#include "watchdog.h"
#include "vavaserver.h"

#define VAVA_SEARCH_BUFF_SIZE					28000

int g_commonfd[MAX_CHANNEL_NUM];
int g_avfd[MAX_CHANNEL_NUM];
char *g_searchbuff = NULL;	//录像搜索

static int g_AFplayflag = 0;

void VAVAHAL_Print(int level, const char *format, ...)
{
	va_list ap;
	char typestr[10];
	char buf[LOG_BUF_SIZE];

	if(level <= g_debuglevel)
	{
		memset(typestr, 0, 10);
		switch(level)
		{
			case LOG_LEVEL_ERR:
				strcpy(typestr, "   [ERR] ");
				break;
			case LOG_LEVEL_WARING:
				strcpy(typestr, "[WARING] ");
				break;
			case LOG_LEVEL_DEBUG:
				strcpy(typestr, " [DEBUG] ");
				break;
			default:
				break;
		}
		
		va_start(ap, format);
		vsnprintf(buf, LOG_BUF_SIZE, format, ap);
		va_end(ap);

		printf("%s%s", typestr, buf);
	}
}

void VAVAHAL_Print_NewLine(int level)
{
	if(level <= g_debuglevel)
	{
		printf("\n");
	}
}

int VAVAHAL_ReadGateWayInfo()
{
	int ret;
	
	//读取P2P服务器信息
	VAVAHAL_ReadSyInfo();

	//读取基站序列号和硬件版配号
	ret = VAVAHAL_ReadSn();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Read sn or hardver fail\n", FUN, LINE);

		VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_FAST_FLASH);
		VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);

		while(g_running)
		{	
			sleep(1);
		}
		
		return -1;
	}

	VAVAHAL_BuildDomanWithSn(g_gatewayinfo.sn);

	//如有设置域名则使用已设置的
	VAVAHAL_ReadDoman();

	VAVAHAL_BuildGwRfAddr();

	//读取发布日期
	ret = VAVAHAL_ReadReleaseVer();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read releasedate fail\n", FUN, LINE);
		Err_Log("read releasedate fail");
		return -1;
	}
	
	//读取软件版本号
	ret = VAVAHAL_ReadSoftVer();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read softver fail\n", FUN, LINE);
		Err_Log("read softver fail");
		return -1;
	}

	//读取MAC地址
	ret = VAVAHAL_ReadMac();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read mac fail\n", FUN, LINE);
		Err_Log("read mac fail");
		return -1;
	}

	//读取SSID和密码
	ret = VAVAHAL_GetSSIDInfo();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read ssid or pass fail\n", FUN, LINE);
		Err_Log("read ssid pass fail");
		return -1;
	}

	//AV服务器关闭状态
	g_avstop = 0;
	g_update_sd_nochek = 0;
	g_devinfo_update = 0;

	//获取wifi信道
	VAVAHAL_GetNetChannel();

	//初始化NAS配置参数
	ret = VAVAHAL_ReadNasConfig();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read nas config fail\n", FUN, LINE);
		Err_Log("read nas config fail");
		return -1;
	}

	if(g_nas_config.ctrl == 0)
	{
		g_nas_status = VAVA_NAS_STATUS_IDLE;
	}
	else
	{
		g_nas_status = VAVA_NAS_STATUS_CONFIGING;
	}
	g_nas_change = 0;

	//读取NTP信息
	VAVAHAL_ReadNtpInfo();
	
	//读取推送配置信息
	VAVAHAL_ReadPushConfig();
	VAVAHAL_ReadEmailConfig();
	
	return 0;
}

int VAVAHAL_InitCameraInfo(int channel)
{
	memset(&g_camerainfo[channel], 0, sizeof(VAVA_CameraInfo));

	g_camerainfo[channel].videocodec = 0;	//H264 + H264
	g_camerainfo[channel].audiocodec = 3;	//AAC
	g_camerainfo[channel].mic = 1;
	g_camerainfo[channel].speeker = 1;
	g_camerainfo[channel].m_res = VAVA_VIDEO_RESOULT_1080P;
	g_camerainfo[channel].m_fps = 15;
	g_camerainfo[channel].s_res = VAVA_VIDEO_RESOULT_360P;
	g_camerainfo[channel].s_fps = 15;
	g_camerainfo[channel].a_fps = 8;
	g_camerainfo[channel].a_bit = 16;
	g_camerainfo[channel].channel = 1;
	g_camerainfo[channel].mirror = 0;
	g_camerainfo[channel].irledmode = VAVA_IRLED_MODE_AUTO;
	g_camerainfo[channel].osdmode = 0;
	g_camerainfo[channel].powermode = 0;
	g_camerainfo[channel].pirstatus = 0;
	g_camerainfo[channel].mdctrl = 1;
	g_camerainfo[channel].md_sensitivity = 0;
	g_camerainfo[channel].md_startx = 0;
	g_camerainfo[channel].md_endx = 100;
	g_camerainfo[channel].md_starty = 0;
	g_camerainfo[channel].md_endy = 100;
	g_camerainfo[channel].v_quality = VAVA_VIDEO_QUALITY_AUTO;
	g_camerainfo[channel].wakeup_status = 0;
	g_camerainfo[channel].sleep_status = 0;
	g_camerainfo[channel].tfset_status = 0;
	g_camerainfo[channel].sleep_check = 0;
	g_camerainfo[channel].pirget_status = 0;
	g_camerainfo[channel].pirset_status = 0;
	g_camerainfo[channel].heart_status = 0;
	g_camerainfo[channel].wifi_heart = 0;
	g_camerainfo[channel].first_flag = 1;
	g_camerainfo[channel].wakeup_flag = 0;
	g_camerainfo[channel].sleep_flag = 0;
	g_camerainfo[channel].alarm_flag = 0;
	g_camerainfo[channel].cloud_flag = 0;
	g_camerainfo[channel].config_flag = 0;
	g_camerainfo[channel].poweroff_flag = 0;
	g_camerainfo[channel].up_flag = 0;
	g_camerainfo[channel].wifi_num = 0;
	g_camerainfo[channel].wake_fail = 0;
	g_camerainfo[channel].m_bitrate = 0;
	g_camerainfo[channel].s_bitrate = 0;
	g_camerainfo[channel].samprate = 8000;

	memset(g_camerainfo[channel].softver, 0, 16);
	memset(g_camerainfo[channel].hardver, 0, 16);
	memset(g_camerainfo[channel].sn, 0, 32);
	memset(g_camerainfo[channel].rfver, 0, 16);
	memset(g_camerainfo[channel].rfhw, 0, 16);

	g_camera_talk[channel] = -1;
	g_tmp_quality[channel] = VAVA_VIDEO_QUALITY_AUTO;
	
	g_camerainfo_dynamic[channel].battery = 0;
	g_camerainfo_dynamic[channel].batteryload = 100;
	g_camerainfo_dynamic[channel].online = 0;
	g_camerainfo_dynamic[channel].signal = 0;
	g_camerainfo_dynamic[channel].voltage = 0;
	#ifdef BATTEY_INFO
	g_camerainfo_dynamic[channel].temperature = 0;
	g_camerainfo_dynamic[channel].electricity = 0;
	#endif

	g_channel[channel] = channel;

	g_pir_sensitivity[channel] = VAVA_PIR_SENSITIVITY_MIDDLE;
	
	g_online_flag[channel].online = 0;
	g_online_flag[channel].battery = 0;
	g_online_flag[channel].voltage = 0;
	#ifdef BATTEY_INFO
	g_online_flag[channel].temperature = 0;
	g_online_flag[channel].electricity = 0;
	#endif

	g_snapshot[channel].flag = 0;
	g_snapshot[channel].sessionid = -1;

	g_snapshot_alarm[channel].flag = 0;
	g_snapshot_alarm[channel].sessionid = -1;

	g_md_result[channel] = 0;
	
	g_wakenoheart[channel] = 0;
	g_sleepnoheart[channel] = 0;

	g_manaul_ntsamp[channel] = 0;

	memset(&g_ipc_otainfo[channel], 0, sizeof(VAVA_IPC_OtaInfo));
	strcpy(g_ipc_otainfo[channel].f_code, VAVA_CAMERA_F_CODE);
	strcpy(g_ipc_otainfo[channel].f_secret, VAVA_CAMERA_F_SECRET);
	strcpy(g_ipc_otainfo[channel].f_inver, VAVA_CAMERA_F_APPVER_IN);
	strcpy(g_ipc_otainfo[channel].f_outver, VAVA_CAMERA_F_APPVER_OUT);

#ifdef ALARM_PHOTO_IOT
	memset(g_imgname[channel], 0, 256);
	g_imgflag[channel] = 0;
#endif

	g_cloudrecheck[channel] = 1;

	g_cameraname[channel].nstate = 0;
	memset(g_cameraname[channel].name, 0, 32);

	g_firsttag = 1;

	return 0;
}

int VAVAHAL_ReadLanguage()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	int i;
	int j;
	int language;

	language = VAVA_LANGUAGE_ENGLIST;

	fd = fopen(LANGUAGE_FILE, "r");
	if(fd == NULL)
	{
		//未配置过 读取默认值
		fd = fopen(LANGUAGE_FILE_DEFAULT, "r");
		if(fd == NULL)
		{
			return language;
		}

		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);
			
			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			if(strcmp(key, "language") == 0)
			{
				language = atoi(val);
				break;
			}
		}

		fclose(fd);
	}
	else
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);
			
			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			if(strcmp(key, "language") == 0)
			{
				language = atoi(val);
				break;
			}
		}

		fclose(fd);
	}

	if(language < VAVA_LANGUAGE_ENGLIST || language > VAVA_LANGUAGE_CHINESE)
	{
		language = VAVA_LANGUAGE_ENGLIST;
	}
	
	return language;
}

int VAVAHAL_SetLanguage(int language)
{	
	char cmd[128];
	char str[16];
	
	VAVAHAL_SystemCmd("mkdir -p /tmp/sound");

	memset(cmd, 0, 128);
	memset(str, 0, 16);

	switch(language)
	{
		case VAVA_LANGUAGE_ENGLIST:
			strcpy(str, "english");
			break;
		case VAVA_LANGUAGE_CHINESE:
			strcpy(str, "chinese");
			break;
		default:
			break;
	}
	
	sprintf(cmd, "/bin/cp /usr/share/sound/language/%s/* /tmp/sound", str);
	VAVAHAL_SystemCmd(cmd);

	return 0;
}

int VAVAHAL_OpenAPNetwork()
{
	VAVAHAL_SystemCmd("ifconfig apcli0 up");
	VAVAHAL_SystemCmd("ifconfig ra0 up");
	return 0;
}

int VAVAHAL_CloseAPNetwork()
{
	
#if 0 //新wifi驱动
	FILE *fd = NULL;
	char tmp[128];

	VAVAHAL_SystemCmd("ifconfig apcli0 down");
	VAVAHAL_SystemCmd("ifconfig ra0 down");

	VAVAHAL_SystemCmd("uci get wireless.ap0.ApCliEnable > /tmp/apstatus");

	fd = fopen("/tmp/apstatus", "r");
	if(fd != NULL)
	{
		memset(tmp, 0, 128);
		fgets(tmp, 128, fd);
		fclose(fd);

		tmp[strlen(tmp) - 1] = '\0';

		if(atoi(tmp) != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: close ap mode\n", FUN, LINE);
			
			VAVAHAL_SystemCmd("uci set wireless.ap0.ApCliEnable=0");
			VAVAHAL_SystemCmd("uci commit");
		}
	}
#endif

	return 0;
}

int	VAVAHAL_InitSocket()
{
	int i;
	
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_commonfd[i] = -1;
		g_avfd[i] = -1;
	}

	return 0;
}

int VAVAHAL_ReadSn()
{
	int ret;
	VAVA_EEPROM_DATA e2data;

	ret = VAVA_EEPROM_Open();
	if(ret != VAVA_EEPROM_ERRCODE_SUCCESS)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open eeprom fail\n", FUN, LINE);
		return -1;
	}

	memset(&e2data, 0, sizeof(VAVA_EEPROM_DATA));
	ret = VAVA_EEPROM_Read(&e2data);
	if(ret != VAVA_EEPROM_ERRCODE_SUCCESS)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read eeprom fail\n", FUN, LINE);
		
		VAVA_EEPROM_Close();
		return -1;
	}

	memset(g_gatewayinfo.sn, 0, 32);
	memset(g_gatewayinfo.hardver, 0, 20);
	strcpy(g_gatewayinfo.sn, e2data.sn);
	strcpy(g_gatewayinfo.hardver, e2data.hardver);

	VAVA_EEPROM_Close();
	
	return 0;
}

int VAVAHAL_ReadSoftVer()
{
	strcpy(g_gatewayinfo.softver, PROGRAM_VER);
	return 0;
}

int VAVAHAL_ReadReleaseVer()
{
	FILE *fd = NULL;
	char tmpbuff[128];

	fd = fopen("/etc_ro/version", "r");
	if(fd == NULL)
	{
		return -1;
	}

	memset(tmpbuff, 0, 128);
	fgets(tmpbuff, 128, fd);
	tmpbuff[strlen(tmpbuff) - 1] = '\0';
	fclose(fd);

	memset(g_gatewayinfo.releasedate, 0, 12);
	memcpy(g_gatewayinfo.releasedate, tmpbuff + 3, 8);

	return 0;
}

int VAVAHAL_ReadMac()
{
	FILE *fd = NULL;
	char tmpbuff[128];

	fd = fopen("/tmp/factorytest/ETH_Mac", "r");
	if(fd == NULL)
	{
		return -1;
	}

	memset(tmpbuff, 0, 128);
	fgets(tmpbuff, 128, fd);
	tmpbuff[strlen(tmpbuff) - 1] = '\0';
	fclose(fd);

	memset(g_gatewayinfo.mac, 0, 18);
	strcpy(g_gatewayinfo.mac, tmpbuff);

	return 0;
}

int VAVAHAL_BuildGwRfAddr()
{
	int i;
	char rfstr[9];
#if 0
	char testlog[128];
#endif
	
	//生成RF地址
	memset(rfstr, 0, 9);
	memcpy(rfstr, g_gatewayinfo.sn + (strlen(g_gatewayinfo.sn) - 8), 8);

	for(i = 0; i < 8; i++)
	{
		if(rfstr[i] < '0' || rfstr[i] > '9')
		{
			rfstr[i] = (rfstr[i] - '0') % 9 + '0';
		}
	}
		
	g_gatewayinfo.rfaddr = atoi(rfstr) + 50000000 + 0xA0000000;

#if 0 //正式发布时需要关闭
	memset(testlog, 0, 128);
	sprintf(testlog, "[init] sn = %s, addr = %x", rfstr, g_gatewayinfo.rfaddr);
	Err_Info(testlog);
#endif

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfaddr = %x\n", FUN, LINE, g_gatewayinfo.rfaddr);

	return 0;
}

int VAVAHAL_ReadSocketId(int type, int channel)
{
	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		return -1;
	}
	
	if(type == 0) //返回命令SOCKET句柄
	{
		return g_commonfd[channel];
	}
	else if(type == 1) //返回AV SOCKET句柄
	{
		return g_avfd[channel];
	}

	return -1;
}

int VAVAHAL_WriteSocketId(int type, int channel, int fd)
{
	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		return -1;
	}

	if(type == 0) //命令SOCKET句柄
	{
		if(g_commonfd[channel] != fd)
		{
			if(g_commonfd[channel] != -1)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] close cmd socket [%d] -> [-1]\n", 
				                                FUN, LINE, channel, g_commonfd[channel]);

				#if 1
				shutdown(g_commonfd[channel], SHUT_RDWR);
				close(g_commonfd[channel]);
				#else
				close(g_commonfd[channel]);
				#endif
				g_commonfd[channel] = -1;
			}

			if(fd != -1)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] change cmd socket [%d] -> [%d]\n", 
				                                FUN, LINE, channel, g_commonfd[channel], fd);
				g_commonfd[channel] = fd;
			}
		}
	}
	else if(type == 1) //AV SOCKET句柄
	{
		if(g_avfd[channel] != fd)
		{
			if(g_avfd[channel] != -1)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] close av socket [%d] -> [-1]\n", 
				                                FUN, LINE, channel, g_avfd[channel]);

				#if 1
				shutdown(g_avfd[channel], SHUT_RDWR);
				close(g_avfd[channel]);
				#else
				close(g_avfd[channel]);
				#endif
				g_avfd[channel] = -1;
			}

			if(fd != -1)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [channel-%d] change av cmd: [%d]->[%d]\n", 
				                                FUN, LINE, channel, g_avfd[channel], fd);
				g_avfd[channel] = fd;
			}
		}
	}

	return 0;
}

int VAVAHAL_InitUpdate()
{
	int i;

	g_update.sessionid = -1;
	g_update.status = 0;
	g_update.type = 0;
	g_update.loading = 0;
	g_update.current = 0;
	g_update.wait = 0;
	g_update.result = -1;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_update.upchannel[i] = 0;
	}
	
	memset(g_update.url, 0, 256);

	return 0;
}

int VAVAHAL_ReadSyInfo()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	int i;
	int j;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: read sy server conf\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	memset(g_gatewayinfo.crckey, 0, 16);
	memset(g_gatewayinfo.initstr, 0, 128);

	//初始为尚云服务器
	strcpy(g_gatewayinfo.crckey, "EasyView");
	strcpy(g_gatewayinfo.initstr, "EBGAEIBIKHJJGFJKEOGCFAEPHPMAHONDGJFPBKCPAJJMLFKBDBAGCJPBGOLKIKLKAJMJKFDOOFMOBECEJIMM");

	fd = fopen(P2PSERVER_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			if(strcmp(key, "crckey") == 0)
			{
				memset(g_gatewayinfo.crckey, 0, 16);
				strcpy(g_gatewayinfo.crckey, val);
			}
			else if(strcmp(key, "initstr") == 0)
			{
				memset(g_gatewayinfo.initstr, 0, 128);
				strcpy(g_gatewayinfo.initstr, val);
			}
		}

		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_WriteSyInfo()
{
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo crckey = %s > %s", g_gatewayinfo.crckey, P2PSERVER_FILE);
	VAVAHAL_SystemCmd(str);

	memset(str, 0, 128);
	sprintf(str, "echo initstr = %s >> %s", g_gatewayinfo.initstr, P2PSERVER_FILE);
	VAVAHAL_SystemCmd(str);

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_ReadDoman()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	int i;
	int j;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read doman\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(DOMAN_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			if(strcmp(key, "doman_factory") == 0)
			{
				memset(g_gatewayinfo.domain_factory, 0, 64);
				strcpy(g_gatewayinfo.domain_factory, val);
			}
			else if(strcmp(key, "doman_user") == 0)
			{
				memset(g_gatewayinfo.domain_user, 0, 64);
				strcpy(g_gatewayinfo.domain_user, val);
			}
		}

		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_BuildDomanWithSn(char *sn)
{
	char tmphead[5];
	char tmpstr[5];

	memset(tmphead, 0, 5);
	memcpy(tmphead, sn, 4);

	memset(tmpstr, 0, 5);
	memcpy(tmpstr, sn + 7, 4);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get str = [%s]\n", FUN, LINE, tmpstr);

	if(strcmp(tmphead, "P020") == 0)
	{
		if(strcmp(tmpstr, "0001") == 0) //研发环境
		{
			strcpy(g_gatewayinfo.domain_factory, DOMAIN_DEV);
			strcpy(g_gatewayinfo.domain_user, DOMAIN_DEV);
		}
		else if(strcmp(tmpstr, "0002") == 0) //测试环境
		{
			strcpy(g_gatewayinfo.domain_factory, DOMAIN_TEST);
			strcpy(g_gatewayinfo.domain_user, DOMAIN_TEST);
		}
		else if(strcmp(tmpstr, "0003") == 0) //公测环境
		{
			strcpy(g_gatewayinfo.domain_factory, DOMAIN_UAT);
			strcpy(g_gatewayinfo.domain_user, DOMAIN_UAT);
		}
		else if(strcmp(tmpstr, "0004") == 0) //演示环境
		{
			strcpy(g_gatewayinfo.domain_factory, DOMAIN_DEMO);
			strcpy(g_gatewayinfo.domain_user, DOMAIN_DEMO);
		}
	}
	else if(strcmp(tmphead, "64XI") == 0) //VAVA正式服务器
	{
		strcpy(g_gatewayinfo.domain_factory, DOMAIN_DEMO);
		strcpy(g_gatewayinfo.domain_user, DOMAIN_USER);
	}
		
	return 0;
}

//更新认证服务器信息
int VAVAHAL_WriteDoman()
{
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo doman_factory = %s > %s", g_gatewayinfo.domain_factory, DOMAN_FILE);
	VAVAHAL_SystemCmd(str);

	memset(str, 0, 128);
	sprintf(str, "echo doman_user = %s >> %s", g_gatewayinfo.domain_user, DOMAN_FILE);
	VAVAHAL_SystemCmd(str);

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_ReadPairInfo()
{
	int i;
#ifdef FREQUENCY_OFFSET
	FILE *fd_old = NULL;
#endif
	FILE *fd = NULL;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
#ifdef FREQUENCY_OFFSET
		g_pair_old[i].nstat = 0;
		g_pair_old[i].lock = 0;
		g_pair_old[i].ipaddr = 0xFFFFFFFF;
		g_pair_old[i].addr = 0xFFFFFFFF;
		memset(g_pair_old[i].mac, 0, 18);
		memset(g_pair_old[i].id, 0, 32);
#endif	
		g_pair[i].nstat = 0;
		g_pair[i].lock = 0;
		g_pair[i].ipaddr = 0xFFFFFFFF;
#ifdef FREQUENCY_OFFSET
		g_pair[i].index = 0;
#endif
		g_pair[i].addr = 0xFFFFFFFF;
		memset(g_pair[i].mac, 0, 18);
		memset(g_pair[i].id, 0, 32);
	}

	pthread_mutex_lock(&mutex_config_lock);

#ifdef FREQUENCY_OFFSET
	fd = fopen(PAIR_FILE, "r");
	if(fd != NULL)
	{
		fread(g_pair, sizeof(VAVA_Pair_info) * MAX_CHANNEL_NUM, 1, fd);
		fclose(fd);
	}
	else
	{
		fd_old = fopen(PAIR_FILE_OLD, "r");
		if(fd_old != NULL)
		{
			fread(g_pair_old, sizeof(VAVA_Pair_info_old) * MAX_CHANNEL_NUM, 1, fd_old);
			fclose(fd_old);

			fd = fopen(PAIR_FILE, "wb");
			if(fd != NULL)
			{
				for(i = 0; i < MAX_CHANNEL_NUM; i++)
				{
					if(g_pair_old[i].nstat == 1)
					{
						g_pair[i].nstat = 1;
						g_pair[i].lock = 0;
						g_pair[i].ipaddr = g_pair_old[i].ipaddr;
						g_pair[i].index = 0;
						g_pair[i].addr = g_pair_old[i].addr;
						strcpy(g_pair[i].mac, g_pair_old[i].mac);
						strcpy(g_pair[i].id, g_pair_old[i].id);
					}
				}

				fwrite(g_pair, sizeof(VAVA_Pair_info) * MAX_CHANNEL_NUM, 1, fd);
				fclose(fd);
			}
		}
	}
#else
	fd = fopen(PAIR_FILE, "r");
	if(fd != NULL)
	{
		fread(g_pair, sizeof(VAVA_Pair_info) * MAX_CHANNEL_NUM, 1, fd);
		fclose(fd);
	}
#endif

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			strcpy((char *)g_camerainfo[i].sn, g_pair[i].id);
		}
		else
		{
			g_pair[i].lock = 0;
			g_pair[i].ipaddr = 0xFFFFFFFF;
			g_pair[i].addr = 0xFFFFFFFF;
#ifdef FREQUENCY_OFFSET
			g_pair[i].index = 0;
#endif
			memset(g_pair[i].mac, 0, 18);
			memset(g_pair[i].id, 0, 32);
		}
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_WritePairInfo()
{
	FILE *fd = NULL;

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(PAIR_FILE, "wb");
	if(fd != NULL)
	{
		fwrite(g_pair, sizeof(VAVA_Pair_info) * MAX_CHANNEL_NUM, 1, fd);
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_ReadMicInfo()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	char chstr[32];
	int i;
	int j;
	int k;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read micinfo ========\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(MIC_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			for(k = 0; k < MAX_CHANNEL_NUM; k++)
			{
				memset(chstr, 0, 32);
				sprintf(chstr, "channel%d", k);

				if(strcmp(key, chstr) == 0)
				{
					g_camerainfo[k].mic = atoi(val);
					break;
				}
			}
		}
		
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_WriteMicInfo()
{
	int i;
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo channel0 = %d > %s", g_camerainfo[0].mic,  MIC_FILE);
	VAVAHAL_SystemCmd(str);

	for(i = 1; i < MAX_CHANNEL_NUM; i++)
	{
		memset(str, 0, 128);
		sprintf(str, "echo channel%d = %d >> %s", i, g_camerainfo[i].mic, MIC_FILE);
		VAVAHAL_SystemCmd(str);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int	VAVAHAL_ReadSpeakerInfo()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	char chstr[32];
	int i;
	int j;
	int k;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read speakerinfo ========\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(SPEAKER_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			for(k = 0; k < MAX_CHANNEL_NUM; k++)
			{
				memset(chstr, 0, 32);
				sprintf(chstr, "channel%d", k);

				if(strcmp(key, chstr) == 0)
				{
					g_camerainfo[k].speeker = atoi(val);
					break;
				}
			}
		}
		
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_WriteSpeakerInfo()
{
	int i;
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo channel0 = %d > %s", g_camerainfo[0].speeker, SPEAKER_FILE);
	VAVAHAL_SystemCmd(str);

	for(i = 1; i < MAX_CHANNEL_NUM; i++)
	{
		memset(str, 0, 128);
		sprintf(str, "echo channel%d = %d >> %s", i, g_camerainfo[i].speeker, SPEAKER_FILE);
		VAVAHAL_SystemCmd(str);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_ReadVideoQuality()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	char chstr[32];
	int i;
	int j;
	int k;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read vidoequality\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(VIDEOQUALITY_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== key = [%s], val = [%s]\n", FUN, LINE, key, val);

			for(k = 0; k < MAX_CHANNEL_NUM; k++)
			{
				memset(chstr, 0, 32);
				sprintf(chstr, "channel%d", k);

				if(strcmp(key, chstr) == 0)
				{
					g_camerainfo[k].v_quality = atoi(val);
					break;
				}
			}
		}
		
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_WriteVideoQuality()
{
	int i;
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo channel0 = %d > %s", g_camerainfo[0].v_quality, VIDEOQUALITY_FILE);
	VAVAHAL_SystemCmd(str);

	for(i = 1; i < MAX_CHANNEL_NUM; i++)
	{
		memset(str, 0, 128);
		sprintf(str, "echo channel%d = %d >> %s", i, g_camerainfo[i].v_quality, VIDEOQUALITY_FILE);
		VAVAHAL_SystemCmd(str);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_ReadPirSensitivity()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	char chstr[32];
	int i;
	int j;
	int k;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read pirsensitivity\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(PIRSENSITIVITY_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			for(k = 0; k < MAX_CHANNEL_NUM; k++)
			{
				memset(chstr, 0, 32);
				sprintf(chstr, "channel%d", k);

				if(strcmp(key, chstr) == 0)
				{
					g_pir_sensitivity[k] = atoi(val);
					break;
				}
			}
		}
		
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_WritePirSensitivity()
{
	int i;
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo channel0 = %d > %s", g_pir_sensitivity[0], PIRSENSITIVITY_FILE);
	VAVAHAL_SystemCmd(str);

	for(i = 1; i < MAX_CHANNEL_NUM; i++)
	{
		memset(str, 0, 128);
		sprintf(str, "echo channel%d = %d >> %s", i, g_pir_sensitivity[i], PIRSENSITIVITY_FILE);
		VAVAHAL_SystemCmd(str);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_InitSearchBuff()
{
	g_searchbuff = malloc(VAVA_SEARCH_BUFF_SIZE);
	if(g_searchbuff == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc send buff fail\n", FUN, LINE);
		
		Err_Log("malloc sendbuff fail");
		g_running = 0;
		return -1;
	}

	return 0;
}

int VAVAHAL_DeInitSearchBuff()
{
	if(g_searchbuff != NULL)
	{
		free(g_searchbuff);
		g_searchbuff = NULL;
	}

	return 0;
}

int VAVAHAL_ReadRecTime()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	char tmp[12];
	int i;
	int j;

	int fulltime;
	int alaramtime;
	int manaualtime;
	int ctrl;

	g_rectime.full_time = 100;
	g_rectime.alarm_time = 10;
	g_rectime.manaua_time = 300;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_rectime.fulltime_ctrl[i] = 0;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read rectime ========\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(RECTIME_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			if(strcmp(key, "fulltime") == 0)
			{
				fulltime = atoi(val);
				if(fulltime >= 100 && fulltime <= 300)
				{
					g_rectime.full_time = fulltime;
				}
			}
			else if(strcmp(key, "alarm") == 0)
			{
				alaramtime = atoi(val);
				if(alaramtime >= 10 && alaramtime <= 60)
				{
					g_rectime.alarm_time = alaramtime;
				}
			}
			else if(strcmp(key, "manaul") == 0)
			{
				manaualtime = atoi(val);
				if(manaualtime > 0 && manaualtime <= 300)
				{
					g_rectime.manaua_time = manaualtime;
				}
			}
			else
			{
				for(i = 0; i < MAX_CHANNEL_NUM; i++)
				{
					memset(tmp, 0, 12);
					sprintf(tmp, "channel%d", i);

					if(strcmp(key, tmp) == 0)
					{
						ctrl = atoi(val);
						if(ctrl >= VAVA_CTRL_DISABLE && ctrl <= VAVA_CTRL_ENABLE)
						{
							g_rectime.fulltime_ctrl[i] = ctrl;
						}

						break;
					}
				}
			}
		}
		
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_WriteRecTime()
{
	//int i;
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo alarm = %d > %s", g_rectime.alarm_time, RECTIME_FILE);
	VAVAHAL_SystemCmd(str);
	
	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_GetSSIDInfo()
{
	int ret;
	char ssid[32];
	char passwd[32];

	memset(ssid, 0, 32);
	memset(passwd, 0, 32);
	
	//获取SSID
	ret = VAVAHAL_SystemCmd_Ex("uci get wireless.default_ra.ssid", ssid, 32);
	if(ret != 0)
	{
		Err_Log("get ssid fail");
		return -1;
	}
	ssid[strlen(ssid) - 1] = '\0';
	
	//获取密码
	ret = VAVAHAL_SystemCmd_Ex("uci get wireless.default_ra.key", passwd, 32);
	if(ret != 0)
	{
		Err_Log("open /tmp/passwd.txt fail");
		return -1;
	}
	passwd[strlen(passwd) - 1] = '\0';
	
	strcpy(g_gatewayinfo.ssid, ssid);
	strcpy(g_gatewayinfo.pass, passwd);

	return 0;
}

int VAVAHAL_ReadNasConfig()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	int i;
	int j;
	int ctrl;

	memset(&g_nas_config, 0, sizeof(VAVA_NasConfig));
	g_nas_config.ctrl = 0;
	strcpy(g_nas_config.ip, "0.0.0.0");
	strcpy(g_nas_config.path, "/mynas");

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read nasconfig\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(NASCONFIG_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			if(strcmp(key, "ctrl") == 0)
			{
				ctrl = atoi(val);
				if(ctrl >= 0 && ctrl <= 1)
				{
					g_nas_config.ctrl = ctrl;
				}
			}
			else if(strcmp(key, "ip") == 0)
			{
				if(VAVAHAL_CheckIP(val) == 0)
				{
					strcpy(g_nas_config.ip, val);
				}
			}
			else if(strcmp(key, "path") == 0)
			{
				if(VAVAHAL_CheckLinuxPath(val) == 0)
				{
					strcpy(g_nas_config.path, val);
				}
			}
		}
		
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_WriteNasConfig()
{
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo ctrl = %d > %s", g_nas_config.ctrl, NASCONFIG_FILE);
	VAVAHAL_SystemCmd(str);

	memset(str, 0, 128);
	sprintf(str, "echo ip = %s >> %s", g_nas_config.ip, NASCONFIG_FILE);
	VAVAHAL_SystemCmd(str);

	memset(str, 0, 128);
	sprintf(str, "echo path = %s >> %s", g_nas_config.path, NASCONFIG_FILE);
	VAVAHAL_SystemCmd(str);

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_GetP2Pdid()
{
	int ret;
	g_domaintype = SY_DID_TYPE_USER; //0 从用户环境获取  1 从演示环境获取

	while(g_running)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: type - %d\n", FUN, LINE, g_domaintype);
		
		if(g_domaintype == SY_DID_TYPE_USER)
		{
			ret = VAVASERVER_GetToken();
			if(ret == 0)
			{
				ret = VAVASERVER_GetSyDid();
				if(ret == 0)
				{
					break;
				}
				else if(ret == 1)
				{
					g_domaintype = SY_DID_TYPE_FACTORY;
				}
				else
				{
					sleep(10);
					continue;
				}
			}
			else if(ret == 1)
			{
				g_domaintype = SY_DID_TYPE_FACTORY;
				continue;
			}
			else
			{
				sleep(5);
				continue;
			}
		}
		else
		{
			ret = VAVASERVER_GetToken();
			if(ret == 0)
			{
				ret = VAVASERVER_GetSyDid();
				if(ret == 0)
				{
					break;
				}
				else
				{
					sleep(10);
					continue;
				}
			}
			else
			{
				sleep(5);
				continue;
			}
		}
	}
	
	return 0;
}

int VAVAHAL_CheckPairWithMac(char *mac, int clientfd)
{
	int i;
	int channel = -1;
	
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 0)
		{
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel - %d, fd - %d, current_mac = [%s], input_mac = [%s]\n", 
		                                FUN, LINE, i, clientfd, g_pair[i].mac, mac);
		
		if(strcmp(mac, g_pair[i].mac) == 0)
		{
			channel = i;
			break;
		}
	}

	return channel;
}

int VAVAHAL_CheckPairWithSn(char *sn)
{
	int i;
	int channel = -1;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 0)
		{
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel - %d, current_sn = [%s], input_sn = [%s]\n", 
		                                FUN, LINE, i, g_pair[i].id, sn);
		
		if(strcmp(g_pair[i].id, sn) == 0)
		{
			channel = i;
			break;
		}
	}

	return channel;
}

int VAVAHAL_StartAVMemCahce(int channel, int mode)
{
	switch(mode)
	{
		case 0: //全部
			g_avmemchace[channel].mv_read = g_avmemchace[channel].mv_write;
			g_avmemchace[channel].mv_nstats = 1;
			g_avmemchace[channel].sv_read = g_avmemchace[channel].sv_write;
			g_avmemchace[channel].sv_nstats = 1;
			break;
		case 1: //仅录像
			g_avmemchace[channel].mv_read = g_avmemchace[channel].mv_write;
			g_avmemchace[channel].mv_nstats = 1;
			break;
		case 2: //仅实时流
			g_avmemchace[channel].sv_read = g_avmemchace[channel].sv_write;
			g_avmemchace[channel].sv_nstats = 1;
			break;
		default:
			break;
	}
	
	return 0;
}

int VAVAHAL_StopAVMemCahce(int channel)
{
	g_avmemchace[channel].mv_read = -1;
	g_avmemchace[channel].mv_nstats = 0;

	g_avmemchace[channel].sv_read = -1;
	g_avmemchace[channel].sv_nstats = 0;

	return 0;
}

int VAVAHAL_GetNtp()
{
	#if 0
	time_t time_utc;
    struct tm tm_gmt;
	int time_zone;

	// Get the UTC time
    time(&time_utc);
	struct tm tm_local;

	// Get the local time
    // Use localtime_r for threads safe
    localtime_r(&time_utc, &tm_local);

	// Change tm to time_t 
    mktime(&tm_local);

	// Change it to GMT tm
    gmtime_r(&time_utc, &tm_gmt);

	time_zone = tm_local.tm_hour - tm_gmt.tm_hour;

	if(time_zone < -12)
	{
        time_zone += 24; 
    } 
	else if(time_zone > 12)
	{
        time_zone -= 24;
    }

	return time_zone;
	#else
	return g_ntpinfo.ntp;
	#endif
}

int VAVAHAL_PlayAudioFile(char *path)
{
	char str[128];

	if(g_AFplayflag == 1)
	{
		VAVAHAL_SystemCmd("killall Opus_decode");
		VAVAHAL_SystemCmd("killall aplay");
		usleep(100000);
	}

	g_AFplayflag = 1;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [%s]\n", FUN, LINE, path);

	memset(str, 0, 128);
	sprintf(str, "/usr/sbin/Opus_decode %s /tmp/AFplay.pcm 16000 16 1", path);
	VAVAHAL_SystemCmd(str);

	memset(str, 0, 128);
	sprintf(str, "/usr/bin/aplay -M -r 16000 -f S16_LE -c 1 /tmp/AFplay.pcm");
	VAVAHAL_SystemCmd(str);

	g_AFplayflag = 0;
	
	return 0;
}

int VAVAHAL_LedCtrl(VAVA_LED_ID led, VAVA_LED_CTRL val)
{
	char cmd[128];
	memset(cmd, 0, 128);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---- led ctrl, led = %d, val = %d\n", FUN, LINE, led, val);
	
	switch(led)
	{
		case VAVA_LED_RED:
			switch(val)
			{
				case VAVA_LED_SLAKE:
					sprintf(cmd, "echo 1 > /dev/wlink_led");
					break;
				case VAVA_LED_LIGHT:
					sprintf(cmd, "echo 0 > /dev/wlink_led");
					break;
				case VAVA_LED_FAST_FLASH:
					sprintf(cmd, "/bin/echo \"2\" > /dev/wlink_led");
					break;
			}
			break;
		case VAVA_LED_WHITE:
			switch(val)
			{
				case VAVA_LED_SLAKE:
					sprintf(cmd, "echo 1 > /dev/power_led");
					break;
				case VAVA_LED_LIGHT:
					sprintf(cmd, "echo 0 > /dev/power_led");
					break;
				case VAVA_LED_FAST_FLASH:
					sprintf(cmd, "/bin/echo \"2\" > /dev/power_led");
					break;
			}
			break;
		default:
			break;
	}
	
	VAVAHAL_SystemCmd(cmd);
	return 0;
}

int VAVAHAL_BuzzerCtrl(VAVA_BUZZER_TYPE val)
{
	switch(val)
	{
		case VAVA_BUZZER_TYPE_CLOSE:
			VAVAHAL_SystemCmd("echo 1 > /dev/buzze_gpio");
			break;
		case VAVA_BUZZER_TYPE_OPEN:
			VAVAHAL_SystemCmd("echo 0 > /dev/buzze_gpio");
			break;
		case VAVA_BUZZER_TYPE_INTERVAL:
			VAVAHAL_SystemCmd("echo 2 > /dev/buzze_gpio");
			break;
		default:
			break;
	}

	return 0;
}

int VAVAHAL_ResetLedStatus()
{
	//恢复灯状态
	switch(g_router_link_status)
	{
		case VAVA_NETWORK_LINKOK:
			VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_SLAKE);
			VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_LIGHT);
			break;
		case VAVA_NETWORK_LINKING:
			VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);
			VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
			break;
		case VAVA_NETWORK_LINKFAILD:
			VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);
			VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
			break;
		default:
			break;
	}

	return 0;
}

int VAVAHAL_Date_Verification(char *date)
{
	int year;
	int month;
	int day;

	char year_str[5];
	char mouth_str[3];
	char day_str[3];
	char a[13]={0,31,28,31,30,31,30,31,31,30,31,30,31};

	if(date == NULL)
	{
		return -1;
	}

	if(strlen(date) < 8)
	{
		return -1;
	}

	memset(year_str, 0, 5);
	memset(mouth_str, 0, 3);
	memset(day_str, 0, 3);

	memcpy(year_str, date, 4);
	memcpy(mouth_str, date + 4, 2);
	memcpy(day_str, date + 6, 2);

	year = atoi(year_str);
	month = atoi(mouth_str);
	day = atoi(day_str);

	if(year < 1 || month < 1 || month > 12 || day < 1 
		|| (day > (a[month] + ((month == 2) & ((year % 4 == 0 && year % 100 > 0) || year % 400 == 0)))))
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [%s] is not a valid date\n", FUN, LINE, date);
		return -1;
	}

	return 0;
}

int VAVAHAL_Date_CheckFile(char *date)
{
	FILE *fd = NULL;
	char path[128];

	int ret;
	VAVA_Idx_Head idxhead;

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, date, RECIDX_FILE);
	fd = fopen(path, "r");
	if(fd == NULL)
	{
		return 0;
	}

	memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
	ret = fread(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
	fclose(fd);
	
	if(ret <= 0)
	{
		return 0;
	}

	if(idxhead.tag == VAVA_IDX_HEAD && (idxhead.totol - idxhead.first > 0))
	{
		return 1;
	}

	return 0;
}

int VAVAHAL_Date_CurrentCheck(char *date)
{
	time_t time_date;
    struct tm* time_info;
	char currentdate[9];

	time(&time_date);
	time_info = localtime(&time_date);

	memset(currentdate, 0, 9);
	sprintf(currentdate, "%d%02d%02d", 1900 + time_info->tm_year, time_info->tm_mon + 1, time_info->tm_mday);

	if(strcmp(currentdate, date) == 0)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Date is today\n", FUN, LINE);
		return 0;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Date is not today\n", FUN, LINE);

	return -1;
}

int VAVAHAL_Time_Verification(char *checktime)
{
	int hour;
	int min;
	int sec;

	char hour_str[3];
	char min_str[3];
	char sec_str[3];

	if(checktime == NULL)
	{
		return -1;
	}

	if(strlen(checktime) < 6)
	{
		return -1;
	}

	memset(hour_str, 0, 3);
	memset(min_str, 0, 3);
	memset(sec_str, 0, 3);

	memcpy(hour_str, checktime, 2);
	memcpy(min_str, checktime + 2, 2);
	memcpy(sec_str, checktime + 4, 2);

	hour = atoi(hour_str);
	min = atoi(min_str);
	sec = atoi(sec_str);

	if(hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [%s] is not a valid time\n", FUN, LINE, checktime);
		return -1;
	}

	return 0;
}

int VAVAHAL_Time_Verification_Ex(char *checktime, int *out_hour, int *out_min, int *out_sec)
{
	int hour;
	int min;
	int sec;

	char hour_str[3];
	char min_str[3];
	char sec_str[3];

	if(checktime == NULL || out_hour == NULL || out_min == NULL || out_sec == NULL)
	{
		return -1;
	}

	if(strlen(checktime) < 6)
	{
		return -1;
	}

	memset(hour_str, 0, 3);
	memset(min_str, 0, 3);
	memset(sec_str, 0, 3);

	memcpy(hour_str, checktime, 2);
	memcpy(min_str, checktime + 2, 2);
	memcpy(sec_str, checktime + 4, 2);

	hour = atoi(hour_str);
	min = atoi(min_str);
	sec = atoi(sec_str);

	if(hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [%s] is not a valid time\n", FUN, LINE, checktime);
		return -1;
	}

	*out_hour = hour;
	*out_min = min;
	*out_sec = sec;

	return 0;
}	

int VAVAHAL_RecFile_Verification(char *recfilename)
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
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check time fail, time = [%s]\n", FUN, LINE, checktime);
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

	if(type < VAVA_RECFILE_TYPE_FULLTIME || type > VAVA_RECFILE_TYPE_SNAPSHOT)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check type fail, type = %d\n", FUN, LINE, type);
		return -1;
	}
	
	return 0;
}

int VAVAHAL_RecFile_Verification_Ex(char *recfilename, int *filetype, int *filechannel)
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
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check time fail, time = [%s]\n", FUN, LINE, checktime);
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
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check time fail, channel = %d\n", FUN, LINE, channel);
		return -1;
	}

	if(type < VAVA_RECFILE_TYPE_FULLTIME || type > VAVA_RECFILE_TYPE_SNAPSHOT)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check type fail, type = %d\n", FUN, LINE, type);
		return -1;
	}

	*filetype = type;
	*filechannel = channel;
	
	return 0;
}

int VAVAHAL_Armtime_Verification(char *timelist, int *status,
	                             int *s_h, int *s_m, int *s_s,
                                 int *e_h, int *e_m, int *e_s)
{
	char tmp[3];
	int val;
	
	if(timelist == NULL || status == NULL || s_h == NULL || s_m == NULL
		|| s_s == NULL || e_h == NULL || e_m == NULL || e_s == NULL)
	{
		return -1;
	}

	if(timelist[1] != '_' || timelist[8] != '-' || strlen(timelist) != 15)
	{
		return -1;
	}

	//状态
	memset(tmp, 0, 3);
	memcpy(tmp, timelist, 1);
	val = atoi(tmp);
	if(val != 0 && val != 1)
	{
		return -1;
	}
	*status = val;
		
	//起始小时
	memset(tmp, 0, 3);
	memcpy(tmp, timelist + 2, 2);
	val = atoi(tmp);
	if(val < 0 || val > 23)
	{
		return -1;
	}
	*s_h = val;

	//起始分钟
	memset(tmp, 0, 3);
	memcpy(tmp, timelist + 4, 2);
	val = atoi(tmp);
	if(val < 0 || val > 59)
	{
		return -1;
	}
	*s_m = val;

	//起始秒
	memset(tmp, 0, 3);
	memcpy(tmp, timelist + 6, 2);
	val = atoi(tmp);
	if(val < 0 || val > 59)
	{
		return -1;
	}
	*s_s = val;

	//结束小时
	memset(tmp, 0, 3);
	memcpy(tmp, timelist + 9, 2);
	val = atoi(tmp);
	if(val < 0 || val > 23)
	{
		return -1;
	}
	*e_h = val;

	//结束分钟
	memset(tmp, 0, 3);
	memcpy(tmp, timelist + 11, 2);
	val = atoi(tmp);
	if(val < 0 || val > 59)
	{
		return -1;
	}
	*e_m = val;

	//结束秒
	memset(tmp, 0, 3);
	memcpy(tmp, timelist + 13, 2);
	val = atoi(tmp);
	if(val < 0 || val > 59)
	{
		return -1;
	}
	*e_s = val;

	return 0;
}

int VAVAHAL_Armtime_Verification_v1(char *listtime, int *s_h, int *s_m, int *s_s, int *e_h, int *e_m, int *e_s)
{
	char tmp[3];
	int val;
	
	if(listtime == NULL || s_h == NULL || s_m == NULL || s_s == NULL 
		|| e_h == NULL || e_m == NULL || e_s == NULL)
	{
		return -1;
	}

	if(listtime[6] != '-' || strlen(listtime) != 13)
	{
		return -1;
	}

	//起始小时
	memset(tmp, 0, 3);
	memcpy(tmp, listtime, 2);
	val = atoi(tmp);
	if(val < 0 || val > 23)
	{
		return -1;
	}
	*s_h = val;

	//起始分钟
	memset(tmp, 0, 3);
	memcpy(tmp, listtime + 2, 2);
	val = atoi(tmp);
	if(val < 0 || val > 59)
	{
		return -1;
	}
	*s_m = val;

	//起始秒
	memset(tmp, 0, 3);
	memcpy(tmp, listtime + 4, 2);
	val = atoi(tmp);
	if(val < 0 || val > 59)
	{
		return -1;
	}
	*s_s = val;

	//结束小时
	memset(tmp, 0, 3);
	memcpy(tmp, listtime + 7, 2);
	val = atoi(tmp);
	if(val < 0 || val > 23)
	{
		return -1;
	}
	*e_h = val;

	//结束分钟
	memset(tmp, 0, 3);
	memcpy(tmp, listtime + 9, 2);
	val = atoi(tmp);
	if(val < 0 || val > 59)
	{
		return -1;
	}
	*e_m = val;

	//结束秒
	memset(tmp, 0, 3);
	memcpy(tmp, listtime + 11, 2);
	val = atoi(tmp);
	if(val < 0 || val > 59)
	{
		return -1;
	}
	*e_s = val;

	return 0;
}

int VAVAHAL_CheckIP(char *ip)
{
	int i;
	int ret;
	int n[4];
  	char c[4];

	if(ip == NULL)
	{
		return -1;
	}

	ret = sscanf(ip, "%d%c%d%c%d%c%d%c", &n[0], &c[0], &n[1], &c[1], &n[2], &c[2], &n[3], &c[3]);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ret = %d, [%d%c%d%c%d%c%d]\n", 
	                                FUN, LINE, ret, n[0], c[0], n[1], c[1], n[2], c[2], n[3]);
	
	if(ret == 7)
	{
		for(i = 0; i < 3; ++i)
		{
			if (c[i] != '.')
			{
				return -1;
			}
		}
		
		for(i = 0; i < 4; ++i)
		{
			if(n[i] > 255 || n[i] < 0)
			{
				return -1;
			}
		}
		
		return 0;
	}
	else
	{
		return -1;
	}
}

int VAVAHAL_CheckLinuxPath(char *path)
{
	if(path == NULL)
	{
		return -1;
	}

	if(strlen(path) >= 32)
	{
		return -1;
	}

	if(path[0] != '/')
	{
		return -1;
	}

	return 0;
}

int VAVAHAL_ReadArmInfo_v1()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	int i, j;

	int s_hour;
	int s_min;
	int s_sec;
	int e_hour;
	int e_min;
	int e_sec;

	int ret;
	int type;
	int channel;
	int armlist;
	int weekday;

	char path[64];
	
	//初始化
	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++) //摄像机通道
	{
		g_camera_arminfo_v1[channel].type = VAVA_ARMING_STATUS_ENABLE;  //默认常开
		g_camera_arminfo_v1[channel].status = VAVA_ARMING_STATUS_ENABLE;//默认常开
		
		for(armlist = 0; armlist< MAX_ARM_LIST_NUM; armlist++)
		{
			g_camera_arminfo_v1[channel].armdata[armlist].nstat = 0;
			g_camera_arminfo_v1[channel].armdata[armlist].s_h = 0;
			g_camera_arminfo_v1[channel].armdata[armlist].s_m = 0;
			g_camera_arminfo_v1[channel].armdata[armlist].s_s = 0;
			g_camera_arminfo_v1[channel].armdata[armlist].e_h = 0;
			g_camera_arminfo_v1[channel].armdata[armlist].e_m = 0;
			g_camera_arminfo_v1[channel].armdata[armlist].e_s = 0;

			for(weekday = 0; weekday < VAVA_WEEKDAYS; weekday++)
			{
				g_camera_arminfo_v1[channel].armdata[armlist].weekday[weekday] = 0;
			}
		}
	}

	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)  //按摄像机通道获取
	{
		if(g_pair[channel].nstat == 0)
		{
			continue;
		}

		memset(path, 0, 64);
		sprintf(path, "%s_C%d", ARMINFO_FILE_V1, channel);
		fd = fopen(path, "r");
		if(fd == NULL)
		{
			continue;
		}

		armlist = -1;

		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			if(strcmp(key, "type") == 0)
			{
				type = atoi(val);
				if(type >=  VAVA_ARMING_STATUS_DISABLE && type <= VAVA_ARMING_STATUS_ONTIME_OFF)
				{
					g_camera_arminfo_v1[channel].type = type;
				}
			}
			else if(strcmp(key, "list") == 0)
			{
				armlist = atoi(val);
				g_camera_arminfo_v1[channel].armdata[armlist].nstat = 1;
			}
			else if(strcmp(key, "time") == 0)
			{
				if(armlist >= 0 && armlist < MAX_ARM_LIST_NUM)
				{
					ret = VAVAHAL_Armtime_Verification_v1(val, &s_hour, &s_min, &s_sec, &e_hour, &e_min, &e_sec);
					if(ret == 0)
					{
						g_camera_arminfo_v1[channel].armdata[armlist].s_h = s_hour;
						g_camera_arminfo_v1[channel].armdata[armlist].s_m = s_min;
						g_camera_arminfo_v1[channel].armdata[armlist].s_s = s_sec;
						g_camera_arminfo_v1[channel].armdata[armlist].e_h = e_hour;
						g_camera_arminfo_v1[channel].armdata[armlist].e_m = e_min;
						g_camera_arminfo_v1[channel].armdata[armlist].e_s = e_sec;
					}
					else
					{
						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: VAVAHAL_Armtime_Verification_v1 fail\n", FUN, LINE);
					}
				}
			}
			else if(strcmp(key, "weekday") == 0)
			{
				weekday = atoi(val);
				if(weekday >= 0 && weekday < VAVA_WEEKDAYS)
				{
					if(armlist >= 0 && armlist < MAX_ARM_LIST_NUM)
					{
						g_camera_arminfo_v1[channel].armdata[armlist].weekday[weekday] = 1;
					}
				}
			}
		}

		fclose(fd);

		VAVAHAL_PrintArmInfo_v1(channel);
	}

	return 0;
}

int VAVAHAL_WriteArmInfo_v1(int channel)
{
	char str[128];

	int armlist;
	int weekday;
	int listnum = 0;
	
	time_t tt;
	struct tm* t_info;

	pthread_mutex_lock(&mutex_config_lock);

	time(&tt);
	t_info = localtime(&tt);

	//更新时间
	memset(str, 0, 128);
	sprintf(str, "echo ArmConfig = %d%02d%02d_%02d%02d%02d > %s_C%d", 
		         t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday,
		         t_info->tm_hour, t_info->tm_min, t_info->tm_sec, ARMINFO_FILE_V1, channel);
	VAVAHAL_SystemCmd(str);

	//更新类型
	memset(str, 0, 128);
	sprintf(str, "echo type = %d >> %s_C%d", g_camera_arminfo_v1[channel].type, ARMINFO_FILE_V1, channel);
	VAVAHAL_SystemCmd(str);

	for(armlist = 0; armlist < MAX_ARM_LIST_NUM; armlist++)
	{
		if(g_camera_arminfo_v1[channel].armdata[armlist].nstat == 1)
		{
			//列表序号
			memset(str, 0, 128);
			sprintf(str, "echo list = %d >> %s_C%d", listnum, ARMINFO_FILE_V1, channel);
			VAVAHAL_SystemCmd(str);

			//时间
			memset(str, 0, 128);
			sprintf(str, "echo time = %02d%02d%02d-%02d%02d%02d >> %s_C%d", g_camera_arminfo_v1[channel].armdata[armlist].s_h,
				                                                  			g_camera_arminfo_v1[channel].armdata[armlist].s_m,
				                                                  			g_camera_arminfo_v1[channel].armdata[armlist].s_s,
				                                                  			g_camera_arminfo_v1[channel].armdata[armlist].e_h,
				                                                  			g_camera_arminfo_v1[channel].armdata[armlist].e_m,
				                                                 			g_camera_arminfo_v1[channel].armdata[armlist].e_s,
				                                                 			ARMINFO_FILE_V1, channel);
			VAVAHAL_SystemCmd(str);

			for(weekday = 0; weekday < VAVA_WEEKDAYS; weekday++)
			{
				if(g_camera_arminfo_v1[channel].armdata[armlist].weekday[weekday] == 1)
				{
					//星期列表
					memset(str, 0, 128);
					sprintf(str, "echo weekday = %d >> %s_C%d", weekday, ARMINFO_FILE_V1, channel);
					VAVAHAL_SystemCmd(str);
				}
			}
		}

		listnum++;
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

char *VAVAHAL_GetWeekStr(int weekday)
{
	char *weekstr = NULL;
	
	if(weekday < 0 || weekday >= VAVA_WEEKDAYS)
	{
		return NULL;
	}

	switch(weekday)
	{
		case VAVA_WEEKDAY_SUN:
			weekstr = "Sun";
			break;
		case VAVA_WEEKDAY_MON:
			weekstr = "Mon";
			break;
		case VAVA_WEEKDAY_TUE:
			weekstr = "Tue";
			break;
		case VAVA_WEEKDAY_WED:
			weekstr = "Wed";
			break;
		case VAVA_WEEKDAY_THU:
			weekstr = "Thu";
			break;
		case VAVA_WEEKDAY_FRI:
			weekstr = "Fri";
			break;
		case VAVA_WEEKDAY_SAT:
			weekstr = "Sat";
			break;
		default:
			break;
	}

	return weekstr;
}

int VAVAHAL_PrintArmInfo_v1(int channel)
{
	int armlist;
	int weekday;
	int listnum = 0;

	char tmpstr[3];
	char tmpbuff[128];

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------ Arm Info Channel - %d  type - %d ------------\n", 
		                            FUN, LINE, channel, g_camera_arminfo_v1[channel].type);

	for(armlist = 0; armlist < MAX_ARM_LIST_NUM; armlist++)
	{
		if(g_camera_arminfo_v1[channel].armdata[armlist].nstat == 1)
		{
			memset(tmpbuff, 0, 128);
			sprintf(tmpbuff, "%d: %02d%02d%02d - %02d%02d%02d [", listnum++, 
								                                  g_camera_arminfo_v1[channel].armdata[armlist].s_h,
							                                      g_camera_arminfo_v1[channel].armdata[armlist].s_m,
							                                      g_camera_arminfo_v1[channel].armdata[armlist].s_s,
							                                      g_camera_arminfo_v1[channel].armdata[armlist].e_h,
							                                      g_camera_arminfo_v1[channel].armdata[armlist].e_m,
							                                      g_camera_arminfo_v1[channel].armdata[armlist].e_s);
			for(weekday = 0; weekday < VAVA_WEEKDAYS; weekday++)
			{
				if(g_camera_arminfo_v1[channel].armdata[armlist].weekday[weekday] == 1)
				{
					memset(tmpstr, 0, 3);
					sprintf(tmpstr, "%d ", weekday);
					strcat(tmpbuff, tmpstr);
				}
			}
			strcat(tmpbuff, "]");

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);
		}
	}

	return 0;
}

int VAVAHAL_ClearArmInfo_v1(int channel)
{
	int armlist;
	int weekday;
	char tmp[128];
	
	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		return -1;
	}

	g_camera_arminfo_v1[channel].type = VAVA_ARMING_STATUS_ENABLE;  //默认常开
	g_camera_arminfo_v1[channel].status = VAVA_ARMING_STATUS_ENABLE;//默认常开

	for(armlist = 0; armlist< MAX_ARM_LIST_NUM; armlist++)
	{
		g_camera_arminfo_v1[channel].armdata[armlist].nstat = 0;
		g_camera_arminfo_v1[channel].armdata[armlist].s_h = 0;
		g_camera_arminfo_v1[channel].armdata[armlist].s_m = 0;
		g_camera_arminfo_v1[channel].armdata[armlist].s_s = 0;
		g_camera_arminfo_v1[channel].armdata[armlist].e_h = 0;
		g_camera_arminfo_v1[channel].armdata[armlist].e_m = 0;
		g_camera_arminfo_v1[channel].armdata[armlist].e_s = 0;

		for(weekday = 0; weekday < VAVA_WEEKDAYS; weekday++)
		{
			g_camera_arminfo_v1[channel].armdata[armlist].weekday[weekday] = 0;
		}
	}
	
	memset(tmp, 0, 128);
	sprintf(tmp, "/bin/rm -f %s_C%d", ARMINFO_FILE_V1, channel);
	VAVAHAL_SystemCmd(tmp);

	return 0;
}

int VAVAHAL_ReadNtpInfo()
{
	FILE *fd = NULL;
	char cmd[128];
	char str[128];
	char key[32];
	char val[32];
	int i, j;
	int ntp;
	
	//初始化
	g_ntpinfo.ntp = 8;
	memset(g_ntpinfo.str, 0, 256);
	strcpy(g_ntpinfo.str, "");

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read NtpInfo\n", FUN, LINE);
	
	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(NTPINFO_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			if(strcmp(key, "ntp") == 0)
			{
				ntp = atoi(val);
				if(ntp >= -12 && ntp <= 12)
				{
					g_ntpinfo.ntp = ntp;
				}
			}
			else if(strcmp(key, "ntpstr") == 0)
			{
				strcpy(g_ntpinfo.str, val);
			}
		}

		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	memset(cmd, 0, 128); 
	if(g_ntpinfo.ntp < 0)
	{
		sprintf(cmd, "/usr/sbin/SetTimeZone %d", g_ntpinfo.ntp);
	}
	else
	{
		sprintf(cmd, "/usr/sbin/SetTimeZone +%d", g_ntpinfo.ntp);
	}
	
	VAVAHAL_SystemCmd(cmd);

	return 0;
}

int VAVAHAL_WriteNtpInfo(int ntp, char *ntpstr)
{
	char cmd[128];

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ntp = %d, [%s]\n", FUN, LINE, ntp, ntpstr);
	
	memset(cmd, 0, 128);
	sprintf(cmd, "echo ntp = %d > %s", ntp, NTPINFO_FILE);
	VAVAHAL_SystemCmd(cmd);

	memset(cmd, 0, 128);
	sprintf(cmd, "echo ntpstr = %s >> %s", ntpstr, NTPINFO_FILE);
	VAVAHAL_SystemCmd(cmd);

	VAVAHAL_SystemCmd("sync");

	return 0;
}

int VAVAHAL_ClearNtp()
{
	char cmd[128];

	memset(cmd, 0, 128);
	sprintf(cmd, "/bin/rm -f %s", NTPINFO_FILE);
	VAVAHAL_SystemCmd(cmd);

	VAVAHAL_SystemCmd("sync");

	return 0;
}

int VAVAHAL_ReadPushConfig()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	char pushstr[32];
	int i;
	int j;
	int k;

	//初始化
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_pushflag[i].push = 1;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read pushconfig\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(PUSH_FLAG_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: key = [%s], val = [%s]\n", FUN, LINE, key, val);

			for(k = 0; k < MAX_CHANNEL_NUM; k++)
			{
				memset(pushstr, 0, 32);
				sprintf(pushstr, "channel%d", k);

				if(strcmp(key, pushstr) == 0)
				{
					g_pushflag[k].push = atoi(val) == 0 ? 0 : 1;
					break;
				}
			}
		}
		
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

#if 0
	VAVAHAL_PrintPush();
#endif

	return 0;
}

int VAVAHAL_WritePushConfig()
{
	int i;
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo channel0 = %d > %s", g_pushflag[0].push, PUSH_FLAG_FILE);
	VAVAHAL_SystemCmd(str);

	for(i = 1; i < MAX_CHANNEL_NUM; i++)
	{
		memset(str, 0, 128);
		sprintf(str, "echo channel%d = %d >> %s", i, g_pushflag[i].push, PUSH_FLAG_FILE);
		VAVAHAL_SystemCmd(str);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}	

int VAVAHAL_ReadEmailConfig()
{
	FILE *fd = NULL;
	char str[128];
	char key[32];
	char val[32];
	char mailstr[32];
	int i;
	int j;
	int k;

	//初始化(默认邮件为关)
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_pushflag[i].email = 0;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== read emailconfig\n", FUN, LINE);

	pthread_mutex_lock(&mutex_config_lock);

	fd = fopen(EMAIL_FLAG_FILE, "r");
	if(fd != NULL)
	{
		while(g_running)
		{
			if(feof(fd))
			{
				break;
			}

			memset(str, 0, 128);
			memset(key, 0, 32);
			memset(val, 0, 32);

			if(fgets(str, 128, fd) == NULL)
			{
				break;
			}

			for(i = 0, j = 0; i < strlen(str); i++)
			{
				if(str[i] == '=')
				{
					break;
				}

				if(str[i] != ' ')
				{
					key[j] = str[i];
					j++;
				}
			}

			key[j] = '\0';
			i++;

			for(j = 0; i < strlen(str); i++)
			{
				if(str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
				{
					break;
				}

				if(str[i] != ' ')
				{
					val[j] = str[i];
					j++;
				}
			}

			val[j] = '\0';

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======== key = [%s], val = [%s]\n", FUN, LINE, key, val);

			for(k = 0; k < MAX_CHANNEL_NUM; k++)
			{
				memset(mailstr, 0, 32);
				sprintf(mailstr, "channel%d", k);

				if(strcmp(key, mailstr) == 0)
				{
					g_pushflag[k].email = atoi(val) == 0 ? 0 : 1;
					break;
				}
			}
		}
		
		fclose(fd);
	}

	pthread_mutex_unlock(&mutex_config_lock);

#if 0
	VAVAHAL_PrintEmail();
#endif

	return 0;
}

int VAVAHAL_WriteEmailConfig()
{
	int i;
	char str[128];

	pthread_mutex_lock(&mutex_config_lock);

	memset(str, 0, 128);
	sprintf(str, "echo channel0 = %d > %s", g_pushflag[0].email, EMAIL_FLAG_FILE);
	VAVAHAL_SystemCmd(str);

	for(i = 1; i < MAX_CHANNEL_NUM; i++)
	{
		memset(str, 0, 128);
		sprintf(str, "echo channel%d = %d >> %s", i, g_pushflag[i].email, EMAIL_FLAG_FILE);
		VAVAHAL_SystemCmd(str);
	}

	pthread_mutex_unlock(&mutex_config_lock);

	return 0;
}

int VAVAHAL_InsertCmdList(int channel, int sessionid, int cmd_code, void *data, int size)
{
	int ret;
	int timeout = 3;
	qCmdData cmddata;

	static int cmdfail = 0;

	memset(&cmddata, 0, sizeof(qCmdData));
	cmddata.channel = channel;
	cmddata.sessionid = sessionid;
	cmddata.cmdtype = cmd_code;

	if(data != NULL && size != 0)
	{
		memcpy(cmddata.param, data, size);
	}

	//确保加入到队列
	while(g_running)
	{
		if(timeout-- <= 0)
		{
			Err_Log("cmd instert fail");
			
			cmdfail++;

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: insert fail, cmdfail = %d\n", FUN, LINE, cmdfail);
			break;
		}
		
		ret = CmdList_InsertData(cmddata);
		if(ret == 0)
		{
			cmdfail = 0;
			break;
		}

		sleep(1);
	}

	if(cmdfail >= 30)
	{
		//队列堵塞 由看门狗重启
		WatchDog_Stop(__FUNCTION__);
	}

	if(timeout <= 0)
	{
		return VAVA_ERR_CODE_INSET_LIST_FAIL;
	}
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_InsertRecList(int type, char *dirname, char *filename)
{
	int ret;
	int timeout = 3;
	qRecData recdata;
	
	static int recfail = 0;
	
	memset(&recdata, 0, sizeof(qRecData));
	recdata.type = type;
	strcpy(recdata.dirname, dirname);
	strcpy(recdata.filename, filename);

	//确保加入到队列
	while(g_running)
	{
		if(timeout-- <= 0)
		{
			Err_Log("rec instert fail");
			
			recfail++;

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: insert fail, recfail = %d\n", FUN, LINE, recfail);
			break;
		}
		
		ret = RecList_InsertData(recdata);
		if(ret == 0)
		{
			recfail = 0;
			break;
		}

		sleep(1);
	}

	if(recfail >= 30)
	{
		//队列堵塞 由看门狗重启
		WatchDog_Stop(__FUNCTION__);
	}

	if(timeout <= 0)
	{
		return VAVA_ERR_CODE_INSET_LIST_FAIL;
	}
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_InsertImgList(int channel, int sessionid, char *dirname, char *filename)
{
	int ret;
	int timeout = 3;
	qImgData imgdata;

	static int imgfail = 0;

	if(dirname == NULL || filename == NULL)
	{
		return -1;
	}

	memset(&imgdata, 0, sizeof(qImgData));
	imgdata.channel = channel;
	imgdata.sessionid = sessionid;
	strcpy(imgdata.dirname, dirname);
	strcpy(imgdata.filename, filename);
	
	//确保加入到队列
	while(g_running)
	{
		if(timeout-- <= 0)
		{
			Err_Log("Img instert fail");
			
			imgfail++;

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: insert fail, imgfail = %d\n", FUN, LINE, imgfail);
			break;
		}

		ret = ImgList_InsertData(imgdata);
		if(ret == 0)
		{
			imgfail = 0;
			break;
		}

		sleep(1);
	}

	if(imgfail >= 30)
	{
		//队列堵塞 由看门狗重启
		WatchDog_Stop(__FUNCTION__);
	}

	if(timeout <= 0)
	{
		return VAVA_ERR_CODE_INSET_LIST_FAIL;
	}
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_InsertPushList(int channel, int pushtype, int filetype, char *dirname, char *filename, char *msg, int time, int ntsamp)
{
	int ret;
	int timeout = 3;
	qPushData pushdata;
	
	static int pushfail = 0;

	if(dirname == NULL || filename == NULL)
	{
		return -1;
	}

	memset(&pushdata, 0, sizeof(qPushData));
	pushdata.channel = channel;
	pushdata.pushtype = pushtype;
	pushdata.filetype = filetype;
	pushdata.time = time;
	pushdata.ntsamp = ntsamp;
	strcpy(pushdata.dirname, dirname);
	strcpy(pushdata.filename, filename);
	strcpy(pushdata.msg, msg);

	while(g_running)
	{
		if(timeout-- <= 0)
		{
			Err_Log("Push instert fail");
			
			pushfail++;

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: insert fail, pushfail = %d\n", FUN, LINE, pushfail);
			break;
		}

		ret = PushList_InsertData(pushdata);
		if(ret == 0)
		{
			pushfail = 0;
			break;
		}
	}

	if(pushfail >= 30)
	{
		//队列堵塞 由看门狗重启
		WatchDog_Stop(__FUNCTION__);
	}

	if(timeout <= 0)
	{
		return VAVA_ERR_CODE_INSET_LIST_FAIL;
	}
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_Crc32Gen(char *buff, int size)
{
	CRC32_Init();
	CRC32_Calculate((unsigned char *)buff, size);
	return CRC32_GetResult();
}

int VAVAHAL_NumToWeekStr(int num, char *str)
{
	if(num < 0 || num >= VAVA_WEEKDAYS || str == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	
	switch(num)
	{
		case VAVA_WEEKDAY_SUN:
			strcpy(str, "Sun");
			break;
		case VAVA_WEEKDAY_MON:
			strcpy(str, "Mon");
			break;
		case VAVA_WEEKDAY_TUE:
			strcpy(str, "Tue");
			break;
		case VAVA_WEEKDAY_WED:
			strcpy(str, "Wed");
			break;
		case VAVA_WEEKDAY_THU:
			strcpy(str, "Thu");
			break;
		case VAVA_WEEKDAY_FRI:
			strcpy(str, "Fri");
			break;
		case VAVA_WEEKDAY_SAT:
			strcpy(str, "Sat");
			break;
		default:
			break;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_WeekStrToNum(int *num, char *str)
{
	if(num == NULL || str == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	*num = -1;

	if(strcmp(str, "Sun") == 0)
	{
		*num = 0;
	}
	else if(strcmp(str, "Mon") == 0)
	{
		*num = 1;
	}
	else if(strcmp(str, "Tue") == 0)
	{
		*num = 2;
	}
	else if(strcmp(str, "Wed") == 0)
	{
		*num = 3;
	}
	else if(strcmp(str, "Thu") == 0)
	{
		*num = 4;
	}
	else if(strcmp(str, "Fri") == 0)
	{
		*num = 5;
	}
	else if(strcmp(str, "Sat") == 0)
	{
		*num = 6;
	}

	if(*num < 0 || *num >= VAVA_WEEKDAYS)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_CheckSession(int sessionid)
{
	int i;

	for(i = 0; i < MAX_SESSION_NUM; i++)
	{
		if(g_session[i].id == sessionid)
		{
			return 0;
		}
	}

	return -1;
}

int VAVAHAL_FindFile(char *dirname, char *filename)
{
	FILE *fd = NULL;
	char path[128];
	
	if(dirname == NULL || filename == NULL)
	{
		return -1;
	}
	
	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, filename);

	fd = fopen(path, "r");
	if(fd == NULL)
	{
		return -1;
	}

	fclose(fd);

	return 0;
}

int VAVAHAL_ForMatSD()
{
	char devname[32];
	char str[128];
	int ret;

	int tmpstatus = 0;
	int tmpformat = 0;

	ret = SD_CheckDev(devname);
	if(ret != 0)
	{
		return VAVA_ERR_CODE_SD_NOFOUND;
	}

	//先去除挂载
	VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");
	usleep(500000);
	VAVAHAL_SystemCmd("/bin/rm -rf /mnt/sd0");
	VAVAHAL_SystemCmd("sync");

	g_gatewayinfo.sdstatus = VAVA_SD_STATUS_NOCRAD;
	g_gatewayinfo.format_flag = 0;

	memset(str, 0, 128);
	sprintf(str, "/sbin/mkfs.vfat /dev/%s", devname);
	ret = VAVAHAL_SystemCmd(str);
	if(ret != 0)
	{
		return VAVA_ERR_CODE_SD_FORMAT_FAIL;
	}

	SD_TFCheck(&tmpstatus, &tmpformat);

	if(tmpformat == 1)
	{
		return VAVA_ERR_CODE_SD_FORMAT_FAIL;
	}
	
	g_gatewayinfo.sdstatus = VAVA_SD_STATUS_HAVECARD;
	g_gatewayinfo.format_flag = tmpformat;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_CheckSDStatus()
{
	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_NOCRAD)
	{
		return VAVA_ERR_CODE_SD_NOFOUND;
	}

	if(g_gatewayinfo.format_flag == 1)
	{
		return VAVA_ERR_CODE_SD_NEED_FORMAT;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_PopTFCard()
{
	int i;
	
	//禁止录像
	VAVAHAL_CloseRec(1, 0);

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{	
		//关闭录像
		VAVAHAL_StopManualRec(i);
		VAVAHAL_StopFullTimeRec(i);
		VAVAHAL_StopAlarmRec(i);
	}

	sleep(1);
	VAVAHAL_SystemCmd("sync");
	VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");
	g_gatewayinfo.sdstatus = VAVA_SD_STATUS_NOCRAD;
	g_gatewayinfo.format_flag = 0;
	g_gatewayinfo.totol = 0;
	g_gatewayinfo.used = 0;
	g_gatewayinfo.free = 0;
	sleep(1);

	VAVAHAL_OpenRec(1, 0);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_CheckTFRWStatus()
{
	int ret;
	char m_type[256];

	ret = VAVAHAL_SystemCmd_Ex("mount | grep \"/mnt/sd0 type vfat (ro\"", m_type, 256);
	if(ret == 0)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: TF Card check ro type, need remount\n", FUN, LINE);
		
		VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");
		g_gatewayinfo.sdstatus = VAVA_SD_STATUS_NOCRAD;
		g_gatewayinfo.format_flag = 0;
	}
	
	return 0;
}

int VAVAHAL_StartAlarmRec(int channel)
{
	g_alarmrec[channel].ctrl = 1;
	g_alarmrec[channel].start = 0;
	return 0;
}

int VAVAHAL_StopAlarmRec(int channel)
{
	g_alarmrec[channel].ctrl = 0;
	g_alarmrec[channel].start = 0;
	return 0;
}

int VAVAHAL_StartAlarmCloud(int channel)
{
	g_cloudrec[channel].ctrl = 1;
	g_cloudrec[channel].start = 0;
	g_cloudsend[channel] = 0;
	return 0;
}

int VAVAHAL_StopAlarmCloud(int channel)
{
	g_cloudrec[channel].ctrl = 0;
	g_cloudrec[channel].start = 0;
	g_cloudsend[channel] = 0;
	return 0;
}

int VAVAHAL_StartFullTimeRec(int channel)
{
	g_fulltimerec[channel].ctrl = 1;
	g_fulltimerec[channel].start = 0;
	return 0;
}

int VAVAHAL_StopFullTimeRec(int channel)
{
	g_fulltimerec[channel].ctrl = 0;
	g_fulltimerec[channel].start = 0;
	return 0;
}

int VAVAHAL_StartManualRec(int channel, int session)
{
	g_manaulrec[channel].session = session;
	g_manaulrec[channel].ctrl = 1;
	g_manaulrec[channel].start = 0;
	return 0;
}

int VAVAHAL_StopManualRec(int channel)
{
	g_manaulrec[channel].ctrl = 0;
	g_manaulrec[channel].start = 0;
	return 0;
}

int VAVAHAL_OpenRec(int type, int channel)
{
	int i;

	if(type == 0) //开启单通道录像
	{
		g_fulltimerec[channel].status = VAVA_REC_STATUS_IDLE;
		g_alarmrec[channel].status = VAVA_REC_STATUS_IDLE;
		g_manaulrec[channel].status = VAVA_REC_STATUS_IDLE;
	}
	else if(type == 1) //开启所有通道录像
	{
		g_rec_delaytime = REC_DELAY_TIME; 
		
		for(i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			g_fulltimerec[i].status = VAVA_REC_STATUS_IDLE;
			g_alarmrec[i].status = VAVA_REC_STATUS_IDLE;
			g_manaulrec[i].status = VAVA_REC_STATUS_IDLE;
		}
	}

	return 0;
}

int VAVAHAL_CloseRec(int type, int channel)
{
	int i;

	if(type == 0) //关闭单通道录像
	{
		g_fulltimerec[channel].status = VAVA_REC_STATUS_STOP;
		g_alarmrec[channel].status = VAVA_REC_STATUS_STOP;
		g_manaulrec[channel].status = VAVA_REC_STATUS_STOP;
	}
	else if(type == 1) //关闭所有通道录像
	{
		for(i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			g_fulltimerec[i].status = VAVA_REC_STATUS_STOP;
			g_alarmrec[i].status = VAVA_REC_STATUS_STOP;
			g_manaulrec[i].status = VAVA_REC_STATUS_STOP;
		}
	}

	return 0;
}

int VAVAHAL_OpenCloudRec(int type, int channel)
{
	int i;

	if(type == 0)
	{
		g_cloudrec[channel].status = VAVA_REC_STATUS_IDLE;
	}
	else if(type == 1)
	{
		for(i= 0; i < MAX_CHANNEL_NUM; i++)
		{
			g_cloudrec[i].status = VAVA_REC_STATUS_IDLE;
		}
	}

	return 0;
}

int VAVAHAL_CloseCloudRec(int type, int channel)
{
	int i;

	if(type == 0)
	{
		g_cloudrec[channel].status = VAVA_REC_STATUS_STOP;
	}
	else if(type == 1)
	{
		for(i= 0; i < MAX_CHANNEL_NUM; i++)
		{
			g_cloudrec[i].status = VAVA_REC_STATUS_STOP;
		}
	}

	return 0;
}

int VAVAHAL_SaveAlarmPhoto(int channel, int offset, int size, int ntsamp, int pushflag, char *tmpdir, char *tmpfile)
{
	FILE *fd = NULL;

	int ret;
	int ntp;
	int phototime;
	time_t tt;
	struct tm* t_info;

	char dirname[9];
	char filename[11];
	char photopath[128];

	ntp = VAVAHAL_GetNtp();
	phototime = ntsamp - ntp * 3600;
	tt = phototime;
	t_info = localtime(&tt);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: tt %d, change ==> [%d%02d%02d_%02d%02d%02d] flag = %d\n", 
	                                FUN, LINE, (int)tt, t_info->tm_year + 1900, t_info->tm_mon + 1,
		                            t_info->tm_mday, t_info->tm_hour, t_info->tm_min, t_info->tm_sec, pushflag);
	
	memset(dirname, 0, 9);
	sprintf(dirname, "%d%02d%02d", t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday);

	ret = VAVAHAL_Date_Verification(dirname);
	if(ret != 0)
	{
		return -1;
	}

	memset(filename, 0, 11);
	sprintf(filename, "%02d%02d%02d_%d_%d", t_info->tm_hour, 
		                                    t_info->tm_min, 
		                                    t_info->tm_sec, 
		                                    VAVA_RECFILE_TYPE_IMG, 
		                                    channel);

	ret = VAVAHAL_RecFile_Verification(filename);
	if(ret != 0)
	{
		return -1;
	}

	if(tmpdir == NULL && tmpfile == NULL)
	{
		memset(photopath, 0, 128);
		sprintf(photopath, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, filename);

		fd = fopen(photopath, "wb");
		if(fd == NULL)
		{
			return -1;
		}

		fwrite(g_avmemchace[channel].pmvMemBegin + offset, size, 1, fd);
		fclose(fd);

		//推送
		if(pushflag == 1)
		{
			Rec_InserFileToIdx(channel, dirname, filename, VAVA_RECFILE_TYPE_IMG, 1);
			
			VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_IMG, dirname, filename, "", 0, ntsamp);
		}
	}
	else
	{
		if(strcmp(tmpdir, dirname) == 0 && strcmp(tmpfile, filename) == 0)
		{
			//同名文件只存一张

			//推送
			if(pushflag == 1)
			{
				Rec_InserFileToIdx(channel, dirname, filename, VAVA_RECFILE_TYPE_IMG, 1);
				VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_IMG, dirname, filename, "", 0, ntsamp);
			}
		}
		else
		{
			memset(photopath, 0, 128);
			sprintf(photopath, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, filename);

			fd = fopen(photopath, "wb");
			if(fd == NULL)
			{
				return -1;
			}

			fwrite(g_avmemchace[channel].pmvMemBegin + offset, size, 1, fd);
			fclose(fd);

			//推送
			if(pushflag == 1)
			{
				Rec_InserFileToIdx(channel, tmpdir, tmpfile, VAVA_RECFILE_TYPE_IMG, 1);
				Rec_InserFileToIdx(channel, dirname, filename, VAVA_RECFILE_TYPE_IMG, 1);
				
				VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_IMG, tmpdir, tmpfile, "", 0, ntsamp);
				VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_IMG, dirname, filename, "", 0, ntsamp);
			}
		}
	}

	return 0;
}

int VAVAHAL_SavePhotoInfo(int channel, int ntsamp, int *flag, char *dirname, char *filename)
{
	int ret;
	int ntp;
	int phototime;
	time_t tt;
	struct tm* t_info;
	
	ntp = VAVAHAL_GetNtp();
	phototime = ntsamp - ntp * 3600;
	tt = phototime;
	t_info = localtime(&tt);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: tt %d, change ==> [%d%02d%02d_%02d%02d%02d]\n", 
	                                FUN, LINE, (int)tt, t_info->tm_year + 1900, t_info->tm_mon + 1,
	                                t_info->tm_mday, t_info->tm_hour, t_info->tm_min, t_info->tm_sec);

	memset(dirname, 0, 9);
	sprintf(dirname, "%d%02d%02d", t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday);

	ret = VAVAHAL_Date_Verification(dirname);
	if(ret != 0)
	{
		return -1;
	}

	memset(filename, 0, 11);
	sprintf(filename, "%02d%02d%02d_%d_%d", t_info->tm_hour, 
		                                    t_info->tm_min, 
		                                    t_info->tm_sec, 
		                                    VAVA_RECFILE_TYPE_IMG, 
		                                    channel);

	ret = VAVAHAL_RecFile_Verification(filename);
	if(ret != 0)
	{
		return -1;
	}

	*flag = 1;

	return 0;
}

int VAVAHAL_SaveSnapShot(int channel, int session, char *buff, int size, int ntsamp)
{
	FILE *fd = NULL;

	int ret;
	time_t tt;
	struct tm* t_info;

	char dirname[9];
	char filename[11];
	char photopath[128];

	tt = ntsamp;
	t_info = localtime(&tt);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: tt %d, change ==> [%d%02d%02d_%02d%02d%02d]\n", 
	                                FUN, LINE, (int)tt, t_info->tm_year + 1900, t_info->tm_mon + 1,
	                                t_info->tm_mday, t_info->tm_hour, t_info->tm_min, t_info->tm_sec);

	memset(dirname, 0, 9);
	sprintf(dirname, "%d%02d%02d", t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday);

	ret = VAVAHAL_Date_Verification(dirname);
	if(ret != 0)
	{
		//通知APP抓图失败
		VAVAHAL_SendSnapShotResult(session, channel, "", "", 1);
		return -1;
	}

	ret = Rec_CheckDir(dirname);
	if(ret != 0)
	{
		//通知APP抓图失败
		VAVAHAL_SendSnapShotResult(session, channel, "", "", 1);
		return -1;
	}

	memset(filename, 0, 11);
	sprintf(filename, "%02d%02d%02d_%d_%d", t_info->tm_hour, 
		                                    t_info->tm_min, 
		                                    t_info->tm_sec, 
		                                    VAVA_RECFILE_TYPE_SNAPSHOT, 
		                                    channel);

	ret = VAVAHAL_RecFile_Verification(filename);
	if(ret != 0)
	{
		//通知APP抓图失败
		VAVAHAL_SendSnapShotResult(session, channel, "", "", 1);
		return -1;
	}

	memset(photopath, 0, 128);
	sprintf(photopath, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, filename);

	fd = fopen(photopath, "wb");
	if(fd == NULL)
	{
		//通知APP抓图失败
		VAVAHAL_SendSnapShotResult(session, channel, dirname, filename, 1);
		return -1;
	}

	fwrite(buff, size, 1, fd);
	fclose(fd);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [---------- $$ SnapShot End $$----------]\n", FUN, LINE);

	//抓图结果不放入媒体
	//Rec_InserFileToIdx(channel, dirname, filename, VAVA_RECFILE_TYPE_IMG, 1);

	//上报给APP抓图结果
	VAVAHAL_SendSnapShotResult(session, channel, dirname, filename, 0);
	
	return 0;
}

int VAVAHAL_SavePhoto(int channel, int offset, int size)
{
	FILE *fd = NULL;
	char *path = NULL;

	switch(channel)
	{
		case 0:
			path = "/tmp/pic0";
			break;
		case 1:
			path = "/tmp/pic1";
			break;
		case 2:
			path = "/tmp/pic2";
			break;
		case 3:
			path = "/tmp/pic3";
			break;
		default:
			break;
	}

	if(path == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: input channel fail\n", FUN, LINE);
		return -1;
	}

	fd = fopen(path, "wb");
	if(fd == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open savefile fail\n", FUN, LINE);
		return -1;
	}

	fwrite(g_avmemchace[channel].pmvMemBegin + offset, size, 1, fd);
	fclose(fd);

	return 0;
}

int VAVAHAL_EncryptionDateStr(char *indirname, char *outdirname, int random)
{
	int i;
	char tmpdirname[9];

	if(indirname == NULL || outdirname == NULL)
	{
		return -1;
	}

	memset(tmpdirname, 0, 9);
	memcpy(tmpdirname, indirname, 8);

	for(i = 0; i < 8; i++)
	{
		tmpdirname[i] = tmpdirname[i] ^ random;
	}

	memset(outdirname, 0, 9);
	memcpy(outdirname, tmpdirname, 8);
	
	return 0;
}

int VAVAHAL_ParseDateStr(char *indirname, char *outdirname, int random)
{
	int i;
	int tmprandom;
	char tmpdirname[9];

	if(indirname == NULL || outdirname == NULL)
	{
		return -1;
	}

	memset(tmpdirname, 0, 9);
	memcpy(tmpdirname, indirname, 8);
	tmprandom = random - 0x1A;

	for(i = 0; i < 8; i++)
	{
		tmpdirname[i] = tmpdirname[i] ^ tmprandom;
	}

	//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: dirname = [%s]\n", FUN, LINE, tmpdirname);

	memset(outdirname, 0, 9);
	memcpy(outdirname, tmpdirname, 8);

	return 0;
}

int VAVAHAL_EncryptionFileStr(char *infilename, char *outfilename, int random)
{
	int i;
	char tmpfilename[11];

	if(infilename == NULL || outfilename == NULL)
	{
		return -1;
	}

	memset(tmpfilename, 0, 11);
	memcpy(tmpfilename, infilename, 10);

	for(i = 0; i < 10; i++)
	{
		tmpfilename[i] = tmpfilename[i] ^ random;
	}

	memset(outfilename, 0, 11);
	memcpy(outfilename, tmpfilename, 10);
	
	return 0;
}

int VAVAHAL_ParseFileStr(char *infilename, char *outfilename, int random)
{
	int i;
	int tmprandom = 0;
	char tmpfilename[11];

	if(infilename == NULL || outfilename == NULL)
	{
		return -1;
	}

	memset(tmpfilename, 0, 11);
	memcpy(tmpfilename, infilename, 10);
	tmprandom = random - 0x1A;

	for(i = 0; i < 10; i++)
	{
		tmpfilename[i] = tmpfilename[i] ^ tmprandom;
	}
	
	memset(outfilename, 0, 11);
	memcpy(outfilename, tmpfilename, 10);

	return 0;
}

int VAVAHAL_SearchList_Init()
{
	int i;

	pthread_mutex_lock(&mutex_search_camera_lock);

	g_searchcamera.totol = 0;
	g_searchcamera.flush = 0;

	for(i = 0; i < MAX_SEARCH_CAMERA_NUM; i++)
	{
		g_searchcamera.data[i].flag = 0;
		g_searchcamera.data[i].addr = 0xFFFFFFFF;
		g_searchcamera.data[i].time = 0;
		memset(g_searchcamera.data[i].sn, 0, 32);
	}

	pthread_mutex_unlock(&mutex_search_camera_lock);

	return 0;
}

int VAVAHAL_SearchList_Insert(unsigned int addr, char *sn)
{
	int i;
	struct timeval t_time;

	pthread_mutex_lock(&mutex_search_camera_lock);

	if(g_searchcamera.totol >= MAX_SEARCH_CAMERA_NUM)
	{
		pthread_mutex_unlock(&mutex_search_camera_lock);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: list full\n", FUN, LINE);
		return -1;
	}

	//先查找是否已经有数据
	for(i = 0; i < MAX_SEARCH_CAMERA_NUM; i++)
	{
		if(strcmp(g_searchcamera.data[i].sn, sn) == 0 || g_searchcamera.data[i].addr == addr)
		{
			g_searchcamera.data[i].flag = 1;
			g_searchcamera.data[i].addr = addr;
			strcpy(g_searchcamera.data[i].sn, sn);

			break;
		}
	}

	//新增一条信息
	if(i >= MAX_SEARCH_CAMERA_NUM)
	{
		for(i = 0; i < MAX_SEARCH_CAMERA_NUM; i++)
		{
			if(g_searchcamera.data[i].flag == 0)
			{
				gettimeofday(&t_time, NULL);

				g_searchcamera.flush = 1;
				
				g_searchcamera.data[i].flag = 1;
				g_searchcamera.data[i].addr = addr;
				g_searchcamera.data[i].time = (int)(t_time.tv_sec);
				memset(g_searchcamera.data[i].sn, 0, 32);
				strcpy(g_searchcamera.data[i].sn, sn);
				
				g_searchcamera.totol += 1;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: insert addr = %x, SN [%s], i = %d, time = %d, totol = %d\n", 
				                                FUN, LINE, addr, sn, i, g_searchcamera.data[i].time, g_searchcamera.totol);
				break;
			}
		}
	}

	pthread_mutex_unlock(&mutex_search_camera_lock);

	return 0;
}

int VAVAHAL_SearchList_Remove(unsigned int addr)
{
	int i;

	pthread_mutex_lock(&mutex_search_camera_lock);

	for(i = 0; i < MAX_SEARCH_CAMERA_NUM; i++)
	{
		if(g_searchcamera.data[i].addr == addr)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: remove addr = %x, SN [%s], i = %d, totol = %d\n", 
				                            FUN, LINE, addr, g_searchcamera.data[i].sn, i, g_searchcamera.totol);
			
			if(g_searchcamera.data[i].flag != 0)
			{
				g_searchcamera.data[i].flag = 0;
				g_searchcamera.totol -= 1;
			}

			g_searchcamera.data[i].addr = 0xFFFFFFFF;
			g_searchcamera.data[i].time = 0;
			memset(g_searchcamera.data[i].sn, 0, 32);
			break;
		}
	}

	pthread_mutex_unlock(&mutex_search_camera_lock);

	return 0;
}

int VAVAHAL_SearchList_Update()
{
	int i;
	unsigned int current;
	struct timeval t_time;

	pthread_mutex_lock(&mutex_search_camera_lock);
	
	if(g_searchcamera.totol == 0)
	{
		if(g_searchcamera.flush == 1)
		{
			g_searchcamera.flush = 0;
			
			for(i = 0; i < MAX_SEARCH_CAMERA_NUM; i++)
			{
				g_searchcamera.data[i].flag = 0;
				g_searchcamera.data[i].addr = 0xFFFFFFFF;
				g_searchcamera.data[i].time = 0;
				memset(g_searchcamera.data[i].sn, 0, 32);
			}
		}
	}
	else
	{
		gettimeofday(&t_time, NULL);
		current = (int)(t_time.tv_sec);

		for(i = 0; i < MAX_SEARCH_CAMERA_NUM; i++)
		{
			if(g_searchcamera.data[i].flag == 1)
			{
				if(current - g_searchcamera.data[i].time >= 120)
				{
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: SN [%s] is remove from list\n", FUN, LINE, g_searchcamera.data[i].sn);
					
					g_searchcamera.data[i].flag = 0;
					g_searchcamera.data[i].addr = 0xFFFFFFFF;
					g_searchcamera.data[i].time = 0;
					memset(g_searchcamera.data[i].sn, 0, 32);

					g_searchcamera.totol -= 1;
				}
			}
		}
	}
	
	pthread_mutex_unlock(&mutex_search_camera_lock);

	return 0;
}

int VAVAHAL_SearchList_Find(char *sn, unsigned int *addr)
{
	int i;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: need find sn = [%s]\n", FUN, LINE, sn);

	pthread_mutex_lock(&mutex_search_camera_lock);

	for(i = 0; i < MAX_SEARCH_CAMERA_NUM; i++)
	{
		if(g_searchcamera.data[i].flag == 0)
		{
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: find sn = [%s]\n", FUN, LINE, g_searchcamera.data[i].sn);

		if(strcmp(g_searchcamera.data[i].sn, sn) == 0)
		{
			*addr = g_searchcamera.data[i].addr;
			
			pthread_mutex_unlock(&mutex_search_camera_lock);
			return VAVA_ERR_CODE_SUCCESS;
		}
	}

	pthread_mutex_unlock(&mutex_search_camera_lock);

	return VAVA_ERR_CODE_NOT_PAIRMODE;
}

int VAVAHAL_SearchList_FindOne(char *sn, unsigned int *addr)
{
	int i;

	pthread_mutex_lock(&mutex_search_camera_lock);

	if(g_searchcamera.totol <= 0)
	{
		pthread_mutex_unlock(&mutex_search_camera_lock);
		return -1;
	}
	else if(g_searchcamera.totol > 1)
	{
		pthread_mutex_unlock(&mutex_search_camera_lock);
		return -2;
	}

	for(i = 0; i < MAX_SEARCH_CAMERA_NUM; i++)
	{
		if(g_searchcamera.data[i].flag == 1)
		{
			*addr = g_searchcamera.data[i].addr;
			strcpy(sn, g_searchcamera.data[i].sn);
			
			break;
		}
	}

	pthread_mutex_unlock(&mutex_search_camera_lock);

	return 0;
}

int VAVAHAL_DelRecFile(char *dirname, char *filename)
{
	FILE *fd = NULL;
	char path[128];
	char cmd[128];

	if(dirname == NULL || filename == NULL)
	{
		return -1;
	}

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, filename);
	
	fd = fopen(path, "r");
	if(fd == NULL)
	{
		return -1;
	}
	fclose(fd);
	
	memset(cmd, 0, 128);
	sprintf(cmd, "/bin/rm -f /mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, filename);
	return VAVAHAL_SystemCmd_Enx(cmd);
}

int VAVAHAL_DelRecDir(char *dirname)
{
	DIR *dirp = NULL;
	char path[128];
	char cmd[128];

	if(dirname == NULL)
	{
		return -1;
	}

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s", g_gatewayinfo.sn, dirname);

	dirp = opendir(path);
	if(dirp == NULL)
	{
		return -1;
	}
	closedir(dirp);

	memset(cmd, 0, 128);
	sprintf(cmd, "/bin/rm -rf /mnt/sd0/%s/%s", g_gatewayinfo.sn, dirname);
	return VAVAHAL_SystemCmd_Enx(cmd);
}

int VAVAHAL_GetPirStatus(int channel)
{
	int timeout;
	unsigned int addr;
#ifdef FREQUENCY_OFFSET
	unsigned int index;
#endif

	//获取短波地址
	addr = g_pair[channel].addr;
#ifdef FREQUENCY_OFFSET
	index = g_pair[channel].index;
#endif

	g_camerainfo[channel].pirget_status = 1;

	//timeout = 20; //1.5秒超时(按100ms每次)
	timeout = 80;

	while(g_running && g_camerainfo[channel].pirget_status)
	{
		if(timeout > 65 && timeout % 5 == 0)
		{
#ifdef FREQUENCY_OFFSET
			RF_PirGet(addr, index);
#else
			RF_PirGet(addr);
#endif
		}

		usleep(RF_CMD_SLEEP_TIME);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: pir get timeout, channel = %d\n", FUN, LINE, channel);
			return -1;
		}
	}

	if(g_camerainfo[channel].pirget_status == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int VAVAHAL_SetPirStatus(int channel, int sensitivity)
{
	int timeout;
	unsigned int addr;
#ifdef FREQUENCY_OFFSET
	unsigned int index;
#endif

	//获取短波地址
	addr = g_pair[channel].addr;
#ifdef FREQUENCY_OFFSET
	index = g_pair[channel].index;
#endif

	g_camerainfo[channel].pirset_status = 1;

	//timeout = 20; //1.5秒超时(按100ms每次)
	timeout = 80;

	while(g_running && g_camerainfo[channel].pirset_status)
	{
		if(timeout > 65 && timeout % 5 == 0)
		{
#ifdef FREQUENCY_OFFSET
			RF_PirSet(addr, index, sensitivity);
#else
			RF_PirSet(addr, sensitivity);
#endif
		}

		usleep(RF_CMD_SLEEP_TIME);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: pir set timeout, channel = %d\n", FUN, LINE, channel);
			return -1;
		}
	}

	if(g_camerainfo[channel].pirset_status == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}	
}

#ifdef FREQUENCY_OFFSET
int VAVAHAL_PairReq(int channel, unsigned int addr, unsigned int index)
#else
int VAVAHAL_PairReq(int channel, unsigned int addr)
#endif
{
	int timeout;

	#ifdef PAIR_SIMPLIFY
	g_pair_wifi_addr = addr;
	#endif
	
	g_pair_status[channel] = 1;

	//timeout = 20; //1.5秒超时(按500ms每次)
	timeout = 80;

	while(g_running && g_pair_status[channel])
	{
		if(timeout > 65 && timeout % 5 == 0)
		{
#ifdef FREQUENCY_OFFSET
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ===> PairReq, addr = %x, index = %d, channel = %d\n", FUN, LINE, addr, index, channel);
#else
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ===> PairReq, channel = %d\n", FUN, LINE, channel);
#endif

#ifdef FREQUENCY_OFFSET
			RF_Req_Pair(addr, index, (unsigned char)channel, g_gatewayinfo.ssid, g_gatewayinfo.pass);
#else
			RF_Req_Pair(addr, (unsigned char)channel, g_gatewayinfo.ssid, g_gatewayinfo.pass);
#endif
		}
		
		usleep(RF_CMD_HEART_TIME);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Pairreq timeout, channel = %d\n", FUN, LINE, channel);
			return VAVA_ERR_CODE_PAIR_TIMEOUT;
		}
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_PairCamera(cJSON *pJsonRoot, int sessionid, unsigned int addr, char *sn, char *name, int channel)
{
	int i;
	int ret; 
	pthread_t pth_id;
	pthread_attr_t attr;

	VAVA_CameraPair camerapair;
	
	g_pairmode = 1;

	//清除锁定信息
	VAVAHAL_ClearPairLock();

	pthread_mutex_lock(&mutex_pair_lock);

	//查找是否已配对
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1 && strcmp(g_pair[i].id, sn) == 0 && g_pair[i].addr == addr)
		{
			break;
		}
	}

	if(i < MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Camera has beed paird $$$$$$, channel = %d\n", FUN, LINE, channel);
		channel = i;
		
		#ifdef FREQUENCY_OFFSET
		camerapair.index = g_pair[channel].index;
		#endif
	}
	else
	{
		if(channel == -1)
		{
			//自动选择空闲通道
			for(i = 0; i < MAX_CHANNEL_NUM; i++)
			{
				if(g_pair[i].nstat == 0 && g_pair[i].lock == 0)
				{
					channel = i;

					#ifdef FREQUENCY_OFFSET
					camerapair.index = VAVAHAL_GetPairFreeIndex();
					#endif

					break;
				}
			}

			if(i >= MAX_CHANNEL_NUM)
			{
				//通道满
				pthread_mutex_unlock(&mutex_pair_lock);
				
				g_pairmode = 0;
				
				return VAVA_ERR_CODE_CHANNEL_FULL;
			}
		}
		else
		{
			if(channel < 0 || channel >= MAX_CHANNEL_NUM)
			{
				pthread_mutex_unlock(&mutex_pair_lock);
				
				g_pairmode = 0;
				
				return VAVA_ERR_CODE_PAIR_FAIL;
			}

			if(g_pair[channel].nstat != 0 || g_pair[channel].lock != 0)
			{
				pthread_mutex_unlock(&mutex_pair_lock);
				
				g_pairmode = 0;
				
				return VAVA_ERR_CODE_PAIR_FAIL;
			}
		}
	}
	
	camerapair.sessionid = sessionid;
	camerapair.channel = channel;
	camerapair.addr = addr;

	//锁定通道 插入数据
	g_pair[channel].lock = 1;
	g_pair[channel].addr = addr;
	memset(g_pair[channel].id, 0, 32);
	strcpy(g_pair[channel].id, sn);

	if(strlen(name) > 0)
	{
		g_cameraname[channel].nstate = 1;
		strcpy(g_cameraname[channel].name, name);
	}
	else
	{
		g_cameraname[channel].nstate = 0;
		memset(g_cameraname[channel].name, 0, 32);
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: lock channel = %d\n", FUN, LINE, channel);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: lock addr = %x\n", FUN, LINE, g_pair[channel].addr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: lock sn = [%s]\n", FUN, LINE, g_pair[channel].id);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: lock name = [%s]\n", FUN, LINE, g_cameraname[channel].name);

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, camerapair_pth, &camerapair);
	pthread_attr_destroy(&attr);

	//保证线程参数正确传入
	usleep(50000);

	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: Create camerapair_pth fail, ret = %d\n", FUN, LINE, ret);
		
		g_pair[channel].lock = 0;
		g_pair[channel].addr = 0xFFFFFFFF;
		memset(g_pair[channel].id, 0, 32);
		
		pthread_mutex_unlock(&mutex_pair_lock);
		
		g_pairmode = 0;
		
		return VAVA_ERR_CODE_PAIR_FAIL;
	}

	if(pJsonRoot != NULL)
	{
		cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	}

	pthread_mutex_unlock(&mutex_pair_lock);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ClearPairLock()
{ 
	int i;
	
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_pair[i].lock = 0;
	}

	return 0;
}

int VAVAHAL_WakeupCamera(int channel, unsigned char mode)
{
	int timeout;
	unsigned int addr;
#ifdef FREQUENCY_OFFSET
	unsigned int index;
#endif
	
	//获取短波地址
	addr = g_pair[channel].addr;
#ifdef FREQUENCY_OFFSET
	index = g_pair[channel].index;
#endif

	if(addr == 0xFFFFFFFF)
	{
		return VAVA_ERR_CODE_WAKEUP_FAIL;
	}

	g_camerainfo[channel].wakeup_status = 1;

	//timeout = 20; //1.5秒超时(按500ms每次)
	timeout = 100;

	while(g_running && g_camerainfo[channel].wakeup_status)
	{
		if(timeout > 85 && timeout % 5 == 0)
		{
			#ifdef WAKE_TIME_USER_TEST
			gettimeofday(&g_testval_1, NULL);
			#endif

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ===> wake up camera, channel = %d\n", FUN, LINE, channel);
			
#ifdef FREQUENCY_OFFSET
			RF_WakeUP(addr, index, mode, g_gatewayinfo.netch);
#else
			RF_WakeUP(addr, mode, g_gatewayinfo.netch);
#endif
		}
		
		usleep(RF_CMD_HEART_TIME);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Wake up camera timeout, channel = %d\n", FUN, LINE, channel);
			return VAVA_ERR_CODE_WAKEUP_TIMEOUT;
		}
	}

	if(g_wakeup_result[channel] == 0)
	{
		return VAVA_ERR_CODE_SUCCESS;
	}
	else
	{
		return VAVA_ERR_CODE_WAKEUP_FAIL;
	}
}

int VAVAHAL_WakeupCamera_Ex(int channel, unsigned char mode)
{
	int ret = VAVA_ERR_CODE_WAKEUP_FAIL;
	int cmdfd;

	pthread_mutex_lock(&mutex_power_lock[channel]);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [WakeUP] ch - %d, wake - %d, alarm - %d, cloud - %d, config - %d, update - %d\n",
	                                FUN, LINE, channel, g_camerainfo[channel].wakeup_flag, g_camerainfo[channel].alarm_flag, 
		                            g_camerainfo[channel].cloud_flag, g_camerainfo[channel].config_flag, g_camerainfo[channel].up_flag);

	if(g_camerainfo[channel].wakeup_flag == 0 && g_camerainfo[channel].alarm_flag == 0 && g_camerainfo[channel].cloud_flag == 0 
		&& g_camerainfo[channel].config_flag == 0 && g_camerainfo[channel].up_flag == 0)
	{
		//关闭连接
		VAVAHAL_WriteSocketId(0, channel, -1);
		VAVAHAL_WriteSocketId(1, channel, -1);

		g_wakenoheart[channel] = 1;
		
		ret = VAVAHAL_WakeupCamera(channel, mode);
		if(ret == VAVA_ERR_CODE_SUCCESS)
		{
			//开启音视频缓冲区
			if(mode == WAKE_UP_MODE_APP)
			{
				VAVAHAL_StartAVMemCahce(channel, VAVA_AVMEM_MODE_LIVE);
			}
			else
			{
				VAVAHAL_StartAVMemCahce(channel, VAVA_AVMEM_MODE_ALL);
			}

			g_camerainfo[channel].wake_fail = 0;
		}
		else
		{
			g_camerainfo[channel].wake_fail++;
			if(g_camerainfo[channel].wake_fail >= 3)
			{
				g_camerainfo[channel].wake_fail = 0;
			
				//唤醒指令辅助判定离线
				g_camerainfo_dynamic[channel].online = 0;
				//g_camerainfo_dynamic[channel].battery = 0;
				//g_camerainfo_dynamic[channel].voltage = 0;
			}
		}
		
		g_wakenoheart[channel] = 0;
		
		pthread_mutex_unlock(&mutex_power_lock[channel]);
		return ret;
	}
	else
	{
		cmdfd = VAVAHAL_ReadSocketId(0, channel);
		if(cmdfd != -1)
		{
			VAVAHAL_CmdReq_InsertIframe(cmdfd, VAVA_STREAM_SUB);
		}
	}

	pthread_mutex_unlock(&mutex_power_lock[channel]);
	
	return VAVA_ERR_CODE_SUCCESS;
}

#ifdef FREQUENCY_OFFSET
int VAVAHAL_WakeupCamera_Ext(unsigned int addr, unsigned int index)
#else
int VAVAHAL_WakeupCamera_Ext(unsigned int addr)
#endif
{
	int i;
	int timeout;
	unsigned char mode = WAKE_UP_CLEAR_IPC;
	
	if(addr == 0xFFFFFFFF)
	{
		return VAVA_ERR_CODE_WAKEUP_FAIL;
	}

	g_clear_addr = addr;
	g_clear_status = 1;

	for(i = 0; i < 2; i++)
	{
		timeout = 60;
		
		while(g_running && g_clear_status)
		{
			if(timeout > 40 && timeout % 5 == 0)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: wakeup with clearcamera, addr = %x\n", FUN, LINE, addr);
				
#ifdef FREQUENCY_OFFSET
				RF_WakeUP(addr, index, mode, g_gatewayinfo.netch);
#else		
				RF_WakeUP(addr, mode, g_gatewayinfo.netch);
#endif
			}

			usleep(RF_CMD_HEART_TIME);

			if(timeout-- <= 0)
			{
				break;
			}
		}

		if(g_clear_status == 0)
		{
			break;
		}

		sleep(2);
	}

	g_clear_addr = 0;
	g_clear_status = 0;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_WakeupCamera_Ex1(int channel, unsigned char mode)
{
	int ret;
	int cmdfd;
	
	pthread_mutex_lock(&mutex_power_lock[channel]);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [WakeUP] ch - %d, wake - %d, alarm - %d, cloud - %d, config - %d, update - %d\n",
	                                FUN, LINE, channel, g_camerainfo[channel].wakeup_flag, g_camerainfo[channel].alarm_flag, 
		                            g_camerainfo[channel].cloud_flag, g_camerainfo[channel].config_flag, g_camerainfo[channel].up_flag);
	
	if(g_camerainfo[channel].wakeup_flag == 0 && g_camerainfo[channel].alarm_flag == 0 && g_camerainfo[channel].cloud_flag == 0 
		&& g_camerainfo[channel].config_flag == 0 && g_camerainfo[channel].up_flag == 0)
	{
		//关闭连接
		VAVAHAL_WriteSocketId(0, channel, -1);
		VAVAHAL_WriteSocketId(1, channel, -1);
		
		//ret = VAVAHAL_WakeupCamera(channel, mode);
		//if(ret == VAVA_ERR_CODE_SUCCESS)
		//{
			//开启音视频缓冲区
			if(mode == WAKE_UP_MODE_APP)
			{
				VAVAHAL_StartAVMemCahce(channel, VAVA_AVMEM_MODE_LIVE);
			}
			else
			{
				VAVAHAL_StartAVMemCahce(channel, VAVA_AVMEM_MODE_ALL);
			}
		//}
		pthread_mutex_unlock(&mutex_power_lock[channel]);
		return ret;
	}
	else
	{
		cmdfd = VAVAHAL_ReadSocketId(0, channel);
		if(cmdfd != -1)
		{
			VAVAHAL_CmdReq_InsertIframe(cmdfd, VAVA_STREAM_SUB);
		}
	}

	pthread_mutex_unlock(&mutex_power_lock[channel]);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_SleepCamera(int channel)
{
	int timeout;
	unsigned int addr;
#ifdef FREQUENCY_OFFSET
	unsigned int index;
#endif

	//重置WIFI信号
	g_camerainfo_dynamic[channel].signal = 0;
	
	//获取短波地址
	addr = g_pair[channel].addr;
#ifdef FREQUENCY_OFFSET
	index = g_pair[channel].index;
#endif

	if(addr == 0xFFFFFFFF)
	{
		return VAVA_ERR_CODE_SLEEP_FAIL;
	}

	g_camerainfo[channel].sleep_status = 1;
	
	//timeout = 20; ////4秒超时(按100ms每次)
	timeout = 80;

	while(g_running && g_camerainfo[channel].sleep_status)
	{
		if(timeout > 65 && timeout % 5 == 0)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ===> sleep camera, channel = %d\n", FUN, LINE, channel);
			
#ifdef FREQUENCY_OFFSET
			RF_Sleep(addr, index);
#else		
			RF_Sleep(addr);
#endif
		}
		
		usleep(RF_CMD_SLEEP_TIME);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: sleep camera timeout, channel = %d\n", FUN, LINE, channel);

			//释放WIFI心跳检测
			g_camerainfo[channel].wifi_heart = 0;
			return VAVA_ERR_CODE_SLEEP_TIMEOUT;
		}
	}

	//释放WIFI心跳检测
	g_camerainfo[channel].wifi_heart = 0;

	if(g_sleep_result[channel] == 0)
	{
		return VAVA_ERR_CODE_SUCCESS;
	}
	else
	{
		return VAVA_ERR_CODE_SLEEP_FAIL;
	}
}

int VAVAHAL_SleepCamera_Ex(int channel)
{
	int ret; 
	
	pthread_mutex_lock(&mutex_power_lock[channel]);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [Sleep] ch - %d, wake - %d, alarm - %d, cloud - %d, config - %d, update_flag - %d\n",
	                                FUN, LINE, channel, g_camerainfo[channel].wakeup_flag, g_camerainfo[channel].alarm_flag, 
		                            g_camerainfo[channel].cloud_flag, g_camerainfo[channel].config_flag, g_camerainfo[channel].up_flag);
	
	if(g_camerainfo[channel].wakeup_flag == 0 && g_camerainfo[channel].alarm_flag == 0 && g_camerainfo[channel].cloud_flag == 0 
		&& g_camerainfo[channel].config_flag == 0 && g_camerainfo[channel].up_flag == 0)
	{
		if(g_camerainfo[channel].sleep_flag == 0)
		{
			pthread_mutex_unlock(&mutex_power_lock[channel]);
			return VAVA_ERR_CODE_SUCCESS;
		}
		
		g_sleepnoheart[channel] = 1;
		
		ret = VAVAHAL_SleepCamera(channel);
		if(ret == VAVA_ERR_CODE_SUCCESS)
		{
			//关闭音视频缓冲区
			VAVAHAL_StopAVMemCahce(channel);
		}
		
		//关闭连接
		VAVAHAL_WriteSocketId(0, channel, -1);
		VAVAHAL_WriteSocketId(1, channel, -1);
		
		g_sleepnoheart[channel] = 0;
		
		pthread_mutex_unlock(&mutex_power_lock[channel]);
		return ret;
	}

	pthread_mutex_unlock(&mutex_power_lock[channel]);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_WakeupCamera_WithSet(int channel, int *cmdfd)
{
	int ret;
	int errnum;
	int cmdsock;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	errnum = 0;
	while(g_running)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [WAKE-UP][param-config] channel - %d\n", FUN, LINE, channel);
		
		ret = VAVAHAL_WakeupCamera_Ex(channel, WAKE_UP_MODE_APP);
		if(ret != VAVA_ERR_CODE_SUCCESS)
		{
			sleep(1);

			errnum++;
			if(errnum >= 3)
			{
				return VAVA_ERR_CODE_CONFIG_TIMEOUT;
			}
		}
		else
		{
			g_camerainfo[channel].config_flag = 1;
			break;
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: wake up success\n", FUN, LINE);

	errnum = 0;
	while(g_running)
	{
		cmdsock = VAVAHAL_ReadSocketId(0, channel);
		if(cmdsock == -1)
		{
			sleep(1);
			
			errnum++;
			if(errnum >= 20) //20秒超时
			{
				g_camerainfo[channel].config_flag = 0;
				return VAVA_ERR_CODE_CONFIG_TIMEOUT;
			}

			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get cmdfd success, fd = %d\n", FUN, LINE, cmdsock);
		break;
	}

	*cmdfd = cmdsock;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_WakeupCamera_WithSnapshort(int channel)
{
	int ret;
	int errnum;
	int avsock;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	errnum = 0;
	while(g_running)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [WAKE-UP][snapshort] channel - %d\n", FUN, LINE, channel);
		
		ret = VAVAHAL_WakeupCamera_Ex(channel, WAKE_UP_MODE_PIR);
		if(ret != VAVA_ERR_CODE_SUCCESS)
		{
			sleep(1);

			errnum++;
			if(errnum >= 3)
			{
				return VAVA_ERR_CODE_CONFIG_TIMEOUT;
			}
		}
		else
		{
			g_camerainfo[channel].config_flag = 1;
			break;
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Wake up success\n", FUN, LINE);

	errnum = 0;
	while(g_running)
	{
		avsock = VAVAHAL_ReadSocketId(1, channel);
		if(avsock == -1)
		{
			sleep(1);
			
			errnum++;
			if(errnum >= 20) //20秒超时
			{
				g_camerainfo[channel].config_flag = 0;
				return VAVA_ERR_CODE_CONFIG_TIMEOUT;
			}

			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get avfd success, fd = %d\n", FUN, LINE, avsock);
		break;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

#ifdef FREQUENCY_OFFSET
int VAVAHAL_WakeupCamera_WithPair(unsigned int addr, unsigned int index)
#else
int VAVAHAL_WakeupCamera_WithPair(unsigned int addr)
#endif
{
	int timeout;
	
	g_wakeup_withpair_addr = addr;
	g_wakeup_withpair_status = 1;

	timeout = 120;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ===> wake camera with pair, addr = %x\n", FUN, LINE, addr);
	
	while(g_running && g_wakeup_withpair_status)
	{
		if(timeout > 105 && timeout % 5 == 0)
		{
#ifdef FREQUENCY_OFFSET
			RF_WakeUP(addr, index, WAKE_UP_MODE_APP, g_gatewayinfo.netch);
#else
			RF_WakeUP(addr, WAKE_UP_MODE_APP, g_gatewayinfo.netch);
#endif
		}
		
		usleep(RF_CMD_SLEEP_TIME);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Wake up timeout\n", FUN, LINE);

			g_wakeup_withpair_addr = 0xFFFFFFFF;
			g_wakeup_withpair_status = 0;

			return VAVA_ERR_CODE_WAKEUP_TIMEOUT;
		}
	}

	return VAVA_ERR_CODE_SUCCESS;
}

#ifdef FREQUENCY_OFFSET
int VAVAHAL_SleepCamera_WithPair(unsigned int addr, unsigned int index)
#else
int VAVAHAL_SleepCamera_WithPair(unsigned int addr)
#endif
{
	int timeout;
	
	g_sleep_withpair_addr = addr;
	g_sleep_withpair_status = 1;

	timeout = 80;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ===> sleep camera with pair, addr = %x\n", FUN, LINE, addr);
	
	while(g_running && g_sleep_withpair_status)
	{
		if(timeout > 65 && timeout % 5 == 0)
		{
#ifdef FREQUENCY_OFFSET
			RF_Sleep(addr, index);
#else
			RF_Sleep(addr);
#endif
		}
		
		usleep(RF_CMD_HEART_TIME);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: sleep timeout timeout, addr = %x\n", FUN, LINE, addr);

			g_sleep_withpair_addr = 0xFFFFFFFF;
			g_sleep_withpair_status = 0;
			
			return VAVA_ERR_CODE_WAKEUP_TIMEOUT;
		}
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_HeartCheck(int channel)
{
	int timeout;
	unsigned int addr;
#ifdef FREQUENCY_OFFSET
	unsigned int index;
#endif

	//获取短波地址
	addr = g_pair[channel].addr;
#ifdef FREQUENCY_OFFSET
	index = g_pair[channel].index;
#endif

	g_camerainfo[channel].heart_status = 1;

	timeout = 100;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Heart check, channel = %d, addr = %x\n", FUN, LINE, channel, addr);

	while(g_running && g_camerainfo[channel].heart_status)
	{
		if(g_wakenoheart[channel] == 1 || g_sleepnoheart[channel] == 1)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [------ wait wakeup or sleep ------], channel = %d\n", FUN, LINE, channel);
			sleep(1);
			continue;
		}
		
		if(timeout > 85 && timeout % 5 == 0)
		{
#ifdef FREQUENCY_OFFSET
			RF_HeartCheck(addr, index, g_gatewayinfo.netch);
#else
			RF_HeartCheck(addr, g_gatewayinfo.netch);
#endif
		}

		usleep(RF_CMD_HEART_TIME);
		//sleep(1);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: heart check timeout, rfaddr = %x\n", FUN, LINE, g_pair[channel].addr);
			return -1;
		}
	}

	if(g_camerainfo[channel].heart_status == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int VAVAHAL_TfSet(int channel, unsigned char status)
{
	int timeout;
	unsigned int addr;
#ifdef FREQUENCY_OFFSET
	unsigned int index;
#endif

	//获取短波地址
	addr = g_pair[channel].addr;
#ifdef FREQUENCY_OFFSET
	index = g_pair[channel].index;
#endif

	g_camerainfo[channel].tfset_status = 1;

	timeout = 40;

	while(g_running && g_camerainfo[channel].tfset_status)
	{
		if(timeout > 25 && timeout % 5 == 0)
		{
#ifdef FREQUENCY_OFFSET
			RF_TFSet(addr, index, status);
#else
			RF_TFSet(addr, status);
#endif
		}

		usleep(RF_CMD_SLEEP_TIME);

		if(timeout-- <= 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: tf set timeout, channel = %d\n", FUN, LINE, channel);
			return -1;
		}
	}

	return 0;
}

int VAVAHAL_ConnectAp(char *ssid, char *pass)
{
	char str[128];

	if(ssid == NULL || pass == NULL)
	{
		return -1;
	}

	if(strlen(ssid) < 1 || strlen(ssid) > 32 || strlen(pass) < 6 || strlen(pass) > 32)
	{
		return -1;
	}

	memset(str, 0, 128);
	sprintf(str, "WifiConnetAP -n %s -p %s", ssid, pass);
	return VAVAHAL_SystemCmd(str);
}

int VAVAHAL_GetWireleseStatus()
{
	FILE *fd;
	char str[128];
	
	VAVAHAL_SystemCmd("/bin/cat /proc/net/arp | awk '/apcli0/{print $3}' | awk 'NR==1{print$1}' > /tmp/net_status");
	VAVAHAL_SystemCmd("/bin/sync");

	sleep(1);

	fd = fopen("/tmp/net_status", "r");
	if(fd == NULL)
	{
		return -1;
	}

	memset(str, 0, 128);
	fgets(str, 128, fd);
	fclose(fd);

	str[3] = '\0';

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: str = [%s]\n", FUN, LINE, str);

	if(strcmp(str, "0x0") == 0)
	{
		return 0x300;
	}
	else if(strcmp(str, "0x1") == 0)
	{
		return 0x400;
	}
	else if(strcmp(str, "0x2") == 0)
	{
		return 0x500;
	}

	return 0x300;
}

int VAVAHAL_PingStatus()
{
	FILE *fd = NULL;
	char str[128];

	//检测谷歌
	pthread_mutex_lock(&mutex_ping_lock);
	VAVAHAL_SystemCmd("/bin/ping www.google.com -c 3 > /tmp/pingstatus &");
	sleep(5);
	VAVAHAL_SystemCmd_Enx("killall -9 ping");
	pthread_mutex_unlock(&mutex_ping_lock);
	
	fd = fopen("/tmp/pingstatus", "r");
	if(fd == NULL)
	{
		return 0x300;
	}

	memset(str, 0, 128);
	fgets(str, 128, fd);
	memset(str, 0, 128);
	fgets(str, 128, fd);
	fclose(fd);

	if(strlen(str) > 0)
	{
		str[strlen(str) - 1] = '\0';
	}
	else
	{
		str[0] = '\0';		
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: str = [%s]\n", FUN, LINE, str);

	if(strlen(str) > 0)
	{
		if((strstr(str, "64 bytes from") != NULL) 
		&& (strstr(str, "seq=") != NULL)
		&& (strstr(str, "time=") != NULL))
		{
			return 0x500;
		}
	}

	//检测百度
	pthread_mutex_lock(&mutex_ping_lock);
	VAVAHAL_SystemCmd("/bin/ping www.baidu.com -c 3 > /tmp/pingstatus &");
	sleep(5);
	VAVAHAL_SystemCmd_Enx("killall -9 ping");
	pthread_mutex_unlock(&mutex_ping_lock);
	
	fd = fopen("/tmp/pingstatus", "r");
	if(fd == NULL)
	{
		return 0x300;
	}

	memset(str, 0, 128);
	fgets(str, 128, fd);
	memset(str, 0, 128);
	fgets(str, 128, fd);
	fclose(fd);

	if(strlen(str) > 0)
	{
		str[strlen(str) - 1] = '\0';
	}
	else
	{
		str[0] = '\0';		
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: str = [%s]\n", FUN, LINE, str);

	if(strlen(str) > 0)
	{
		if((strstr(str, "64 bytes from") != NULL) 
		&& (strstr(str, "seq=") != NULL)
		&& (strstr(str, "time=") != NULL))
		{
			return 0x500;
		}
	}
	
	return 0x300;
}

int VAVAHAL_CheckServerStatus()
{
	int status;
	int result = 0x300;
	int timeout = 2;

	while(g_running)
	{
		if(timeout <= 0)
		{
			result = 0x300;
			break;
		}
		
		status = VAVASERVER_CheckServerStatus();
		if(status == -1)
		{
			result = 0x300;
			break;
		}
		else if(status == 0)
		{
			result = 0x500;
			break;
		}

		timeout--;

		sleep(1);
	}
	
	return result;
}

int VAVAHAL_GetNetChannel()
{
	int ret;
	char netch[32];

	ret = VAVAHAL_SystemCmd_Ex("iwinfo ra0 info | grep -E Channel | awk -F \" \" \'{print $4}\'", netch, 32);
	if(ret != 0)
	{
		return -1;
	}
	netch[strlen(netch) - 1] = '\0';
	g_gatewayinfo.netch = atoi(netch);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: netch = %d\n", FUN, LINE, g_gatewayinfo.netch);
	
	return 0;
}

int VAVAHAL_GetSessionNum(int *sessionnum)
{
	int i;
	int session = 0;
	
	if(sessionnum == NULL)
	{
		return -1;
	}

	for(i = 0; i < MAX_SESSION_NUM; i++)
	{
		if(g_session[i].id != -1)
		{
			session++;
		}
	}

	*sessionnum = session;

	return 0;
}

int VAVAHAL_GetChannel_ConnectNum(int channel, int *wakeupnum, int *videonum)
{
	int i;
	int wakeup = 0;
	int video = 0;

	for(i = 0; i < MAX_SESSION_NUM; i++)
	{
		if(g_session[i].id != -1 && g_session[i].camerachannel == channel)
		{
			if(g_session[i].wakeupstatus == 1)
			{
				wakeup++;
			}

			if(g_session[i].videostatus == 1)
			{
				video++;
			}
		}
	}

	*wakeupnum = wakeup;
	*videonum = video;

	return 0;
}

void VAVAHAL_PrintOnline()
{
	int i;
	char tmpstr[5];
	char tmpbuff[50];

	memset(tmpbuff, 0, 50);
	strcpy(tmpbuff, "CameraOnlie: ");

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			memset(tmpstr, 0, 3);
			sprintf(tmpstr, "[%d] ", g_camerainfo_dynamic[i].online);
		}
		else
		{
			memset(tmpstr, 0, 3);
			strcpy(tmpstr, "[x] ");
		}

		strcat(tmpbuff, tmpstr);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);

	memset(tmpbuff, 0, 50);
	strcpy(tmpbuff, "WifiHeart: ");

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			memset(tmpstr, 0, 3);
			sprintf(tmpstr, "[%d] ", g_camerainfo[i].wifi_heart);
		}
		else
		{
			memset(tmpstr, 0, 3);
			strcpy(tmpstr, "[x] ");
		}

		strcat(tmpbuff, tmpstr);
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);

	memset(tmpbuff, 0, 50);
	strcpy(tmpbuff, "PowerOff: ");

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			memset(tmpstr, 0, 3);
			sprintf(tmpstr, "[%d] ", g_camerainfo[i].poweroff_flag);
		}
		else
		{
			memset(tmpstr, 0, 3);
			strcpy(tmpstr, "[x] ");
		}

		strcat(tmpbuff, tmpstr);
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: pair mode = %d\n", FUN, LINE, g_pairmode);
	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);

	return;
}

void VAVAHAL_PrintPush()
{
	int i;
	char tmpstr[5];
	char tmpbuff[50];

	memset(tmpbuff, 0, 50);
	strcpy(tmpbuff, "PushConf: ");

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			memset(tmpstr, 0, 3);
			sprintf(tmpstr, "[%d] ", g_pushflag[i].push);
		}
		else
		{
			memset(tmpstr, 0, 3);
			strcpy(tmpstr, "[x] ");
		}

		strcat(tmpbuff, tmpstr);
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);
	
	return;
}

void VAVAHAL_PrintEmail()
{
	int i;

	char tmpstr[5];
	char tmpbuff[50];

	memset(tmpbuff, 0, 50);
	strcpy(tmpbuff, "EmailConf: ");

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			memset(tmpstr, 0, 3);
			sprintf(tmpstr, "[%d] ", g_pushflag[i].email);
		}
		else
		{
			memset(tmpstr, 0, 3);
			strcpy(tmpstr, "[x] ");
		}

		strcat(tmpbuff, tmpstr);
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);

	return;
}

void VAVAHAL_GetApStatus(char *ssid)
{
	FILE *fd = NULL;
	int timeout = 30;
	char tmp_1[128];
	char tmp_2[128];
	int i, j;

	while(g_running)
	{
		sleep(1);

		if(timeout-- <= 0)
		{
			break;
		}

		if(timeout % 3 == 0)
		{
			VAVAHAL_SystemCmd("iwconfig apcli0 | awk '/ESSID/{print $4}' | awk -F : '{print $2}' > /tmp/apstatus");

			fd = fopen("/tmp/apstatus", "r");
			if(fd != NULL)
			{
				memset(tmp_1, 0, 128);
				memset(tmp_2, 0, 128);
				
				fgets(tmp_1, 128, fd);
				fclose(fd);

				tmp_1[strlen(tmp_1) - 1] = '\0';
				
				//去掉""
				for(i = 0, j = 0; i < strlen(tmp_1); i++)
				{
					if(tmp_1[i] != '"')
					{
						tmp_2[j] = tmp_1[i];
						j++;
					}
				}

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get ap status [%s]\n", FUN, LINE, tmp_2);

				if(strcmp(tmp_2, ssid) == 0)
				{
					break;
				}
			}
		}
	}
}

int VAVAHAL_PirAlarmNoTF(int channel)
{
	//struct timeval t_current;

	if(g_cloudflag[channel] == 1 && g_cloudrec[channel].status == VAVA_REC_STATUS_IDLE)
	{
		VAVAHAL_StartAlarmCloud(channel);
	}
	#if 0 //无卡无云存暂时不推送
	else
	{
		gettimeofday(&t_current, NULL);
		if(t_current.tv_sec - g_salarm[channel].tv_sec > g_rectime.alarm_time)
		{
			//推送文本消息
			VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_TEXT, "", "", "", 0, t_current.tv_sec);
			g_salarm[channel].tv_sec = t_current.tv_sec;
		}
	}
	#endif

	return 0;
}

int VAVAHAL_PirAlarm(int channel)
{
	//struct timeval t_current;
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Pir Alarm, channel = %d, sdstatus = %d, format_flag = %d, sdtotol = %d, recstatus = %d\n",
	                                FUN, LINE, channel, g_gatewayinfo.sdstatus, g_gatewayinfo.format_flag, g_gatewayinfo.totol, 
		                            g_alarmrec[channel].status);

	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD && g_gatewayinfo.format_flag == 0 && g_gatewayinfo.totol > 100
		&& g_alarmrec[channel].status == VAVA_REC_STATUS_IDLE)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========> 1 [channel - %d]\n", FUN, LINE, channel);
		
		VAVAHAL_StartAlarmRec(channel);
		g_salarm[channel].tv_sec = 0;
	}
	else
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========> 2 [channel - %d]\n", FUN, LINE, channel);
		
		if(g_cloudflag[channel] == 1 && g_cloudrec[channel].status == VAVA_REC_STATUS_IDLE)
		{
			VAVAHAL_StartAlarmCloud(channel);
		}
		#if 0 //无卡无云存暂时不推送
		else
		{
			gettimeofday(&t_current, NULL);
			if(t_current.tv_sec - g_salarm[channel].tv_sec > g_rectime.alarm_time)
			{
				//推送文本消息
				VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_TEXT, "", "", "", 0, t_current.tv_sec);
				g_salarm[channel].tv_sec = t_current.tv_sec;
			}
		}
		#endif
	}

	return 0;
}

int VAVAHAL_PushAlarm(int channel, int pushtype, int pushtime, int ntsamp, int pushfiletype, 
                      char *dirname, char *filename, char *msg)
{
	int ret;

	ret = VAVASERVER_GetToken();
	if(ret != 0)
	{
		return -1;
	}

	ret = VAVASERVER_PushAlarm(channel, pushtype, pushtime, ntsamp, pushfiletype, dirname, filename, msg);
	if(ret == 0)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: push success, channel = %d, pushtype = %d, pushfile = %d\n", 
		                                FUN, LINE, channel, pushtype, pushfiletype);
	}
	else
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: push fail, channel = %d, pushtype = %d, pushfile = %d\n", 
		                                FUN, LINE, channel, pushtype, pushfiletype);
	}

	return ret;
}

int VAVAHAL_CloudPush(int channel, int ntsamp)
{
	int ntp;
	
	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD && g_gatewayinfo.format_flag == 0 && g_gatewayinfo.totol > 100)
	{
		return 0;
	}

	//UTC时间恢复为正常时间
	ntp = VAVAHAL_GetNtp();

	//推送文本消息
	VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_FW, VAVA_PUSH_FILE_TYPE_TEXT, "", "", "", 0, ntsamp + (ntp * 3600));
	
	return 0;
}

int VAVAHAL_ResetGateWay()
{
	int i;

#ifdef NAS_NFS
	//关闭NAS
	Nas_stop();
#endif
	
	//关闭录像
	VAVAHAL_CloseRec(1, 0);

	//关闭云存录像
	VAVAHAL_CloseCloudRec(1, 0);

	sleep(1);

	//停止录像
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		VAVAHAL_StopManualRec(i);
		VAVAHAL_StopFullTimeRec(i);
		VAVAHAL_StopAlarmRec(i);
		VAVAHAL_StopAlarmCloud(i);
	}

	//关闭看门狗
	VAVAHAL_SystemCmd("vava-dogctrl -e 0");

	VAVAHAL_SystemCmd("/bin/umount -l /mnt/sd0");
	VAVAHAL_SystemCmd("/bin/umount -l /mnt/nas");

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			//休眠摄像机
			VAVAHAL_SleepCamera(i);
		}
	}

	//清除时区信息
	VAVAHAL_ClearNtp();

	VAVAHAL_SystemCmd("system_resume");
	VAVAHAL_SystemCmd("reboot");

	return 0;
}

int VAVAHAL_ResetCamera(int channel)
{
	if(g_pir_sensitivity[channel] != VAVA_PIR_SENSITIVITY_MIDDLE)
	{
		if(g_pir_sensitivity[channel] != VAVA_PIR_SENSITIVITY_OFF)	
		{
			VAVAHAL_SetPirStatus(channel, VAVA_PIR_SENSITIVITY_MIDDLE);
		}
		
		g_pir_sensitivity[channel] = VAVA_PIR_SENSITIVITY_MIDDLE;
		VAVAHAL_WritePirSensitivity();
	}

	g_camerainfo[channel].v_quality = VAVA_VIDEO_QUALITY_AUTO;
	VAVAHAL_WriteVideoQuality();

	//清除布防信息
	VAVAHAL_ClearArmInfo_v1(channel);

	//清除摄像机推送配置
	g_pushflag[channel].push = 1;
	g_pushflag[channel].email = 0;
	VAVAHAL_WritePushConfig();
	VAVAHAL_WriteEmailConfig();

	return 0;
}

int VAVAHAL_GetLocalIP(char *ipstr)
{
	int sockfd;
	struct sockaddr_in *sin;
	struct ifreq if_ra0;

	if(ipstr == NULL)
	{
		return -1;
	}

	strcpy(if_ra0.ifr_name, WIRED_NET);	

	sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	if(sockfd >= 0)
	{
		if(ioctl(sockfd, SIOCGIFADDR, &if_ra0) >= 0)
		{
			sin = (struct sockaddr_in *)&(if_ra0.ifr_addr);
			strcpy(ipstr, inet_ntoa(sin->sin_addr));
			
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get ip = %s\n", FUN, LINE, ipstr);
		}
		
		close(sockfd);
	}

	return 0;
}

int VAVAHAL_PingIPStatus(char *ip)
{
	FILE *fd = NULL;
	char str[128];
	
	memset(str, 0, 128);
	sprintf(str, "/bin/ping %s -c 3 > /tmp/ipstatus &", ip);
	pthread_mutex_lock(&mutex_ping_lock);
	VAVAHAL_SystemCmd(str);
	sleep(5);
	VAVAHAL_SystemCmd_Enx("killall -9 ping");
	pthread_mutex_unlock(&mutex_ping_lock);

	fd = fopen("/tmp/ipstatus", "r");
	if(fd == NULL)
	{
		return -1;
	}

	memset(str, 0, 128);
	fgets(str, 128, fd);
	memset(str, 0, 128);
	fgets(str, 128, fd);
	fclose(fd);

	if(strlen(str) > 0)
	{
		str[strlen(str) - 1] = '\0';
	}
	else
	{
		str[0] = '\0';		
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: str = [%s]\n", FUN, LINE, str);

	if(strlen(str) > 0)
	{
		if((strstr(str, "64 bytes from") != NULL) 
		&& (strstr(str, "seq=") != NULL)
		&& (strstr(str, "time=") != NULL))
		{
			return 0;
		}
	}

	return -1;
}

int VAVAHAL_CheckSysIdle()
{
	int i;
	
	//检查录像
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 0)
		{
			continue;
		}

		if(g_alarmrec[i].ctrl == 1 || g_manaulrec[i].ctrl == 1)
		{
			return -1;
		}
	}

	//检查视频流传输
	for(i= 0; i < MAX_SESSION_NUM; i++)
	{
		if(g_session[i].id == -1)
		{
			continue;
		}

		if(g_session[i].camerachannel != -1 && g_session[i].videostatus == 1)
		{
			return -1;
		}
	}

	return 0;
}

int VAVAHAL_CheckNasDir(char *dirname)
{
	DIR *dirp = NULL;
	FILE *fd = NULL;
	char str[64];
	char cmd[128];

	VAVA_NasSync nassync;

	//检测文件夹是否存在
	memset(str, 0, 64);
	sprintf(str, "/mnt/nas/%s/%s", g_gatewayinfo.sn, dirname);

	dirp = opendir(str);
	if(dirp == NULL)
	{
		//创建文件夹
		memset(cmd, 0, 128);
		sprintf(cmd, "/bin/mkdir -p /mnt/nas/%s/%s", g_gatewayinfo.sn, dirname);
		VAVAHAL_SystemCmd(cmd);

		//创建同步文件
		memset(str, 0, 64);
		sprintf(str, "/mnt/nas/%s/%s/%s", g_gatewayinfo.sn, dirname, NAS_SYNC_FILE);
		fd = fopen(str, "wb");
		if(fd != NULL)
		{
			memset(&nassync, 0, sizeof(VAVA_NasSync));
			nassync.tag = VAVA_SYNC_HEAD;
			nassync.sync = 0;

			fwrite(&nassync, sizeof(VAVA_NasSync), 1, fd);
			fclose(fd);
		}
		else
		{
			return -1;
		}
	}
	else
	{
		closedir(dirp);

		memset(str, 0, 64);
		sprintf(str, "/mnt/nas/%s/%s/%s", g_gatewayinfo.sn, dirname, NAS_SYNC_FILE);
		fd = fopen(str, "r");
		if(fd == NULL)
		{
			fd = fopen(str, "wb");
			if(fd == NULL)
			{
				return -1;
			}
			
			memset(&nassync, 0, sizeof(VAVA_NasSync));
			nassync.tag = VAVA_SYNC_HEAD;
			nassync.sync = 0;

			fwrite(&nassync, sizeof(VAVA_NasSync), 1, fd);
			fclose(fd);
		}
		else
		{
			memset(&nassync, 0, sizeof(VAVA_NasSync));
			fread(&nassync, sizeof(VAVA_NasSync), 1, fd);
			fclose(fd);

			if(nassync.tag == VAVA_SYNC_HEAD)
			{
				if(nassync.sync == 1)
				{
					return 1;
				}
			}
		}
	}

	return 0;
}

int VAVAHAL_UpdateNasDir(char *dirname)
{
	FILE *fd = NULL;
	VAVA_NasSync nassync;
	char str[64];
	char currentdata[9];

	time_t tt;
	struct tm *timeinfo;

	time(&tt);
	timeinfo = localtime(&tt);

	//忽略当前日期
	memset(currentdata, 0, 9);
	sprintf(currentdata, "%d%02d%02d", 1900 + timeinfo->tm_year, timeinfo->tm_mon + 1, timeinfo->tm_mday);
	//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: currentdata = %s, dirname = %s\n\n", FUN, LINE, currentdata, dirname);
	if(strcmp(currentdata, dirname) == 0)
	{
		return 0;
	}

	memset(str, 0, 64);
	sprintf(str, "/mnt/nas/%s/%s/%s", g_gatewayinfo.sn, dirname, NAS_SYNC_FILE);
	fd = fopen(str, "wb");
	if(fd != NULL)
	{
		memset(&nassync, 0, sizeof(VAVA_NasSync));
		nassync.tag = VAVA_SYNC_HEAD;
		nassync.sync = 1;

		fwrite(&nassync, sizeof(VAVA_NasSync), 1, fd);
		fclose(fd);
	}

	return 0;
}

int VAVAHAL_BuildNasPath(char *path, char *dirname, char *filename)
{
	DIR *dirp = NULL;
	
	char tmpch[2];
	char tmptype[2];
	char tmpfilename[7];

	int channel;
	int type;
	char type_str[8];
	char cmd[128];
	char dirpath[128];

	memset(tmpfilename, 0, 7);
	memcpy(tmpfilename, filename, 6);

	memset(tmptype, 0, 2);
	memcpy(tmptype, filename + 7, 1);

	memset(tmpch, 0, 2);
	memcpy(tmpch, filename + 9, 1);

	memset(type_str, 0, 8);
	type = atoi(tmptype);
	if(type == 1)
	{
		strcpy(type_str, "motion");
	}
	else if(type == 3)
	{
		strcpy(type_str, "user");
	}

	channel = atoi(tmpch);
	if(g_pair[channel].nstat == 0)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: channel not pair, channel = %d\n", FUN, LINE, channel);
		return -1;
	}

	sprintf(path, "/mnt/nas/%s/%s/%s/%s_%s.mp4", g_gatewayinfo.sn, dirname, g_pair[channel].id, tmpfilename, type_str);

	memset(dirpath, 0, 128);
	sprintf(dirpath, "/mnt/nas/%s/%s/%s", g_gatewayinfo.sn, dirname, g_pair[channel].id);

	dirp = opendir(dirpath);
	if(dirp == NULL)
	{
		//创建文件夹
		memset(cmd, 0, 128);
		sprintf(cmd, "/bin/mkdir -p %s", dirpath);
		VAVAHAL_SystemCmd(cmd);
	}
	else
	{
		closedir(dirp);
	}

	return 0;
}

int VAVAHAL_UpgradeRf()
{
	//关闭串口
	RF_StopUart();

	sleep(2);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: GPIO BOOT HIGH\n", FUN, LINE);
	
	//拉高gpio
	VAVAHAL_SystemCmd("echo 1 > /sys/devices/gpio-leds/leds/vava:orange:boot/brightness");

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: GPIO RESET LOW\n", FUN, LINE);

 	//复位gpio接低再拉高 让短波进行loader模式
 	VAVAHAL_SystemCmd("echo 1 > /sys/devices/gpio-leds/leds/vava:orange:reset/brightness");
	sleep(1);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: GPIO RESET HIGH\n", FUN, LINE);
	
 	VAVAHAL_SystemCmd("echo 0 > /sys/devices/gpio-leds/leds/vava:orange:reset/brightness");
	sleep(1);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Begin Upgrade\n", FUN, LINE);

	return vStartCC1310Upgrade(); //bertream接口
}

int VAVAHAL_ResetRf()
{
	//拉低gpio
	VAVAHAL_SystemCmd("echo 0 > /sys/devices/gpio-leds/leds/vava:orange:boot/brightness");
	sleep(1);
	//复位gpio接低再拉高 让短波进行loader模式
 	VAVAHAL_SystemCmd("echo 1 > /sys/devices/gpio-leds/leds/vava:orange:reset/brightness");
	sleep(1);
 	VAVAHAL_SystemCmd("echo 0 > /sys/devices/gpio-leds/leds/vava:orange:reset/brightness");
	sleep(1);

	return 0;
}

int VAVAHAL_CreateUpgradingTimeout(int upnum)
{
	int ret;

	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, UpgringTimeout_pth, &upnum);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int VAVAHAL_SystemCmd(char *str)
{
	int ret;
	int timeout = 0;

	while(g_running)
	{
		if(timeout++ >= 3)
		{
			break;
		}

		ret = system(str);
		if(ret == 0)
		{
			break;
		}
		else
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: ret = %d, errno = %d\n", FUN, LINE, ret, errno);
		}

		usleep(100000);
	}
	
	if(timeout >= 3)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: system called fail, [%s]\n", FUN, LINE, str);
		return -1;
	}

	return 0;
}

int VAVAHAL_SystemCmd_Ex(char *cmd, char *buff, int len)
{
	FILE* fp = NULL;

	if(cmd == NULL)
	{
		return -1;
	}

	fp = popen(cmd, "r");
	memset(buff, 0, len);
    fgets(buff, len, fp);
 	pclose(fp);

	if(strlen(buff) <= 0)
	{
		return -1;
	}

    return 0;
}

int VAVAHAL_SystemCmd_Enx(char *cmd)
{
	FILE* fp = NULL;
	
	if(cmd == NULL)
	{
		return -1;
	}

	fp = popen(cmd, "r");
	pclose(fp);

	return 0;
}

int VAVAHAL_RefreshCloud()
{
	int i;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_cloudflag[i] == 0)
		{
			g_cloudrecheck[i] = 1;
		}
	}
	
	return 0;
}

int VAVAHAL_GetCameraOnlineStatus(int channel)
{
	int online;

	online = g_camerainfo_dynamic[channel].online;
	if(g_camerainfo[channel].wifi_heart == 1 
		|| (g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo_dynamic[channel].online == 1))
	{
		online = 1;
	}
	else if(g_camerainfo[channel].first_flag == 1 || (g_camerainfo_dynamic[channel].battery == 0 && g_camerainfo[channel].powermode == 0))
	{
		online = 0;
	}

	return online;
}

int VAVAHAL_CheckVideoFrame(int channel, char *frame, int type, char *str)
{
	if(frame == NULL)
	{
		return -1;
	}
	
	if(frame[0] != 0 || frame[1] != 0 || frame[2] != 0 || frame[3] != 1)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [%s] check frame head fail, [%x %x %x %x]\n", 
		                                 FUN, LINE, str, frame[0], frame[1], frame[2], frame[3]);
	}

	if(type == 1)
	{
		if(g_camerainfo[channel].videocodec == 0)
		{
			if((frame[4] & 0x1F) != 7)
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [%s] check sps fail [%d]\n", FUN, LINE, str, frame[4] & 0x1F);
			}
		}
		else
		{
			if((frame[4] & 0x7E) >> 1 != 32)
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [%s] check vps fail [%d]\n", FUN, LINE, str, (frame[4] & 0x7E) >> 1);
			}
		}
	}
	else
	{
		if(g_camerainfo[channel].videocodec == 0)
		{
			if((frame[4] & 0x1F) != 1)
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [%s] check Pframe fail [%d]\n", FUN, LINE, str, frame[4] & 0x1F);
			}
		}
		else
		{
			if((frame[4] & 0x7E) >> 1 != 1)
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [%s] check vps fail [%d]\n", FUN, LINE, str, (frame[4] & 0x7E) >> 1);
			}
		}
	}

	return 0;
}

int VAVAHAL_FreeAvMem()
{
	int i;
	
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_avmemchace[i].pmvMemBegin != NULL)
		{
			free(g_avmemchace[i].pmvMemBegin);
			g_avmemchace[i].pmvMemBegin = NULL;
		}

		if(g_avmemchace[i].psvMemBegin != NULL)
		{
			free(g_avmemchace[i].psvMemBegin);
			g_avmemchace[i].psvMemBegin = NULL;
		}

		if(g_avmemchace[i].papMemBegin != NULL)
		{
			free(g_avmemchace[i].papMemBegin);
			g_avmemchace[i].papMemBegin = NULL;
		}
	}

	return 0;
}

int VAVAHAL_StopAvServer()
{
	g_avstop = 1;
	
	return 0;
}

#ifdef FREQUENCY_OFFSET
int VAVAHAL_GetPairFreeIndex()
{
	int i;
	unsigned int random;
	struct timeval randomtime;

	gettimeofday(&randomtime, NULL);
	srand((unsigned int)randomtime.tv_usec); //用时间做种，每次产生随机数不一样
	random = rand() % 16 + 1; //生成1-16随机数
		
	while(1)
	{
		for(i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			if(random == g_pair[i].index)
			{
				random++;
				if(random == 17)
				{
					random = 1;
				}

				break;
			}
		}

		if(i >= MAX_CHANNEL_NUM)
		{
			break;
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get free index = %d\n", FUN, LINE, random);

	return random;
}
#endif

int VAVAHAL_ResetUpServer()
{
	int ret;

	ret = VAVAHAL_SystemCmd("killall Ppcs_tfupdate");
	return ret;
}

int VAVAHAL_PrintUnformatted(char *str, char *buff)
{
	int i;

	if(str == NULL)
	{
		return -1;
	}

	for(i = 0; i < strlen(str); i++)
	{
		if(str[i] == '\0' || str[i] == '\r' || str[i] == '\n')
		{
			break;
		}
	}

	memcpy(buff, str, i);
	buff[i] = '\0';
	
	return 0;
}

int VAVAHAL_FindFirstIFrame(char *buff, int size)
{
	VAVA_RecHead *head;
	int tmpsize = 0;
	int iflag = 0;
	
	if(buff == NULL || size == 0)
	{
		return -1;
	}

	while(g_running)
	{
		if(tmpsize >= size)
		{
			return -1;
		}

		head = (VAVA_RecHead *)buff;
		if(head->tag != 0xEB0000AA)
		{
			return -1;
		}
		
		if(head->type == 1)
		{
			iflag++;

			if(iflag >= 2)
			{
				break;
			}
		}

		tmpsize += (sizeof(VAVA_RecHead) + head->size);
	}

	return tmpsize;
}

int VAVAHAL_SendUpStats(int sessionid, int status, int loading, int channel)
{
	int ret;
	int sendsize;
	char sendbuff[1024];

	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;

	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	ret = VAVAHAL_CheckSession(sessionid);
	if(ret == 0)
	{
		pJsonRoot = cJSON_CreateObject();
		if(pJsonRoot == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
			
			Err_Log("json malloc fail");
			g_running = 0;
			return 0;
		}

		cJSON_AddNumberToObject(pJsonRoot, "status", status);
		cJSON_AddNumberToObject(pJsonRoot, "type", g_update.type);

		if(g_update.type == VAVA_UPDATE_TYPE_CAMERA)
		{
			cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
		}

		if(status == VAVA_UPDATE_LOADING || status == VAVA_UPDATE_TRANSMITTING || status == VAVA_UPDATE_UPGRADING
			|| status == VAVA_UPDATE_SUCCESS || status == VAVA_UPDATE_FAIL || status == VAVA_UPDATE_TIMEOUT)
		{
			cJSON_AddNumberToObject(pJsonRoot, "loaddata", loading);
		}
			
		pstr = cJSON_PrintUnformatted(pJsonRoot);

		memset(sendbuff, 0, 1024);
		vavahead = (VAVA_CMD_HEAD *)sendbuff;
		vavahead->sync_code = VAVA_TAG_APP_CMD;
		vavahead->cmd_code = VAVA_CMD_UPGRATE_STATUS;
		vavahead->cmd_length = strlen(pstr);

		memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
		sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

		free(pstr);
		cJSON_Delete(pJsonRoot);
			
		pthread_mutex_lock(&mutex_session_lock);
		ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
		if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
		{
			PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
		}
		pthread_mutex_unlock(&mutex_session_lock);
	}
	else
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: session is closed\n", FUN, LINE);
	}

	return 0;
}

int VAVAHAL_SendDevInfo(int sessionid, int session_channel)
{
	int i;
	int ret;
	unsigned int checksize;
	int cameranum;
	int sessionnum;
	int wakeupnum;
	int videonum;
	int sendsize;
	char sendbuff[2048];

	VAVA_CMD_HEAD *vavahead;

	cJSON *pJsonRoot = NULL;
	cJSON *pCameralist = NULL;
	cJSON *pCameraitem[MAX_CHANNEL_NUM];
	char *pstr = NULL;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}
	
	pCameralist = cJSON_CreateArray();
	if(pCameralist == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cameranum = 0;
		
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			cameranum++;

			pCameraitem[i] = cJSON_CreateObject();
			if(pCameraitem[i] == NULL)
			{
				cJSON_Delete(pCameralist);
				cJSON_Delete(pJsonRoot);

				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
				
				Err_Log("json malloc fail");
				g_running = 0;
				return -1;
			}

			cJSON_AddStringToObject(pCameraitem[i], "sn", g_pair[i].id);
			cJSON_AddNumberToObject(pCameraitem[i], "channel", i);
			cJSON_AddNumberToObject(pCameraitem[i], "online", VAVAHAL_GetCameraOnlineStatus(i));
			cJSON_AddNumberToObject(pCameraitem[i], "signal", g_camerainfo_dynamic[i].signal);
			cJSON_AddNumberToObject(pCameraitem[i], "lever", g_camerainfo_dynamic[i].battery);
			cJSON_AddNumberToObject(pCameraitem[i], "voltage", g_camerainfo_dynamic[i].voltage);
			#ifdef BATTEY_INFO
			cJSON_AddNumberToObject(pCameraitem[i], "temperature", g_camerainfo_dynamic[i].temperature);
			cJSON_AddNumberToObject(pCameraitem[i], "electricity", g_camerainfo_dynamic[i].electricity);
			#endif
			cJSON_AddNumberToObject(pCameraitem[i], "powermode", g_camerainfo[i].powermode);
			cJSON_AddNumberToObject(pCameraitem[i], "armingstatus", g_camera_arminfo_v1[i].status);
			cJSON_AddNumberToObject(pCameraitem[i], "armingtype", g_camera_arminfo_v1[i].type);
			cJSON_AddNumberToObject(pCameraitem[i], "cloud", g_cloudflag[i]);
			VAVAHAL_GetChannel_ConnectNum(i, &wakeupnum, &videonum);
			cJSON_AddNumberToObject(pCameraitem[i], "wakeup", wakeupnum);
			cJSON_AddNumberToObject(pCameraitem[i], "video", videonum);
			if(g_update.status != VAVA_UPDATE_IDLE && g_update.type == VAVA_UPDATE_TYPE_CAMERA && g_update.current == i)
			{
				cJSON_AddNumberToObject(pCameraitem[i], "upstatus", g_update.status);
				if(g_update.status == VAVA_UPDATE_LOADING)
				{
					cJSON_AddNumberToObject(pCameraitem[i], "loaddata", g_update.loading);
				}
			}
			else
			{
				cJSON_AddNumberToObject(pCameraitem[i], "upstatus", VAVA_UPDATE_IDLE);
			}

			cJSON_AddItemToArray(pCameralist, pCameraitem[i]);
		}
	}

	cJSON_AddStringToObject(pJsonRoot, "sn", g_gatewayinfo.sn);

	#if 1
	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_NOCRAD)
	{
		cJSON_AddNumberToObject(pJsonRoot, "sdstatus", g_gatewayinfo.sdstatus);
		cJSON_AddNumberToObject(pJsonRoot, "totolsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "usedsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "freesize", 0);
	}
	else if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.sdstatus == VAVA_SD_STATUS_FULL)
	{
		if(g_gatewayinfo.format_flag == 1)
		{
			cJSON_AddNumberToObject(pJsonRoot, "sdstatus", VAVA_SD_STATUS_NEEDFORMAT);
		}
		else
		{
			cJSON_AddNumberToObject(pJsonRoot, "sdstatus", g_gatewayinfo.sdstatus);
		}

		cJSON_AddNumberToObject(pJsonRoot, "totolsize", g_gatewayinfo.totol);
		cJSON_AddNumberToObject(pJsonRoot, "usedsize", g_gatewayinfo.used);
		cJSON_AddNumberToObject(pJsonRoot, "freesize", g_gatewayinfo.free);
	}
	#else
	cJSON_AddNumberToObject(pJsonRoot, "sdstatus", g_gatewayinfo.sdstatus);
	cJSON_AddNumberToObject(pJsonRoot, "totolsize", g_gatewayinfo.totol);
	cJSON_AddNumberToObject(pJsonRoot, "usedsize", g_gatewayinfo.used);
	cJSON_AddNumberToObject(pJsonRoot, "freesize", g_gatewayinfo.free);
	#endif

	#ifdef NAS_NFS
	cJSON_AddNumberToObject(pJsonRoot, "nasstatus", g_nas_status);
	if(g_nas_status == VAVA_NAS_STATUS_SYNC || g_nas_status == VAVA_NAS_STATUS_LACKOF_SPACE)
	{
		cJSON_AddNumberToObject(pJsonRoot, "nas_totolsize", g_gatewayinfo.nas_totol);
		cJSON_AddNumberToObject(pJsonRoot, "nas_usedsize", g_gatewayinfo.nas_used);
		cJSON_AddNumberToObject(pJsonRoot, "nas_freesize", g_gatewayinfo.nas_free);
	}
	else
	{
		cJSON_AddNumberToObject(pJsonRoot, "nas_totolsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "nas_usedsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "nas_freesize", 0);
	}
	#endif
	
	if(g_update.status != VAVA_UPDATE_IDLE && g_update.type == VAVA_UPDATE_TYPE_GATEWAY)
	{
		cJSON_AddNumberToObject(pJsonRoot, "upstatus", g_update.status);
		if(g_update.status == VAVA_UPDATE_LOADING)
		{
			cJSON_AddNumberToObject(pJsonRoot, "loaddata", g_update.loading);
		}
	}
	else
	{
		cJSON_AddNumberToObject(pJsonRoot, "upstatus", VAVA_UPDATE_IDLE);
	}
	
	VAVAHAL_GetSessionNum(&sessionnum);

	cJSON_AddNumberToObject(pJsonRoot, "session", sessionnum);
	cJSON_AddNumberToObject(pJsonRoot, "sendmode", g_session[session_channel].sendmode);
	//cJSON_AddNumberToObject(pJsonRoot, "signal", g_gatewayinfo.signal);
	cJSON_AddNumberToObject(pJsonRoot, "buzzer", g_buzzer_flag);

	cJSON_AddNumberToObject(pJsonRoot, "camera", cameranum);
	cJSON_AddItemToObject(pJsonRoot, "camerainfo", pCameralist);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	memset(sendbuff, 0, 2048);
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_DEV_INFO;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);

	free(pstr);
	cJSON_Delete(pJsonRoot);
		
	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendPaseErr(int sessionid)
{
	int ret;
	unsigned int checksize;
	int sendsize;
	char sendbuff[1024];

	VAVA_CMD_HEAD *vavahead;

	memset(sendbuff, 0, 1024);
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_PARSEERR;
	vavahead->cmd_length = 0;

	sendsize = sizeof(VAVA_CMD_HEAD);

	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendErrInfo(int sessionid, int errnum, int cmdcode, void *data)
{
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;
	
	VAVA_CMD_HEAD *vavahead;
	char sendbuff[1024];

	int ret;
	int sendsize;
	unsigned int checksize;
	
	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}
	
	cJSON_AddStringToObject(pJsonRoot, "result", "fail");
	cJSON_AddNumberToObject(pJsonRoot, "errno", errnum);
	
	if(cmdcode == VAVA_CMD_RECORDLIST_SEARCH && errnum == VAVA_ERR_CODE_IDX_OPEN_FAIL)
	{
		cJSON_AddStringToObject(pJsonRoot, "dirname", (char *)data);
	}

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	
	memset(sendbuff, 0, 1024);
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = cmdcode;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);
		
	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendSearchRecDate(int sessionid)
{
	int ret;
	char datename[9];
	VAVA_Idx_Head *idxhead;
	VAVA_Idx_Date *idxdate;
	char *datelist = NULL;
	
	cJSON *pDateList = NULL;

	int current;
	int totolnum;
	int datenum;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ============  A\n", FUN, LINE);
	
	pthread_mutex_lock(&mutex_idx_lock);
	datelist = Rec_ImportDateIdx();
	pthread_mutex_unlock(&mutex_idx_lock);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ============  B\n", FUN, LINE);

	if(datelist == NULL)
	{
		VAVAHAL_SendErrInfo(sessionid, VAVA_ERR_CODE_IDX_OPEN_FAIL, VAVA_CMD_RECORDDATE_SEARCH, NULL);
		return 0;
	}

	idxhead = (VAVA_Idx_Head *)datelist;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol = %d, first = %d\n", FUN, LINE, idxhead->totol, idxhead->first);
	
	totolnum = idxhead->totol - idxhead->first;
	datenum = 0;

	if(totolnum == 0)
	{
		free(datelist);
		datelist = NULL;
		
		VAVAHAL_SendNoRecDate(sessionid);
		return 0;
	}

	for(current = idxhead->first; current < idxhead->totol; current++)
	{
		idxdate = (VAVA_Idx_Date *)(datelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_Date));

		if(pDateList == NULL)
		{
			pDateList = cJSON_CreateArray();
			if(pDateList == NULL)
			{
				free(datelist);
				datelist = NULL;

				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);
				
				Err_Log("json malloc fail");
				g_running = 0;
				return -1;
			}

			datenum = 0;
		}

		ret = VAVAHAL_ParseDateStr(idxdate->dirname, datename, idxdate->random);
		if(ret == 0)
		{
			ret = VAVAHAL_Date_Verification(datename);
			if(ret == 0)
			{
				ret = VAVAHAL_Date_CheckFile(datename);
				if(ret > 0)
				{
					cJSON *pTmpItem = NULL;

					pTmpItem = cJSON_CreateObject();
					if(pTmpItem == NULL)
					{
						free(datelist);
						datelist = NULL;

						cJSON_Delete(pDateList);

						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
						
						Err_Log("json malloc fail");
						g_running = 0;
						return -1;
					}

					cJSON_AddStringToObject(pTmpItem, "date", datename);
					cJSON_AddItemToArray(pDateList, pTmpItem);
					datenum++;
				}
			}
		}

		totolnum--;
		
		if(totolnum <= 0)
		{
			VAVAHAL_SendRecDatePack(sessionid, 1, datenum, pDateList);
			pDateList = NULL;
			break;
		}

		if(datenum >= REC_PACKAGE_NUM)
		{
			VAVAHAL_SendRecDatePack(sessionid, 0, datenum, pDateList);
			pDateList = NULL;
		}

		usleep(1000);
	}

	if(datelist != NULL)
	{
		free(datelist);
		datelist = NULL;
	}

	if(pDateList != NULL)
	{	
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: totolnum = %d\n", FUN, LINE, totolnum);
		
		VAVAHAL_SendRecDatePack(sessionid, 1, datenum, pDateList);
		pDateList = NULL;
	}
	
	return 0;
}

int VAVAHAL_SendNoRecDate(int sessionid)
{
	cJSON *pJsonRoot = NULL;
	cJSON *pItem = NULL;
	char *pstr = NULL;
	char sendbuff[1024];

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pItem = cJSON_CreateObject();
	if(pItem == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "ok");
	cJSON_AddNumberToObject(pJsonRoot, "datenum", 0);
	cJSON_AddNumberToObject(pJsonRoot, "endflag", 1);
	cJSON_AddItemToObject(pJsonRoot, "datelist", pItem);

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	
	memset(sendbuff, 0, 1024);
	
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECORDDATE_SEARCH;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendRecStop(int sessionid, int channel, int type)
{
	cJSON *pJsonRoot = NULL;
	cJSON *pItem = NULL;
	char *pstr = NULL;
	char sendbuff[1024];

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pItem = cJSON_CreateObject();
	if(pItem == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "stoptype", type);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	memset(sendbuff, 0, 1024);
	
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_MANUAL_REC_STOP;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: send manastop result, session - %d, type - %d\n", FUN, LINE, sessionid, type);
		
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendRecDatePack(int sessionid, int endflag, int datenum, cJSON *pDateList)
{
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "ok");
	cJSON_AddNumberToObject(pJsonRoot, "datenum", datenum);
	cJSON_AddNumberToObject(pJsonRoot, "endflag", endflag);
	cJSON_AddItemToObject(pJsonRoot, "datelist", pDateList);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(g_searchbuff, 0, VAVA_SEARCH_BUFF_SIZE);
	
	vavahead = (VAVA_CMD_HEAD *)g_searchbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECORDDATE_SEARCH;
	vavahead->cmd_length = strlen(pstr);

	memcpy(g_searchbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);
	
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, g_searchbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendSearchRecFile(int sessionid, char *dirname, int type)
{
	int ret;
	char filename[11];
	VAVA_Idx_Head *idxhead;
	VAVA_Idx_File *idxfile;
	char *filelist = NULL;
	
	cJSON *pFileList = NULL;

	int current;
	int totolnum;
	int filenum;

	ret = VAVAHAL_Date_Verification(dirname);
	if(ret != 0)
	{
		VAVAHAL_SendErrInfo(sessionid, VAVA_ERR_CODE_PARAM_INVALID, VAVA_CMD_RECORDLIST_SEARCH, NULL);
		return 0;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ============  A\n", FUN, LINE);
	
	pthread_mutex_lock(&mutex_idx_lock);
	filelist = Rec_ImportFileIdx(dirname);
	pthread_mutex_unlock(&mutex_idx_lock);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ============  B\n", FUN, LINE);

	if(filelist == NULL)
	{
		VAVAHAL_SendErrInfo(sessionid, VAVA_ERR_CODE_IDX_OPEN_FAIL, VAVA_CMD_RECORDLIST_SEARCH, (void *)dirname);
		return 0;
	}

	idxhead = (VAVA_Idx_Head *)filelist;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol = %d, first = %d\n", FUN, LINE, idxhead->totol, idxhead->first);
	
	totolnum = idxhead->totol - idxhead->first;
	filenum = 0;

	if(totolnum == 0)
	{
		free(filelist);
		filelist = NULL;
		
		VAVAHAL_SendNoRecFile(sessionid, dirname);
		return 0;
	}
	
	for(current = 0; current < idxhead->totol; current++)
	{
		idxfile = (VAVA_Idx_File *)(filelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_File));

		if(pFileList == NULL)
		{
			pFileList = cJSON_CreateArray();
			if(pFileList == NULL)
			{
				free(filelist);
				filelist = NULL;

				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);
				
				Err_Log("json malloc fail");
				g_running = 0;
				return -1;
			}

			filenum = 0;
		}

		ret = VAVAHAL_ParseFileStr(idxfile->filename, filename, idxfile->random);
		if(ret == 0)
		{
			ret = VAVAHAL_RecFile_Verification(filename);
			if(ret == 0)
			{
				cJSON *pTmpItem = NULL;

				pTmpItem = cJSON_CreateObject();
				if(pTmpItem == NULL)
				{
					free(filelist);
					filelist = NULL;

					cJSON_Delete(pFileList);

					VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
					
					Err_Log("json malloc fail");
					g_running = 0;
					return -1;
				}

				if(type != VAVA_RECFILE_TYPE_ALL)
				{
					if(type != idxfile->type)
					{
						totolnum--;
						continue;
					}
				}

				cJSON_AddStringToObject(pTmpItem, "file", filename);
				cJSON_AddNumberToObject(pTmpItem, "channel", idxfile->channel);
				cJSON_AddNumberToObject(pTmpItem, "type", idxfile->type);
				cJSON_AddNumberToObject(pTmpItem, "time", idxfile->rectime);
				
				cJSON_AddItemToArray(pFileList, pTmpItem);
				filenum++;
			}
		}
		
		totolnum--;

		if(totolnum <= 0)
		{
			VAVAHLA_SendRecFilePack(sessionid, dirname, 1, filenum, pFileList);
			pFileList = NULL;
			break;
		}

		if(filenum >= REC_PACKAGE_NUM)
		{
			VAVAHLA_SendRecFilePack(sessionid, dirname, 0, filenum, pFileList);
			pFileList = NULL;
		}

		usleep(1000);
	}

	if(filelist != NULL)
	{
		free(filelist);
		filelist = NULL;
			
	}

	if(pFileList != NULL)
	{	
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: totolnum = %d\n", FUN, LINE, totolnum);
		
		VAVAHLA_SendRecFilePack(sessionid, dirname, 1, filenum, pFileList);
		pFileList = NULL;
	}

	return 0;
}

int VAVAHAL_SendNoRecFile(int sessionid, char *dirname)
{
	cJSON *pJsonRoot = NULL;
	cJSON *pItem = NULL;
	char *pstr = NULL;

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;
	char sendbuff[1024];

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pItem = cJSON_CreateObject();
	if(pItem == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "ok");
	cJSON_AddStringToObject(pJsonRoot, "date", dirname);
	cJSON_AddNumberToObject(pJsonRoot, "filenum", 0);
	cJSON_AddNumberToObject(pJsonRoot, "endflag", 1);
	cJSON_AddItemToObject(pJsonRoot, "filelist", pItem);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	memset(sendbuff, 0, 1024);
	
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECORDLIST_SEARCH;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHLA_SendRecFilePack(int sessionid, char *dirname, int endflag, int filenum, cJSON *pFileList)
{
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "ok");
	cJSON_AddStringToObject(pJsonRoot, "date", dirname);
	cJSON_AddNumberToObject(pJsonRoot, "filenum", filenum);
	cJSON_AddNumberToObject(pJsonRoot, "endflag", endflag);
	cJSON_AddItemToObject(pJsonRoot, "filelist", pFileList);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(g_searchbuff, 0, VAVA_SEARCH_BUFF_SIZE);

	vavahead = (VAVA_CMD_HEAD *)g_searchbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECORDLIST_SEARCH;
	vavahead->cmd_length = strlen(pstr);

	memcpy(g_searchbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);
	
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, g_searchbuff, sendsize);
	}
	
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendSearchRecShareDate(int sessionid, struct _recsharedatelist list[])
{
	int i;
	int ret;
	char datename[9];
	VAVA_Idx_Head *idxhead;
	VAVA_Idx_Date *idxdate;
	char *datelist = NULL;

	cJSON *pDateList = NULL;

	int current;
	int totolnum;
	int datenum;

	pthread_mutex_lock(&mutex_idx_lock);
	datelist = Rec_ImportDateIdx();
	pthread_mutex_unlock(&mutex_idx_lock);

	if(datelist == NULL)
	{
		VAVAHAL_SendErrInfo(sessionid, VAVA_ERR_CODE_IDX_OPEN_FAIL, VAVA_CMD_RECSHAREDATE_SEARCH, NULL);
		return 0;
	}

	idxhead = (VAVA_Idx_Head *)datelist;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol = %d, first = %d\n", FUN, LINE, idxhead->totol, idxhead->first);
	
	totolnum = idxhead->totol - idxhead->first;
	datenum = 0;

	if(totolnum == 0)
	{
		free(datelist);
		datelist = NULL;
		
		VAVAHAL_SendNoRecShareDate(sessionid);
		return 0;
	}

	for(current = idxhead->first; current < idxhead->totol; current++)
	{
		idxdate = (VAVA_Idx_Date *)(datelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_Date));

		//解析目录名
		ret = VAVAHAL_ParseDateStr(idxdate->dirname, datename, idxdate->random);
		if(ret == 0)
		{
			ret = VAVAHAL_Date_Verification(datename);
			if(ret == 0)
			{
				//查看日期是否符合
				for(i = 0; i < MAX_CHANNEL_NUM; i++)
				{
					if(list[i].channel == 1)
					{
						if(atoi(list[i].startdate) <= atoi(datename))
						{
							if(pDateList == NULL)
							{
								pDateList = cJSON_CreateArray();
								if(pDateList == NULL)
								{
									free(datelist);
									datelist = NULL;

									VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);
									
									Err_Log("json malloc fail");
									g_running = 0;
									return -1;
								}

								datenum = 0;
							}
							
							//查找通道是否有符合的数据
							ret = VAVAHAL_CheckRecDateWithChannel(datename, i);
							if(ret == 0)
							{
								cJSON *pTmpItem = NULL;
								pTmpItem = cJSON_CreateObject();
								if(pTmpItem == NULL)
								{
									free(datelist);
									datelist = NULL;

									cJSON_Delete(pDateList);

									VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
									
									Err_Log("json malloc fail");
									g_running = 0;
									return -1;
								}

								cJSON_AddStringToObject(pTmpItem, "date", datename);
								cJSON_AddItemToArray(pDateList, pTmpItem);
								datenum++;

								break;
							}
						}
					}
				}
			}
		}

		totolnum--;

		if(totolnum <= 0)
		{
			VAVAHAL_SendRecShareDatePack(sessionid, 1, datenum, pDateList);
			pDateList = NULL;
			break;
		}

		if(datenum >= REC_PACKAGE_NUM)
		{
			VAVAHAL_SendRecShareDatePack(sessionid, 0, datenum, pDateList);
			pDateList = NULL;
		}

		usleep(1000);
	}

	if(datelist != NULL)
	{
		free(datelist);
		datelist = NULL;
	}

	if(pDateList != NULL)
	{	
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: totolnum = %d\n", FUN, LINE, totolnum);
		
		VAVAHAL_SendRecShareDatePack(sessionid, 1, datenum, pDateList);
		Err_Log("date idx err");
	}

	return 0;
}

int VAVAHAL_SendNoRecShareDate(int sessionid)
{
	cJSON *pJsonRoot = NULL;
	cJSON *pItem = NULL;
	char *pstr = NULL;
	char sendbuff[1024];

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pItem = cJSON_CreateObject();
	if(pItem == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "ok");
	cJSON_AddNumberToObject(pJsonRoot, "datenum", 0);
	cJSON_AddNumberToObject(pJsonRoot, "endflag", 1);
	cJSON_AddItemToObject(pJsonRoot, "datelist", pItem);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	memset(sendbuff, 0, 1024);
	
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECSHAREDATE_SEARCH;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendRecShareDatePack(int sessionid, int endflag, int datenum, cJSON *pDateList)
{
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "ok");
	cJSON_AddNumberToObject(pJsonRoot, "datenum", datenum);
	cJSON_AddNumberToObject(pJsonRoot, "endflag", endflag);
	cJSON_AddItemToObject(pJsonRoot, "datelist", pDateList);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(g_searchbuff, 0, VAVA_SEARCH_BUFF_SIZE);
	
	vavahead = (VAVA_CMD_HEAD *)g_searchbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECSHAREDATE_SEARCH;
	vavahead->cmd_length = strlen(pstr);

	memcpy(g_searchbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);
	
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, g_searchbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_CheckRecDateWithChannel(char *dirname, int channel)
{
	FILE *fd = NULL;
	VAVA_Idx_Head idxhead;
	VAVA_Idx_File idxfile;

	char path[128];

	int ret;
	int totolnum;

	if(dirname == NULL)
	{
		return -1;
	}

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, dirname, RECIDX_FILE);

	fd = fopen(path, "r");
	if(fd == NULL)
	{
		return -1;
	}

	memset(&idxhead, 0, sizeof(VAVA_Idx_Head));
	fread(&idxhead, sizeof(VAVA_Idx_Head), 1, fd);
	if(idxhead.tag != VAVA_IDX_HEAD)
	{
		fclose(fd);
		return -1;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: dirname = %s, channel = %d, totol = %d, first = %d\n", 
	                                FUN, LINE, dirname, channel, idxhead.totol, idxhead.first);

	totolnum = idxhead.totol - idxhead.first;
	if(totolnum == 0)
	{
		fclose(fd);
		return -1;
	}

	if(idxhead.first != 0)
	{
		ret = fseek(fd, sizeof(VAVA_Idx_Head) + idxhead.first * sizeof(VAVA_Idx_Date), SEEK_SET);
		if(ret != 0)
		{
			fclose(fd);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: fseek fail\n", FUN, LINE);
			return -1;
		}
	}

	while(g_running)
	{
		if(feof(fd) || totolnum <= 0)
		{
			break;
		}

		memset(&idxfile, 0, sizeof(VAVA_Idx_File));
		ret = fread(&idxfile, sizeof(VAVA_Idx_File), 1, fd);
		if(ret <= 0)
		{
			break;
		}

		if(idxfile.channel == channel)
		{
			fclose(fd);
			return 0;
		}

		#if 0
		for(i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			if(idxfile.channel == i && channel[i] == 1)
			{
				fclose(fd);
				return 0;	
			}
		}
		#endif
		
		totolnum--;
	}

	fclose(fd);

	return -1;
}

int VAVAHAL_SendSearchRecShareList(int sessionid, char *dirname, int type, int channel[])
{
	int i;
	int ret;
	char filename[11];
	VAVA_Idx_Head *idxhead;
	VAVA_Idx_File *idxfile;
	char *filelist = NULL;

	cJSON *pFileList = NULL;

	int current;
	int totolnum;
	int filenum;

	ret = VAVAHAL_Date_Verification(dirname);
	if(ret != 0)
	{
		VAVAHAL_SendErrInfo(sessionid, VAVA_ERR_CODE_PARAM_INVALID, VAVA_CMD_RECSHARELIST_SEARCH, NULL);
		return 0;
	}

	pthread_mutex_lock(&mutex_idx_lock);
	filelist = Rec_ImportFileIdx(dirname);
	pthread_mutex_unlock(&mutex_idx_lock);

	if(filelist == NULL)
	{
		VAVAHAL_SendErrInfo(sessionid, VAVA_ERR_CODE_IDX_OPEN_FAIL, VAVA_CMD_RECSHARELIST_SEARCH, NULL);
		return 0;
	}

	idxhead = (VAVA_Idx_Head *)filelist;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: totol = %d, first = %d\n", FUN, LINE, idxhead->totol, idxhead->first);
	
	totolnum = idxhead->totol - idxhead->first;
	filenum = 0;
	
	if(totolnum == 0)
	{
		free(filelist);
		filelist = NULL;
		
		VAVAHAL_SendNoRecShareList(sessionid, dirname);
		return 0;
	}

	for(current = 0; current < idxhead->totol; current++)
	{
		idxfile = (VAVA_Idx_File *)(filelist + sizeof(VAVA_Idx_Head) + current * sizeof(VAVA_Idx_File));

		if(pFileList == NULL)
		{
			pFileList = cJSON_CreateArray();
			if(pFileList == NULL)
			{
				free(filelist);
				filelist = NULL;

				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);
				
				Err_Log("json malloc fail");
				g_running = 0;
				return -1;
			}

			filenum = 0;
		}

		for(i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			if(idxfile->channel == i && channel[i] == 1)
			{
				if(pFileList == NULL)
				{
					pFileList = cJSON_CreateArray();
					if(pFileList == NULL)
					{
						free(filelist);
						filelist = NULL;

						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);
						
						Err_Log("json malloc fail");
						g_running = 0;
						return -1;
					}

					filenum = 0;
				}

				ret = VAVAHAL_ParseFileStr(idxfile->filename, filename, idxfile->random);
				if(ret == 0)
				{
					cJSON *pTmpItem = NULL;

					pTmpItem = cJSON_CreateObject();
					if(pTmpItem == NULL)
					{
						free(filelist);
						filelist = NULL;

						cJSON_Delete(pFileList);

						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
						
						Err_Log("json malloc fail");
						g_running = 0;
						return -1;
					}

					if(type == VAVA_RECFILE_TYPE_ALL || type == idxfile->type)
					{
						cJSON_AddStringToObject(pTmpItem, "file", filename);
						cJSON_AddNumberToObject(pTmpItem, "channel", idxfile->channel);
						cJSON_AddNumberToObject(pTmpItem, "type", idxfile->type);
						cJSON_AddNumberToObject(pTmpItem, "time", idxfile->rectime);
						
						cJSON_AddItemToArray(pFileList, pTmpItem);
						filenum++;
					}
				}

				break;
			}
		}

		totolnum--;

		if(totolnum <= 0)
		{
			VAVAHAL_SendRecShareListPack(sessionid, dirname, 1, filenum, pFileList);
			pFileList = NULL;
			break;
		}

		if(filenum >= REC_PACKAGE_NUM)
		{
			VAVAHAL_SendRecShareListPack(sessionid, dirname, 0, filenum, pFileList);
			pFileList = NULL;
		}

		usleep(1000);
	}

	if(filelist != NULL)
	{
		free(filelist);
		filelist = NULL;
	}

	if(pFileList != NULL)
	{	
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: totolnum = %d\n", FUN, LINE, totolnum);
		
		VAVAHAL_SendRecShareListPack(sessionid, dirname, 1, filenum, pFileList);
		Err_Log("file idx err");
	}

	return 0;
}

int VAVAHAL_SendNoRecShareList(int sessionid, char *dirname)
{
	cJSON *pJsonRoot = NULL;
	cJSON *pItem = NULL;
	char *pstr = NULL;
	char sendbuff[1024];

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pItem = cJSON_CreateObject();
	if(pItem == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "ok");
	cJSON_AddStringToObject(pJsonRoot, "date", dirname);
	cJSON_AddNumberToObject(pJsonRoot, "filenum", 0);
	cJSON_AddNumberToObject(pJsonRoot, "endflag", 1);
	cJSON_AddItemToObject(pJsonRoot, "filelist", pItem);

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	
	memset(sendbuff, 0, 1024);
	
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECSHARELIST_SEARCH;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendRecShareListPack(int sessionid, char *dirname, int endflag, int filenum, cJSON *pFileList)
{
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	VAVA_CMD_HEAD *vavahead;
	int ret;
	unsigned int checksize;
	int sendsize;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "ok");
	cJSON_AddStringToObject(pJsonRoot, "date", dirname);
	cJSON_AddNumberToObject(pJsonRoot, "filenum", filenum);
	cJSON_AddNumberToObject(pJsonRoot, "endflag", endflag);
	cJSON_AddItemToObject(pJsonRoot, "filelist", pFileList);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(g_searchbuff, 0, VAVA_SEARCH_BUFF_SIZE);

	vavahead = (VAVA_CMD_HEAD *)g_searchbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECSHARELIST_SEARCH;
	vavahead->cmd_length = strlen(pstr);

	memcpy(g_searchbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);
	
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, g_searchbuff, sendsize);
	}
	
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendPairResp(int sessionid, int errnum)
{
	int i;
	int ret;
	cJSON *pJsonRoot = NULL;
	cJSON *pPairlist = NULL;
	cJSON *pPairitem[MAX_CHANNEL_NUM];
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	if(errnum == VAVA_ERR_CODE_SUCCESS)
	{
		pPairlist = cJSON_CreateArray();
		if(pPairlist == NULL)
		{
			cJSON_AddStringToObject(pJsonRoot, "result", "fail");
			cJSON_AddNumberToObject(pJsonRoot, "errno", VAVA_ERR_CODE_JSON_MALLOC_FIAL);
		}
		else
		{
			for(i = 0; i < MAX_CHANNEL_NUM; i++)
			{
				pPairitem[i] = cJSON_CreateObject();
				if(pPairitem[i] == NULL)
				{
					cJSON_Delete(pPairlist);
					cJSON_AddStringToObject(pJsonRoot, "result", "fail");
					cJSON_AddNumberToObject(pJsonRoot, "errno", VAVA_ERR_CODE_JSON_MALLOC_FIAL);
					break;
				}
				else
				{
					cJSON_AddNumberToObject(pPairitem[i], "channel", i);
					cJSON_AddNumberToObject(pPairitem[i], "status", g_pair[i].nstat);

					if(g_pair[i].nstat == 0)
					{
						cJSON_AddStringToObject(pPairitem[i], "sn", "null");
					}
					else
					{
						cJSON_AddStringToObject(pPairitem[i], "sn", g_pair[i].id);
					}

					cJSON_AddItemToArray(pPairlist, pPairitem[i]);
				}
			}
		}

		cJSON_AddStringToObject(pJsonRoot, "result", "ok");
		cJSON_AddItemToObject(pJsonRoot, "pairinfo", pPairlist);
	}
	else
	{
		cJSON_AddStringToObject(pJsonRoot, "result", "fail");
		cJSON_AddNumberToObject(pJsonRoot, "errno", errnum);
	}

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);

	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_PAIR_RESP;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);
	
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendRfTestErr(int sessionid, int savenum, int currentnum)
{
	int ret;
	
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddNumberToObject(pJsonRoot, "savenum", savenum);
	cJSON_AddNumberToObject(pJsonRoot, "currentnum", currentnum);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);

	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RF_TEST_WARING;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);
	
	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendCameraSetAck(int sessionid, int channel, int result, int cmd)
{
	int ret;
	
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	if(result == 0)
	{
		cJSON_AddStringToObject(pJsonRoot, "result", "ok");
		cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	}
	else
	{
		cJSON_AddStringToObject(pJsonRoot, "result", "fail");
		cJSON_AddNumberToObject(pJsonRoot, "errno", VAVA_ERR_CODE_CONFIG_FAIL);
	}

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);
	
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = cmd;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}

	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendCameraSetTimeout(int sessionid, int cmd)
{
	int ret;
	
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;

	if(sessionid == -1)
	{
		return 0;
	}

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "fail");
	cJSON_AddNumberToObject(pJsonRoot, "errno", VAVA_ERR_CODE_CONFIG_TIMEOUT);
		
	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);
	
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = cmd;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}

	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendCameraNotReady(int sessionid, int cmd)
{
	int ret;
	
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;

	if(sessionid == -1)
	{
		return 0;
	}

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "fail");
	cJSON_AddNumberToObject(pJsonRoot, "errno", VAVA_ERR_CODE_CAMERA_NOREADY);
		
	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);
	
	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = cmd;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}

	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendRecDelResult(int sessionid, int type, char *dirname, char *filename, int result)
{
	int ret;
	
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;
	
	//返回删除结果
	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	if(result == 0)
	{
		cJSON_AddNumberToObject(pJsonRoot, "result", 0);
	}
	else
	{
		cJSON_AddNumberToObject(pJsonRoot, "result", 1);
	}
	cJSON_AddStringToObject(pJsonRoot, "dirname", dirname);
	cJSON_AddNumberToObject(pJsonRoot, "flag", type);
	if(type == VAVA_RECFILE_DEL_NORMAL)
	{
		cJSON_AddStringToObject(pJsonRoot, "filename", filename);
	}

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);

	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_RECORD_DEL_RESP;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendSnapShotResult(int sessionid, int channel, char *dirname, char *filename, int result)
{
	int ret;
	
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;
	
	//返回删除结果
	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	if(result == 0)
	{
		cJSON_AddNumberToObject(pJsonRoot, "result", 0);
	}
	else
	{
		cJSON_AddNumberToObject(pJsonRoot, "result", 1);
	}
	
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddStringToObject(pJsonRoot, "dirname", dirname);
	cJSON_AddStringToObject(pJsonRoot, "filename", filename);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);

	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_SNAPSHOT_RESP;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendCameraNoWakeup(int sessionid, int cmd)
{
	int ret;
	
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;
	
	//返回删除结果
	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "result", "fail");
	cJSON_AddNumberToObject(pJsonRoot, "errno", VAVA_ERR_CODE_CAMERA_DORMANT);

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);

	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = cmd;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_SendRecStartResult(int sessionid, int channel, int result)
{
	int ret;
	
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	char sendbuff[1024];
	int sendsize;
	unsigned int checksize;
	VAVA_CMD_HEAD *vavahead;
	
	//返回删除结果
	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	if(result == VAVA_ERR_CODE_SUCCESS)
	{
		cJSON_AddStringToObject(pJsonRoot, "result", "ok");
		cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
		cJSON_AddNumberToObject(pJsonRoot, "ntsamp", g_manaul_ntsamp[channel]);
	}
	else
	{
		cJSON_AddStringToObject(pJsonRoot, "result", "fail");
		cJSON_AddNumberToObject(pJsonRoot, "errno", result);
	}

	pstr = cJSON_PrintUnformatted(pJsonRoot);

	pthread_mutex_lock(&mutex_session_lock);

	memset(sendbuff, 0, 1024);

	vavahead = (VAVA_CMD_HEAD *)sendbuff;
	vavahead->sync_code = VAVA_TAG_APP_CMD;
	vavahead->cmd_code = VAVA_CMD_START_REC;
	vavahead->cmd_length = strlen(pstr);

	memcpy(sendbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
	sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);

	free(pstr);
	cJSON_Delete(pJsonRoot);

	ret = PPCS_Check_Buffer(sessionid, P2P_CHANNEL_CMD, &checksize, NULL);
	if(ret == ERROR_PPCS_SUCCESSFUL && checksize <= SYP2P_CMD_BUFF_SIZE)
	{
		PPCS_Write(sessionid, P2P_CHANNEL_CMD, sendbuff, sendsize);
	}
	
	pthread_mutex_unlock(&mutex_session_lock);

	return 0;
}

int VAVAHAL_UpdateCamera(int cmdfd, int cmdtype, int param1, int param2)
{
	int ret;
	int crc32;
	char tmpbuff[128];

	VAVA_Msg msghead;
	VAVA_Upgrate_Crc32 upcrc;

	memset(&msghead, 0, sizeof(VAVA_Msg));
	msghead.tag = VAVA_TAG_CAMERA_CMD;
	msghead.comtype = cmdtype;
	memset(msghead.data, 0, CMD_DATA_SIZE);

	if(cmdtype == VAVA_CONFIG_UPDATE_REQ)
	{
		upcrc.status = param1;
		upcrc.size = param2;
		memcpy(msghead.data, &upcrc, sizeof(VAVA_Upgrate_Crc32));
	}

	memset(tmpbuff, 0, 128);
	memcpy(tmpbuff, &msghead.comtype, sizeof(int));
	memcpy(tmpbuff + sizeof(int), msghead.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(tmpbuff, sizeof(int) + CMD_DATA_SIZE);
	msghead.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	ret = send(cmdfd, &msghead, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	if(ret <= 0)
	{
		return -1;
	}

	return 0;
}

int VAVAHAL_CmdResp_NoPair(int cmdfd)
{
	int crc32;
	char crcbuff[128];

	VAVA_Msg vava_resp;
	
	vava_resp.tag = VAVA_TAG_CAMERA_CMD;
	vava_resp.comtype = VAVA_MSGTYPE_NOPAIR_RESP;
	memset(vava_resp.data, 0, CMD_DATA_SIZE);
	
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_resp.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_resp.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_resp.crc32 = crc32;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: crc32 = %x\n", FUN, LINE, vava_resp.crc32);

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_resp, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdResp_Heartbeat(int cmdfd, int sleep)
{
	int ntp;
	int crc32;
	char crcbuff[128];

	VAVA_Msg vava_resp;
	VAVA_Time_Sync *vava_timesync;
	struct timeval timesync;
	
	//回复心跳包
	vava_resp.tag = VAVA_TAG_CAMERA_CMD;
	vava_resp.comtype = VAVA_MSGTYPE_HEARTBEAT;
	memset(vava_resp.data, 0, CMD_DATA_SIZE);

	ntp = VAVAHAL_GetNtp();
	gettimeofday(&timesync, NULL);
	
	vava_timesync = (VAVA_Time_Sync *)vava_resp.data;
	vava_timesync->sec = timesync.tv_sec + ntp * 3600;
	vava_timesync->usec = timesync.tv_usec;
	vava_timesync->flag = 0xEE;
	vava_timesync->sleep = sleep;

	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_resp.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_resp.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_resp.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_resp, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdResp_PairResp(int cmdfd, VAVA_Pair_Resp *pairinfo)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_resp;

	if(pairinfo == NULL)
	{
		return -1;
	}
	
	vava_resp.tag = VAVA_TAG_CAMERA_CMD;
	vava_resp.comtype = VAVA_MSGTYPE_PAIR_RESP;
	memset(vava_resp.data, 0, CMD_DATA_SIZE);
	memcpy(vava_resp.data, pairinfo, sizeof(VAVA_Pair_Resp));

	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_resp.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_resp.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_resp.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_resp, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdResp_FirstConnectAck(int cmdfd)
{
	int ntp;
	int crc32;
	char crcbuff[128];

	struct timeval timesync;

	VAVA_Msg vava_resp;
	VAVA_Time_Sync *vava_timesync;
	
	vava_resp.tag = VAVA_TAG_CAMERA_CMD;
	vava_resp.comtype = VAVA_MSGTYPE_CONNECT_ACK;
	memset(vava_resp.data, 0, CMD_DATA_SIZE);

	ntp = VAVAHAL_GetNtp();
	gettimeofday(&timesync, NULL);
	
	vava_timesync = (VAVA_Time_Sync *)vava_resp.data;
	vava_timesync->sec = timesync.tv_sec + ntp * 3600;
	vava_timesync->usec = timesync.tv_usec;

	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_resp.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_resp.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_resp.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_resp, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdResp_TimeSync(int cmdfd)
{
	int ntp;
	int crc32;
	char crcbuff[128];
	struct timeval timesync;

	VAVA_Msg vava_resp;
	VAVA_Time_Sync vava_timtsync;

	ntp = VAVAHAL_GetNtp();

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get ntp = %d\n", FUN, LINE, ntp);

	gettimeofday(&timesync, NULL);
	vava_timtsync.sec = timesync.tv_sec + ntp * 3600;
	vava_timtsync.usec = timesync.tv_usec;
						
	vava_resp.tag = VAVA_TAG_CAMERA_CMD;
	vava_resp.comtype = VAVA_MSGTYPE_TIME_SYNC;
	memset(vava_resp.data, 0, CMD_DATA_SIZE);
	memcpy(vava_resp.data, &vava_timtsync, sizeof(VAVA_Time_Sync));

	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_resp.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_resp.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_resp.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_resp, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_InsertIframe(int cmdfd, unsigned int stream)
{
	int crc32;
	char crcbuff[128];

	VAVA_Msg vava_req;
	VAVA_Insertiframe *vava_insertiframe;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Insert I frame, stream = %d\n", FUN, LINE, stream);

	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_CONFIG_INSERT_IFRAME;
	
	memset(vava_req.data, 0, CMD_DATA_SIZE);
	vava_insertiframe = (VAVA_Insertiframe *)vava_req.data;
	vava_insertiframe->stream = stream;
	vava_insertiframe->result = 0;

	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_SetRes(int cmdfd, qCmdData cmddata)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_req;
	VAVA_ResConfig *resparam;
	VAVA_ResConfig *resconfig;
	
	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_CONFIG_SET_RESOLUTION;

	memset(vava_req.data, 0, CMD_DATA_SIZE);
	resparam = (VAVA_ResConfig *)cmddata.param;
	resconfig = (VAVA_ResConfig *)vava_req.data;
	
	resconfig->stream = resparam->stream;
	resconfig->res = resparam->res;
	resconfig->fps = resparam->fps;
	resconfig->bitrate = resparam->bitrate;
	resconfig->sessionid = cmddata.sessionid;
	resconfig->channel = cmddata.channel;
	resconfig->result = 0;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: set res = %d, fps = %d, bitrate = %d\n", 
	                                FUN, LINE, resconfig->res, resconfig->fps, resconfig->bitrate);
				
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_SetMirr(int cmdfd, qCmdData cmddata)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_req;
	VAVA_MirrConfig *mirrparam;
	VAVA_MirrConfig *mirrconfig;
	
	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_CONFIG_SET_MIRROR;

	memset(vava_req.data, 0, CMD_DATA_SIZE);
	mirrparam = (VAVA_MirrConfig *)cmddata.param;
	mirrconfig = (VAVA_MirrConfig *)vava_req.data;
	
	mirrconfig->param = mirrparam->param;
	mirrconfig->sessionid = cmddata.sessionid;
	mirrconfig->channel = cmddata.channel;
	mirrconfig->result = 0;
				
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_SetIrMode(int cmdfd, qCmdData cmddata)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_req;
	VAVA_IrLedConfig *irledparam;
	VAVA_IrLedConfig *irledconfig;

	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_CONFIG_SET_IRLEDMODE;

	memset(vava_req.data, 0, CMD_DATA_SIZE);
	irledparam = (VAVA_IrLedConfig *)cmddata.param;
	irledconfig = (VAVA_IrLedConfig *)vava_req.data;

	irledconfig->param = irledparam->param;
	irledconfig->sessionid = cmddata.sessionid;
	irledconfig->channel = cmddata.channel;
	irledconfig->result = 0;
				
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_SetMdParam(int cmdfd, qCmdData cmddata)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_req;
	VAVA_MdConfig *mdparam;
	VAVA_MdConfig *mdconfig;
	
	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_CONFIG_SET_MDPARAM;

	memset(vava_req.data, 0, CMD_DATA_SIZE);
	mdparam = (VAVA_MdConfig *)cmddata.param;
	mdconfig = (VAVA_MdConfig *)vava_req.data;
	
	mdconfig->enabel = mdparam->enabel;
	mdconfig->startx = mdparam->startx;
	mdconfig->endx = mdparam->endx;
	mdconfig->starty = mdparam->starty;
	mdconfig->endy = mdparam->endy;
	mdconfig->sensitivity = mdparam->sensitivity;
	mdconfig->sessionid = cmddata.sessionid;
	mdconfig->channel = cmddata.channel;
	mdconfig->result = 0;
				
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_CameraReset(int cmdfd, qCmdData cmddata)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_req;
	VAVA_CameraFactory *resetconfig;
	
	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_CONFIG_RESET_FACTORY;
	memset(vava_req.data, 0, CMD_DATA_SIZE);

	resetconfig = (VAVA_CameraFactory *)vava_req.data;
	
	resetconfig->sessionid = cmddata.sessionid;
	resetconfig->channel = cmddata.channel;
	resetconfig->result = 0;
				
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_OpenRec(int cmdfd, qCmdData cmddata)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_req;
	VAVA_CameraRecCtrl *recctrl;
	
	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_CONFIG_START_REC;
	memset(vava_req.data, 0, CMD_DATA_SIZE);

	recctrl = (VAVA_CameraRecCtrl *)vava_req.data;
	
	recctrl->sessionid = cmddata.sessionid;
	recctrl->channel = cmddata.channel;
	recctrl->result = 0;
				
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_CloseRec(int cmdfd, qCmdData cmddata)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_req;
	VAVA_CameraRecCtrl *recctrl;
	
	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_CONFIG_STOP_REC;
	memset(vava_req.data, 0, CMD_DATA_SIZE);

	recctrl = (VAVA_CameraRecCtrl *)vava_req.data;
	
	recctrl->sessionid = cmddata.sessionid;
	recctrl->channel = cmddata.channel;
	recctrl->result = 0;
				
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_CmdReq_SetComCtrl(int cmdfd, qCmdData cmddata)
{
	int crc32;
	char crcbuff[128];
	
	VAVA_Msg vava_req;
	VAVA_IPCComConfig *comparam;
	VAVA_IPCComConfig *comconfig;
	
	memset(&vava_req, 0, sizeof(VAVA_Msg));
	vava_req.tag = VAVA_TAG_CAMERA_CMD;
	vava_req.comtype = VAVA_MSGTYPE_COM_CTRL;

	memset(vava_req.data, 0, CMD_DATA_SIZE);
	comparam = (VAVA_IPCComConfig *)cmddata.param;
	comconfig = (VAVA_IPCComConfig *)vava_req.data;

	comconfig->ctrl = comparam->ctrl;
	comconfig->sessionid = cmddata.sessionid;
	comconfig->channel = cmddata.channel;
	comconfig->result = 0;
			
	memset(crcbuff, 0, 128);
	memcpy(crcbuff, &vava_req.comtype, sizeof(int));
	memcpy(crcbuff + sizeof(int), vava_req.data, CMD_DATA_SIZE);
	crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
	vava_req.crc32 = crc32;

	pthread_mutex_lock(&mutex_cmdsend_lock);
	send(cmdfd, &vava_req, sizeof(VAVA_Msg), 0);
	pthread_mutex_unlock(&mutex_cmdsend_lock);

	return 0;
}

int VAVAHAL_ParamParse_SessionAuth(cJSON *pRoot, int *random, char *md5, char *authkey)
{
	cJSON *pRandom = NULL;
	cJSON *pMd5 = NULL;
	cJSON *pAuthKey = NULL;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(random == NULL || md5 == NULL || authkey == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(md5, 0, 36);
	memset(authkey, 0, 36);

	pRandom = cJSON_GetObjectItem(pRoot, "random");
	if(pRandom == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*random = pRandom->valueint;

	pMd5 = cJSON_GetObjectItem(pRoot, "auth");
	if(pMd5 == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	strcpy(md5, pMd5->valuestring);

	pAuthKey = cJSON_GetObjectItem(pRoot, "key");
	if(pAuthKey == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	strcpy(authkey, pAuthKey->valuestring);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_Channel(cJSON *pRoot, int *channel)
{
	cJSON *pChannel = NULL;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(channel == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel"); 
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*channel = pChannel->valueint;
	if(*channel < 0 || *channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_Ctrl(cJSON *pRoot, int *channel, int *ctrl)
{
	cJSON *pChannel = NULL;
	cJSON *pCtrl = NULL;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(channel == NULL || ctrl == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*channel = pChannel->valueint;
	if(*channel < 0 || *channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pCtrl = cJSON_GetObjectItem(pRoot, "ctrl");
	if(pCtrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*ctrl = pCtrl->valueint;
	if(*ctrl < VAVA_CTRL_DISABLE || *ctrl > VAVA_CTRL_ENABLE)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_VideoQuality(cJSON *pRoot, int *channel, int *quality)
{
	cJSON *pChannel = NULL;
	cJSON *pQuality = NULL;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(channel == NULL || quality == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*channel = pChannel->valueint;
	if(*channel < 0 || *channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pQuality = cJSON_GetObjectItem(pRoot, "quality");
	if(pQuality == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*quality = pQuality->valueint;
	if(*quality < VAVA_VIDEO_QUALITY_BEST || *quality > VAVA_VIDEO_QUALITY_AUTO)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;	
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_MirrMode(cJSON *pRoot, int *channel, int *mirrmode)
{
	cJSON *pChannel = NULL;
	cJSON *pMirror = NULL;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(channel == NULL || mirrmode == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*channel = pChannel->valueint;
	if(*channel < 0 || *channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pMirror = cJSON_GetObjectItem(pRoot, "mirrormode");
	if(pMirror == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*mirrmode = pMirror->valueint;
	if(*mirrmode < VAVA_MIRROR_TYPE_NORMAL || *mirrmode > VAVA_MIRROR_TYPE_BOTH)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;	
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_IrLedMode(cJSON *pRoot, int *channel, int *irledmode)
{
	cJSON *pChannel = NULL;
	cJSON *pIred = NULL;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(channel == NULL || irledmode == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*channel = pChannel->valueint;
	if(*channel < 0 || *channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pIred = cJSON_GetObjectItem(pRoot, "irmode");
	if(pIred == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*irledmode = pIred->valueint;
	if(*irledmode < VAVA_IRLED_MODE_CLOSE || *irledmode > VAVA_IRLED_MODE_OPEN)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;	
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_PushConfig(cJSON *pRoot)
{
	cJSON *pChannel = NULL;
	cJSON *pCtrl = NULL;

	int channel;
	int ctrl;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	channel = pChannel->valueint;
	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pCtrl = cJSON_GetObjectItem(pRoot, "ctrl");
	if(pCtrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	ctrl = pCtrl->valueint;
	if(ctrl < 0 || ctrl > 1)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	
	g_pushflag[channel].push = ctrl;
	VAVAHAL_WritePushConfig();

	g_cameraattr_sync_flag[channel] = 1;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_EmailConfig(cJSON *pRoot)
{
	cJSON *pChannel = NULL;
	cJSON *pCtrl = NULL;

	int channel;
	int ctrl;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	channel = pChannel->valueint;
	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pCtrl = cJSON_GetObjectItem(pRoot, "ctrl");
	if(pCtrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	ctrl = pCtrl->valueint;
	if(ctrl < 0 || ctrl > 1)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	g_pushflag[channel].email = ctrl;
	VAVAHAL_WriteEmailConfig();

	g_cameraattr_sync_flag[channel] = 1;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_PirSsty(cJSON *pRoot, int *channel)
{
	cJSON *pChannel = NULL;
	cJSON *pSensitivity = NULL;
	int sensitivity;
	int ret;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(channel == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*channel = pChannel->valueint;
	if(*channel < 0 || *channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pSensitivity = cJSON_GetObjectItem(pRoot, "sensitivity");
	if(pSensitivity == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	sensitivity = pSensitivity->valueint;
	if(sensitivity < VAVA_PIR_SENSITIVITY_HIGH || sensitivity > VAVA_PIR_SENSITIVITY_LOW)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;	
	}

	//更新PIR灵敏度值
	if(g_pir_sensitivity[*channel] != sensitivity)
	{
		if(g_camera_arminfo_v1[*channel].status == 1)
		{
			ret = VAVAHAL_SetPirStatus(*channel, sensitivity);
			if(ret != VAVA_ERR_CODE_SUCCESS)
			{
				return VAVA_ERR_CODE_CONFIG_FAIL;
			}
		}
		
		g_pir_sensitivity[*channel] = sensitivity;
		VAVAHAL_WritePirSensitivity();	

		g_cameraattr_sync_flag[*channel] = 1;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_ArmInfo_V1(cJSON *pRoot, int *channel)
{
	cJSON *pChannel = NULL;
	cJSON *pType = NULL;
	cJSON *pArmlist = NULL;
	cJSON *pArmitem = NULL;
	cJSON *pStarttime = NULL;
	cJSON *pEndtime = NULL;
	cJSON *pDays = NULL;

	int ret;
	int tmpch;
	int tmptype;
	
	int armlist;
	int weekday;
	int datanum;
	int listtnum;
	int tmpweek;
	char weekstr[2];
	VAVA_ArmData_v1 tmpdata[MAX_ARM_LIST_NUM];
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(channel == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	tmpch = pChannel->valueint;
	if(tmpch < 0 || tmpch >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pType = cJSON_GetObjectItem(pRoot, "type");
	if(pType == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	tmptype = pType->valueint;
	if(tmptype < VAVA_ARMING_STATUS_DISABLE || tmptype > VAVA_ARMING_STATUS_ONTIME_OFF)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pArmlist = cJSON_GetObjectItem(pRoot, "arminglist");
	if(pArmlist == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	listtnum = cJSON_GetArraySize(pArmlist);
	if(listtnum < 0 || listtnum > 10)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	//初始化临存数组
	datanum = 0;
	for(armlist = 0; armlist < MAX_ARM_LIST_NUM; armlist++)
	{
		tmpdata[armlist].nstat = 0;
		tmpdata[armlist].s_h = 0;
		tmpdata[armlist].s_m = 0;
		tmpdata[armlist].s_s = 0;
		tmpdata[armlist].e_h = 0;
		tmpdata[armlist].e_m = 0;
		tmpdata[armlist].e_s = 0;

		for(weekday = 0; weekday < VAVA_WEEKDAYS; weekday++)
		{
			tmpdata[armlist].weekday[weekday] = 0;
		}
	}

	//解析条目
	for(armlist = 0; armlist < listtnum; armlist++)
	{
		pArmitem = cJSON_GetArrayItem(pArmlist, armlist);
		if(pArmitem == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		pStarttime = cJSON_GetObjectItem(pArmitem, "starttime");
		if(pStarttime == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		ret = VAVAHAL_Time_Verification_Ex(pStarttime->valuestring, &tmpdata[armlist].s_h, &tmpdata[armlist].s_m, &tmpdata[armlist].s_s);
		if(ret != 0)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		pEndtime = cJSON_GetObjectItem(pArmitem, "endtime");
		if(pEndtime == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		ret = VAVAHAL_Time_Verification_Ex(pEndtime->valuestring, &tmpdata[armlist].e_h, &tmpdata[armlist].e_m, &tmpdata[armlist].e_s);
		if(ret != 0)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		pDays = cJSON_GetObjectItem(pArmitem, "days");
		if(pDays == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		for(weekday = 0; weekday < strlen(pDays->valuestring); weekday++)
		{
			memset(weekstr, 0, 2);
			memcpy(weekstr, pDays->valuestring + weekday, 1);
			tmpweek = atoi(weekstr);

			if(tmpweek < 0 || tmpweek >= VAVA_WEEKDAYS)
			{
				return VAVA_ERR_CODE_PARAM_INVALID;
			}

			tmpdata[armlist].weekday[tmpweek] = 1;
		}

		tmpdata[armlist].nstat = 1;
		datanum++;
	}

	memset(&g_camera_arminfo_v1[tmpch], 0, sizeof(VAVA_ArmInfo_v1));

	//保存参数
	g_camera_arminfo_v1[tmpch].type = tmptype;

	for(armlist = 0; armlist < datanum; armlist++)
	{
		if(tmpdata[armlist].nstat == 0)
		{
			break;
		}
		
		g_camera_arminfo_v1[tmpch].armdata[armlist].nstat = 1;
		g_camera_arminfo_v1[tmpch].armdata[armlist].s_h = tmpdata[armlist].s_h;
		g_camera_arminfo_v1[tmpch].armdata[armlist].s_m = tmpdata[armlist].s_m;
		g_camera_arminfo_v1[tmpch].armdata[armlist].s_s = tmpdata[armlist].s_s;
		g_camera_arminfo_v1[tmpch].armdata[armlist].e_h = tmpdata[armlist].e_h;
		g_camera_arminfo_v1[tmpch].armdata[armlist].e_m = tmpdata[armlist].e_m;
		g_camera_arminfo_v1[tmpch].armdata[armlist].e_s = tmpdata[armlist].e_s;

		for(weekday = 0; weekday < VAVA_WEEKDAYS; weekday++)
		{
			g_camera_arminfo_v1[tmpch].armdata[armlist].weekday[weekday] = tmpdata[armlist].weekday[weekday];
		}
	}

	VAVAHAL_PrintArmInfo_v1(tmpch);
	VAVAHAL_WriteArmInfo_v1(tmpch);

	g_cameraattr_sync_flag[tmpch] = 1;

	*channel = tmpch;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_MdInfo(cJSON *pRoot, int *channel, VAVA_MdConfig *mdconfig)
{
	cJSON *pChannel = NULL;
	cJSON *pEnable = NULL;
	cJSON *pStartx = NULL;
	cJSON *pEndx = NULL;
	cJSON *pStarty = NULL;
	cJSON *pEndy = NULL;
	cJSON *pSensitivity = NULL;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(channel == NULL || mdconfig == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	*channel = pChannel->valueint;
	if(*channel < 0 || *channel >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pEnable = cJSON_GetObjectItem(pRoot, "enable");
	if(pEnable == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	mdconfig->enabel = pEnable->valueint;
	if(mdconfig->enabel < VAVA_CTRL_DISABLE || mdconfig->enabel > VAVA_CTRL_ENABLE)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pStartx = cJSON_GetObjectItem(pRoot, "startx");
	if(pStartx == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	mdconfig->startx = pStartx->valueint;
	if(mdconfig->startx < 0 || mdconfig->startx > 100)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pEndx = cJSON_GetObjectItem(pRoot, "endx");
	if(pEndx == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	mdconfig->endx = pEndx->valueint;
	if(mdconfig->endx < 0 || mdconfig->endx > 100 || mdconfig->endx <= mdconfig->startx)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pStarty = cJSON_GetObjectItem(pRoot, "starty");
	if(pStarty == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	mdconfig->starty = pStarty->valueint;
	if(mdconfig->starty < 0 || mdconfig->starty > 100)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pEndy = cJSON_GetObjectItem(pRoot, "endy");
	if(pEndy == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	mdconfig->endy = pEndy->valueint;
	if(mdconfig->endy < 0 || mdconfig->endy > 100 || mdconfig->endy <= mdconfig->starty)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pSensitivity = cJSON_GetObjectItem(pRoot, "sensitivity");
	if(pSensitivity == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	mdconfig->sensitivity = pSensitivity->valueint;
	if(mdconfig->sensitivity < 0 || mdconfig->sensitivity > 100)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RecTimeInfo(cJSON *pRoot)
{
	//cJSON *pFulltime = NULL;
	cJSON *pAlarm = NULL;
	//cJSON *pManaul = NULL;

	//int fulltime;
	int alarm;
	//int manaul;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pAlarm = cJSON_GetObjectItem(pRoot, "alarmrecord");
	if(pAlarm == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	alarm = pAlarm->valueint;
	if(alarm < 10 || alarm > 60)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	if(g_rectime.alarm_time != alarm)
	{
		g_rectime.alarm_time = alarm;
		VAVAHAL_WriteRecTime();

		g_bsattr_sync_flag = 1;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RecSearch(cJSON *pRoot, VAVA_RecSearch *recsearch)
{
	cJSON *pDate = NULL;
	cJSON *pType = NULL;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(recsearch == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(recsearch, 0, sizeof(VAVA_RecSearch));

	pDate = cJSON_GetObjectItem(pRoot, "date");
	if(pDate == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	if(VAVAHAL_Date_Verification(pDate->valuestring) != 0)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(recsearch->date, 0, 9);
	strcpy(recsearch->date, pDate->valuestring);
	
	pType = cJSON_GetObjectItem(pRoot, "type");
	if(pDate == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	recsearch->type = pType->valueint;
	if(recsearch->type < VAVA_RECFILE_TYPE_FULLTIME || recsearch->type > VAVA_RECFILE_TYPE_ALL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RecShareDate(cJSON *pRoot, VAVA_RecShareDateSearch *recsharedate)
{
	cJSON *pChlist = NULL;
	cJSON *pChannelitem = NULL;
	cJSON *pChannel = NULL;
	cJSON *pStartdate = NULL;

	int i;
	int channelnum = 0;
	int tmpchannel;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(recsharedate == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(recsharedate, 0, sizeof(VAVA_RecShareDateSearch));

	pChlist = cJSON_GetObjectItem(pRoot, "channellist");
	if(pChlist == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	channelnum = cJSON_GetArraySize(pChlist);
	for(i = 0; i < channelnum; i++)
	{
		pChannelitem = cJSON_GetArrayItem(pChlist, i);
		if(pChannelitem == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		pChannel = cJSON_GetObjectItem(pChannelitem, "channel");
		if(pChannel == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		tmpchannel = pChannel->valueint;
		if(tmpchannel < 0 || tmpchannel >= MAX_CHANNEL_NUM)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		pStartdate = cJSON_GetObjectItem(pChannelitem, "startdate");
		if(pStartdate == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		if(VAVAHAL_Date_Verification(pStartdate->valuestring) != 0)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		recsharedate->list[tmpchannel].channel = 1;
		memset(recsharedate->list[tmpchannel].startdate, 0, 9);
		strcpy(recsharedate->list[tmpchannel].startdate, pStartdate->valuestring);
	}
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RecShareList(cJSON *pRoot, VAVA_RecShareListSearch *recsharelist)
{
	cJSON *pDate = NULL;
	cJSON *pType = NULL;
	cJSON *pChlist = NULL;
	cJSON *pChannelitem = NULL;
	cJSON *pChannel = NULL;

	int i;
	int type;
	int channelnum = 0;
	int tmpchannel;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(recsharelist == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(recsharelist, 0, sizeof(VAVA_RecShareListSearch));

	pDate = cJSON_GetObjectItem(pRoot, "date");
	if(pDate == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	if(VAVAHAL_Date_Verification(pDate->valuestring) != 0)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(recsharelist->date, 0, 9);
	strcpy(recsharelist->date, pDate->valuestring);

	pType = cJSON_GetObjectItem(pRoot, "type");
	if(pType == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	type = pType->valueint;
	if(type < VAVA_RECFILE_TYPE_FULLTIME || type > VAVA_RECFILE_TYPE_ALL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	recsharelist->type = type;
		
	pChlist = cJSON_GetObjectItem(pRoot, "channellist");
	if(pChlist == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	channelnum = cJSON_GetArraySize(pChlist);
	for(i = 0; i < channelnum; i++)
	{
		pChannelitem = cJSON_GetArrayItem(pChlist, i);
		if(pChannelitem == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		pChannel = cJSON_GetObjectItem(pChannelitem, "channel");
		if(pChannel == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}
		
		tmpchannel = pChannel->valueint;
		if(tmpchannel < 0 || tmpchannel >= MAX_CHANNEL_NUM)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		recsharelist->channel[tmpchannel] = 1;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RecImg(cJSON *pRoot, int *channel, VAVA_RecImgGet *recimgget)
{
	int tmpch;
	cJSON *pChannel = NULL;
	cJSON *pDirname = NULL;
	cJSON *pFilename = NULL;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	if(recimgget == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	tmpch = pChannel->valueint;
	if(tmpch < 0 || tmpch >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	*channel = tmpch;

	pDirname = cJSON_GetObjectItem(pRoot, "dirname");
	if(pDirname == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	if(VAVAHAL_Date_Verification(pDirname->valuestring) != 0)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	strcpy(recimgget->dirname, pDirname->valuestring);

	pFilename = cJSON_GetObjectItem(pRoot, "filename");
	if(pFilename == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	if(VAVAHAL_RecFile_Verification(pFilename->valuestring) != 0)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	strcpy(recimgget->filename, pFilename->valuestring);
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RecDelList(cJSON *pRoot, cJSON *pJsonRoot, int sessionid)
{
	cJSON *pDirlist = NULL;
	cJSON *pDiritem = NULL;
	cJSON *pDirname = NULL;
	cJSON *pFilelist = NULL;
	cJSON *pFileitem = NULL;
	cJSON *pFilename = NULL;
	cJSON *pFlag = NULL;
	int i, j;
	int ret;
	int dirnum;
	int filenum;
	int flag;
	char *datelist = NULL;
	char *filelist = NULL;

	if(pRoot == NULL || pJsonRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pDirlist = cJSON_GetObjectItem(pRoot, "dirlist");
	if(pDirlist == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	
	dirnum = cJSON_GetArraySize(pDirlist);
	for(i = 0; i < dirnum; i++)
	{
		pDiritem = cJSON_GetArrayItem(pDirlist, i);
		if(pDiritem == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		pDirname = cJSON_GetObjectItem(pDiritem, "dirname");
		if(pDirname == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		if(VAVAHAL_Date_Verification(pDirname->valuestring) != 0)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		pFlag = cJSON_GetObjectItem(pDiritem, "flag");
		if(pFlag == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}
		flag = pFlag->valueint;
		if(flag < VAVA_RECFILE_DEL_NORMAL || flag > VAVA_RECFILE_DEL_ALLDIR)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		if(flag == VAVA_RECFILE_DEL_ALLDIR)
		{
			datelist = Rec_ImportDateIdx();
			if(datelist == NULL)
			{
				return VAVA_ERR_CODE_RECFILE_NOTFOUND;
			}

			ret = Rec_DelRecDate(datelist, pDirname->valuestring);
			if(ret == 0)
			{
				Rec_ReleaseDateIdx(datelist);

				VAVAHAL_InsertRecList(VAVA_RECFILE_DEL_ALLDIR, pDirname->valuestring, "null");
			}

			if(datelist != NULL)
			{
				free(datelist);
				datelist = NULL;
			}

			usleep(1000);
			
			continue;
		}

		pFilelist = cJSON_GetObjectItem(pDiritem, "filelist");
		if(pFilelist == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}

		filelist = Rec_ImportFileIdx(pDirname->valuestring);
		if(filelist == NULL)
		{
			return VAVA_ERR_CODE_IDX_OPEN_FAIL;
		}

		filenum = cJSON_GetArraySize(pFilelist);
		for(j = 0; j < filenum; j++)
		{
			pFileitem = cJSON_GetArrayItem(pFilelist, j);
			if(pFileitem == NULL)
			{
				Rec_ReleaseFileIdx(filelist, pDirname->valuestring);

				free(filelist);
				filelist = NULL;

				return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
			}

			pFilename = cJSON_GetObjectItem(pFileitem, "filename");
			if(pFilename == NULL)
			{
				Rec_ReleaseFileIdx(filelist, pDirname->valuestring);

				free(filelist);
				filelist = NULL;

				return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
			}

			if(VAVAHAL_RecFile_Verification(pFilename->valuestring) != 0)
			{
				Rec_ReleaseFileIdx(filelist, pDirname->valuestring);
				
				free(filelist);
				filelist = NULL;

				return VAVA_ERR_CODE_PARAM_INVALID;
			}

			ret = Rec_DelRecFile(filelist, pFilename->valuestring);
			if(ret == 0)
			{
				VAVAHAL_InsertRecList(VAVA_RECFILE_DEL_NORMAL, pDirname->valuestring, pFilename->valuestring);
			}

			usleep(1000);
		}

		Rec_ReleaseFileIdx(filelist, pDirname->valuestring);

		free(filelist);
		filelist = NULL;
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_GateWayTime(cJSON *pRoot)
{
	cJSON *pSec = NULL;
	cJSON *pUsec = NULL;	
	cJSON *pZone = NULL;
	cJSON *pDescribe = NULL;

	int sec;
	int usec;
	int zone;
	char tmpstr[128];
	struct timeval time_t;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pSec = cJSON_GetObjectItem(pRoot, "sec");
	if(pSec == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	sec = pSec->valueint;
	if(sec < 0)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pUsec = cJSON_GetObjectItem(pRoot, "usec");
	if(pUsec == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	usec = pUsec->valueint;
	if(usec < 0)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pZone = cJSON_GetObjectItem(pRoot, "zone");
	if(pZone == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	zone = pZone->valueint;
	zone = zone / 3600;
	if(zone < -12 || zone > 12)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pDescribe = cJSON_GetObjectItem(pRoot, "describe");

	time_t.tv_sec = sec;
	time_t.tv_usec = usec;
	if(settimeofday(&time_t, NULL) < 0)
	{
		return VAVA_ERR_CODE_TIME_SET_ERR;
	}

	memset(tmpstr, 0, 128); 
	if(zone < 0)
	{
		sprintf(tmpstr, "/usr/sbin/SetTimeZone %d", zone);
	}
	else
	{
		sprintf(tmpstr, "/usr/sbin/SetTimeZone +%d", zone);
	}
	
	if(VAVAHAL_SystemCmd(tmpstr) != 0)
	{
		return VAVA_ERR_CODE_TIME_SET_ERR;
	}

	g_ntpinfo.ntp = zone;

	if(pDescribe != NULL)
	{
		memset(g_ntpinfo.str, 0, 256);
		strcpy(g_ntpinfo.str, pDescribe->valuestring);
	}

	VAVAHAL_WriteNtpInfo(g_ntpinfo.ntp, g_ntpinfo.str);

	g_bsattr_sync_flag = 1;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_Language(cJSON *pRoot)
{
	cJSON *pLanguage = NULL;
	int language;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pLanguage = cJSON_GetObjectItem(pRoot, "language");
	if(pLanguage == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	language = pLanguage->valueint;
	if(language < VAVA_LANGUAGE_ENGLIST || language > VAVA_LANGUAGE_CHINESE)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	if(g_gatewayinfo.language != language)
	{
		g_gatewayinfo.language = language;
		VAVAHAL_SetLanguage(language);
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_WifiConfig(cJSON *pRoot)
{
	VAVA_WifiConfig wificonfig;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	cJSON *pSSid = NULL;
	cJSON *pPass = NULL;

	pSSid = cJSON_GetObjectItem(pRoot, "ssid");
	if(pSSid == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	if(strlen(pSSid->valuestring) < 0 || strlen(pSSid->valuestring) > 32)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pPass = cJSON_GetObjectItem(pRoot, "passwd");
	if(pPass == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	if(strlen(pPass->valuestring) < 0 || strlen(pPass->valuestring) > 32)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(&wificonfig, 0, sizeof(VAVA_WifiConfig));
	strcpy(wificonfig.ssid, pSSid->valuestring);
	strcpy(wificonfig.pass, pPass->valuestring);

	VAVAHAL_InsertCmdList(0, 0, VAVA_CMD_WIFI_CONFIG, (void *)&wificonfig, sizeof(VAVA_WifiConfig));

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_Camerapair(cJSON *pRoot, int *channel, char *sn, char *name)
{
	cJSON *pSn = NULL;
	cJSON *pChannel = NULL;
	cJSON *pCameraName = NULL;

	int tmpch;
	*channel = -1;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pSn = cJSON_GetObjectItem(pRoot, "sn");
	if(pSn == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	
	strcpy(sn, pSn->valuestring);

	if(strlen(sn) == 0)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel != NULL)
	{
		tmpch = pChannel->valueint;
		if(tmpch < 0 || tmpch >= MAX_CHANNEL_NUM)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		*channel = tmpch;
	}

	pCameraName = cJSON_GetObjectItem(pRoot, "cameraName");
	if(pCameraName != NULL)
	{
		strcpy(name, pCameraName->valuestring);
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_ClearPair(cJSON *pRoot)
{
	cJSON *pType = NULL;
	cJSON *pChannel = NULL;
	int i;
	int type;
	int channel;

	VAVA_ClearCamera cleardata;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pType = cJSON_GetObjectItem(pRoot, "type");
	if(pType == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	type = pType->valueint;
	if(type < VAVA_PAIR_CLEAR_CHANNEL || type > VAVA_PAIR_CLEAR_ALL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	if(type == VAVA_PAIR_CLEAR_CHANNEL)
	{
		pChannel = cJSON_GetObjectItem(pRoot, "channel");
		if(pChannel == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}
		channel = pChannel->valueint;
		if(channel < 0 || channel >= MAX_CHANNEL_NUM)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

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
#ifdef FREQUENCY_OFFSET
		cleardata.index = g_pair[channel].index;
#endif

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
	else
	{
		//关闭录像功能
		VAVAHAL_CloseRec(1, 0);

		//关闭云录像功能
		VAVAHAL_CloseCloudRec(1, 0);
		
		for(i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			if(g_pair[i].nstat == 1)
			{
				//停止录像
				VAVAHAL_StopManualRec(i);
				VAVAHAL_StopFullTimeRec(i);
				VAVAHAL_StopAlarmRec(i);
				VAVAHAL_StopAlarmCloud(i);
		
				//休眠摄像机
				VAVAHAL_SleepCamera(i);

				//恢复参数
				VAVAHAL_ResetCamera(i);

				g_cloudflag[i] = 0;
			}

			memset(&cleardata, 0, sizeof(VAVA_ClearCamera));
			cleardata.addr = g_pair[i].addr;
#ifdef FREQUENCY_OFFSET
			cleardata.index = g_pair[i].index;
#endif

			//清除配对信息
			g_pair[i].nstat = 0;
			g_pair[i].addr = 0xFFFFFFFF;
			g_pair[i].ipaddr = 0xFFFFFFFF;
			g_pair[i].lock = 0;
			memset(g_pair[i].mac, 0, 18);
			memset(g_pair[i].id, 0, 32);

			g_cloudrecheck[i] = 1;

			VAVAHAL_InitCameraInfo(i);

			VAVAHAL_InsertCmdList(i, -1, VAVA_CMD_CLEARMATCH, &cleardata, sizeof(VAVA_ClearCamera));
		}

		//写入配置文件
		VAVAHAL_WritePairInfo();

		//开启录像功能
		VAVAHAL_OpenRec(1, 0);
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_NewVersion(cJSON *pRoot, int sessionid)
{
	cJSON *pUrl = NULL;
	cJSON *pType = NULL;
	cJSON *pChannel = NULL;
	
	int type;
	int channel = -1;

	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: g_update.status = %d\n", FUN, LINE, g_update.status);

	if(g_update.status != VAVA_UPDATE_IDLE)
	{
		return VAVA_ERR_CODE_UPDATE_NOIDLE;
	}

	//初始化升级参数
	VAVAHAL_InitUpdate();

	pUrl = cJSON_GetObjectItem(pRoot, "url");
	if(pUrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	pType = cJSON_GetObjectItem(pRoot, "type");
	if(pType == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	type = pType->valueint;
	if(type < VAVA_UPDATE_TYPE_GATEWAY || type > VAVA_UPDATE_TYPE_CAMERA)
	{
		return VAVA_ERR_CODE_UPDATE_TYPE_ERR;
	}

	if(type == VAVA_UPDATE_TYPE_CAMERA)
	{
		pChannel = cJSON_GetObjectItem(pRoot, "channel");
		if(pChannel == NULL)
		{
			return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
		}
		channel = pChannel->valueint;
		if(channel < 0 || channel > MAX_CHANNEL_NUM)
		{
			return VAVA_ERR_CODE_PARAM_INVALID;
		}

		//判断电量
		if(g_camerainfo_dynamic[channel].battery <= CAMERA_UPDATE_LOW_POWER)
		//	|| g_camerainfo_dynamic[channel].voltage <= 7270)
		{
			return VAVA_ERR_CODE_POWER_LOW;
		}
	}

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, update_pth, NULL);
	pthread_attr_destroy(&attr);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create updatepth err, ret = %d\n", FUN, LINE, ret);
		
		g_update.status = VAVA_UPDATE_IDLE;
		return VAVA_ERR_CODE_INTO_UPDATE_FAIL;
	}

	g_update.sessionid = sessionid;
	g_update.type = type;
	strcpy(g_update.url, pUrl->valuestring);
	g_update.current = channel;
	
	g_update.status = VAVA_UPDATE_START;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_Doman(cJSON *pRoot)
{
	cJSON *pDoman_factory = NULL;
	cJSON *pDoman_user = NULL;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pDoman_factory = cJSON_GetObjectItem(pRoot, "domain_factory");
	if(pDoman_factory == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	memset(g_gatewayinfo.domain_factory, 0, 64);
	strcpy(g_gatewayinfo.domain_factory, pDoman_factory->valuestring);

	pDoman_user = cJSON_GetObjectItem(pRoot, "domain_user");
	if(pDoman_user == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	memset(g_gatewayinfo.domain_user, 0, 64);
	strcpy(g_gatewayinfo.domain_user, pDoman_user->valuestring);

	VAVAHAL_WriteDoman();
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_P2PServer(cJSON *pRoot)
{
	cJSON *pInitstr = NULL;
	cJSON *pCrckey = NULL;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pInitstr = cJSON_GetObjectItem(pRoot, "initstr");
	if(pInitstr == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	pCrckey = cJSON_GetObjectItem(pRoot, "crckey");
	if(pCrckey == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	memset(g_gatewayinfo.initstr, 0, 128);
	strcpy(g_gatewayinfo.initstr, pInitstr->valuestring);

	memset(g_gatewayinfo.crckey, 0, 16);
	strcpy(g_gatewayinfo.crckey, pCrckey->valuestring);

	VAVAHAL_WriteSyInfo();

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_Param_Parse_NasServer(cJSON *pRoot)
{
	int ret;
	cJSON *pCtrl = NULL;
	cJSON *pIp = NULL;
	cJSON *pPath = NULL;
	int ctrl;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pCtrl = cJSON_GetObjectItem(pRoot, "ctrl");
	if(pCtrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	
	ctrl = pCtrl->valueint;
	if(ctrl < 0 || ctrl > 1)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	
	pIp = cJSON_GetObjectItem(pRoot, "ip");
	if(pIp == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	ret = VAVAHAL_CheckIP(pIp->valuestring);
	if(ret != 0)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	if(ctrl == 1)
	{
		ret = VAVAHAL_PingIPStatus(pIp->valuestring);
		if(ret != 0)
		{
			return VAVA_ERR_CODE_IP_NOTCONNECT;
		}
	}

	pPath = cJSON_GetObjectItem(pRoot, "path");
	if(pPath == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	ret = VAVAHAL_CheckLinuxPath(pPath->valuestring);
	if(ret != 0)
	{
		return VAVA_ERR_CODE_PAHT_INVALID;
	}

	if(ctrl == 0)
	{
		//关闭直接返回
		memset(&g_nas_config, 0, sizeof(VAVA_NasConfig));
		g_nas_config.ctrl = 0;
		strcpy(g_nas_config.ip, pIp->valuestring);
		strcpy(g_nas_config.path, pPath->valuestring);

		VAVAHAL_WriteNasConfig();

		g_nas_status = VAVA_NAS_STATUS_IDLE;
		g_nas_change = 0;
		g_devinfo_update = 1;

		return VAVA_ERR_CODE_SUCCESS;
	}

	g_nas_config.ctrl = 1;
	strcpy(g_nas_config.ip, pIp->valuestring);
	strcpy(g_nas_config.path, pPath->valuestring);
	
	VAVAHAL_WriteNasConfig();
	
	g_nas_status = VAVA_NAS_STATUS_CONFIGING;
	g_nas_change = 1;
	g_devinfo_update = 1;

	g_bsattr_sync_flag = 1;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RecPlay(cJSON *pRoot, int sessionid, int *channel, int *recchannel)
{
	int i;
	int ret;
	int tmpch;
	int type;
	int timeout = 10;
	cJSON *pChannel = NULL;
	cJSON *pDirname = NULL;
	cJSON *pFilename = NULL;
	cJSON *pType = NULL;
	char path[128];
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pthread_mutex_lock(&mutex_recplay_lock);

	//查询会话是否处于回放状态
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_recplay[i].fd != NULL && g_recplay[i].sesssion == sessionid)
		{
			#if 0
			pthread_mutex_unlock(&mutex_recplay_lock);
			*recchannel = i;
			return VAVA_ERR_CODE_RECPLAY_REREQ;
			#else
			g_recplay[i].ctrl = 2;
			timeout = 20;

			while(g_running)
			{
				if(g_recplay[i].fd == NULL || timeout-- <= 0)
				{
					break;
				}

				usleep(200000);
			}

			break;
			#endif
		}
	}
	
	//检查是否有空闲通道
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_recplay[i].fd == NULL)
		{
			break;
		}
	}

	if(i == MAX_CHANNEL_NUM)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_RECPLAY_NOIDLE;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	tmpch = pChannel->valueint;
	if(tmpch < 0 || tmpch >= MAX_CHANNEL_NUM)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	*channel = tmpch;

	pDirname = cJSON_GetObjectItem(pRoot, "dirname");
	if(pDirname == NULL)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	ret = VAVAHAL_Date_Verification(pDirname->valuestring);
	if(ret != 0)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	
	pFilename = cJSON_GetObjectItem(pRoot, "filename");
	if(pFilename == NULL)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	ret = VAVAHAL_RecFile_Verification(pFilename->valuestring);
	if(ret != 0)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	pType = cJSON_GetObjectItem(pRoot, "type");
	if(pType == NULL)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	type = pType->valueint;
	if(type < VAVA_RECFILE_TRANSPORT_NORMA || type > VAVA_RECFILE_TRANSPORT_FAST)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, pDirname->valuestring, pFilename->valuestring);

	g_recplay[i].fd = fopen(path, "r");
	if(g_recplay[i].fd == NULL)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_RECFILE_NOTFOUND;
	}

	g_recplay[i].sesssion = sessionid;
	g_recplay[i].type = type;
	memset(g_recplay[i].dirname, 0, 9);
	memset(g_recplay[i].filename, 0, 11);
	strcpy(g_recplay[i].dirname, pDirname->valuestring);
	strcpy(g_recplay[i].filename, pFilename->valuestring);
	
	*recchannel = i;

	pthread_mutex_unlock(&mutex_recplay_lock);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RecPlayCtrl(cJSON *pRoot, int sessionid, int *channel, int *ctrl)
{
	cJSON *pChannel = NULL;
	cJSON *pToken = NULL;
	cJSON *pCtrl = NULL;

	int i;
	int tmpch;
	int token;
	int tmpctrl;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	tmpch = pChannel->valueint;
	if(tmpch < 0 || tmpch >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	*channel = tmpch;

	pToken = cJSON_GetObjectItem(pRoot, "token");
	if(pToken == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	token = pToken->valueint;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_recplay[i].sesssion == sessionid)
		{
			if(token != g_recplay[i].token)
			{
				return VAVA_ERR_CODE_TOKEN_FAIL;
			}

			break;
		}
	}

	if(i >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_TOKEN_FAIL;
	}

	pCtrl = cJSON_GetObjectItem(pRoot, "ctrl");
	if(pCtrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	tmpctrl = pCtrl->valueint;
	if(tmpctrl < VAVA_RECFILE_PLAY_CTRL_CONTINUE || tmpctrl > VAVA_RECFILE_PLAY_CTRL_STOP)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	*ctrl = tmpctrl;
	g_recplay[i].ctrl = tmpctrl;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_RfTest(cJSON *pRoot, int sessionid)
{
	cJSON *pCtrl = NULL;
	int ctrl;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pCtrl = cJSON_GetObjectItem(pRoot, "ctrl");
	if(pCtrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	ctrl = pCtrl->valueint;
	if(ctrl < VAVA_CTRL_DISABLE || ctrl > VAVA_CTRL_ENABLE)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	if(ctrl == VAVA_CTRL_ENABLE)
	{
		if(g_rftest_flag == 1)
		{
			return VAVA_ERR_CODE_RFTEST_BESTART;
		}

		g_rftest_session = sessionid;
	}
	else
	{
		g_rftest_flag = 0;
		g_rftest_session = -1;
	}

	RF_TestCtrl(ctrl);
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_Sn(cJSON *pRoot)
{
	cJSON *pSn = NULL;
	char cmd[128];

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pSn = cJSON_GetObjectItem(pRoot, "sn");
	if(pSn == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	if(pSn->valuestring == NULL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	if((strncmp(pSn->valuestring, "P0202", strlen("P0202")) != 0 && strncmp(pSn->valuestring, "64XI7D", strlen("64XI7D")) != 0) 
		|| strlen(pSn->valuestring) != 25)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	memset(cmd, 0, 128);
	sprintf(cmd, "eeprom_test -w %s", pSn->valuestring);
	VAVAHAL_SystemCmd(cmd);

	VAVAHAL_SystemCmd("sync");

	//退出让看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_Buzzertype(cJSON *pRoot)
{
	cJSON *pType = NULL;
	int type;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pType = cJSON_GetObjectItem(pRoot, "type");
	if(pType == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	type = pType->valueint;

	if(type < VAVA_BUZZER_TYPE_OPEN || type > VAVA_BUZZER_TYPE_INTERVAL)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	g_buzzer_type = type;
	g_buzzer_flag = 1;

	g_devinfo_update = 1;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_CloudDebugLevel(cJSON *pRoot)
{
	cJSON *pDebugLever = NULL;
	int level;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pDebugLever = cJSON_GetObjectItem(pRoot, "lever");
	if(pDebugLever == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	level = pDebugLever->valueint;
	if(level < 2 || level > 6)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	g_clouddebuglever = level;
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_ComCtrl(cJSON *pRoot)
{
	cJSON *pCtrl = NULL;
	int ctrl;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pCtrl = cJSON_GetObjectItem(pRoot, "ctrl");
	if(pCtrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	ctrl = pCtrl->valueint;
	if(ctrl < 0 || ctrl > 1)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	if(ctrl == 0)
	{
		VAVAHAL_SystemCmd("/bin/cp /etc/inittab_n  /etc/inittab");
		VAVAHAL_SystemCmd("sync");
	}
	else
	{
		VAVAHAL_SystemCmd("/bin/cp /etc/inittab_y  /etc/inittab");
		VAVAHAL_SystemCmd("sync");
	}
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_IPCComCtrl(cJSON *pRoot, int *channel, int *ctrl)
{
	cJSON *pChannel = NULL;
	cJSON *pCtrl = NULL;

	int tmpch;
	int tmpctrl;

	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pChannel = cJSON_GetObjectItem(pRoot, "channel");
	if(pChannel == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	tmpch = pChannel->valueint;
	if(tmpch < 0 || tmpch >= MAX_CHANNEL_NUM)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	*channel = tmpch;

	pCtrl = cJSON_GetObjectItem(pRoot, "ctrl");
	if(pCtrl == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	tmpctrl = pCtrl->valueint;
	if(tmpctrl < 0 || tmpctrl > 1)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	*ctrl = tmpctrl;

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamParse_LogLevel(cJSON *pRoot)
{
	cJSON *pLogLever = NULL;
	int level;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pLogLever = cJSON_GetObjectItem(pRoot, "lever");
	if(pLogLever == NULL)
	{
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	level = pLogLever->valueint;
	if(level < LOG_LEVEL_OFF || level > LOG_LEVEL_DEBUG)
	{
		return VAVA_ERR_CODE_PARAM_INVALID;
	}

	g_debuglevel = level;
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_Auth(cJSON *pJsonRoot)
{
	cJSON_AddStringToObject(pJsonRoot, "releasedate", g_gatewayinfo.releasedate);
	cJSON_AddStringToObject(pJsonRoot, "hardver", g_gatewayinfo.hardver);
	cJSON_AddStringToObject(pJsonRoot, "softver", g_gatewayinfo.softver);
	cJSON_AddStringToObject(pJsonRoot, "rfver", g_gatewayinfo.rfver);
	cJSON_AddStringToObject(pJsonRoot, "rfhw", g_gatewayinfo.rfhw);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_Channel(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_Ctrl(cJSON *pJsonRoot, int channel, int ctrl)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "ctrl", ctrl);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [TEST316] PLAY CTRL SUCCESS, ctrl = %d\n", FUN, LINE, ctrl);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_Status(cJSON * pJsonRoot, int channel, int status)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "status", status);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_VideoInfo(cJSON *pJsonRoot, int channel)
{
	int wakeupnum;
	int videonum;

	VAVAHAL_GetChannel_ConnectNum(channel, &wakeupnum, &videonum);
	
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "videocodec", g_camerainfo[channel].videocodec);
	cJSON_AddNumberToObject(pJsonRoot, "videores", g_camerainfo[channel].s_res);
	cJSON_AddNumberToObject(pJsonRoot, "videoframerate", g_camerainfo[channel].s_fps);
	cJSON_AddNumberToObject(pJsonRoot, "videobitrate", g_camerainfo[channel].s_bitrate);
	cJSON_AddNumberToObject(pJsonRoot, "videonum", videonum);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_AudioOpen(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "audiocodec", g_camerainfo[channel].audiocodec);
	cJSON_AddNumberToObject(pJsonRoot, "audiorate", g_camerainfo[channel].samprate);
	cJSON_AddNumberToObject(pJsonRoot, "audiobitper", g_camerainfo[channel].a_bit);
	cJSON_AddNumberToObject(pJsonRoot, "audiochannel", g_camerainfo[channel].channel);
	cJSON_AddNumberToObject(pJsonRoot, "audioframerate", g_camerainfo[channel].a_fps);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_GateWayInfo(cJSON *pJsonRoot)
{
	int i;
	int pairnum;
	cJSON *pCameraList = NULL;
	cJSON *pItem[MAX_CHANNEL_NUM];

	pCameraList = cJSON_CreateArray();
	if(pCameraList == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return VAVA_ERR_CODE_JSON_MALLOC_FIAL;
	}

	pairnum = 0;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			pItem[i] = cJSON_CreateObject();
			if(pItem[i] == NULL)
			{
				cJSON_Delete(pCameraList);
				return VAVA_ERR_CODE_JSON_MALLOC_FIAL;
			}

			cJSON_AddNumberToObject(pItem[i], "channel", i);
			cJSON_AddNumberToObject(pItem[i], "online", VAVAHAL_GetCameraOnlineStatus(i));
			cJSON_AddStringToObject(pItem[i], "id", g_pair[i].id);

			cJSON_AddItemToArray(pCameraList, pItem[i]);
			pairnum++;
		}
	}

	cJSON_AddStringToObject(pJsonRoot, "hardver", g_gatewayinfo.hardver);
	cJSON_AddStringToObject(pJsonRoot, "softver", g_gatewayinfo.softver);
	cJSON_AddStringToObject(pJsonRoot, "sn", g_gatewayinfo.sn);
	cJSON_AddStringToObject(pJsonRoot, "mac", g_gatewayinfo.mac);
	cJSON_AddStringToObject(pJsonRoot, "rfver", g_gatewayinfo.rfver);
	cJSON_AddStringToObject(pJsonRoot, "rfhw", g_gatewayinfo.rfhw);
	cJSON_AddStringToObject(pJsonRoot, "f_code", VAVA_GATEWAY_F_CODE);
	cJSON_AddStringToObject(pJsonRoot, "f_secret", VAVA_GATEWAY_F_SECRET);
	cJSON_AddStringToObject(pJsonRoot, "f_firmnum", VAVA_GATEWAY_F_FIRMNUM);
	cJSON_AddStringToObject(pJsonRoot, "f_appversionin", VAVA_GATEWAY_F_APPVER_IN);
	cJSON_AddStringToObject(pJsonRoot, "f_appversionout", VAVA_GATEWAY_F_APPVER_OUT);
	cJSON_AddStringToObject(pJsonRoot, "h_code", VAVA_GATEWAY_H_CODE);
	cJSON_AddStringToObject(pJsonRoot, "h_secret", VAVA_GATEWAY_H_SECRET);
	cJSON_AddStringToObject(pJsonRoot, "h_devicenum", VAVA_GATEWAY_H_DEVICNUM);
	cJSON_AddStringToObject(pJsonRoot, "h_appversionin", VAVA_GATEWAY_H_APPVER_IN);
	cJSON_AddStringToObject(pJsonRoot, "h_appversionout", VAVA_GATEWAY_H_APPVER_OUT);
	
	cJSON_AddNumberToObject(pJsonRoot, "signal", g_gatewayinfo.signal);
	cJSON_AddNumberToObject(pJsonRoot, "language", g_gatewayinfo.language);
	
	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_NOCRAD)
	{
		cJSON_AddNumberToObject(pJsonRoot, "sdstatus", g_gatewayinfo.sdstatus);
		cJSON_AddNumberToObject(pJsonRoot, "totolsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "usedsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "freesize", 0);
	}
	else if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.sdstatus == VAVA_SD_STATUS_FULL)
	{
		if(g_gatewayinfo.format_flag == 1)
		{
			cJSON_AddNumberToObject(pJsonRoot, "sdstatus", VAVA_SD_STATUS_NEEDFORMAT);
		}
		else
		{
			cJSON_AddNumberToObject(pJsonRoot, "sdstatus", g_gatewayinfo.sdstatus);
		}

		cJSON_AddNumberToObject(pJsonRoot, "totolsize", g_gatewayinfo.totol);
		cJSON_AddNumberToObject(pJsonRoot, "usedsize", g_gatewayinfo.used);
		cJSON_AddNumberToObject(pJsonRoot, "freesize", g_gatewayinfo.free);
	}

	#ifdef NAS_NFS
	cJSON_AddNumberToObject(pJsonRoot, "nasstatus", g_nas_status);
	if(g_nas_status == VAVA_NAS_STATUS_SYNC || g_nas_status == VAVA_NAS_STATUS_LACKOF_SPACE)
	{
		cJSON_AddNumberToObject(pJsonRoot, "nas_totolsize", g_gatewayinfo.nas_totol);
		cJSON_AddNumberToObject(pJsonRoot, "nas_usedsize", g_gatewayinfo.nas_used);
		cJSON_AddNumberToObject(pJsonRoot, "nas_freesize", g_gatewayinfo.nas_free);
	}
	else
	{
		cJSON_AddNumberToObject(pJsonRoot, "nas_totolsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "nas_usedsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "nas_freesize", 0);
	}
	#endif
	cJSON_AddStringToObject(pJsonRoot, "nas_ip", g_nas_config.ip);
	cJSON_AddStringToObject(pJsonRoot, "nas_path", g_nas_config.path);
	cJSON_AddNumberToObject(pJsonRoot, "alarmrecord", g_rectime.alarm_time);
	cJSON_AddNumberToObject(pJsonRoot, "ntp", g_ntpinfo.ntp);
	cJSON_AddStringToObject(pJsonRoot, "ntp_describe", g_ntpinfo.str);
	
	cJSON_AddNumberToObject(pJsonRoot, "cameranum", pairnum);
	cJSON_AddItemToObject(pJsonRoot, "cameralist", pCameraList);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_SDInfo(cJSON *pJsonRoot)
{
	int timeout = 150; 
	
	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_NOCRAD)
	{
		cJSON_AddNumberToObject(pJsonRoot, "sdstatus", 0);
		cJSON_AddNumberToObject(pJsonRoot, "totolsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "usedsize", 0);
		cJSON_AddNumberToObject(pJsonRoot, "freesize", 0);
	}
	else if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.sdstatus == VAVA_SD_STATUS_FULL)
	{
		if(g_gatewayinfo.format_flag == 1)
		{
			cJSON_AddNumberToObject(pJsonRoot, "sdstatus", VAVA_SD_STATUS_NEEDFORMAT);
		}
		else
		{
			cJSON_AddNumberToObject(pJsonRoot, "sdstatus", g_gatewayinfo.sdstatus);

			//10秒超时
			while(g_running)
			{
				if(timeout-- <= 0 || g_gatewayinfo.totol > 100)
				{
					break;
				}

				usleep(100000);
			}
		}

		cJSON_AddNumberToObject(pJsonRoot, "totolsize", g_gatewayinfo.totol);
		cJSON_AddNumberToObject(pJsonRoot, "usedsize", g_gatewayinfo.used);
		cJSON_AddNumberToObject(pJsonRoot, "freesize", g_gatewayinfo.free);
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_BatteryInfo(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "lever", g_camerainfo_dynamic[channel].battery);
	cJSON_AddNumberToObject(pJsonRoot, "voltage", g_camerainfo_dynamic[channel].voltage);
	#ifdef BATTEY_INFO
	cJSON_AddNumberToObject(pJsonRoot, "temperature", g_camerainfo_dynamic[channel].temperature);
	cJSON_AddNumberToObject(pJsonRoot, "electricity", g_camerainfo_dynamic[channel].electricity);
	#endif

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_CameraInfo(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "armingstatus", g_camera_arminfo_v1[channel].status);
	cJSON_AddNumberToObject(pJsonRoot, "armingtype", g_camera_arminfo_v1[channel].type);
	cJSON_AddNumberToObject(pJsonRoot, "micstatus", g_camerainfo[channel].mic);
	cJSON_AddNumberToObject(pJsonRoot, "speakerstatus", g_camerainfo[channel].speeker);
	cJSON_AddNumberToObject(pJsonRoot, "type", 0);
	cJSON_AddStringToObject(pJsonRoot, "f_code", g_ipc_otainfo[channel].f_code);
	cJSON_AddStringToObject(pJsonRoot, "f_secret", g_ipc_otainfo[channel].f_secret);
	cJSON_AddStringToObject(pJsonRoot, "f_firmnum", VAVA_CAMERA_F_FIRMNUM);
	cJSON_AddStringToObject(pJsonRoot, "f_appversionin", g_ipc_otainfo[channel].f_inver);
	cJSON_AddStringToObject(pJsonRoot, "f_appversionout", g_ipc_otainfo[channel].f_outver);
	cJSON_AddStringToObject(pJsonRoot, "h_code", VAVA_CAMERA_H_CODE);
	cJSON_AddStringToObject(pJsonRoot, "h_secret", VAVA_CAMERA_H_SECRET);
	cJSON_AddStringToObject(pJsonRoot, "h_devicenum", VAVA_CAMERA_H_DEVICNUM);
	cJSON_AddStringToObject(pJsonRoot, "h_appversionin", VAVA_CAMERA_H_APPVER_IN);
	cJSON_AddStringToObject(pJsonRoot, "h_appversionout", VAVA_CAMERA_H_APPVER_OUT);
	cJSON_AddNumberToObject(pJsonRoot, "lever", g_camerainfo_dynamic[channel].battery);
	cJSON_AddNumberToObject(pJsonRoot, "voltage", g_camerainfo_dynamic[channel].voltage);
	#ifdef BATTEY_INFO
	cJSON_AddNumberToObject(pJsonRoot, "temperature", g_camerainfo_dynamic[channel].temperature);
	cJSON_AddNumberToObject(pJsonRoot, "electricity", g_camerainfo_dynamic[channel].electricity);
	#endif
	cJSON_AddNumberToObject(pJsonRoot, "powermode", g_camerainfo[channel].powermode);
	cJSON_AddNumberToObject(pJsonRoot, "signal", g_camerainfo_dynamic[channel].signal);
	cJSON_AddNumberToObject(pJsonRoot, "videocodec", g_camerainfo[channel].videocodec);
	cJSON_AddNumberToObject(pJsonRoot, "m_res", g_camerainfo[channel].m_res);
	cJSON_AddNumberToObject(pJsonRoot, "m_fps", g_camerainfo[channel].m_fps);
	cJSON_AddNumberToObject(pJsonRoot, "m_bitrate", g_camerainfo[channel].m_bitrate);
	cJSON_AddNumberToObject(pJsonRoot, "s_res", g_camerainfo[channel].s_res);
	cJSON_AddNumberToObject(pJsonRoot, "s_fps", g_camerainfo[channel].s_fps);
	cJSON_AddNumberToObject(pJsonRoot, "s_bitrate", g_camerainfo[channel].s_bitrate);
	cJSON_AddNumberToObject(pJsonRoot, "mirrormode", g_camerainfo[channel].mirror);
	cJSON_AddNumberToObject(pJsonRoot, "irmode", g_camerainfo[channel].irledmode);
	cJSON_AddNumberToObject(pJsonRoot, "pirsensitivity", g_pir_sensitivity[channel]);
	cJSON_AddNumberToObject(pJsonRoot, "audiocodec", g_camerainfo[channel].audiocodec);
	cJSON_AddNumberToObject(pJsonRoot, "audiorate", g_camerainfo[channel].samprate);
	cJSON_AddNumberToObject(pJsonRoot, "audiobitper", g_camerainfo[channel].a_bit);
	cJSON_AddNumberToObject(pJsonRoot, "audiochannel", g_camerainfo[channel].channel);
	cJSON_AddNumberToObject(pJsonRoot, "audioframerate", g_camerainfo[channel].a_fps);
	cJSON_AddNumberToObject(pJsonRoot, "md_enable", g_camerainfo[channel].mdctrl);
	cJSON_AddNumberToObject(pJsonRoot, "md_startx", g_camerainfo[channel].md_startx);
	cJSON_AddNumberToObject(pJsonRoot, "md_endx", g_camerainfo[channel].md_endx);
	cJSON_AddNumberToObject(pJsonRoot, "md_starty", g_camerainfo[channel].md_starty);
	cJSON_AddNumberToObject(pJsonRoot, "md_endy", g_camerainfo[channel].md_endy);
	cJSON_AddNumberToObject(pJsonRoot, "md_sensitivity", g_camerainfo[channel].md_sensitivity);
	cJSON_AddStringToObject(pJsonRoot, "hardver", (char *)g_camerainfo[channel].hardver);
	cJSON_AddStringToObject(pJsonRoot, "softver", (char *)g_camerainfo[channel].softver);
	cJSON_AddStringToObject(pJsonRoot, "id", g_pair[channel].id);
	cJSON_AddStringToObject(pJsonRoot, "rfver", (char *)g_camerainfo[channel].rfver);
	cJSON_AddStringToObject(pJsonRoot, "rfhw", (char *)g_camerainfo[channel].rfhw);
	cJSON_AddStringToObject(pJsonRoot, "mac", g_pair[channel].mac);
	cJSON_AddNumberToObject(pJsonRoot, "push_ctrl", g_pushflag[channel].push);
	cJSON_AddNumberToObject(pJsonRoot, "email_ctrl", g_pushflag[channel].email);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_CameraConfig(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "irmode", g_camerainfo[channel].irledmode);
	cJSON_AddNumberToObject(pJsonRoot, "mirrormode", g_camerainfo[channel].mirror);
	cJSON_AddNumberToObject(pJsonRoot, "armtype", g_camera_arminfo_v1[channel].type);
	cJSON_AddNumberToObject(pJsonRoot, "pirsensitivity", g_pir_sensitivity[channel]);
	cJSON_AddNumberToObject(pJsonRoot, "mdsensitivity", g_camerainfo[channel].md_sensitivity);
	cJSON_AddNumberToObject(pJsonRoot, "email", g_pushflag[channel].email);
	cJSON_AddNumberToObject(pJsonRoot, "push", g_pushflag[channel].push);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_VideoQuality(cJSON *pJsonRoot, int channel, int streamtype)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);

	if(streamtype == VAVA_STREAM_TYPE_MAIN)
	{
		cJSON_AddNumberToObject(pJsonRoot, "quality", VAVA_VIDEO_QUALITY_BEST);
	}
	else
	{
		cJSON_AddNumberToObject(pJsonRoot, "quality", g_camerainfo[channel].v_quality);
	}
	
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_MirrMode(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "mirrormode", g_camerainfo[channel].mirror);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_IrMode(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "irmode", g_camerainfo[channel].irledmode);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_PushConfig(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "ctrl", g_pushflag[channel].push);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_EmailConfig(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "ctrl", g_pushflag[channel].email);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_CameraArmInfo_v1(cJSON *pJsonRoot, int channel)
{
	int armlist;
	int weekday;

	char timestr[9];
	char weekstr[2];
	char days[8];

	cJSON *pArmList = NULL;
	cJSON *pArmdata[MAX_ARM_LIST_NUM];

	pArmList = cJSON_CreateArray();
	if(pArmList == NULL)
	{
		return VAVA_ERR_CODE_JSON_MALLOC_FIAL;
	}

	for(armlist = 0; armlist < MAX_ARM_LIST_NUM; armlist++)
	{
		if(g_camera_arminfo_v1[channel].armdata[armlist].nstat == 0)
		{
			break;
		}

		pArmdata[armlist] = cJSON_CreateObject();
		if(pArmdata[armlist] == NULL)
		{
			cJSON_Delete(pArmList);
			return VAVA_ERR_CODE_JSON_MALLOC_FIAL;
		}

		//起始时间
		memset(timestr, 0, 9);
		sprintf(timestr, "%02d%02d%02d", g_camera_arminfo_v1[channel].armdata[armlist].s_h,
			                             g_camera_arminfo_v1[channel].armdata[armlist].s_m,
			                             g_camera_arminfo_v1[channel].armdata[armlist].s_s);
		cJSON_AddStringToObject(pArmdata[armlist], "starttime", timestr);

		//结束时间
		memset(timestr, 0, 9);
		sprintf(timestr, "%02d%02d%02d", g_camera_arminfo_v1[channel].armdata[armlist].e_h,
			                             g_camera_arminfo_v1[channel].armdata[armlist].e_m,
			                             g_camera_arminfo_v1[channel].armdata[armlist].e_s);
		cJSON_AddStringToObject(pArmdata[armlist], "endtime", timestr);

		memset(days, 0, 8);
		for(weekday = 0; weekday < VAVA_WEEKDAYS; weekday++)
		{
			if(g_camera_arminfo_v1[channel].armdata[armlist].weekday[weekday] == 1)
			{
				memset(weekstr, 0, 2);
				sprintf(weekstr, "%d", weekday);
				strcat(days, weekstr);
			}
		}

		cJSON_AddStringToObject(pArmdata[armlist], "days", days);
		cJSON_AddItemToArray(pArmList, pArmdata[armlist]);
	}

	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "type", g_camera_arminfo_v1[channel].type);
	cJSON_AddItemToObject(pJsonRoot, "arminglist", pArmList);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_PirSensitivity(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "sensitivity", g_pir_sensitivity[channel]);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_MDInfo(cJSON *pJsonRoot, int channel)
{
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "enable", g_camerainfo[channel].mdctrl);
	cJSON_AddNumberToObject(pJsonRoot, "startx", g_camerainfo[channel].md_startx);
	cJSON_AddNumberToObject(pJsonRoot, "endx", g_camerainfo[channel].md_endx);
	cJSON_AddNumberToObject(pJsonRoot, "starty", g_camerainfo[channel].md_starty);
	cJSON_AddNumberToObject(pJsonRoot, "endy", g_camerainfo[channel].md_endy);
	cJSON_AddNumberToObject(pJsonRoot, "sensitivity", g_camerainfo[channel].md_sensitivity);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_RecTimeInfo(cJSON *pJsonRoot)
{
	//cJSON_AddNumberToObject(pJsonRoot, "alltimerecord", g_rectime.full_time);
	cJSON_AddNumberToObject(pJsonRoot, "alarmrecord", g_rectime.alarm_time);
	//cJSON_AddNumberToObject(pJsonRoot, "manualrecord", g_rectime.manaua_time);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_GateWayTime(cJSON *pJsonRoot)
{
	struct timeval get_time;
	struct timezone get_tz;

	gettimeofday(&get_time, &get_tz);

	cJSON_AddNumberToObject(pJsonRoot, "sec", get_time.tv_sec);
	cJSON_AddNumberToObject(pJsonRoot, "usec", get_time.tv_usec);
	cJSON_AddNumberToObject(pJsonRoot, "zone", get_tz.tz_minuteswest * (-60));
	cJSON_AddStringToObject(pJsonRoot, "describe", g_ntpinfo.str);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_UpStatus(cJSON *pJsonRoot)
{
	cJSON_AddNumberToObject(pJsonRoot, "status", g_update.status);

	if(g_update.status != VAVA_UPDATE_IDLE)
	{
		cJSON_AddNumberToObject(pJsonRoot, "type", g_update.type);

		if(g_update.type == VAVA_UPDATE_TYPE_CAMERA)
		{
			cJSON_AddNumberToObject(pJsonRoot, "channel", g_update.current);
		}

		if(g_update.status == VAVA_UPDATE_LOADING || g_update.status == VAVA_UPDATE_TRANSMITTING)
		{
			cJSON_AddNumberToObject(pJsonRoot, "loaddata", g_update.loading);
		}
	}

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_SyDid(cJSON *pJsonRoot)
{
	cJSON_AddStringToObject(pJsonRoot, "did", g_gatewayinfo.sydid);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_Doman(cJSON *pJsonRoot)
{
	cJSON_AddStringToObject(pJsonRoot, "domain_user", g_gatewayinfo.domain_user);
	cJSON_AddStringToObject(pJsonRoot, "domain_factory", g_gatewayinfo.domain_factory);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_P2PServer(cJSON *pJsonRoot)
{
	cJSON_AddStringToObject(pJsonRoot, "initstr", g_gatewayinfo.initstr);
	cJSON_AddStringToObject(pJsonRoot, "crckey", g_gatewayinfo.crckey);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_NasServer(cJSON *pJsonRoot)
{
	cJSON_AddNumberToObject(pJsonRoot, "ctrl", g_nas_config.ctrl);
	cJSON_AddStringToObject(pJsonRoot, "ip", g_nas_config.ip);
	cJSON_AddStringToObject(pJsonRoot, "path", g_nas_config.path);
	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_RecFileSize(cJSON *pRoot, cJSON *pJsonRoot)
{
	FILE *fd = NULL;
	cJSON *pDirname = NULL;
	cJSON *pFilename = NULL;

	int ret;
	char path[128];
	VAVA_RecInfo recinfo;
	
	if(pRoot == NULL)
	{
		return VAVA_ERR_CODE_JSON_PARSE_FAIL;
	}

	pthread_mutex_lock(&mutex_recplay_lock);

	pDirname = cJSON_GetObjectItem(pRoot, "dirname");
	if(pDirname == NULL)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}
	ret = VAVAHAL_Date_Verification(pDirname->valuestring);
	if(ret != 0)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_PARAM_INVALID;
	}
	
	pFilename = cJSON_GetObjectItem(pRoot, "filename");
	if(pFilename == NULL)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_JSON_NODE_NOFOUND;
	}

	memset(path, 0, 128);
	sprintf(path, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, pDirname->valuestring, pFilename->valuestring);
	fd = fopen(path, "r");
	if(fd == NULL)
	{
		pthread_mutex_unlock(&mutex_recplay_lock);
		return VAVA_ERR_CODE_RECFILE_NOTFOUND;
	}

	
	memset(&recinfo, 0, sizeof(VAVA_RecInfo));
	fread(&recinfo, sizeof(VAVA_RecInfo), 1, fd);
	fclose(fd);
	
	pthread_mutex_unlock(&mutex_recplay_lock);

	cJSON_AddStringToObject(pJsonRoot, "dirname", pDirname->valuestring);
	cJSON_AddStringToObject(pJsonRoot, "filename", pFilename->valuestring);
	cJSON_AddNumberToObject(pJsonRoot, "filesize", recinfo.size);

	return VAVA_ERR_CODE_SUCCESS;
}

int VAVAHAL_ParamBuild_RecPlay(cJSON *pJsonRoot, int channel, int recchannel)
{
	int ret;
	int recplaychannel;
	pthread_t pth_id;
	pthread_attr_t attr;
	struct timeval randomtime;
	VAVA_RecInfo recinfo;

	gettimeofday(&randomtime, NULL);

	srand((unsigned int)randomtime.tv_usec);
	g_recplay[recchannel].token = rand() % 900000 + 100000;
	g_recplay[recchannel].ctrl = VAVA_RECFILE_PLAY_CTRL_CONTINUE;

	recplaychannel = recchannel;

	memset(&recinfo, 0, sizeof(VAVA_RecInfo));
	fread(&recinfo, sizeof(VAVA_RecInfo), 1, g_recplay[recchannel].fd);

	if(recinfo.tag == 0)
	{
		fclose(g_recplay[recchannel].fd);
		g_recplay[recchannel].fd = NULL;
		g_recplay[recchannel].sesssion = -1;
		g_recplay[recchannel].token = 0;
		g_recplay[recchannel].type = -1;
		g_recplay[recchannel].ctrl = 0;
		g_recplay[recchannel].v_encode = -1;
		g_recplay[recchannel].a_encode = -1;
		g_recplay[recchannel].fps = 0;
		g_recplay[recchannel].res = -1;
		g_recplay[recchannel].encrypt = VAVA_REC_ENCRYPT_FREE;
		memset(g_recplay[recchannel].dirname, 0, 9);
		memset(g_recplay[recchannel].filename, 0, 11);

		pthread_mutex_unlock(&mutex_recplay_lock);

		return VAVA_ERR_CODE_RECFILE_DAMAGE;
	}

	g_recplay[recchannel].v_encode = VAVA_ENCODE_H264;
	g_recplay[recchannel].a_encode = recinfo.a_encode;
	g_recplay[recchannel].fps = recinfo.fps;
	g_recplay[recchannel].res = recinfo.res;
	g_recplay[recchannel].encrypt = recinfo.encrypt;

	pthread_attr_init(&attr); 
	pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, Rec_PlayBack, &recplaychannel);
	pthread_attr_destroy(&attr);

	if(ret != 0)
	{
		pthread_mutex_lock(&mutex_recplay_lock);

		fclose(g_recplay[recchannel].fd);
		g_recplay[recchannel].fd = NULL;
		g_recplay[recchannel].sesssion = -1;
		g_recplay[recchannel].token = 0;
		g_recplay[recchannel].type = -1;
		g_recplay[recchannel].ctrl = 0;
		g_recplay[recchannel].v_encode = -1;
		g_recplay[recchannel].a_encode = -1;
		g_recplay[recchannel].fps = 0;
		g_recplay[recchannel].res = -1;
		g_recplay[recchannel].encrypt = VAVA_REC_ENCRYPT_FREE;
		memset(g_recplay[recchannel].dirname, 0, 9);
		memset(g_recplay[recchannel].filename, 0, 11);

		pthread_mutex_unlock(&mutex_recplay_lock);

		return VAVA_ERR_CODE_RECPLAY_FAIL;
	}

	//确保线程参数传递成功
	usleep(50000);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [TEST315] PLAY REC SUCCESS, TOKEN = %d\n", FUN, LINE, g_recplay[recchannel].token);

	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);
	cJSON_AddNumberToObject(pJsonRoot, "token", g_recplay[recchannel].token);
	cJSON_AddNumberToObject(pJsonRoot, "filesize", recinfo.size);
	
	return VAVA_ERR_CODE_SUCCESS;
}


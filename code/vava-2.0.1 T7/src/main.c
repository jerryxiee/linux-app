#include "basetype.h"
#include "vavahal.h"
#include "vavaserver.h"
#include "buttonmanage.h"
#include "networkcheck.h"
#include "mutex.h"
#include "errlog.h"
#include "avserver.h"
#include "devdiscovery.h"
#include "apconfig.h"
#include "p2pserver.h"
#include "sdcheck.h"
#include "armcheck.h"
#include "rfserver.h"
#include "update.h"
#include "record.h"
#include "network.h"
#include "usboverload.h"
#include "cloud_fsk.h"
#include "serversync.h"
#include "watchdog.h"

//全局运行状态
volatile unsigned char g_running = 1;
volatile unsigned char g_exitflag = 0;
volatile unsigned char g_waitflag = 0;
volatile unsigned char g_debuglevel = LOG_LEVEL_DEBUG;
volatile int g_domaintype = SY_DID_TYPE_USER;

volatile unsigned char g_pir_sensitivity[MAX_CHANNEL_NUM];

unsigned int g_channel[MAX_CHANNEL_NUM];

volatile unsigned int g_wakeup_withpair_addr;
volatile unsigned int g_sleep_withpair_addr;
volatile unsigned int g_pair_wifi_addr;
volatile unsigned int g_clear_addr;

volatile unsigned char g_keyparing = 0;
volatile unsigned char g_pair_result = 0;
volatile unsigned char g_addstation = 0;
volatile unsigned char g_wakeup_withpair_status;
volatile unsigned char g_sleep_withpair_status;
volatile unsigned char g_clear_status;

volatile unsigned char g_wakeup_result[MAX_CHANNEL_NUM];
volatile unsigned char g_sleep_result[MAX_CHANNEL_NUM];
volatile unsigned char g_pair_status[MAX_CHANNEL_NUM];

volatile char g_tfcheck[MAX_CHANNEL_NUM];	//TF卡状态更新
volatile char g_tfstatus[MAX_CHANNEL_NUM];	//短波TF卡状态

volatile unsigned char g_buzzer_flag; 		//蜂鸣器开关
volatile unsigned char g_buzzer_type; 		//蜂鸣器鸣叫类型
volatile unsigned char g_devinfo_update;	//心跳包更新标记
volatile VAVA_Snapshot g_snapshot[MAX_CHANNEL_NUM]; //抓图不带报警
volatile VAVA_Snapshot g_snapshot_alarm[MAX_CHANNEL_NUM]; //抓图带报警
struct timeval g_salarm[MAX_CHANNEL_NUM];
volatile unsigned char g_update_sd_nochek;
volatile unsigned char g_wakenoheart[MAX_CHANNEL_NUM];
volatile unsigned char g_sleepnoheart[MAX_CHANNEL_NUM];
volatile unsigned long long g_manaul_ntsamp[MAX_CHANNEL_NUM];

volatile unsigned char g_rec_delaytime = REC_DELAY_TIME;//录像延时开启开关

#ifdef ALARM_PHOTO_IOT
char g_imgname[MAX_CHANNEL_NUM][256];
volatile unsigned char g_imgflag[MAX_CHANNEL_NUM];
#endif

volatile unsigned char g_pairmode; //配对状态

#ifdef WAKE_TIME_USER_TEST
struct timeval g_testval_1;
struct timeval g_testval_2;
#endif

//音视频缓冲区
VAVA_AVmemchace g_avmemchace[MAX_CHANNEL_NUM];

//音视频接收buff
VAVA_AVRecvMem g_avrecvbuff[MAX_CHANNEL_NUM];

VAVA_AVNasMem g_avnasbuff;
VAVA_AVPlayMem g_avplaybuff;

#ifdef CLOUDE_SUPPORT
//云存接收BUFF
VAVA_AVCloudMem g_avcloudbuff[MAX_CHANNEL_NUM];
#endif

//基站信息
VAVA_GateWayInfo g_gatewayinfo;

//摄像机信息
VAVA_CameraInfo g_camerainfo[MAX_CHANNEL_NUM];

//摄像机名称(配对选填)
VAVA_CameraName g_cameraname[MAX_CHANNEL_NUM];

//配对信息
#ifdef FREQUENCY_OFFSET
VAVA_Pair_info_old g_pair_old[MAX_CHANNEL_NUM];
#endif
VAVA_Pair_info g_pair[MAX_CHANNEL_NUM];

//布防参数
VAVA_ArmInfo_v1 g_camera_arminfo_v1[MAX_CHANNEL_NUM];

//录像时长信息
VAVA_RecTime g_rectime;

//升级状态
VAVA_Update g_update;

//基站临时保存搜索到的摄像机列表(5秒更新一次)
VAVA_SearchCamera g_searchcamera;

//录像管理
VAVA_RecPlay g_recplay[MAX_CHANNEL_NUM];

//临时状态
VAVA_OnlineStatus g_online_flag[MAX_CHANNEL_NUM];

//推送信息
VAVA_PushFlag g_pushflag[MAX_CHANNEL_NUM];

//NTP信息
VAVA_NtpInfo g_ntpinfo;

//初次连接状态
volatile unsigned char g_firsttag;  //立即检测标记

//格式化卡后重新获取大小
volatile unsigned char g_sdformat;
volatile unsigned char g_sdformating;  	//正在格式化 禁止检测T卡
volatile unsigned char g_sdpoping;	 	//主动弹出SD卡 禁止检测T卡

//移动侦测确认
volatile unsigned char g_md_result[MAX_CHANNEL_NUM];

//NAS服务配置
volatile unsigned char g_nas_status;	//NAS工作状态
volatile unsigned char g_nas_change;	//NAS参数变更
VAVA_NasConfig g_nas_config;			//NAS配置

//线程锁
pthread_mutex_t mutex_config_lock;
pthread_mutex_t mutex_cmdsend_lock;
pthread_mutex_t mutex_session_lock;
pthread_mutex_t mutex_cmdlist_lock;
pthread_mutex_t mutex_reclist_lock;
pthread_mutex_t mutex_imglist_lock;
pthread_mutex_t mutex_pushlist_lock;
pthread_mutex_t mutex_upgrate_lock;
pthread_mutex_t mutex_idx_lock;
pthread_mutex_t mutex_rf_lock;
pthread_mutex_t mutex_search_camera_lock;
pthread_mutex_t mutex_pair_lock;
pthread_mutex_t mutex_recplay_lock;
pthread_mutex_t mutex_format_lock;
pthread_mutex_t mutex_pir_lock;
pthread_mutex_t mutex_power_lock[MAX_CHANNEL_NUM];
pthread_mutex_t mutex_ping_lock;
pthread_mutex_t mutex_curl_lock;

void sig_stop(int signo)
{
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: sig_stop: signo = %d\n", FUN, LINE, signo);

	g_running = 0;
	g_exitflag = 1;
	return;
}

void sig_continue(int signo)
{
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: pipe, continue...\n", FUN, LINE);
}

int init_watchdog()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, WatchDog_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_gatewayinfo()
{
	memset(&g_gatewayinfo, 0, sizeof(VAVA_GateWayInfo));

	//网络连接状态
	g_gatewayinfo.nettype = -1;

	//基站到路由器信号
	g_gatewayinfo.signal = 100;

	//提示音语言
	g_gatewayinfo.language = VAVA_LANGUAGE_ENGLIST;

	//SD卡信息
	g_gatewayinfo.sdstatus = VAVA_SD_STATUS_NOCRAD;
	g_gatewayinfo.format_flag = 0;
	g_gatewayinfo.totol = 0;
	g_gatewayinfo.used = 0;
	g_gatewayinfo.free = 0;

	//NAS信息
	#ifdef NAS_NFS
	g_gatewayinfo.nas_totol = 0;
	g_gatewayinfo.nas_used = 0;
	g_gatewayinfo.nas_free = 0;
	#endif

	//短波地址
	g_gatewayinfo.rfaddr = 0xFFFFFFFF;

	//WIFI信道
	g_gatewayinfo.netch = -1;

	//基站信息
	memset(g_gatewayinfo.sn, 0, 32);
	memset(g_gatewayinfo.releasedate, 0, 12);
	memset(g_gatewayinfo.hardver, 0, 16);
	memset(g_gatewayinfo.softver, 0, 16);
	memset(g_gatewayinfo.mac, 0, 18);
	memset(g_gatewayinfo.ipstr, 0, 16);
	memset(g_gatewayinfo.ssid, 0, 16);
	memset(g_gatewayinfo.pass, 0, 16);

	//短波信息
	memset(g_gatewayinfo.rfver, 0, 16);
	memset(g_gatewayinfo.rfhw, 0, 16);

	//服务器信息
	memset(g_gatewayinfo.domain_user, 0, 64);
	memset(g_gatewayinfo.domain_factory, 0, 64);
	memset(g_gatewayinfo.token, 0, 64);
	memset(g_gatewayinfo.sydid, 0, 32);
	memset(g_gatewayinfo.apilisence, 0, 16);
	memset(g_gatewayinfo.crckey, 0, 16);
	memset(g_gatewayinfo.initstr, 0, 128);

	VAVAHAL_ReadGateWayInfo();

	return 0;
}

int read_gatewayinfo()
{
	int ret;
	
	//读取P2P服务器信息
	VAVAHAL_ReadSyInfo();

	//读取基站序列号和硬件版配号
	ret = VAVAHAL_ReadSn();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: read sn and hardver fail\n", FUN, LINE);
		Err_Log("read sn and hardver fail");

		VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_FAST_FLASH);
		VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);

		while(g_running)
		{	
			sleep(1);
		}
		
		return -1;
	}

	#if 0
	//读取认证服务器域名
	VAVAHAL_ReadDoman();
	#else
	VAVAHAL_BuildDomanWithSn(g_gatewayinfo.sn);
	#endif

	//如有设置域名则使用已设置的
	VAVAHAL_ReadDoman();

	VAVAHAL_BuildGwRfAddr();
	
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

int init_recmanage()
{
	int i;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_manaulrec[i].status = VAVA_REC_STATUS_STOP;
		g_manaulrec[i].ctrl = 0;
		g_manaulrec[i].start = 0;
		g_manaulrec[i].session = -1;
		g_manaulrec[i].fd = NULL;
		memset(g_manaulrec[i].dirname, 0, 9);
		memset(g_manaulrec[i].filename, 0, 11);

		g_alarmrec[i].status = VAVA_REC_STATUS_STOP;
		g_alarmrec[i].ctrl = 0;
		g_alarmrec[i].start = 0;
		g_alarmrec[i].session = -1;
		g_alarmrec[i].fd = NULL;
		memset(g_alarmrec[i].dirname, 0, 9);
		memset(g_alarmrec[i].filename, 0, 11);

		g_fulltimerec[i].status = VAVA_REC_STATUS_STOP;
		g_fulltimerec[i].ctrl = 0;
		g_fulltimerec[i].start = 0;
		g_fulltimerec[i].session = -1;
		g_fulltimerec[i].fd = NULL;
		memset(g_fulltimerec[i].dirname, 0, 9);
		memset(g_fulltimerec[i].filename, 0, 11);

		g_cloudrec[i].status = VAVA_REC_STATUS_STOP;
		g_cloudrec[i].ctrl = 0;
		g_cloudrec[i].start = 0;
		g_cloudrec[i].session = -1;
		g_cloudrec[i].fd = NULL;
		memset(g_cloudrec[i].dirname, 0, 9);
		memset(g_cloudrec[i].filename, 0, 11);
		
		g_recplay[i].fd = NULL;
		g_recplay[i].sesssion = -1;
		g_recplay[i].token = 0;
		g_recplay[i].type = -1;
		g_recplay[i].ctrl = 0;
		g_recplay[i].v_encode = -1;
		g_recplay[i].a_encode = -1;
		g_recplay[i].fps = 0;
		g_recplay[i].res = -1;
		g_recplay[i].encrypt = VAVA_REC_ENCRYPT_FREE;
		memset(g_recplay[i].dirname, 0, 9);
		memset(g_recplay[i].filename, 0, 11);
	}

	return 0;
}

int init_camereinfo()
{
	int i;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		VAVAHAL_InitCameraInfo(i);
	}

	//读取自动视频质量参数
	VAVAHAL_ReadVideoQuality();

	//读取PIR灵敏度
	VAVAHAL_ReadPirSensitivity();

	return 0;
}

int init_sessioninfo()
{
	int i;

	for(i = 0; i < MAX_SESSION_NUM; i++)
	{
		g_session[i].id = -1;
		g_session[i].camerachannel = -1;
		g_session[i].wakeupstatus = 0;
		g_session[i].videostatus = 0;
		g_session[i].videoflag = 0;
		g_session[i].audiostatus = 0;
		g_session[i].sendmode = 0;
		g_session[i].recimgstop = 0;
		g_session[i].framenum = 0;
		g_session[i].debugauth = 0;
	}
	
	return 0;
}

int init_language()
{
	g_gatewayinfo.language = VAVA_LANGUAGE_ENGLIST;
	VAVAHAL_SetLanguage(VAVA_LANGUAGE_ENGLIST);

	return 0;
}

int init_pthreadlock()
{
	int i;
	
	MutexInit(&mutex_config_lock);
	MutexInit(&mutex_cmdsend_lock);
	MutexInit(&mutex_session_lock);
	MutexInit(&mutex_cmdlist_lock);
	MutexInit(&mutex_reclist_lock);
	MutexInit(&mutex_imglist_lock);
	MutexInit(&mutex_pushlist_lock);
	MutexInit(&mutex_upgrate_lock);
	MutexInit(&mutex_idx_lock);
	MutexInit(&mutex_rf_lock);
	MutexInit(&mutex_search_camera_lock);
	MutexInit(&mutex_pair_lock);
	MutexInit(&mutex_recplay_lock);
	MutexInit(&mutex_format_lock);
	MutexInit(&mutex_pir_lock);
	MutexInit(&mutex_ping_lock);
	MutexInit(&mutex_curl_lock);

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		MutexInit(&mutex_power_lock[i]);
	}

	return 0;
}

int deinit_pthreadlock()
{
	int i;
	
	MutexDeInit(&mutex_config_lock);
	MutexDeInit(&mutex_cmdsend_lock);
	MutexDeInit(&mutex_session_lock);
	MutexDeInit(&mutex_cmdlist_lock);
	MutexDeInit(&mutex_reclist_lock);
	MutexDeInit(&mutex_imglist_lock);
	MutexDeInit(&mutex_pushlist_lock);
	MutexDeInit(&mutex_upgrate_lock);
	MutexDeInit(&mutex_idx_lock);
	MutexDeInit(&mutex_rf_lock);
	MutexDeInit(&mutex_search_camera_lock);
	MutexDeInit(&mutex_pair_lock);
	MutexDeInit(&mutex_recplay_lock);
	MutexDeInit(&mutex_format_lock);
	MutexDeInit(&mutex_pir_lock);
	MutexDeInit(&mutex_ping_lock);
	MutexDeInit(&mutex_curl_lock);

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		MutexDeInit(&mutex_power_lock[i]);
	}

	return 0;
}

int init_avmem()
{
	int i;
#ifdef FRAME_COUNT
	int j;
#endif
	char *main_mem_buff[MAX_CHANNEL_NUM];
	char *sub_mem_buff[MAX_CHANNEL_NUM];
	char *audioplay_mem_buff[MAX_CHANNEL_NUM];

	g_avnasbuff.buff = (unsigned char *)malloc(MEM_AV_NAS_SIZE);
	g_avplaybuff.buff = (unsigned char *)malloc(MEM_AV_PLAY_SIZE);
	
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		main_mem_buff[i] = malloc(MEM_MAIN_VIDEO_SIZE);
		sub_mem_buff[i] = malloc(MEM_SUB_VIDEO_SIZE);
		audioplay_mem_buff[i] = malloc(MEM_AUDIO_SIZE);
		g_avrecvbuff[i].buff = (unsigned char *)malloc(MEM_AV_RECV_SIZE);

		#ifdef CLOUDE_SUPPORT
		g_avcloudbuff[i].buff = (unsigned char *)malloc(MEM_AV_CLOUD_SIZE);
		#endif

		#ifdef CLOUDE_SUPPORT
		if(main_mem_buff[i] == NULL || sub_mem_buff[i] == NULL || audioplay_mem_buff[i] == NULL 
			|| g_avrecvbuff[i].buff == NULL || g_avcloudbuff[i].buff == NULL)
		#else
		if(main_mem_buff[i] == NULL || sub_mem_buff[i] == NULL || audioplay_mem_buff[i] == NULL 
			|| g_avrecvbuff[i].buff == NULL)
		#endif
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc mem fail\n", FUN, LINE);
			Err_Log("malloc avmemchace fail");

			if(main_mem_buff[i] != NULL)
			{
				free(main_mem_buff[i]);
			}

			if(sub_mem_buff[i] != NULL)
			{
				free(sub_mem_buff[i]);
			}

			if(audioplay_mem_buff[i] != NULL)
			{
				free(audioplay_mem_buff[i]);
			}

			if(g_avrecvbuff[i].buff != NULL)
			{
				free(g_avrecvbuff[i].buff);
			}

			#ifdef CLOUDE_SUPPORT
			if(g_avcloudbuff[i].buff != NULL)
			{
				free(g_avcloudbuff[i].buff);
			}
			#endif

			if(g_avnasbuff.buff != NULL)
			{
				free(g_avnasbuff.buff);
				g_avnasbuff.buff = NULL;
			}

			if(g_avplaybuff.buff != NULL)
			{
				free(g_avplaybuff.buff);
				g_avplaybuff.buff = NULL;
			}

			return -1;
		}

		memset(g_avmemchace[i].mvlists, 0, sizeof(struct _avunit) * MEM_MAIN_QUEUE_NUM);
		memset(g_avmemchace[i].svlists, 0, sizeof(struct _avunit) * MEM_SUB_QUEUE_NUM);
		memset(g_avmemchace[i].aplists, 0, sizeof(struct _avunit) * MEM_AUDIO_QUEUE_NUM);

#ifdef FRAME_COUNT
		for(j = 0; j < MEM_MAIN_QUEUE_NUM; j++)
		{
			g_avmemchace[i].mvlists[j].count = j;
		}

		for(j = 0; j < MEM_SUB_QUEUE_NUM; j++)
		{
			g_avmemchace[i].svlists[j].count = j;
		}

		for(j = 0; j < MEM_AUDIO_QUEUE_NUM; j++)
		{
			g_avmemchace[i].aplists[j].count = j;
		}
#endif

		g_avmemchace[i].mv_write = 0;
		g_avmemchace[i].mv_read = -1;
		g_avmemchace[i].pmvMemBegin = main_mem_buff[i];
		g_avmemchace[i].pmvMemFree = main_mem_buff[i];
		g_avmemchace[i].pmvMemEnd = g_avmemchace[i].pmvMemBegin + MEM_MAIN_VIDEO_SIZE;

		g_avmemchace[i].sv_write = 0;
		g_avmemchace[i].sv_read = -1;
		g_avmemchace[i].psvMemBegin = sub_mem_buff[i];
		g_avmemchace[i].psvMemFree = sub_mem_buff[i];
		g_avmemchace[i].psvMemEnd = sub_mem_buff[i] + MEM_SUB_VIDEO_SIZE;

		g_avmemchace[i].ap_write = 0;
		g_avmemchace[i].ap_read = 0;
		g_avmemchace[i].papMemBegin = audioplay_mem_buff[i];
		g_avmemchace[i].papMemFree = audioplay_mem_buff[i];
		g_avmemchace[i].papMemEnd = audioplay_mem_buff[i] + MEM_AUDIO_SIZE;
	}

	return 0;
}

int deinit_avmem()
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

		if(g_avrecvbuff[i].buff != NULL)
		{
			free(g_avrecvbuff[i].buff);
			g_avrecvbuff[i].buff = NULL;
		}

		#ifdef CLOUDE_SUPPORT
		if(g_avcloudbuff[i].buff != NULL)
		{
			free(g_avcloudbuff[i].buff);
			g_avcloudbuff[i].buff = NULL;
		}
		#endif
	}

	if(g_avnasbuff.buff != NULL)
	{
		free(g_avnasbuff.buff);
		g_avnasbuff.buff = NULL;
	}

	if(g_avplaybuff.buff != NULL)
	{
		free(g_avplaybuff.buff);
		g_avplaybuff.buff = NULL;
	}

	return 0;
}

int init_armcheck()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, ArmCheck_v1_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_buttonmanage()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, buttionmanage_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_netchange()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	g_gatewayinfo.nettype = -1;

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, netchange_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_internetcheck()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, internetcheck_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_wirlesscheck()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, wirlesscheck_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_sdcheck()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, SDPnp_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_pairsync()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, PairSync_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_statussync()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, StatusSync_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_susreccheck()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, SDSus_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_usboverload_check()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, UsbOverLoadCheck_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_commonserver()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, commonserver_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_avserver()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, avserver_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_avsend()
{
	int ret;
	int i;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		ret = pthread_create(&pth_id, &attr, avsend_pth, &g_channel[i]);
		if(ret != 0)
		{
			pthread_attr_destroy(&attr);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, [channel = %d][ret = %d]\n", FUN, LINE, i, ret);
			Err_Log("cteate videosend pth fail");
			return -1;
		}
		
		ret = pthread_create(&pth_id, &attr, audiorecv_pth, &g_channel[i]);
		if(ret != 0)
		{
			pthread_attr_destroy(&attr);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, [channel = %d][ret = %d]\n", FUN, LINE, i, ret);
			Err_Log("cteate audiorecv recv fail");
			return -1;
		}

		ret = pthread_create(&pth_id, &attr, apsend_pth, &g_channel[i]);
		if(ret != 0)
		{
			pthread_attr_destroy(&attr);
			
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, [channel = %d][ret = %d]\n", FUN, LINE, i, ret);
			Err_Log("cteate apsend recv fail");
			return -1;
		}
	}
	
	pthread_attr_destroy(&attr);

	return ret;
}

int init_rfserver()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, rfserver_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_p2pserver()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, p2pservers_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_cloud_fsk()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, cloud_fsk_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_cloud_fsk_debug()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, cloud_fsk_debug, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_devdiscovery()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, devdiscovery_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_apconfig()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, apservers_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_keysync()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, keysync_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_addstatiton()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, keyaddstation_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_buzzer_manage()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, buzzer_manage_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

void *firstcheck_pth(void *data)
{
	int i;
	int ret;
	int timeout = 0;
	int retrytime = 0;
	
	sleep(10);

	while(g_running)
	{
		sleep(1);

		if(timeout++ >= 30 || g_firsttag == 1)
		{
			g_firsttag = 0;
			timeout = 0;

			sleep(1);

			for(i = 0; i < MAX_CHANNEL_NUM; i++)
			{	
				if(g_pair[i].nstat == 0)
				{
					continue;
				}

				if(g_camerainfo_dynamic[i].battery <= CAMERA_LOW_POWER && g_camerainfo[i].powermode == 0)
				{
					continue;
				}

				if(g_camerainfo_dynamic[i].online == 0)
				{
					continue;
				}

				if( g_update.status != VAVA_UPDATE_IDLE)
				{
					if(g_update.type == VAVA_UPDATE_TYPE_GATEWAY
						|| (g_update.type == VAVA_UPDATE_TYPE_CAMERA && g_update.current == i))
					{
						continue;
					}
				}

				if(g_camerainfo[i].first_flag == 0)
				{
					continue;
				}

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *** FirstCheck Channel = %d ***\n", FUN, LINE, i);

				retrytime = 2;
				while(g_running)
				{
					if(retrytime-- <= 0)
					{
						break;
					}

					ret = VAVAHAL_SleepCamera_Ex(i);
					if(ret == VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					sleep(2);
					continue;
				}

				if(retrytime <= 0)
				{
					continue;
				}

				retrytime = 2;
				while(g_running)
				{
					if(retrytime-- <= 0)
					{
						break;
					}

					ret = VAVAHAL_WakeupCamera_Ex(i, WAKE_UP_MODE_APP);
					if(ret == VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					sleep(2);
					continue;
				}
			}
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}

int init_firstcheck()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, firstcheck_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

void *heartcheck_pth(void * data)
{
	int ret;
	int channel;
	int session;
	int sessionnum;
	int hearttime = 60;
	int printcount = 0;
	
	int checktime[MAX_CHANNEL_NUM];
	int lasttime[MAX_CHANNEL_NUM];
	int lowpower[MAX_CHANNEL_NUM];
	int offpush[MAX_CHANNEL_NUM];
	
	char powerstr[16];
	
	struct timeval t_current;
	int ntp;
	int ntsamp;

	struct timeval t_off1;
	struct timeval t_off2;
	
	time_t t_time;
	struct tm *t_info;

	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		checktime[channel] = 0;
		lasttime[channel] = 0;
		lowpower[channel] = -1;
		offpush[channel] = 0;

		g_online_flag[channel].online = 0;
		g_online_flag[channel].battery = 0;
		g_online_flag[channel].voltage = 0;
		#ifdef BATTEY_INFO
		g_online_flag[channel].temperature = 0;
		g_online_flag[channel].electricity = 0;
		#endif
	}
	
	while(g_running)
	{
		sleep(1);

		if(printcount++ >= 10)
		{
			printcount = 0;
			VAVAHAL_PrintOnline();
		}

		//升级基站过程中不检测心跳
		if(g_update.status != VAVA_UPDATE_IDLE && g_update.type == VAVA_UPDATE_TYPE_GATEWAY) 
		{
			sleep(1);
			continue;
		}

		for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
		{
			//未配对 关机或升级过程不检测心跳亦不更新时间
			if(g_pair[channel].nstat == 0 || g_camerainfo[channel].poweroff_flag == 1 || (g_update.status != VAVA_UPDATE_IDLE && g_update.current == channel)) 
			{
				g_camerainfo[channel].wifi_num = 0;
				g_camerainfo[channel].wifi_heart = 0;
				continue;
			}

			//当wifi异常时修复不切到短波心跳的问题
			if(g_camerainfo[channel].wifi_heart == 1)
			{
				if(g_camerainfo[channel].wifi_num-- <= 0)
				{
					g_camerainfo[channel].wifi_num = 0;
					g_camerainfo[channel].wifi_heart = 0;

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Wifi Heart change [1 -> 0]\n", FUN, LINE);
				}
			}

			//WIFI心跳时不检测心跳但更新时间
			if(g_camerainfo_dynamic[channel].online == 1 && g_camerainfo[channel].wifi_heart == 1)
			{
				gettimeofday(&t_current, NULL);
				lasttime[channel] = t_current.tv_sec;
				continue;
			}

			//处理短波回复慢造成的问题
			if(g_camerainfo_dynamic[channel].online == 0 && g_online_flag[channel].online == 1)
			{
				g_camerainfo_dynamic[channel].online = g_online_flag[channel].online;
				g_camerainfo_dynamic[channel].battery = g_online_flag[channel].battery;
				g_camerainfo_dynamic[channel].voltage = g_online_flag[channel].voltage;
				#ifdef BATTEY_INFO
				g_camerainfo_dynamic[channel].temperature = g_online_flag[channel].temperature;
				g_camerainfo_dynamic[channel].electricity = g_online_flag[channel].electricity;
				#endif

				gettimeofday(&t_current, NULL);
				lasttime[channel] = t_current.tv_sec;
				
				g_firsttag = 1;
				continue;
			}

			//不在线时20秒检测一次
			if(g_camerainfo_dynamic[channel].online == 0) 
			{
				gettimeofday(&t_current, NULL);
				if(t_current.tv_sec - lasttime[channel] >= 20)
				{
					time(&t_time);
					t_info = localtime(&t_time);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ==> Heart check, channel - %d, T - %d [%d%02d%02d_%02d:%02d:%02d] <== *****\n",
						                           FUN, LINE, channel, checktime[channel], t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday, 
					                               t_info->tm_hour, t_info->tm_min, t_info->tm_sec);

					g_online_flag[channel].online = 0;
					//g_online_flag[channel].battery = 0;

					ret = VAVAHAL_HeartCheck(channel);
					if(ret == 0)
					{
						//更新数据
						g_camerainfo_dynamic[channel].online = g_online_flag[channel].online;
						g_camerainfo_dynamic[channel].battery = g_online_flag[channel].battery;
						g_camerainfo_dynamic[channel].voltage = g_online_flag[channel].voltage;
						#ifdef BATTEY_INFO
						g_camerainfo_dynamic[channel].temperature = g_online_flag[channel].temperature;
						g_camerainfo_dynamic[channel].electricity = g_online_flag[channel].electricity;
						#endif

						checktime[channel] = 0;
						lasttime[channel] = t_current.tv_sec;
						g_firsttag = 1;

						offpush[channel] = 0;
					}
					else
					{
						if(++checktime[channel] >= 3)
						{
							checktime[channel] = 0;
							lasttime[channel] = t_current.tv_sec;

							if(offpush[channel] == 1)
							{
								gettimeofday(&t_off2, NULL);
								if(t_off2.tv_sec - t_off1.tv_sec >= 180)
								{
									offpush[channel] = 0;

									//离线后3分钟推送
									gettimeofday(&t_current, NULL);
									ntp = VAVAHAL_GetNtp();
									ntsamp = (int)(t_current.tv_sec + ntp * 3600);
									VAVAHAL_InsertPushList(channel, VAVA_PUSH_CAMERA_OFFLINE, VAVA_PUSH_FILE_TYPE_TEXT, "", "", "", 0, ntsamp);
								}
							}
						}
					}
				}
			}
			else
			{
				//已在线有人在1分钟刷新 无人在3分钟刷新
				VAVAHAL_GetSessionNum(&sessionnum);
				if(sessionnum > 0)
				{
					hearttime = 60;
				}
				else
				{
					hearttime = 180 * 10;
				}

				gettimeofday(&t_current, NULL);
				if(t_current.tv_sec - lasttime[channel] >= hearttime)
				{
					time(&t_time);
					t_info = localtime(&t_time);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ***** ==> Heart check, channel - %d, T - %d [%d%02d%02d_%02d:%02d:%02d] <== *****\n",
						                           FUN, LINE, channel, checktime[channel], t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday, 
					                               t_info->tm_hour, t_info->tm_min, t_info->tm_sec);
					
					g_online_flag[channel].online = 0;
					//g_online_flag[channel].battery = 0;

					ret = VAVAHAL_HeartCheck(channel);
					if(ret == 0)
					{
						offpush[channel] = 0;
						
						//更新数据
						g_camerainfo_dynamic[channel].online = g_online_flag[channel].online;
						g_camerainfo_dynamic[channel].battery = g_online_flag[channel].battery;
						g_camerainfo_dynamic[channel].voltage = g_online_flag[channel].voltage;
						#ifdef BATTEY_INFO
						g_camerainfo_dynamic[channel].temperature = g_online_flag[channel].temperature;
						g_camerainfo_dynamic[channel].electricity = g_online_flag[channel].electricity;
						#endif

						gettimeofday(&t_current, NULL);
						checktime[channel] = 0;
						lasttime[channel] = t_current.tv_sec;
					}
					else
					{
						if(++checktime[channel] >= 3)
						{
							checktime[channel] = 0;
							lasttime[channel] = t_current.tv_sec;

							g_camerainfo_dynamic[channel].online = 0;
							//g_camerainfo_dynamic[channel].battery = 0;

							//离线推送
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------------------------------------\n", FUN, LINE);
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------  Camera %d Offline  -------\n", FUN, LINE, channel);
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------------------------------------\n", FUN, LINE);

							offpush[channel] = 1;
							gettimeofday(&t_off1, NULL);
					
							g_devinfo_update = 1;

							#if 0 //改为3分钟持续不在线再推送
							gettimeofday(&t_current, NULL);
							ntsamp = t_current.tv_sec;
							VAVAHAL_InsertPushList(channel, VAVA_PUSH_CAMERA_OFFLINE, VAVA_PUSH_FILE_TYPE_TEXT, "", "", "", 0, ntsamp);
							#endif

							if(g_camerainfo[channel].poweroff_flag == 0)
							{
								//摄像机离线后重新获取摄像机信息
								if(g_camerainfo[channel].wakeup_flag == 1 || g_camerainfo[channel].alarm_flag == 1 
									|| g_camerainfo[channel].cloud_flag == 1 || g_camerainfo[channel].config_flag == 1)
								{
									g_camerainfo[channel].sleep_flag = 1;
									VAVAHAL_SleepCamera_Ex(channel);
								}
							}

							VAVAHAL_StopFullTimeRec(channel);
							VAVAHAL_StopManualRec(channel);
							VAVAHAL_StopAlarmRec(channel);
							VAVAHAL_StopAlarmCloud(channel);

							g_camerainfo[channel].first_flag = 1;
							g_camerainfo[channel].alarm_flag = 0;
							g_camerainfo[channel].wakeup_flag = 0;
							g_camerainfo[channel].cloud_flag = 0;
							g_camerainfo[channel].config_flag = 0;

							for(session = 0; session < MAX_SESSION_NUM; session++)
							{
								if(g_session[session].camerachannel == channel)
								{
									g_session[session].wakeupstatus = 0;
									g_session[session].videostatus = 0;
									g_session[session].audiostatus = 0;
									g_session[session].camerachannel = -1;
								}
							}
						}
					}
				}
			}
		}

		for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
		{
			if(g_pair[channel].nstat == 0 || g_camerainfo_dynamic[channel].online == 0)
			{
				continue;
			}
			
			if(g_camerainfo_dynamic[channel].battery > 20 || g_camerainfo[channel].powermode == 1)
			{
				lowpower[channel] = -1;
			}
			else
			{
				if(g_camerainfo_dynamic[channel].battery > 0
					&& g_camerainfo_dynamic[channel].battery % 5 == 0
					&& lowpower[channel] != g_camerainfo_dynamic[channel].battery)
				{
					//低电推送
					lowpower[channel] = g_camerainfo_dynamic[channel].battery;
					
					memset(powerstr, 0, 16);
					sprintf(powerstr, "%d", g_camerainfo_dynamic[channel].battery);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [LOW POWER] - powerstr = [%s], channel = %d\n", FUN, LINE, powerstr, channel);

					gettimeofday(&t_current, NULL);
					ntp = VAVAHAL_GetNtp();
					ntsamp = t_current.tv_sec + ntp * 3600;
					VAVAHAL_InsertPushList(channel, VAVA_PUSH_TYPE_LOWPOWER, VAVA_PUSH_FILE_TYPE_TEXT, "", "", powerstr, 0, ntsamp);
				}
			}
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

int init_heartcheck()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, heartcheck_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}
	
void *sleep_manage_pth(void *data)
{
	int channel = *(int *)data;
	int timeout = 0;
	int count = 0;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);
		Err_Log("sleep manage channel err");
		return NULL;
	}
	
	while(g_running)
	{
		sleep(1);

		if(g_pair[channel].nstat == 0)
		{
			continue;
		}
		
		if(g_camerainfo[channel].sleep_check == 1)
		{
			g_camerainfo[channel].sleep_check = 0;
			timeout = 2;
		}
		else if(g_camerainfo[channel].sleep_check == 2)
		{
			g_camerainfo[channel].sleep_check = 0;
			timeout = 10;
		}
		
		if(timeout > 0)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [Camera %d] config time out check ===> %d sec\n", FUN, LINE, channel, timeout);

			count = 0;
			timeout--;
			if(timeout <= 0)
			{
				if(g_snapshot[channel].flag == 1 && g_snapshot[channel].sessionid != -1)
				{
					VAVAHAL_SendSnapShotResult(g_snapshot[channel].sessionid, channel, "null", "null", 1);
					g_snapshot[channel].flag = 0;
					g_snapshot[channel].sessionid = -1;
				}
				
				g_camerainfo[channel].config_flag = 0;
				
				VAVAHAL_SleepCamera_Ex(channel);
			}
		}
		else
		{
			if(g_camerainfo[channel].config_flag == 1)
			{
				count++;
				if(count >= 30)
				{
					count = 0;
					g_camerainfo[channel].config_flag = 0;
				}
			}
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

int init_sleep_manage(int channel)
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, sleep_manage_pth, &g_channel[channel]);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_fulltime_rec_service(int channel)
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, Rec_FullTime_pth, &g_channel[channel]);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_manual_rec_service(int channel)
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, Rec_Manaul_pth, &g_channel[channel]);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_alarm_rec_service(int channel)
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, Rec_Alarm_pth, &g_channel[channel]);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

#ifdef CLOUDE_SUPPORT
int init_cloud_rec_service(int channel)
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, Rec_Cloud_pth, &g_channel[channel]);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}
#endif

int init_ftp_manage()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, FTP_Manage_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_nas_manage()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, Nas_Manage_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int init_nassync()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, Nas_Sync_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

void *tfcheck_syncrf_pth(void *data)
{
	int i;
	char current = 0;
	char status;
	char savestatus[MAX_CHANNEL_NUM];

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_tfstatus[i] = -1;
		g_tfcheck[i] = 0;
		savestatus[i] = -1;
	}
	
	sleep(10);
	
	while(g_running)
	{
		sleep(5);

		if(g_update.status != VAVA_UPDATE_IDLE)
		{
			continue;
		}

		if(g_gatewayinfo.sdstatus == 1 && g_gatewayinfo.format_flag == 0)
		{
			current = 1;
		}
		else 
		{
			current = 0;
		}

		for(i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			if(g_pair[i].nstat == 0 || g_camerainfo_dynamic[i].online == 0)
			{
				continue;
			}

			status = (current || g_cloudflag[i]);

			if(savestatus[i] != status)
			{
				savestatus[i] = status;
				g_tfcheck[i] = 1;
			}

			if((g_tfstatus[i] != status) || (g_tfcheck[i] == 1))
			{
				VAVAHAL_TfSet(i, (unsigned char)status);
			}

			sleep(1);
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}

int init_tfcheck_sycnrf()
{
	int ret;
	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, tfcheck_syncrf_pth, NULL);
	pthread_attr_destroy(&attr);
	
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create pth err, ret = %d\n", FUN, LINE, ret);
	}
	
	return ret;
}

int main()
{
	int i;
	int ret;
	int searchtime;
	struct timeval t_aa;
	struct timeval t_bb;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------------------------------------------\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: VAVA GateWay [7688/7628] %s @2019 Sunvalley\n", FUN, LINE, PROGRAM_VER);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------------------------------------------\n", FUN, LINE);
	
	signal(SIGINT,  sig_stop);
	signal(SIGQUIT, sig_stop);
	signal(SIGTERM, sig_stop);
	signal(SIGPIPE, sig_continue);

	//启动时清理一下 否则会出现socket绑定失败
	VAVAHAL_SystemCmd("killall udhcpc");
	sleep(1);

	/*在初始化任何线程前初始化libcurl*/ 
  	curl_global_init(CURL_GLOBAL_ALL);

	//初始化看门狗线程
	ret = init_watchdog();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_watchdog fail, ret = %d\n", FUN, LINE, ret);
		Err_Log("watchdog init fail");
		return 0;
	}

	//初始化提示音语言
	init_language();

	//初始化线程锁
	init_pthreadlock();

	//初始化基站信息
	ret = init_gatewayinfo();
	if(ret != 0)
	{
		deinit_pthreadlock();
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_gatewayinfo fail, ret = %d\n", FUN, LINE, ret);
		Err_Log("gatewayinfo init fail");
		return 0;
	}

	//初始化录像管理信息
	init_recmanage();
	
	//初始化摄像机信息
	init_camereinfo();

	//初始化摄像机搜索信息列表
	VAVAHAL_SearchList_Init();

	//初始化升级状态
	VAVAHAL_InitUpdate();

	//初始化会话信息
	init_sessioninfo();

	//初始化缓冲区音视频缓冲区
	ret = init_avmem();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_avmem fail, ret = %d\n", FUN, LINE, ret);
		deinit_pthreadlock();
		return 0;
	}

	//初始化各通道socket句柄信息
	VAVAHAL_InitSocket();

	//初始化网卡配置
	//VAVAHAL_ClosePairNetwork();
	//VAVAHAL_CloseAPNetwork();

	//初始化SD卡检测
	ret = init_sdcheck();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();

		Err_Log("sdcheck init fail");
		return 0;
	}

	g_waitflag = 1;
	gettimeofday(&t_aa, NULL);
	while(g_running)
	{
		if(g_waitflag == 0)
		{
			break;
		}

		sleep(1);
	}
	gettimeofday(&t_bb, NULL);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: TFcheck Wait time: %d s\n", FUN, LINE, (int)(t_bb.tv_sec - t_aa.tv_sec));

	//读取配对信息
	VAVAHAL_ReadPairInfo();

	//读取喇叭和麦克风信息
	VAVAHAL_ReadMicInfo();
	VAVAHAL_ReadSpeakerInfo();

	//读取录像时长信息
	VAVAHAL_ReadRecTime();
	
	//初始化布防参数
	VAVAHAL_ReadArmInfo_v1();

	//初始化布防检测
	ret = init_armcheck();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();
		
		Err_Log("armcheck init fail");
		return 0;
	}

	//初始化网络切换
	ret = init_netchange();
	if(ret != 0)
	{			
		deinit_avmem();
		deinit_pthreadlock();

		Err_Log("netchange init fail");
		return 0;
	}

	usleep(500000);
	
	//初始化联网检测
	ret = init_internetcheck();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();
		
		Err_Log("internetcheck init fail");
		return 0;
	}

	usleep(500000);

#ifndef WIRLD_ONLY
	//初始化无线联网状态检测
	ret = init_wirlesscheck();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();
		
		Err_Log("wirlesscheck init fail");
		return 0;
	}
#endif

	//可持续录像检测(覆盖写)
	ret = init_susreccheck();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();

		Err_Log("susreccheck init fail");
		return 0;
	}

	usleep(500000);

	//初始化USB过载检测
	ret = init_usboverload_check();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();
		
		Err_Log("usboverloadcheck init fail");
		return 0;
	}

	usleep(500000);

	//初始化信令服务端
	ret = init_commonserver();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();
		
		Err_Log("commonserver init fail");
		return 0;
	}

	usleep(500000);

	//初始化媒体服务端
	ret = init_avserver();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();

		Err_Log("avserver init fail");
		return 0;
	}

	usleep(500000);

	//初始化媒体发送线程
	ret = init_avsend();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();

		Err_Log("avsend init fail");
		return 0;
	}

	usleep(500000);

	//初始化短波服务
	ret = init_rfserver();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();
		
		Err_Log("rfserver init fail");
		return 0;
	}

	//申请录像搜索空间
	ret = VAVAHAL_InitSearchBuff();
	if(ret != 0)
	{		
		deinit_avmem();
		deinit_pthreadlock();
		return 0;
	}

	usleep(500000);

#ifdef DEV_DISCOVERY
	//初始化设备发现服务
	ret = init_devdiscovery();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("devdiscovery init fail");
		return 0;
	}
#endif

	usleep(500000);

#ifndef WIRLD_ONLY
	//初始化AP配网服务
	ret = init_apconfig();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();

		Err_Log("apconfig init fail");
		return 0;
	}
#endif

	//初始化按键配对服务
	ret = init_keysync();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("keysync init fail");
		return 0;
	}

	//初始化按键添加基站服务
	ret = init_addstatiton();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("keyaddstaion init fail");
		return 0;
	}

	//初始化蜂鸣器管理服务
	ret = init_buzzer_manage();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("buzzer_manage init fail");
		return 0;
	}

	//初始化初次连接检测
	ret = init_firstcheck();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("first check init fail");
		return 0;
	}

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			VAVAHAL_SleepCamera_Ex(i);
		}
	}

	//初始化心跳检测
	ret = init_heartcheck();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("heart check init fail");
		return 0;
	}

	//休眠管理模块
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		ret = init_sleep_manage(i);
		if(ret != 0)
		{			
			deinit_avmem();
			VAVAHAL_DeInitSearchBuff();
			deinit_pthreadlock();

			Err_Log("sleep manage init fail");
			return 0;
		}
	}

	//初始化全时录像和手动录像服务
	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		ret = init_manual_rec_service(i);
		if(ret != 0)
		{			
			deinit_avmem();
			VAVAHAL_DeInitSearchBuff();
			deinit_pthreadlock();

			Err_Log("manaul rec service init fail");
			return 0;
		}

		ret = init_alarm_rec_service(i);
		if(ret != 0)
		{			
			deinit_avmem();
			VAVAHAL_DeInitSearchBuff();
			deinit_pthreadlock();

			Err_Log("alarm rec service init fail");
			return 0;
		}

		#ifdef CLOUDE_SUPPORT
		ret = init_cloud_rec_service(i);
		if(ret != 0)
		{			
			deinit_avmem();
			VAVAHAL_DeInitSearchBuff();
			deinit_pthreadlock();
			
			Err_Log("cloud rec service init fail");
			return 0;
		}
		#endif
	}

#ifdef FTP_SERVER
	//初始化FTP开关服务
	ret = init_ftp_manage();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("ftp manage init fail");
		return 0;
	}
#endif

	//初始化按键管理
	ret = init_buttonmanage();
	if(ret != 0)
	{			
		deinit_avmem();
		deinit_pthreadlock();
		
		Err_Log("buttonmanage init fail");
		return 0;
	}

#ifdef NAS_NFS
	//初始化NAS开关服务
	ret = init_nas_manage();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("nas manage init fail");
		return 0;
	}

	//初始化NAS同步服务
	ret = init_nassync();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("nassync init fail");
		return 0;
	}
#endif

	//初始化TF卡状态检测(同步到摄像机RF)
	ret = init_tfcheck_sycnrf();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();

		Err_Log("tfchecksyncrf init fail");
		return 0;
	}

	//最后初始化P2P连接服务器
	//前面的工作均准备就绪
	sleep(2);

	//初始化P2P服务
	ret = init_p2pserver();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();

		Err_Log("p2pserver init fail");
		return 0;
	}
	
#ifdef CLOUDE_SUPPORT
	//初始化云检测服务
	ret = init_cloud_fsk();
	if(ret != 0)
	{		
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("cloudserver init fail");
		return 0;
	}

	#if 0 //富士康掉线问题debug检测
	init_cloud_fsk_debug();
	#endif
#endif

	ret = init_pairsync();
	if(ret != 0)
	{
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();

		Err_Log("pairsync init fail");
		return 0;
	}

	ret = init_statussync();
	if(ret != 0)
	{
		deinit_avmem();
		VAVAHAL_DeInitSearchBuff();
		deinit_pthreadlock();
		
		Err_Log("statussync init fail");
		return 0;
	}

	searchtime = 0;

	while(g_running)
	{
		sleep(1);

		if(g_rec_delaytime > 0)
		{
			g_rec_delaytime--;
		}

		if(g_gatewayinfo.netch == -1)
		{
			VAVAHAL_GetNetChannel();
		}

		if(g_token_timeout > 0)
		{
			g_token_timeout--;
		}

		if(searchtime++ >= 10) //10秒刷新一次
		{
			//刷新列表
			VAVAHAL_SearchList_Update();
		}
	}
	
	//释放缓存
	deinit_avmem();

	//释放录像搜索空间
	VAVAHAL_DeInitSearchBuff();

	//释放线程锁
	deinit_pthreadlock();

	sleep(2);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===== Vave Programe Exit\n", FUN, LINE);
	
	return 0;
}


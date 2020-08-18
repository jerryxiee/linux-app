#include "basetype.h"
#include "md5.h"
#include "PPCS_API.h"
#include "errlog.h"
#include "vavaserver.h"
#include "vavahal.h"
#include "network.h"
#include "quelist.h"
#include "watchdog.h"
#include "p2pserver.h"

#define P2P_BUFF_SIZE				3072

SessionList g_session[MAX_SESSION_NUM];

void *Session_pth(void *data)
{
	int i;
	int ret;
	int sessionid = *(int *)data;
	int session_channel;
	int recplay_channel;
	st_PPCS_Session Sinfo;
	VAVA_CMD_HEAD *vavahead;

	VAVA_ResConfig resconfig;
	VAVA_MirrConfig mirrconfig;
	VAVA_IrLedConfig irledconfig;
	VAVA_MdConfig mdconfig;
	VAVA_RecSearch recsearch;
	VAVA_RecShareDateSearch recsharedate;
	VAVA_RecShareListSearch recsharelist;
	VAVA_RecImgGet recimgget;
	VAVA_CameraFactory camerafactory;
	VAVA_IPCComConfig comconfig;

	int readsize;
	int sendsize;
	char readbuff[P2P_BUFF_SIZE];

	int errcode;
	char errstr[128];
	char tmpstr[32];
	MD5_CTX md5_handle;
	unsigned char md5_str16[16];
	unsigned char md5_str32[32];

	//会话验证
	int auth_flag = 0;

	//会话连接后信息反馈
	//int firstconnect = 1;

	//主动获取心跳
	int heartflag = 0;

	//解析JOSN
	cJSON *pRoot = NULL;
	int random;
	int channel;
	int pairchannel;
	int ctrl;
	int res;
	int fps;
	int bitrate;
	int quality;
	int mirrmode;
	int irledmode;
	char authkey[36];
	char bskey[36];
	char camerasn[32];
	char cameraname[32];
	char md5[36];
	unsigned int cameraaddr;
	int videochannel = -1;
	
	//返回JSON
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	//struct timeval t_start;
	//struct timeval t_current;
	struct timeval t_format1;
	struct timeval t_format2;
	time_t tt;
	struct tm *timeinfo;

	time(&tt);
	timeinfo = localtime(&tt);
	if(timeinfo->tm_year >= 120)
	{
		//后门调用，动态调整保证安全性
		timeinfo->tm_year -= 1;
	}

	pthread_mutex_lock(&mutex_session_lock);
	
	for(i = 0; i < MAX_SESSION_NUM; i++)
	{
		if(g_session[i].id == -1 || g_session[i].id == sessionid)
		{
			g_session[i].id = sessionid;
			g_session[i].camerachannel = -1;
			g_session[i].videostatus = 0;
			g_session[i].videoflag = 0;
			g_session[i].audiostatus = 0;
			g_session[i].sendmode = 0;
			g_session[i].recimgstop = 0;
			
			session_channel = i;
			break;
		}
	}

	pthread_mutex_unlock(&mutex_session_lock);

	if(i >= MAX_SESSION_NUM)
	{
		PPCS_Close(sessionid);
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: ========> More Than Max Session <========\n", FUN, LINE);
		
		return NULL;
	}

	//t_start.tv_sec = 0;

	ret = PPCS_Check(sessionid, &Sinfo);
	if(ret != ERROR_PPCS_SUCCESSFUL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check err, ret = %d, session_id = %d, session_ch = %d\n", 
		                              FUN, LINE, ret, sessionid, session_channel);
	}
	else
	{
		VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------------- Session[%d] Ready:[%s] ------------------\n", 
		                                FUN, LINE, session_channel, (Sinfo.bMode == 0) ? "P2P" : "RLY");
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Session : %d\n", FUN, LINE, sessionid);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Socket : %d\n", FUN, LINE, sessionid);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Remote Addr : %s:%d\n", FUN, LINE, inet_ntoa(Sinfo.RemoteAddr.sin_addr), ntohs(Sinfo.RemoteAddr.sin_port));
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: My Lan Addr : %s:%d\n", FUN, LINE, inet_ntoa(Sinfo.MyLocalAddr.sin_addr), ntohs(Sinfo.MyLocalAddr.sin_port));
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: My Wan Addr : %s:%d\n", FUN, LINE, inet_ntoa(Sinfo.MyWanAddr.sin_addr), ntohs(Sinfo.MyWanAddr.sin_port));
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Connection time : %d second before\n", FUN, LINE, Sinfo.ConnectTime);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: DID : %s\n", FUN, LINE, Sinfo.DID);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: MODE : %s\n", FUN, LINE, (Sinfo.bCorD == 0) ? "Client" : "Device");
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------------- End of Session[%d] info -----------------\n", FUN, LINE, session_channel);
		VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	}

	while(g_running)
	{
		#if 0
		if(auth_flag == 1) //认证通过后再发送
		{
			if(firstconnect == 1 || g_devinfo_update == 1)
			{
				firstconnect = 0;
				g_devinfo_update = 0;
				
				gettimeofday(&t_start, NULL);

				//发送设备信息
				VAVAHAL_SendDevInfo(sessionid, session_channel);
			}
			else
			{
				gettimeofday(&t_current, NULL);

				if(t_current.tv_sec - t_start.tv_sec >= 10 || heartflag == 1)
				{
					t_start.tv_sec = t_current.tv_sec;
					heartflag = 0;
					
					//发送设备信息
					VAVAHAL_SendDevInfo(sessionid, session_channel);
				}
			}
		}
		#else
		if(heartflag == 1)
		{
			heartflag = 0;
			
			//发送设备信息
			VAVAHAL_SendDevInfo(sessionid, session_channel);
		}
		#endif
		
		memset(readbuff, 0, P2P_BUFF_SIZE);
		readsize = sizeof(VAVA_CMD_HEAD);
		ret = PPCS_Read(sessionid, P2P_CHANNEL_CMD, readbuff, &readsize, 2000);
		if(ret == ERROR_PPCS_SUCCESSFUL && readsize == sizeof(VAVA_CMD_HEAD))
		{
			vavahead = (VAVA_CMD_HEAD *)readbuff;

			VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [session-%d]--> sync_code : %x\n", FUN, LINE, session_channel, vavahead->sync_code);
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [session-%d]--> cmd_code : %d\n", FUN, LINE, session_channel, vavahead->cmd_code);
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [session-%d]--> cmd_length : %d\n", FUN, LINE, session_channel, vavahead->cmd_length);

			if(vavahead->sync_code != VAVA_TAG_APP_CMD || vavahead->cmd_length >= 3000)
			{
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: Get synccode[%x] err or size more than 3000 Byte\n", FUN, LINE, session_channel, vavahead->sync_code);
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: --- session [%d] need reconnect ---\n", FUN, LINE, session_channel);

				VAVAHAL_SendPaseErr(sessionid);
				
				memset(errstr, 0, 128);
				sprintf(errstr, "session %d cmd paress err", session_channel);
				Err_Log(errstr);

				sleep(1);
				break;
			}

			if(vavahead->cmd_length > 0)
			{
				readsize = vavahead->cmd_length;
				ret = PPCS_Read(sessionid, P2P_CHANNEL_CMD, readbuff + sizeof(VAVA_CMD_HEAD), &readsize, 0xFFFFFFFF);
				if(ret != ERROR_PPCS_SUCCESSFUL)
				{
					if(ret == ERROR_PPCS_TIME_OUT)
					{
						continue;
					}

					if(ret == ERROR_PPCS_NOT_INITIALIZED || ret == ERROR_PPCS_INVALID_SESSION_HANDLE
						|| ret == ERROR_PPCS_SESSION_CLOSED_REMOTE || ret == ERROR_PPCS_SESSION_CLOSED_TIMEOUT)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [session-%d] read fail, ret = %d\n", FUN, LINE, session_channel, ret);
						break;
					}
				}

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------- RECV BUFF -----------------\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: session_channel = [%d], recv size = [%d]\n", FUN, LINE, session_channel, readsize);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: buff = %s\n", FUN, LINE, readbuff + sizeof(VAVA_CMD_HEAD));
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------------------------------------------\n", FUN, LINE);
				VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);

				//解析参数
				pRoot = cJSON_Parse(readbuff + sizeof(VAVA_CMD_HEAD));
			}
			else
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------- RECV BUFF -----------------\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: session_channel = [%d], recv size = [0]\n", FUN, LINE, session_channel);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: This CMD no param\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------------------------------------------\n", FUN, LINE);
			}

			pJsonRoot = cJSON_CreateObject();
			if(pJsonRoot == NULL)
			{
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [session-%d] cJSON_CreateObject err\n", FUN, LINE, session_channel);
				
				Err_Log("json malloc fail");
				g_running = 0;
				return NULL;
			}

			switch(vavahead->cmd_code)
			{
				case VAVA_CMD_SESSION_AUTH:
					auth_flag = 0;
					errcode = VAVAHAL_ParamParse_SessionAuth(pRoot, &random, md5, authkey);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//验证MD5值
					memset(tmpstr, 0, 32);
					sprintf(tmpstr, "vava:%d:2017", random);

					memset(md5_str16, 0, 16);
					memset(md5_str32, 0, 32);
					MD5Init(&md5_handle);
					MD5Update(&md5_handle, (unsigned char *)tmpstr, strlen(tmpstr));
					MD5Final(md5_str16, &md5_handle);
					for(i = 0; i < 16; i++)
					{
						sprintf((char *)(md5_str32 + i * 2), "%.2x", md5_str16[i]);
					}
					
					//此处不区分大小写比较
					if(strcasecmp((char *)md5_str32, md5) != 0)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [session-%d], auth err, [%s][%s]\n", FUN, LINE, session_channel, md5_str32, md5);
						
						errcode = VAVA_ERR_CODE_SESSION_AUTH_FAIL;
						break;
					}

					memset(bskey, 0, 36);
					sprintf(bskey, "VAVA-%d%02d%02d-AUTH", 1900 + timeinfo->tm_year, timeinfo->tm_mon + 1, timeinfo->tm_mday);

					if(strcmp(authkey, bskey) != 0)
					{
						//与服务器验证
						ret = VAVASERVER_GetToken();
						if(ret != 0)
						{
							VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [session-%d], get token fail\n", FUN, LINE, session_channel);
							
							errcode = VAVA_ERR_CODE_SESSION_AUTH_FAIL;
							break;
						}
						
						ret = VAVASERVER_AuthKey(authkey);
						if(ret != 0)
						{
							VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [session-%d], auth key fail\n", FUN, LINE, session_channel);
							errcode = VAVA_ERR_CODE_SESSION_AUTH_FAIL;
							break;
						}
					}
					else
					{
						g_session[session_channel].debugauth = 1;
					}

					errcode = VAVAHAL_ParamBuild_Auth(pJsonRoot);

					auth_flag = 1;
					break;
				case VAVA_CMD_WAKE_UP:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否配对
					if(g_pair[channel].nstat == 0)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_NOCAMERA;
						break;
					}

					//检测是否在线
					if(g_camerainfo_dynamic[channel].online == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_OFFLINE;
						break;
					}

					//检测是否已准备好
					if(g_camerainfo[channel].first_flag == 1)
					{
						errcode = VAVA_ERR_CODE_CAMERA_NOREADY;
						break;
					}
					
					if(g_session[session_channel].camerachannel != -1)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_USED;
						break;
					}

					//检测是否低电量
					if(g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo[channel].powermode == 0)
					{
						errcode = VAVA_ERR_CODE_POWER_LOW;
						break;
					}

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [WAKE-UP][User] channel - %d\n", FUN, LINE, channel);
					
					errcode = VAVAHAL_WakeupCamera_Ex(channel, WAKE_UP_MODE_APP);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [session-%d], VAVA_CMD_WAKE_UP errode = %d\n", FUN, LINE, session_channel, errcode);
					#if 0
					//开启全时录像
					VAVAHAL_StartFullTimeRec(channel);
					#endif

					//APP唤醒标记
					g_camerainfo[channel].wakeup_flag = 1;
					g_session[session_channel].wakeupstatus = 1;
					g_session[session_channel].camerachannel = channel;
					videochannel = channel;

					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_SLEEP:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否在线
					if(g_camerainfo_dynamic[channel].online == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_OFFLINE;
						break;
					}

					if(g_session[session_channel].camerachannel != channel)
					{
						errcode = VAVA_ERR_CODE_PARAM_INVALID;
						break;
					}

					g_session[session_channel].camerachannel = -1;
					g_session[session_channel].wakeupstatus = 0;
					g_session[session_channel].videostatus = 0;
					g_session[session_channel].audiostatus = 0;
					g_session[session_channel].videoflag = 0;

					//如果对讲有打开 则关闭对讲
					if(g_camera_talk[channel] == sessionid)
					{
						g_camera_talk[channel] = -1;
						g_avmemchace[channel].ap_nstats = 0;
					}

					//释放手动录像
					if(g_manaulrec[channel].session == sessionid)
					{
						g_manaulrec[channel].ctrl = 0;
						VAVAHAL_InsertCmdList(channel, sessionid, VAVA_CMD_STOP_REC, NULL, 0);
					}

					//查询其它会话是否开启视频
					for(i = 0; i < MAX_SESSION_NUM; i++)
					{
						if(g_session[i].camerachannel == channel && g_session[i].wakeupstatus == 1)
						{
							break;
						}
					}

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [session-%d], sleep camera, channel = %d, i = %d, alarm = %d, cloud = %d\n", 
					                                FUN, LINE, session_channel, channel, i, g_camerainfo[channel].alarm_flag, 
					                                g_camerainfo[channel].cloud_flag);

					if(i >= MAX_SESSION_NUM) //无会话唤醒摄像机
					{
						//APP唤醒标记
						g_camerainfo[channel].wakeup_flag = 0;
						
						if(g_camerainfo[channel].alarm_flag == 0 && g_camerainfo[channel].cloud_flag == 0)
						{
							//停止录像
							VAVAHAL_StopManualRec(channel);

							//休眠摄像机
							#if 0
							errcode = VAVAHAL_SleepCamera_Ex(channel);
							if(errcode != VAVA_ERR_CODE_SUCCESS)
							{
								break;
							}
							#else
							VAVAHAL_SleepCamera_Ex(channel);
							#endif
						}
					}

					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_OPENVIDEO:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_session[session_channel].wakeupstatus == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_DORMANT;
						break;
					}

					if(g_session[session_channel].camerachannel != channel)
					{
						errcode = VAVA_ERR_CODE_PARAM_INVALID;
						break;
					}

					g_session[session_channel].videostatus = 1;
					g_session[session_channel].videoflag = 0;
					g_session[session_channel].framenum = 0;
					errcode = VAVAHAL_ParamBuild_VideoInfo(pJsonRoot, channel);
					break;
				case VAVA_CMD_CLOSEVIDEO:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}
					
					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_session[session_channel].camerachannel != channel)
					{
						errcode = VAVA_ERR_CODE_PARAM_INVALID;
						break;
					}
					
					g_session[session_channel].videostatus = 0;
					g_session[session_channel].videoflag = 0;
					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_AUDIO_OPEN:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_session[session_channel].camerachannel != channel)
					{
						errcode = VAVA_ERR_CODE_PARAM_INVALID;
						break;
					}

					//检测是否已唤醒
					if(g_session[session_channel].wakeupstatus == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_DORMANT;
						break;
					}

					//检测麦克全局开关
					if(g_camerainfo[channel].mic == 0)
					{
						errcode = VAVA_ERR_CODE_MIC_CLOSED;
						break;
					}

					g_session[session_channel].audiostatus = 1;

					errcode = VAVAHAL_ParamBuild_AudioOpen(pJsonRoot, channel);
					break;
				case VAVA_CMD_AUDIO_CLOSE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_session[session_channel].camerachannel != channel)
					{
						errcode = VAVA_ERR_CODE_PARAM_INVALID;
						break;
					}

					g_session[session_channel].audiostatus = 0;
					
					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_TALK_OPEN:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_session[session_channel].camerachannel != channel)
					{
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [session-%d], VAVA_CMD_TALK_OPEN fail, camerachannel = %d, channel = %d\n", 
					                                    FUN, LINE, session_channel, g_session[session_channel].camerachannel, channel);
						
						errcode = VAVA_ERR_CODE_PARAM_INVALID;
						break;
					}

					//检测视频是否已打开
					if(g_session[session_channel].wakeupstatus == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_DORMANT;
						break;
					}

					//检测喇叭全局开关
					if(g_camerainfo[channel].speeker == 0)
					{
						errcode = VAVA_ERR_CODE_SPEAKER_CLOSED;
						break;
					}

					//检测对讲通道是否被占用
					if(g_camera_talk[channel] != -1)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_USED;
						break;
					}

					//保存对讲会话ID
					g_camera_talk[channel] = sessionid;

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [TalkOpen]: sessionid = %d\n", FUN, LINE, sessionid);

					//同步缓冲区
					g_avmemchace[channel].ap_read = g_avmemchace[channel].ap_write;
					g_avmemchace[channel].ap_nstats = 1;

					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_TALK_CLOSE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_session[session_channel].camerachannel != channel)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [session-%d] VAVA_CMD_TALK_CLOSE fail, camerachannel = %d, channel = %d\n", 
						                              FUN, LINE, session_channel, g_session[session_channel].camerachannel, channel);
						
						errcode = VAVA_ERR_CODE_PARAM_INVALID;
						break;
					}

					//检测对讲通道是否被占用
					if(g_camera_talk[channel] != sessionid)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [session-%d] VAVA_CMD_TALK_CLOSE fail, tolk channel is no idle [%d]\n", 
						                              FUN, LINE, session_channel, g_camera_talk[channel]);
						
						errcode = VAVA_ERR_CODE_PARAM_INVALID;
						break;
					}

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [TalkClose]: sessionid = %d\n", FUN, LINE, sessionid);

					g_camera_talk[channel] = -1;
					g_avmemchace[channel].ap_nstats = 0;

					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_MIC_OPEN:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_camerainfo[channel].mic != 1)
					{
						g_camerainfo[channel].mic = 1;
						VAVAHAL_WriteMicInfo();
					}
					
					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_MIC_CLOSE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_camerainfo[channel].mic != 0)
					{
						g_camerainfo[channel].mic = 0;
						VAVAHAL_WriteMicInfo();
					}
					
					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_SPEAKER_OPEN:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_camerainfo[channel].speeker != 1)
					{
						g_camerainfo[channel].speeker = 1;
						VAVAHAL_WriteSpeakerInfo();
					}
					
					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_SPEAKER_CLOSE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_camerainfo[channel].speeker != 0)
					{
						g_camerainfo[channel].speeker = 0;
						VAVAHAL_WriteSpeakerInfo();
					}

					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_BUZZER_OPEN:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Buzzertype(pRoot);
					break;
				case VAVA_CMD_BUZZER_CLOSE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					g_buzzer_flag = 0;
					g_devinfo_update = 1;
					
					errcode = VAVA_ERR_CODE_SUCCESS;
					break;
				case VAVA_CMD_GATEWAYINFO:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}
					
					errcode = VAVAHAL_ParamBuild_GateWayInfo(pJsonRoot);
					break;
				case VAVA_CMD_CAMERAINFO:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否配对
					if(g_pair[channel].nstat == 0)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_NOCAMERA;
						break;
					}

					errcode = VAVAHAL_ParamBuild_CameraInfo(pJsonRoot, channel);
					break;
				case VAVA_CMD_SDINFO:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamBuild_SDInfo(pJsonRoot);
					break;
				case VAVA_CMD_GET_BATTERY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_BatteryInfo(pJsonRoot, channel);
					break;
				case VAVA_CMD_GET_ARMINGSTATUS:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_Status(pJsonRoot, channel, g_camera_arminfo_v1[channel].status);
					break;
				case VAVA_CMD_GET_DEVINFO:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					heartflag = 1;
					errcode = VAVA_ERR_CODE_SUCCESS;
					break;
				case VAVA_CMD_GET_CAMERACONFIG:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_CameraConfig(pJsonRoot, channel);
					break;
				case VAVA_CMD_GET_VIDEO_QUALITY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_VideoQuality(pJsonRoot, channel, VAVA_STREAM_TYPE_SUB);
					break;
				case VAVA_CMD_SET_VIDEO_QUALITY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_VideoQuality(pRoot, &channel, &quality);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否配对
					if(g_pair[channel].nstat == 0)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_NOCAMERA;
						break;
					}

					//检测是否在线
					if(g_camerainfo_dynamic[channel].online == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_OFFLINE;
						break;
					}

					//检测是否已准备好
					if(g_camerainfo[channel].first_flag == 1)
					{
						errcode = VAVA_ERR_CODE_CAMERA_NOREADY;
						break;
					}

					//检测是否低电量
					if(g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo[channel].powermode == 0)
					{
						errcode = VAVA_ERR_CODE_POWER_LOW;
						break;
					}

					if(quality != VAVA_VIDEO_QUALITY_AUTO)
					{
						g_tmp_quality[channel] = quality;
						
						switch(g_tmp_quality[channel])
						{
							case VAVA_VIDEO_QUALITY_BEST:
								res = VAVA_VIDEO_RESOULT_1080P;
								fps = 15;
								bitrate = 1000; //1000
								break;
							case VAVA_VIDEO_QUALITY_HIGH:
								res = VAVA_VIDEO_RESOULT_720P;
								fps = 15;
								bitrate = 700;
								break;
							case VAVA_VIDEO_QUALITY_RENEWAL:
								res = VAVA_VIDEO_RESOULT_360P;
								fps = 15;
								bitrate = 400;
								break;
							default:
								res = VAVA_VIDEO_RESOULT_360P;
								fps = 15;
								bitrate = 400;
								break;
						}

						memset(&resconfig, 0, sizeof(VAVA_ResConfig));
						resconfig.stream = VAVA_STREAM_TYPE_SUB;
						resconfig.res = res;
						resconfig.fps = fps;
						resconfig.bitrate = bitrate;
						errcode = VAVAHAL_InsertCmdList(channel, sessionid, vavahead->cmd_code, (void *)&resconfig, sizeof(VAVA_ResConfig));
						if(errcode == VAVA_ERR_CODE_SUCCESS)
						{
							errcode = VAVA_ERR_CODE_NORETURN;
						}
					}
					else
					{
						errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
						g_camerainfo[channel].v_quality = quality;
						VAVAHAL_WriteVideoQuality();
					}

					break;
				case VAVA_CMD_GET_MIRRORMODE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_MirrMode(pJsonRoot, channel);
					break;
				case VAVA_CMD_SET_MIRRORMODE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_MirrMode(pRoot, &channel, &mirrmode);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否配对
					if(g_pair[channel].nstat == 0)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_NOCAMERA;
						break;
					}

					//检测是否在线
					if(g_camerainfo_dynamic[channel].online == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_OFFLINE;
						break;
					}

					//检测是否已准备好
					if(g_camerainfo[channel].first_flag == 1)
					{
						errcode = VAVA_ERR_CODE_CAMERA_NOREADY;
						break;
					}

					//检测是否低电量
					if(g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo[channel].powermode == 0)
					{
						errcode = VAVA_ERR_CODE_POWER_LOW;
						break;
					}

					memset(&mirrconfig, 0, sizeof(VAVA_MirrConfig));
					mirrconfig.param = mirrmode;
					errcode = VAVAHAL_InsertCmdList(channel, sessionid, vavahead->cmd_code, (void *)&mirrconfig, sizeof(VAVA_MirrConfig));
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_GET_IRMODE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_IrMode(pJsonRoot, channel);
					break;
				case VAVA_CMD_SET_IRMODE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_IrLedMode(pRoot, &channel, &irledmode);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否配对
					if(g_pair[channel].nstat == 0)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_NOCAMERA;
						break;
					}

					//检测是否在线
					if(g_camerainfo_dynamic[channel].online == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_OFFLINE;
						break;
					}

					//检测是否已准备好
					if(g_camerainfo[channel].first_flag == 1)
					{
						errcode = VAVA_ERR_CODE_CAMERA_NOREADY;
						break;
					}

					//检测是否低电量
					if(g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo[channel].powermode == 0)
					{
						errcode = VAVA_ERR_CODE_POWER_LOW;
						break;
					}

					memset(&irledconfig, 0, sizeof(VAVA_IrLedConfig));
					irledconfig.param = irledmode;
					errcode = VAVAHAL_InsertCmdList(channel, sessionid, vavahead->cmd_code, (void *)&irledconfig, sizeof(VAVA_IrLedConfig));
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_GET_PUSHCONFIG:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_PushConfig(pJsonRoot, channel);
					break;
				case VAVA_CMD_SET_PUSHCONFIG:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}
					
					errcode = VAVAHAL_ParamParse_PushConfig(pRoot);
					break;
				case VAVA_CMD_GET_PIR_SENSITIVITY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_PirSensitivity(pJsonRoot, channel);
					break;
				case VAVA_CMD_SET_PIR_SENSITIVITY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_PirSsty(pRoot, &channel);
					break;
				case VAVA_CMD_GET_MDPARAM:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_MDInfo(pJsonRoot, channel);
					break;
				case VAVA_CMD_SET_MDPARAM:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					memset(&mdconfig, 0, sizeof(VAVA_MdConfig));
					errcode = VAVAHAL_ParamParse_MdInfo(pRoot, &channel, &mdconfig);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否配对
					if(g_pair[channel].nstat == 0)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_NOCAMERA;
						break;
					}

					//检测是否在线
					if(g_camerainfo_dynamic[channel].online == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_OFFLINE;
						break;
					}

					//检测是否已准备好
					if(g_camerainfo[channel].first_flag == 1)
					{
						errcode = VAVA_ERR_CODE_CAMERA_NOREADY;
						break;
					}

					//检测是否低电量
					if(g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo[channel].powermode == 0)
					{
						errcode = VAVA_ERR_CODE_POWER_LOW;
						break;
					}
					
					errcode = VAVAHAL_InsertCmdList(channel, sessionid, vavahead->cmd_code, (void *)&mdconfig, sizeof(VAVA_MdConfig));
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_GET_ARMINGINFO_V1:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_CameraArmInfo_v1(pJsonRoot, channel);
					break;
				case VAVA_CMD_SET_ARMINGINFO_V1:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_ArmInfo_V1(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_GET_EMAILCONFIG:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_EmailConfig(pJsonRoot, channel);
					break;
				case VAVA_CMD_SET_EMAILCONFIG:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}
					
					errcode = VAVAHAL_ParamParse_EmailConfig(pRoot);
					break;
				case VAVA_CMD_GET_REC_QUALITY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_VideoQuality(pJsonRoot, channel, VAVA_STREAM_TYPE_MAIN);
					break;
				case VAVA_CMD_SET_REC_QUALITY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVA_ERR_CODE_NOSUPPORT;
					break;

					#if 0
					errcode = VAVAHAL_ParamParse_VideoQuality(pRoot, &channel, &quality);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					memset(&resconfig, 0, sizeof(VAVA_ResConfig));
					resconfig.stream = VAVA_STREAM_TYPE_MAIN;
					resconfig.res = res;
					resconfig.fps = fps;
					resconfig.bitrate = bitrate;
					VAVAHAL_InsertCmdList(channel, sessionid, vavahead->cmd_code, 
						                 (void *)&resconfig, sizeof(VAVA_ResConfig));
					
					errcode = VAVA_ERR_CODE_NORETURN;
					#endif
					break;
				case VAVA_CMD_GET_RECTIME:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamBuild_RecTimeInfo(pJsonRoot);
					break;
				case VAVA_CMD_SET_RECTIME:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_RecTimeInfo(pRoot);
					break;
				case VAVA_CMD_GET_FULLTIMEREC_CTRL:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamBuild_Ctrl(pJsonRoot, channel, g_rectime.fulltime_ctrl[channel]);
					break;
				case VAVA_CMD_SET_FULLTIMEREC_CTRL:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Ctrl(pRoot, &channel, &ctrl);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_rectime.fulltime_ctrl[channel] != ctrl)
					{
						g_rectime.fulltime_ctrl[channel] = ctrl;
						VAVAHAL_WriteRecTime();
					}

					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_RECORDDATE_SEARCH:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_CheckSDStatus();
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}
	
					errcode = VAVAHAL_InsertCmdList(0, sessionid, vavahead->cmd_code, NULL, 0);
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_RECORDLIST_SEARCH:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_CheckSDStatus();
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamParse_RecSearch(pRoot, &recsearch);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_InsertCmdList(0, sessionid, vavahead->cmd_code, &recsearch, sizeof(VAVA_RecSearch));
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_RECSHAREDATE_SEARCH:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_CheckSDStatus();
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamParse_RecShareDate(pRoot, &recsharedate);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_InsertCmdList(0, sessionid, vavahead->cmd_code, &recsharedate, sizeof(VAVA_RecShareDateSearch));
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_RECSHARELIST_SEARCH:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_CheckSDStatus();
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_ParamParse_RecShareList(pRoot, &recsharelist);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_InsertCmdList(0, sessionid, vavahead->cmd_code, &recsharelist, sizeof(VAVA_RecShareListSearch));
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_RECORD_IMG:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					memset(&recimgget, 0, sizeof(VAVA_RecImgGet));
					errcode = VAVAHAL_ParamParse_RecImg(pRoot, &channel, &recimgget);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					ret = VAVAHAL_FindFile(recimgget.dirname, recimgget.filename);
					if(ret != 0)
					{
						errcode = VAVA_ERR_CODE_RECFILE_NOTFOUND;
						cJSON_AddStringToObject(pJsonRoot, "dirname", recimgget.dirname);
						cJSON_AddStringToObject(pJsonRoot, "filename", recimgget.filename);
						break;
					}

					errcode = VAVAHAL_InsertCmdList(session_channel, sessionid, vavahead->cmd_code, &recimgget, sizeof(VAVA_RecImgGet));
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					}
					break;
				case VAVA_CMD_RECORD_IMG_STOP:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					g_session[session_channel].recimgstop = 1;
					errcode = VAVAHAL_InsertCmdList(session_channel, sessionid, vavahead->cmd_code, NULL, 0);
					break;
				case VAVA_CMD_RECORD_DEL:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: VAVA_CMD_RECORD_DEL ============  A\n", FUN, LINE);
					pthread_mutex_lock(&mutex_idx_lock);
					errcode = VAVAHAL_ParamParse_RecDelList(pRoot, pJsonRoot, sessionid);
					pthread_mutex_unlock(&mutex_idx_lock);
					//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: VAVA_CMD_RECORD_DEL ============  B\n", FUN, LINE);
					
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}
					break;
				case VAVA_CMD_RECOCD_GETSIZE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamBuild_RecFileSize(pRoot, pJsonRoot);
					break;
				case VAVA_CMD_RECORD_PLAY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_RecPlay(pRoot, sessionid, &channel, &recplay_channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [TEST315] PLAY REC FAIL, ret = %d\n", FUN, LINE, errcode);
						
						if(errcode == VAVA_ERR_CODE_RECPLAY_REREQ)
						{
							cJSON_AddNumberToObject(pJsonRoot, "token", g_recplay[recplay_channel].token);
						}
						
						break;
					}

					errcode = VAVAHAL_ParamBuild_RecPlay(pJsonRoot, channel, recplay_channel);
					break;
				case VAVA_CMD_RECORD_PLAY_CTRL:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_RecPlayCtrl(pRoot, sessionid, &channel, &ctrl);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: [TEST316] PLAY CTRL FAIL, ret = %d\n", FUN, LINE, errcode);
						break;
					}

					if(ctrl == 2)
					{
						while(g_running)
						{
							if(g_recplay[channel].fd == NULL)
							{
								break;
							}

							usleep(20000);
						}
					}
					
					errcode = VAVAHAL_ParamBuild_Ctrl(pJsonRoot, channel, ctrl);
					break;
				case VAVA_CMD_SNAPSHOT:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否配对
					if(g_pair[channel].nstat == 0)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_NOCAMERA;
						break;
					}

					//检测是否在线
					if(g_camerainfo_dynamic[channel].online == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_OFFLINE;
						break;
					}

					//检测是否已准备好
					if(g_camerainfo[channel].first_flag == 1)
					{
						errcode = VAVA_ERR_CODE_CAMERA_NOREADY;
						break;
					}

					//检测是否低电量
					if(g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo[channel].powermode == 0)
					{
						errcode = VAVA_ERR_CODE_POWER_LOW;
						break;
					}

					errcode = VAVAHAL_InsertCmdList(channel, sessionid, vavahead->cmd_code, NULL, 0);
					break;
				case VAVA_CMD_START_REC:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_gatewayinfo.sdstatus != VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.totol < 100)
					{
						errcode = VAVA_ERR_CODE_SD_NOFOUND;
						break;
					}

					if(g_gatewayinfo.format_flag == 1)
					{
						errcode = VAVA_ERR_CODE_SD_NEED_FORMAT;
						break;
					}

					if(g_session[session_channel].wakeupstatus == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_DORMANT;
						break;
					}

					#if 0
					if(g_alarmrec[channel].ctrl == 1)
					{
						errcode = VAVA_ERR_CODE_RECSTART_ALARM;
						break;
					}
					#endif

					if(g_manaulrec[channel].ctrl == 1)
					{
						errcode = VAVA_ERR_CODE_RECSTART_NOIDLE;
						break;
					}

					if(g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo[channel].powermode == 0)
					{
						errcode = VAVA_ERR_CODE_POWER_LOW;
						break;
					}

					g_manaul_ntsamp[channel] = 0;

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: START REC, session = %d\n", FUN, LINE, sessionid);

					VAVAHAL_StartAVMemCahce(channel, VAVA_AVMEM_MODE_REC);
					errcode = VAVAHAL_InsertCmdList(channel, sessionid, VAVA_CMD_START_REC, NULL, 0);
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					//errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_STOP_REC:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					if(g_session[session_channel].wakeupstatus == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_DORMANT;
						break;
					}

					if(g_manaulrec[channel].session == sessionid)
					{
						g_manaulrec[channel].ctrl = 0;

						if(g_alarmrec[channel].ctrl == 0 && g_cloudrec[channel].ctrl == 0)
						{
							VAVAHAL_InsertCmdList(channel, sessionid, VAVA_CMD_STOP_REC, NULL, 0);
						}
					}
					else
					{
						errcode = VAVA_ERR_CODE_RECSSTOP_NOOPEN;
						break;
					}

					errcode = VAVAHAL_ParamBuild_Channel(pJsonRoot, channel);
					break;
				case VAVA_CMD_GET_TIME:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamBuild_GateWayTime(pJsonRoot);
					break;
				case VAVA_CMD_SET_TIME:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_GateWayTime(pRoot);
					break;
				case VAVA_CMD_SET_LANGUAGE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Language(pRoot);
					break;
				case VAVA_CMD_FORMAT_STORAGE:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					#ifdef NAS_NFS
					//关闭NAS
					Nas_stop();
					#endif

					//格式化卡禁止录像
					VAVAHAL_CloseRec(1, 0);

					for(i = 0; i < MAX_CHANNEL_NUM; i++)
					{	
						//关闭录像
						VAVAHAL_StopManualRec(channel);
						VAVAHAL_StopFullTimeRec(channel);
						VAVAHAL_StopAlarmRec(channel);
					}
					
					sleep(2);

					gettimeofday(&t_format1, NULL);
					
					pthread_mutex_lock(&mutex_format_lock);
					g_sdformating = 1;
					errcode = VAVAHAL_ForMatSD();
					g_sdformating = 0;
					pthread_mutex_unlock(&mutex_format_lock);

					gettimeofday(&t_format2, NULL);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------------\n", FUN, LINE);
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------- TF Format Used Time %d s ----------\n", FUN, LINE, (int)(t_format2.tv_sec - t_format1.tv_sec + 1));
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------------\n", FUN, LINE);

					g_sdformat = 1;
					g_gatewayinfo.free = g_gatewayinfo.totol;
					g_gatewayinfo.used = 0;

					//格式化完成后开启录像
					VAVAHAL_OpenRec(1, 0);

					#ifdef NAS_NFS
					//重新加载NAS配置
					VAVAHAL_ReadNasConfig();
					#endif
					break;
				case VAVA_CMD_WIFI_CONFIG:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_WifiConfig(pRoot);
					break;
				case VAVA_CMD_CAMERA_PAIR:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_pairmode == 1)
					{
						errcode = VAVA_ERR_CODE_PAIRNG;
						break;
					}

					memset(camerasn, 0, 32);
					errcode = VAVAHAL_ParamParse_Camerapair(pRoot, &pairchannel, camerasn, cameraname);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_SearchList_Find(camerasn, &cameraaddr);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					errcode = VAVAHAL_PairCamera(pJsonRoot, sessionid, cameraaddr, camerasn, cameraname, pairchannel);
					if(errcode == VAVA_ERR_CODE_PAIR_FAIL)
					{
						VAVAHAL_PlayAudioFile("/tmp/sound/sync_fail.opus");
					}
					else if(errcode == VAVA_ERR_CODE_CHANNEL_FULL)
					{
						VAVAHAL_PlayAudioFile("/tmp/sound/channel_full.opus");
					}
					break;
				case VAVA_CMD_GET_UPDATE_STATUS:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamBuild_UpStatus(pJsonRoot);
					break;
				case VAVA_CMD_CLEARMATCH:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

				#if 1
					errcode = VAVA_ERR_CODE_NOSUPPORT;
				#else
					errcode = VAVAHAL_ParamParse_ClearPair(pRoot);
				#endif
					break;
				case VAVA_CMD_CAMERA_RESET:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamParse_Channel(pRoot, &channel);
					if(errcode != VAVA_ERR_CODE_SUCCESS)
					{
						break;
					}

					//检测是否配对
					if(g_pair[channel].nstat == 0)
					{
						errcode = VAVA_ERR_CODE_CHANNEL_NOCAMERA;
						break;
					}

					//检测是否在线
					if(g_camerainfo_dynamic[channel].online == 0)
					{
						errcode = VAVA_ERR_CODE_CAMERA_OFFLINE;
						break;
					}

					//检测是否已准备好
					if(g_camerainfo[channel].first_flag == 1)
					{
						errcode = VAVA_ERR_CODE_CAMERA_NOREADY;
						break;
					}

					//检测是否低电量
					if(g_camerainfo_dynamic[channel].battery <= CAMERA_LOW_POWER && g_camerainfo[channel].powermode == 0)
					{
						errcode = VAVA_ERR_CODE_POWER_LOW;
						break;
					}

					memset(&camerafactory, 0, sizeof(VAVA_CameraFactory));
					camerafactory.sessionid = sessionid;
					camerafactory.channel = channel;
					camerafactory.result = 0;
					errcode = VAVAHAL_InsertCmdList(channel, sessionid, vavahead->cmd_code, &camerafactory, sizeof(VAVA_CameraFactory));
					if(errcode == VAVA_ERR_CODE_SUCCESS)
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_RESET_FACTORY:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_InsertCmdList(0, 0, vavahead->cmd_code, NULL, 0);
					break;
				case VAVA_CMD_SYSTEM_NEWVESION:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					pthread_mutex_lock(&mutex_upgrate_lock);
					errcode = VAVAHAL_ParamParse_NewVersion(pRoot, sessionid);
					pthread_mutex_unlock(&mutex_upgrate_lock);
					break;
				case VAVA_CMD_POP_TF:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					g_sdpoping = 1;
					errcode = VAVAHAL_PopTFCard();
					break;
				case VAVA_CMD_GET_SYDID:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamBuild_SyDid(pJsonRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_CLEAR_SYDID:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVA_ERR_CODE_NOAUSH;
					break;
				case VAVA_CMD_GET_DOMAN:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamBuild_Doman(pJsonRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_SET_DOMAN:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamParse_Doman(pRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_GET_P2PSERVER:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamBuild_P2PServer(pJsonRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_SET_P2PSERVER:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamParse_P2PServer(pRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_GET_NASSERVER:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_ParamBuild_NasServer(pJsonRoot);
					break;
				case VAVA_CMD_SET_NASSERVER:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_Param_Parse_NasServer(pRoot);
					break;
				case VAVA_CMD_CLOUD_REFRESH:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					errcode = VAVAHAL_RefreshCloud();
					break;
				case VAVA_CMD_RF_TEST:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamParse_RfTest(pRoot, sessionid);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_CHANGE_SN:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamParse_Sn(pRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_RESET_RF:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						VAVAHAL_ResetRf();
						WatchDog_Stop("UserResetRF");
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_CLOUDE_DEBUG:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamParse_CloudDebugLevel(pRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;	
					}
					break;
				case VAVA_CMD_COM_CTRL:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamParse_ComCtrl(pRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_RESET_SYSTEM:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						VAVAHAL_SystemCmd("reboot");
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_IPC_COM_CTRL:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamParse_IPCComCtrl(pRoot, &channel, &ctrl);
						if(errcode != VAVA_ERR_CODE_SUCCESS)
						{
							break;
						}

						memset(&comconfig, 0, sizeof(VAVA_IPCComConfig));
						comconfig.ctrl = ctrl;
						errcode = VAVAHAL_InsertCmdList(channel, sessionid, vavahead->cmd_code, (void *)&comconfig, sizeof(VAVA_IPCComConfig));
						if(errcode == VAVA_ERR_CODE_SUCCESS)
						{
							errcode = VAVA_ERR_CODE_NORETURN;
						}
					}
					else
					{
						errcode = VAVA_ERR_CODE_NOAUSH;
					}
					break;
				case VAVA_CMD_RESET_UPSERVER:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ResetUpServer();
					}
					else
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				case VAVA_CMD_LOG_LEVER:
					if(auth_flag == 0)
					{
						errcode = VAVA_ERR_CODE_UNAUTHORIZED;
						break;
					}

					if(g_session[session_channel].debugauth == 1)
					{
						errcode = VAVAHAL_ParamParse_LogLevel(pRoot);
					}
					else
					{
						errcode = VAVA_ERR_CODE_NORETURN;
					}
					break;
				default:
					errcode = VAVA_ERR_CODE_NOSUPPORT;
					break;
			}

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [session-%d] cmd - %d, errcode - %d\n", 
				                            FUN, LINE, session_channel, vavahead->cmd_code, errcode);

			if(errcode == VAVA_ERR_CODE_SUCCESS)
			{
				cJSON_AddStringToObject(pJsonRoot, "result", "ok");
			}
			else if(errcode == VAVA_ERR_CODE_NORETURN)
			{
				if(pRoot != NULL)
				{
					cJSON_Delete(pRoot);
					pRoot = NULL;
				}
				
				cJSON_Delete(pJsonRoot);
				continue;
			}
			else
			{
				cJSON_AddStringToObject(pJsonRoot, "result", "fail");
				cJSON_AddNumberToObject(pJsonRoot, "errno", errcode);
			}

			if(pRoot != NULL)
			{
				cJSON_Delete(pRoot);
				pRoot = NULL;
			}
			
			pstr = cJSON_PrintUnformatted(pJsonRoot);

			memset(readbuff + sizeof(VAVA_CMD_HEAD), 0, P2P_BUFF_SIZE - sizeof(VAVA_CMD_HEAD));
			vavahead->cmd_length = strlen(pstr);
			memcpy(readbuff + sizeof(VAVA_CMD_HEAD), pstr, strlen(pstr));
			sendsize = sizeof(VAVA_CMD_HEAD) + strlen(pstr);
			
			free(pstr);
			cJSON_Delete(pJsonRoot);
			
			pthread_mutex_lock(&mutex_session_lock);
			ret = PPCS_Write(sessionid, P2P_CHANNEL_CMD, readbuff, sendsize);
			pthread_mutex_unlock(&mutex_session_lock);

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [session-%d] PPCS_Write ret = %d\n", FUN, LINE, session_channel, ret);
			VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
			
			if(ret == ERROR_PPCS_NOT_INITIALIZED || ret == ERROR_PPCS_INVALID_SESSION_HANDLE
				|| ret == ERROR_PPCS_SESSION_CLOSED_REMOTE || ret == ERROR_PPCS_SESSION_CLOSED_TIMEOUT)
			{
				break;
			}
		}
		else
		{
			if(ret == ERROR_PPCS_TIME_OUT)
			{
				continue;
			}

			if(ret == ERROR_PPCS_NOT_INITIALIZED || ret == ERROR_PPCS_INVALID_SESSION_HANDLE
				|| ret == ERROR_PPCS_SESSION_CLOSED_REMOTE || ret == ERROR_PPCS_SESSION_CLOSED_TIMEOUT)
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [session-%d] Ppcs_read fail, ret = %d\n", FUN, LINE, session_channel, ret);
				break;
			}
		}
	}

	g_session[session_channel].id = -1;
	g_session[session_channel].camerachannel = -1;
	g_session[session_channel].wakeupstatus = 0;
	g_session[session_channel].videostatus = 0;
	g_session[session_channel].audiostatus = 0;
	g_session[session_channel].videoflag = 0;
	g_session[session_channel].sendmode = 0;
	g_session[session_channel].recimgstop = 0;
	g_session[session_channel].debugauth = 0;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_recplay[i].sesssion == sessionid)
		{
			g_recplay[i].ctrl = VAVA_RECFILE_PLAY_CTRL_STOP;
			break;
		}
	}

	if(videochannel != -1)
	{
		//查询其它会话是否唤醒摄像机
		for(i = 0; i < MAX_SESSION_NUM; i++)
		{
			if(g_session[i].camerachannel == videochannel && g_session[i].wakeupstatus == 1)
			{
				break;
			}
		}

		if(i >= MAX_SESSION_NUM)
		{
			//停止录像
			VAVAHAL_StopManualRec(channel);
			VAVAHAL_StopFullTimeRec(channel);

			g_camerainfo[videochannel].wakeup_flag = 0;
							
			//休眠摄像机
			VAVAHAL_SleepCamera_Ex(videochannel);

			//释放对讲资源
			if(g_camera_talk[videochannel] == sessionid)
			{
				g_camera_talk[videochannel] = -1;
			}
		}
	}

	PPCS_Close(sessionid);

	return NULL;
}

void *p2pservers_pth(void *data)
{
	int ret;
	int version;
	int tmpid;
	int sessionid;
	int waitcount = 0;
	st_PPCS_NetInfo NetInfo;
	char apistr[16];

	//延时启动 避免与DHCP服务器冲突
	while(g_running)
	{
		sleep(1);

		waitcount++;

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: wait Internet...%d\n", FUN, LINE, waitcount);

		if(waitcount >= 200)
		{
			//线程异常退出由看门狗重启
			WatchDog_Stop(__FUNCTION__);

			return NULL;
		}

		if(g_router_link_status == VAVA_NETWORK_LINKOK)
		{
			break;
		}
	}

	version = PPCS_GetAPIVersion();
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: PPCS_API Version [%d.%d.%d.%d]\n", 
	                                FUN, LINE, (version & 0xFF000000) >> 24, (version & 0x00FF0000) >> 16,
		                            (version & 0x0000FF00) >> 8, (version & 0x000000FF) >> 0);

	VAVAHAL_GetP2Pdid();

	//测试
	//strcpy(g_gatewayinfo.sydid, "VADE-000003-PRBMD");
	//strcpy(g_gatewayinfo.apilisence, "EUQSZJ");

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------------------- P2P_SERVER ----------------------\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [%s] [%s] [%s] [%s]\n", FUN, LINE, g_gatewayinfo.sn, g_gatewayinfo.sydid,
		                                                              g_gatewayinfo.apilisence, g_gatewayinfo.crckey);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [%s] [%s] [%s] [%s]\n", FUN, LINE, g_gatewayinfo.domain_user, 
		                                                              g_gatewayinfo.domain_factory, g_gatewayinfo.hardver, 
		                                                              g_gatewayinfo.softver);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [%s]\n", FUN, LINE, g_gatewayinfo.initstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --------------------------------------------------------\n", FUN, LINE);

	//初始化尚云服务器
	ret = PPCS_Initialize(g_gatewayinfo.initstr);
	if(ret != ERROR_PPCS_SUCCESSFUL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: PPCS init err, ret = %d\n", FUN, LINE, ret);
		
		Err_Log("PPCS_Initialize fail");
		g_running = 0;
		return NULL;
	}
	else
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ================> **** PPCS init success **** <================\n", FUN, LINE);
	}

	ret = PPCS_NetworkDetect(&NetInfo, 0);
	if(ret != ERROR_PPCS_SUCCESSFUL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: PPCS network detect err, ret = %d\n", FUN, LINE, ret);
		return NULL;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Internet      : %s\n", FUN, LINE, (NetInfo.bFlagInternet == 1) ? "YES" : "NO");
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: P2PServer IP  : %s\n", FUN, LINE, (NetInfo.bFlagHostResolved == 1) ? "YES" : "NO");
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: P2PServer Ack : %s\n", FUN, LINE, (NetInfo.bFlagServerHello == 1) ? "YES" : "NO");
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Nat TYPE = %d  Lan[%s]  Wan[%s]\n", FUN, LINE, NetInfo.NAT_Type, NetInfo.MyLanIP, NetInfo.MyWanIP);

	//该函数需要搜集用户信息声明 先设置为0
	PPCS_Share_Bandwidth(0);

	//初始化信令队列
	ret = CmdList_init();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: CmdList_init fail\n", FUN, LINE);
		
		Err_Log("cmdlist init fail");
		g_running = 0;
		return NULL;
	}

	//初始化录像删除队列
	ret = RecList_init();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: RecList_init fail\n", FUN, LINE);
		
		Err_Log("Reclist init fail");
		g_running = 0;
		return NULL;
	}

	//初始化图片传输队列
	ret = ImgList_init();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: ImgList_init fail\n", FUN, LINE);
		
		Err_Log("imgslist init fail");
		g_running = 0;
		return NULL;
	}

	//初始化推送队列
	ret = PushList_init();
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: PushList_init fail\n", FUN, LINE);
		
		Err_Log("pushlist init fail");
		g_running = 0;
		return NULL;
	}

	memset(apistr, 0, 16);
	sprintf(apistr, "%s:%s", g_gatewayinfo.apilisence, g_gatewayinfo.crckey);
		
	while(g_running)
	{
		tmpid = PPCS_Listen(g_gatewayinfo.sydid, 600, 0, 1, apistr);
		if(tmpid >= 0)
		{
			pthread_t id_session;
			pthread_attr_t attr; 

			sessionid = tmpid;
			
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------> new connect, sessionid = %d\n", FUN, LINE, sessionid);

			g_addstation = 0;
			
			pthread_attr_init(&attr); 
			pthread_attr_setdetachstate(&attr, 1); 
			pthread_attr_setstacksize(&attr, STACK_SIZE * 5);
			pthread_create(&id_session, &attr, Session_pth, &sessionid);
			pthread_attr_destroy(&attr);
		}
		else 
		{
			if(tmpid == ERROR_PPCS_TIME_OUT)
			{
				continue;
			}

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: ----------> listen err, tmpid = %d\n", FUN, LINE, tmpid);

			if(tmpid == ERROR_PPCS_MAX_SESSION)
			{
				sleep(5);
			}
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}


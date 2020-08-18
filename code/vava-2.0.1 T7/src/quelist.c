#include "basetype.h"
#include "vavahal.h"
#include "errlog.h"
#include "PPCS_API.h"
#include "record.h"
#include "vavaserver.h"
#include "aesencrypt.h"
#include "quelist.h"

qCmdList g_cmdlist;
qRecList g_reclist;
qImgList g_imglist;
qPushList g_pushlist;

int CmdList_init()
{
	int ret;
	pthread_t cmd_id;
	pthread_attr_t attr; 
	
	g_cmdlist.front = 0;
	g_cmdlist.rear = 0;

	//创建信令队列处理线程
	pthread_attr_init(&attr); 
	pthread_attr_setdetachstate(&attr, 1); 
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&cmd_id, &attr, CmdQueList_pth, NULL);
	pthread_attr_destroy(&attr);

	return ret;
}

int CmdList_IsEmpty()
{
	if(g_cmdlist.front == g_cmdlist.rear)
	{
		return 1;
	}

	return 0;
}

int CmdList_IsFull()
{
	if(g_cmdlist.front == (g_cmdlist.rear + 1) % CMD_QUEUE_LEN)
	{
		return 1;
	}

	return 0;
}

int CmdList_InsertData(qCmdData data)
{
	if(CmdList_IsFull())
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: list is full\n", FUN, LINE);
		return -1;
	}

	pthread_mutex_lock(&mutex_cmdlist_lock);

	g_cmdlist.data[g_cmdlist.rear] = data;
	g_cmdlist.rear = (g_cmdlist.rear + 1) % CMD_QUEUE_LEN;

	pthread_mutex_unlock(&mutex_cmdlist_lock);

	return 0;
}

int CmdList_OutData(qCmdData *data)
{
	if(CmdList_IsEmpty())
	{
		return -1;
	}

	pthread_mutex_lock(&mutex_cmdlist_lock);
	
	*data = g_cmdlist.data[g_cmdlist.front];
	g_cmdlist.front = (g_cmdlist.front + 1) % CMD_QUEUE_LEN;

	pthread_mutex_unlock(&mutex_cmdlist_lock);
	
	return 0;
}

void *CmdQueList_pth(void *data)
{
	int ret;
	qCmdData cmddata;
	VAVA_RecImgGet *recimgget;
	VAVA_RecDel *recdel;
	VAVA_WifiConfig *wificonfig;
	VAVA_RecSearch *recfilesearch;
	VAVA_RecShareDateSearch *recsharedatesearch;
	VAVA_RecShareListSearch *recsharelistsearch;
	VAVA_ClearCamera *clearcamera;
	int cmdsock;
	
	while(g_running)
	{
		ret = CmdList_OutData(&cmddata);
		if(ret != 0)
		{
			usleep(100000);
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmdtype = %d, channel = %d, session = %d\n", 
			                            FUN, LINE, cmddata.cmdtype, cmddata.channel, cmddata.sessionid);

		switch(cmddata.cmdtype)
		{
			case VAVA_CMD_SET_VIDEO_QUALITY:
			case VAVA_CMD_SET_REC_QUALITY:
				if(g_camerainfo[cmddata.channel].first_flag == 1)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Camera not ready, cmd = %d, channel = %d\n", 
			                                         FUN, LINE, cmddata.cmdtype, cmddata.channel);
					
					VAVAHAL_SendCameraNotReady(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				ret = VAVAHAL_WakeupCamera_WithSet(cmddata.channel, &cmdsock);
				if(ret == VAVA_ERR_CODE_CONFIG_TIMEOUT)
				{
					VAVAHAL_SendCameraSetTimeout(cmddata.sessionid, cmddata.cmdtype);
					break;
				}
				
				VAVAHAL_CmdReq_SetRes(cmdsock, cmddata);
				break;
			case VAVA_CMD_SET_MIRRORMODE:
				if(g_camerainfo[cmddata.channel].first_flag == 1)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Camera not ready, cmd = %d, channel = %d\n", 
			                                         FUN, LINE, cmddata.cmdtype, cmddata.channel);
					
					VAVAHAL_SendCameraNotReady(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				ret = VAVAHAL_WakeupCamera_WithSet(cmddata.channel, &cmdsock);
				if(ret == VAVA_ERR_CODE_CONFIG_TIMEOUT)
				{
					VAVAHAL_SendCameraSetTimeout(cmddata.sessionid, cmddata.cmdtype);
					break;
				}
				
				VAVAHAL_CmdReq_SetMirr(cmdsock, cmddata);
				break;
			case VAVA_CMD_SET_IRMODE:
				if(g_camerainfo[cmddata.channel].first_flag == 1)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Camera not ready, cmd = %d, channel = %d\n", 
			                                         FUN, LINE, cmddata.cmdtype, cmddata.channel);
					
					VAVAHAL_SendCameraNotReady(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				ret = VAVAHAL_WakeupCamera_WithSet(cmddata.channel, &cmdsock);
				if(ret == VAVA_ERR_CODE_CONFIG_TIMEOUT)
				{
					VAVAHAL_SendCameraSetTimeout(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				VAVAHAL_CmdReq_SetIrMode(cmdsock, cmddata);
				break;
			case VAVA_CMD_SET_MDPARAM:
				if(g_camerainfo[cmddata.channel].first_flag == 1)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Camera not ready, cmd = %d, channel = %d\n", 
			                                         FUN, LINE, cmddata.cmdtype, cmddata.channel);
					
					VAVAHAL_SendCameraNotReady(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				ret = VAVAHAL_WakeupCamera_WithSet(cmddata.channel, &cmdsock);
				if(ret == VAVA_ERR_CODE_CONFIG_TIMEOUT)
				{
					VAVAHAL_SendCameraSetTimeout(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				VAVAHAL_CmdReq_SetMdParam(cmdsock, cmddata);
				break;
			case VAVA_CMD_RECORDDATE_SEARCH:
				VAVAHAL_SendSearchRecDate(cmddata.sessionid);
				break;
			case VAVA_CMD_RECORDLIST_SEARCH:
				recfilesearch = (VAVA_RecSearch *)cmddata.param;
				VAVAHAL_SendSearchRecFile(cmddata.sessionid, recfilesearch->date, recfilesearch->type);
				break;
			case VAVA_CMD_RECSHAREDATE_SEARCH:
				recsharedatesearch = (VAVA_RecShareDateSearch *)cmddata.param;
				VAVAHAL_SendSearchRecShareDate(cmddata.sessionid, recsharedatesearch->list);
				break;
			case VAVA_CMD_RECSHARELIST_SEARCH:
				recsharelistsearch = (VAVA_RecShareListSearch *)cmddata.param;
				VAVAHAL_SendSearchRecShareList(cmddata.sessionid, recsharelistsearch->date, recsharelistsearch->type, recsharelistsearch->channel);
				break;
			case VAVA_CMD_RECORD_IMG:
				recimgget = (VAVA_RecImgGet *)cmddata.param;
				VAVAHAL_InsertImgList(cmddata.channel, cmddata.sessionid, recimgget->dirname, recimgget->filename);
				break;
			case VAVA_CMD_RECORD_IMG_STOP:
				VAVAHAL_InsertImgList(cmddata.channel, cmddata.sessionid, "null", "null");
				break;
			case VAVA_CMD_RECORD_DEL:
				recdel = (VAVA_RecDel *)cmddata.param;
				if(recdel->type == VAVA_RECFILE_DEL_NORMAL)
				{
					VAVAHAL_DelRecFile(recdel->dirname, recdel->filename);
				}
				else if(recdel->type == VAVA_RECFILE_DEL_ALLDIR)
				{
					VAVAHAL_DelRecDir(recdel->dirname);
				}
				else
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: unknow type\n", FUN, LINE);
				}

				#if 0 //不再返回删除结果
				if(cmddata.sessionid != -1)
				{
					ret = VAVAHAL_CheckSession(cmddata.sessionid);
					if(ret == 0)
					{
						//返回删除结果
						VAVAHAL_SendRecDelResult(cmddata.sessionid, recdel->type, recdel->dirname, recdel->filename, result);
					}
				}
				#endif
				
				break;
			case VAVA_CMD_SNAPSHOT:
				if(g_camerainfo[cmddata.channel].first_flag == 1)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Camera not ready, cmd = %d, channel = %d\n", 
			                                         FUN, LINE, cmddata.cmdtype, cmddata.channel);
					
					VAVAHAL_SendCameraNotReady(cmddata.sessionid, cmddata.cmdtype);
					break;
				}
				
				ret = VAVAHAL_WakeupCamera_WithSnapshort(cmddata.channel);
				if(ret != VAVA_ERR_CODE_SUCCESS)
				{
					VAVAHAL_SendCameraSetTimeout(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				g_snapshot[cmddata.channel].sessionid = cmddata.sessionid;
				g_snapshot[cmddata.channel].flag = 1;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------- $$ SnapShot Begin $$----------\n", FUN, LINE);

				//添加到休眠检测
				g_camerainfo[cmddata.channel].sleep_check = 2;
				break;
			case VAVA_CMD_START_REC:
				cmdsock = VAVAHAL_ReadSocketId(0, cmddata.channel);
				if(cmdsock == -1)
				{
					if(cmddata.sessionid != -1)
					{
						VAVAHAL_SendCameraNoWakeup(cmddata.sessionid, cmddata.cmdtype);
					}
					
					break;
				}
				
				VAVAHAL_CmdReq_OpenRec(cmdsock, cmddata);
				break;
			case VAVA_CMD_STOP_REC:
				cmdsock = VAVAHAL_ReadSocketId(0, cmddata.channel);
				if(cmdsock != -1)
				{
					VAVAHAL_CmdReq_CloseRec(cmdsock, cmddata);
				}
				break;
			case VAVA_CMD_WIFI_CONFIG:
				wificonfig = (VAVA_WifiConfig *)cmddata.param;
				VAVAHAL_ConnectAp(wificonfig->ssid, wificonfig->pass);

				#if 0
				//阻塞获取AP连接状态，30秒超时
				VAVAHAL_GetApStatus(wificonfig->ssid);
				VAVAHAL_SystemCmd("killall udhcpc");
				#endif
				break;
			case VAVA_CMD_CAMERA_RESET:
				if(g_camerainfo[cmddata.channel].first_flag == 1)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Camera not ready, cmd = %d, channel = %d\n", 
			                                         FUN, LINE, cmddata.cmdtype, cmddata.channel);
					
					VAVAHAL_SendCameraNotReady(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				ret = VAVAHAL_WakeupCamera_WithSet(cmddata.channel, &cmdsock);
				if(ret == VAVA_ERR_CODE_CONFIG_TIMEOUT)
				{
					VAVAHAL_SendCameraSetTimeout(cmddata.sessionid, cmddata.cmdtype);
					break;
				}
				
				VAVAHAL_CmdReq_CameraReset(cmdsock, cmddata);

				//恢复摄像机参数
				VAVAHAL_ResetCamera(cmddata.channel);
				break;
			case VAVA_CMD_RESET_FACTORY:
				VAVAHAL_ResetGateWay();
				break;
			case VAVA_CMD_IPC_COM_CTRL:
				if(g_camerainfo[cmddata.channel].first_flag == 1)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Camera not ready, cmd = %d, channel = %d\n", 
			                                         FUN, LINE, cmddata.cmdtype, cmddata.channel);
					
					VAVAHAL_SendCameraNotReady(cmddata.sessionid, cmddata.cmdtype);
					break;
				}

				ret = VAVAHAL_WakeupCamera_WithSet(cmddata.channel, &cmdsock);
				if(ret == VAVA_ERR_CODE_CONFIG_TIMEOUT)
				{
					VAVAHAL_SendCameraSetTimeout(cmddata.sessionid, cmddata.cmdtype);
					break;
				}
				
				VAVAHAL_CmdReq_SetComCtrl(cmdsock, cmddata);
				break;
			case VAVA_CMD_CLEARMATCH:
				clearcamera = (VAVA_ClearCamera *)cmddata.param;
#ifdef FREQUENCY_OFFSET
				VAVAHAL_WakeupCamera_Ext(clearcamera->addr, clearcamera->index);
#else
				VAVAHAL_WakeupCamera_Ext(clearcamera->addr);
#endif
				break;
			default:
				break;
		}
	}

	return NULL;
}

int RecList_init()
{
	int ret;
	pthread_t img_id;
	pthread_attr_t attr; 
	
	g_reclist.front = 0;
	g_reclist.rear = 0;

	//创建录像删除处理线程
	pthread_attr_init(&attr); 
	pthread_attr_setdetachstate(&attr, 1); 
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&img_id, &attr, RecList_pth, NULL);
	pthread_attr_destroy(&attr);

	return ret;
}

int RecList_IsEmpty()
{
	if(g_reclist.front == g_reclist.rear)
	{
		return 1;
	}

	return 0;
}

int RecList_IsFull()
{
	if(g_reclist.front == (g_reclist.rear + 1) % CMD_QUEUE_LEN)
	{
		return 1;
	}

	return 0;
}

int RecList_InsertData(qRecData data)
{
	if(RecList_IsFull())
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: list is full\n", FUN, LINE);
		return -1;
	}

	pthread_mutex_lock(&mutex_reclist_lock);

	g_reclist.data[g_reclist.rear] = data;
	g_reclist.rear = (g_reclist.rear + 1) % CMD_QUEUE_LEN;

	pthread_mutex_unlock(&mutex_reclist_lock);

	return 0;
}

int RecList_OutData(qRecData *data)
{
	if(RecList_IsEmpty())
	{
		return -1;
	}

	pthread_mutex_lock(&mutex_reclist_lock);
	
	*data = g_reclist.data[g_reclist.front];
	g_reclist.front = (g_reclist.front + 1) % CMD_QUEUE_LEN;

	pthread_mutex_unlock(&mutex_reclist_lock);
	
	return 0;
}

void *RecList_pth(void *data)
{
	int ret;
	qRecData recdata;
	
	
	while(g_running)
	{
		ret = RecList_OutData(&recdata);
		if(ret != 0)
		{
			sleep(1);
			continue;
		}

		if(recdata.type == VAVA_RECFILE_DEL_NORMAL)
		{
			VAVAHAL_DelRecFile(recdata.dirname, recdata.filename);
		}
		else if(recdata.type == VAVA_RECFILE_DEL_ALLDIR)
		{
			VAVAHAL_DelRecDir(recdata.dirname);
		}
		else
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: unknow type\n", FUN, LINE);
		}
	}

	return NULL;
}


int ImgList_init()
{
	int ret;
	pthread_t img_id;
	pthread_attr_t attr; 
	
	g_imglist.front = 0;
	g_imglist.rear = 0;

	//创建图片传输处理线程
	pthread_attr_init(&attr); 
	pthread_attr_setdetachstate(&attr, 1); 
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&img_id, &attr, ImgQueList_pth, NULL);
	pthread_attr_destroy(&attr);

	return ret;
}

int ImgList_IsEmpty()
{
	if(g_imglist.front == g_imglist.rear)
	{
		return 1;
	}

	return 0;
}

int ImgList_IsFull()
{
	if(g_imglist.front == (g_imglist.rear + 1) % CMD_QUEUE_LEN)
	{
		return 1;
	}

	return 0;
}

int ImgList_InsertData(qImgData data)
{
	if(ImgList_IsFull())
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: list is full\n", FUN, LINE);
		return -1;
	}

	pthread_mutex_lock(&mutex_imglist_lock);

	g_imglist.data[g_imglist.rear] = data;
	g_imglist.rear = (g_imglist.rear + 1) % CMD_QUEUE_LEN;

	pthread_mutex_unlock(&mutex_imglist_lock);

	return 0;
}

int ImgList_OutData(qImgData *data)
{
	static int count = 0;
	
	if(ImgList_IsEmpty())
	{
		count++;
		if(count % 30 == 0)
		{
			count = 0;
		}

		return -1;
	}

	pthread_mutex_lock(&mutex_imglist_lock);
	
	*data = g_imglist.data[g_imglist.front];
	g_imglist.front = (g_imglist.front + 1) % CMD_QUEUE_LEN;

	pthread_mutex_unlock(&mutex_imglist_lock);
	
	return 0;
}

void *ImgQueList_pth(void *data)
{
	int ret;
	char tmpch[2];
	qImgData imgdata;
	VAVA_IMG_HEAD imghead;
	VAVA_RecInfo recinfo;
	VAVA_RecHead rechead;
	
	FILE *fd = NULL;
	char filename[128];
	char type_str[2];
	int type;

	unsigned int checksize = 0;
	int imgsize;
	char *imgbuff = NULL;
	int buffsize = MEM_AV_IMG_SIZE;

	imgbuff = malloc(buffsize);
	if(imgbuff == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: malloc imgbuff fail\n", FUN, LINE);
		
		Err_Log("malloc imgbuff fail");
		g_running = 0;
		return NULL;
	}
	
	while(g_running)
	{
		ret = ImgList_OutData(&imgdata);
		if(ret != 0)
		{
			usleep(100000);
			continue;
		}

		if(strcmp(imgdata.dirname, "null") == 0 || strcmp(imgdata.filename, "null") == 0)
		{
			g_session[imgdata.channel].recimgstop = 0;
			continue;
		}

		if(g_session[imgdata.channel].recimgstop == 1)
		{
			continue;
		}

#if 0
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: sessionid = %d, channel = %d, [%s][%s]\n", 
			                            FUN, LINE, imgdata.sessionid, imgdata.channel, imgdata.dirname, imgdata.filename);
#endif

		memset(type_str, 0, 2);
		memcpy(type_str, imgdata.filename + 7, 1);
		type = atoi(type_str);

		memset(filename, 0, 128);
		sprintf(filename, "/mnt/sd0/%s/%s/%s", g_gatewayinfo.sn, imgdata.dirname, imgdata.filename);
		fd = fopen(filename, "r");
		if(fd == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: open file fail, [/mnt/sd0/%s/%s/%s]\n", 
			                              FUN, LINE, g_gatewayinfo.sn, imgdata.dirname, imgdata.filename);
			
			//通知APP文件打开失败
			memset(&imghead, 0, sizeof(VAVA_IMG_HEAD));
			imghead.tag = VAVA_TAG_APP_IMG;
			imghead.type = 0; //目前全部图片均为I帧数据
			imghead.size = 0;
			imghead.result = 1;
			strcpy(imghead.date, imgdata.dirname);
			strcpy(imghead.file, imgdata.filename);

			//增加通道号
			memset(tmpch, 0, 2);
			memcpy(tmpch, imgdata.filename + 9, 1);
			imghead.channel = atoi(tmpch);

			ret = VAVAHAL_CheckSession(imgdata.sessionid);
			if(ret == 0)
			{
				ret = PPCS_Check_Buffer(imgdata.sessionid, P2P_CHANNEL_IMG, &checksize, NULL);
				if(ret == ERROR_PPCS_SUCCESSFUL && checksize < 1000000)
				{
					PPCS_Write(imgdata.sessionid, P2P_CHANNEL_IMG, (char *)&imghead, sizeof(VAVA_IMG_HEAD));
				}
				else
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: PPCS_Check_Buffer fail or imgbuff more than 1M, ret = %d\n", FUN, LINE, ret);
				}
			}

			continue;
		}

		memset(imgbuff, 0, buffsize);
		imgsize = 0;

		if(type == VAVA_RECFILE_TYPE_IMG || type == VAVA_RECFILE_TYPE_SNAPSHOT) //图片
		{
			while(g_running)
			{
				if(feof(fd))
				{
					break;
				}

				ret = fread(imgbuff + imgsize, 1, 50000, fd);
				if(ret <= 0)
				{
					break;
				}

				imgsize += ret;
			}
		}
		else  //I帧
		{
			if(rechead.size <= buffsize)
			{
				memset(&recinfo, 0, sizeof(VAVA_RecInfo));
				fread(&recinfo, sizeof(VAVA_RecInfo), 1, fd);
				memset(&rechead, 0, sizeof(VAVA_RecHead));
				fread(&rechead, sizeof(VAVA_RecHead), 1, fd);
				fread(imgbuff, rechead.size, 1, fd);
				imgsize = rechead.size;

				if(recinfo.encrypt == VAVA_REC_ENCRYPT_AES)
				{
					VAVA_Aes_Decrypt(imgbuff, imgbuff, rechead.size);
				}
			}
			else 
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: I frame size more than %d\n", FUN, LINE, buffsize);
			}
		}	

		fclose(fd);

		memset(&imghead, 0, sizeof(VAVA_IMG_HEAD));
		imghead.tag = VAVA_TAG_APP_IMG;
		imghead.type = 0;
		imghead.size = imgsize;
		imghead.result = 0;
		strcpy(imghead.date, imgdata.dirname);
		strcpy(imghead.file, imgdata.filename);

		//增加通道号
		memset(tmpch, 0, 2);
		memcpy(tmpch, imgdata.filename + 9, 1);
		imghead.channel = atoi(tmpch);

		ret = VAVAHAL_CheckSession(imgdata.sessionid);
		if(ret == 0)
		{
			ret = PPCS_Check_Buffer(imgdata.sessionid, P2P_CHANNEL_IMG, &checksize, NULL);
			if(ret == ERROR_PPCS_SUCCESSFUL && checksize < 1000000)
			{
				PPCS_Write(imgdata.sessionid, P2P_CHANNEL_IMG, (char *)&imghead, sizeof(VAVA_IMG_HEAD));
				PPCS_Write(imgdata.sessionid, P2P_CHANNEL_IMG, imgbuff, imgsize);
			}
			else
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: PPCS_Check_Buffer fail or imgbuff more than 1M, ret = %d\n", FUN, LINE, ret);
			}
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: sessionid = %d, channel = %d, size = %d, [%s][%s]\n", 
		                                FUN, LINE, imgdata.sessionid, imgdata.channel, imgsize, imgdata.dirname, imgdata.filename);
	}

	free(imgbuff);
	imgbuff = NULL;

	return NULL;
}

int PushList_init()
{
	int ret;
	pthread_t push_id;
	pthread_attr_t attr; 
	
	g_pushlist.front = 0;
	g_pushlist.rear = 0;

	//创建推送处理线程
	pthread_attr_init(&attr); 
	pthread_attr_setdetachstate(&attr, 1); 
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&push_id, &attr, PushQueList_pth, NULL);
	pthread_attr_destroy(&attr);

	return ret;
}

int PushList_IsEmpty()
{
	if(g_pushlist.front == g_pushlist.rear)
	{
		return 1;
	}

	return 0;
}

int PushList_IsFull()
{
	if(g_pushlist.front == (g_pushlist.rear + 1) % CMD_QUEUE_LEN)
	{
		return 1;
	}

	return 0;
}

int PushList_InsertData(qPushData data)
{
	if(PushList_IsFull())
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: list is full\n", FUN, LINE);
		return -1;
	}

	pthread_mutex_lock(&mutex_pushlist_lock);

	g_pushlist.data[g_pushlist.rear] = data;
	g_pushlist.rear = (g_pushlist.rear + 1) % CMD_QUEUE_LEN;

	pthread_mutex_unlock(&mutex_pushlist_lock);

	return 0;
}

int PushList_OutData(qPushData *data)
{
	if(PushList_IsEmpty())
	{
		return -1;
	}

	pthread_mutex_lock(&mutex_pushlist_lock);
	
	*data = g_pushlist.data[g_pushlist.front];
	g_pushlist.front = (g_pushlist.front + 1) % CMD_QUEUE_LEN;

	pthread_mutex_unlock(&mutex_pushlist_lock);
	
	return 0;
}

void *PushQueList_pth(void *data)
{
	int ret;
	qPushData pushdata;
	
	while(g_running)
	{
		ret = PushList_OutData(&pushdata);
		if(ret != 0)
		{
			sleep(1);
			continue;
		}

		if(g_router_link_status != VAVA_NETWORK_LINKOK)
		{
			sleep(1);
			continue;
		}
		
		if(pushdata.pushtype == VAVA_PUSH_TYPE_LOWPOWER || pushdata.pushtype == VAVA_PUSH_CAMERA_OFFLINE)
		{
			VAVAHAL_PushAlarm(pushdata.channel, pushdata.pushtype, pushdata.time, pushdata.ntsamp, pushdata.filetype, 
				              pushdata.dirname, pushdata.filename, pushdata.msg);
		}
		else
		{
			if(g_camera_arminfo_v1[pushdata.channel].status == 1)
			{
				VAVAHAL_PushAlarm(pushdata.channel, pushdata.pushtype, pushdata.time, pushdata.ntsamp, pushdata.filetype,
					              pushdata.dirname, pushdata.filename, pushdata.msg);
			}
		}
	}

	return NULL;
}


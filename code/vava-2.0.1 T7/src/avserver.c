#include "basetype.h"
#include "vavahal.h"
#include "vavaserver.h"
#include "errlog.h"
#include "cjson.h"
#include "rfserver.h"
#include "PPCS_API.h"
#include "watchdog.h"
#include "avserver.h"

typedef struct _server_param{
	int clientfd;
	int clientip;
	char mac[18];
}ServerParam;

typedef enum{
	P2P_SEND_MODE_NORMAL = 0,	//正常传输
	P2P_SEND_MODE_LOSTP,		//丢弃P帧
	P2P_SEND_MODE_LOSTALL		//全部丢弃
}P2PSendMode;

VAVA_Camera_Dynamic g_camerainfo_dynamic[MAX_CHANNEL_NUM];
VAVA_IPC_OtaInfo g_ipc_otainfo[MAX_CHANNEL_NUM];

volatile int g_camera_talk[MAX_CHANNEL_NUM];
volatile unsigned char g_tmp_quality[MAX_CHANNEL_NUM];
volatile unsigned char g_avstop;

int tcp_create(int port, char *eth_name)
{
	int ret;
	int fd = -1;
	int reuse = 1;
	struct sockaddr_in server_addr;
	struct ifreq if_ra0;
	struct sockaddr_in *sin;

	if(strlen(eth_name) > 16)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: eth_name is too long\n", FUN, LINE);
		return -1;
	}

	strcpy(if_ra0.ifr_name, eth_name);

	fd = socket(PF_INET, SOCK_STREAM, 0);  //创建socket
	if(fd < 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init socket err, fd = %d\n", FUN, LINE, fd);
		return -1;
	}

	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if(ret < 0)
	{
		close(fd);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: SO_REUSEADDR err\n", FUN, LINE);
		return -1;
	}

	ret = setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&if_ra0, sizeof(if_ra0));
	if(ret < 0)
	{
		close(fd);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: SO_BINDTODEVICE err\n", FUN, LINE);
		return -1;
	}

	memset(&server_addr, 0x0, sizeof(struct sockaddr_in));

	if(ioctl(fd, SIOCGIFADDR, &if_ra0) < 0)
	{
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		sin = (struct sockaddr_in *)&(if_ra0.ifr_addr);
		server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(sin->sin_addr));
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	//绑定端口
	if(bind(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0)
	{
		close(fd);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: bind socket err\n", FUN, LINE);
		return -1;
	}

	//监听端口
	if(listen(fd, 10) < 0)
	{
		close(fd);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: listen err\n", FUN, LINE);
		return -1;
	}

	return fd;	
}

int getpeermac(int sockfd, char *buf) 
{
    struct arpreq arpreq; 
    struct sockaddr_in dstadd_in; 
	socklen_t len;
	unsigned char* ptr = NULL; 

	len = sizeof(struct sockaddr_in);
	memset(&arpreq, 0, sizeof(struct arpreq)); 
    memset(&dstadd_in, 0, sizeof(struct sockaddr_in)); 

	if(getpeername(sockfd, (struct sockaddr*)&dstadd_in, &len) < 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: getpeername err\n", FUN, LINE);
		return -1;
	}

	memcpy(&arpreq.arp_pa, &dstadd_in, sizeof(struct sockaddr_in));
	strcpy(arpreq.arp_dev, BING_NET);
	arpreq.arp_pa.sa_family = AF_INET; 
	arpreq.arp_ha.sa_family = AF_UNSPEC;
	if(ioctl(sockfd, SIOCGARP, &arpreq) < 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: ioctrl err\n", FUN, LINE);
		return -1;
	}

	ptr = (unsigned char *)arpreq.arp_ha.sa_data;
	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5)); 

	return 0;
}

void *commonclient_pth(void *data)
{
	ServerParam *sp = (ServerParam *)data;
	int clientfd; 
	int clientip;
	char clientmac[18];
	char ip_str[16];
	char recvbuff[128];
	char pairaddr[32];
	char logstr[128];
	
	int i;
	int ret;
	int errnum = 0;
	int channel;
	int recvsize;
	int tmppairstatus = -1;
	int tmppairchannel = -1;
	int tmppairip = -1;
	int ntsamptime;
	fd_set rdRd;
	struct timeval timeout;

	VAVA_Msg vava_common;
	VAVA_Pair_Resp vava_pair;
	VAVA_WifiSigle *wifisig;
	VAVA_Pair_Req *pairreq;
	VAVA_ResConfig *resresult;
	VAVA_MirrConfig *mirrresult;
	VAVA_IrLedConfig *irledresult;
	VAVA_MdConfig *mdresult;
	VAVA_CameraFactory *camerafactory;
	VAVA_Upgrate_Status *upstatus;
	VAVA_MdResult *mdstatus;
	VAVA_First_Connect *firstinfo;
	VAVA_IPC_OtaInfo *ipcotainfo;
	VAVA_IPC_RFHW *ipcrfhw;
	VAVA_CameraRecCtrl *recctrlresult;
	VAVA_IPCComConfig *comresult;

	clientfd = sp->clientfd;
	clientip = sp->clientip;
	memset(clientmac, 0, 18);
	strcpy(clientmac, sp->mac);

	memset(ip_str, 0, 16);
	sprintf(ip_str, "%d.%d.%d.%d", (clientip & 0x000000FF),
	                               (clientip & 0x0000FF00) >> 8,
	                               (clientip & 0x00FF0000) >> 16,
	                               (clientip & 0xFF000000) >> 24);

	channel = VAVAHAL_CheckPairWithMac(clientmac, clientfd);
	if(channel >= 0 && channel < MAX_CHANNEL_NUM)
	{
		VAVAHAL_WriteSocketId(0, channel, clientfd);
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: new client, clientfd = %d, [channel - %d][%s][%s]\n", 
		                            FUN, LINE, clientfd, channel, ip_str, clientmac);

	if(channel != -1)
	{
		//由wifi上报心跳包
		g_camerainfo[channel].wifi_heart = 1;
		g_camerainfo[channel].sleep_flag = 1;

		g_camerainfo[channel].poweroff_flag = 0;

		if(g_camerainfo[channel].wakeup_status == 1)
		{
			g_wakeup_result[channel] = 0;
			g_camerainfo[channel].wakeup_status = 0;

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------------------------------\n", FUN, LINE);
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:            WakeUp Success [WIFI] [channel - %d] \n", FUN, LINE, channel);
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------------------------------\n", FUN, LINE);
		}
	}

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{		
		if(g_wakeup_withpair_status == 1)
		{
			if(g_pair[i].addr == g_wakeup_withpair_addr)
			{
				g_wakeup_withpair_status = 0;
				g_wakeup_withpair_addr = 0xFFFFFFFF;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------------------------------\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:          WakeUp Pair Success [WIFI] [channel - %d] \n", FUN, LINE, channel);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------------------------------\n", FUN, LINE);
			}
		}

		if(g_pair_status[i] == 1)
		{
			if(g_pair[i].addr == g_pair_wifi_addr)
			{
				g_pair_status[i] = 0;
				g_pair_wifi_addr = 0xFFFFFFFF;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------------------------------\n", FUN, LINE);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:          WakeUp Pair Success [WIFI] [channel - %d] \n", FUN, LINE, channel);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------------------------------\n", FUN, LINE);
			}
		}
	}

	recvsize = 0;

	while(g_running && g_avstop == 0)
	{
		if(channel != -1)
		{
			clientfd = VAVAHAL_ReadSocketId(0, channel);
			if(clientfd < 0)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel [%d] is close, exit\n", FUN, LINE, channel);
				break;
			}
		}
		
		FD_ZERO(&rdRd);
		FD_SET(clientfd, &rdRd);

		timeout.tv_sec = 0;				
		timeout.tv_usec = 500000;

		ret = select(clientfd + 1, &rdRd, NULL, NULL, &timeout);
		if(ret < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err, channel - %d\n", FUN, LINE, channel);
			break;
		}
		else if(ret == 0)
		{
			continue;  //timeout
		}

		if(FD_ISSET(clientfd, &rdRd))
		{
			ret = recv(clientfd, recvbuff + recvsize, 128 - recvsize, 0);
			if(ret < 0)
			{
				if(errno == EAGAIN)
				{
					continue;
				}

				break;
			}
			else if(ret == 0)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: client exit [channel - %d][%s]\n", FUN, LINE, channel, ip_str);
				break;
			}

			recvsize += ret;

			while(g_running)
			{
				if(recvsize < 128)
				{
					break;
				}

				memset(&vava_common, 0, sizeof(VAVA_Msg));
				memcpy(&vava_common, recvbuff, sizeof(VAVA_Msg));
				recvsize -= sizeof(VAVA_Msg);
				memcpy(recvbuff, recvbuff + sizeof(VAVA_Msg), recvsize);

				if(vava_common.tag != VAVA_TAG_CAMERA_CMD)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check head err [channel - %d][%x]\n", FUN, LINE, channel, vava_common.tag);

					errnum++;
					if(errnum >= 5)
					{
						break;
					}

					#if 0
					//查找头
					for(i = 0; i < 128 - 4; i++)
					{
						if(recvbuff[i] == 0x01 && recvbuff[i + 1] == 0x00
						&& recvbuff[i + 2] == 0x00 && recvbuff[i + 3] == 0xEB)
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: find head, i = %d\n", FUN, LINE, i);
							break;
						}
					}

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: find i = %d, channel = %d\n", FUN, LINE, i, channel);

					if(i == 128 - 4)
					{
						recvsize = 0;
					}
					else
					{
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: find head, i = %d, [%x][%x][%x][%x]\n", 
							                            FUN, LINE, i, recvbuff[i], recvbuff[i + 1], recvbuff[i + 2], recvbuff[i + 3]);
						memcpy(recvbuff, recvbuff + i, recvsize);
						recvsize = 128 - i;
					}
					#endif
					
					break;
				}

				errnum = 0;

				#if 0
				memset(crcbuff, 0, 128);
				memcpy(crcbuff, &vava_common.comtype, sizeof(int));
				memcpy(crcbuff + sizeof(int), vava_common.data, CMD_DATA_SIZE);
				crc32 = VAVAHAL_Crc32Gen(crcbuff, sizeof(int) + CMD_DATA_SIZE);
				if(vava_common.crc32 != crc32)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: check crc32 err [%x, %x]\n", FUN, LINE, vava_common.crc32, crc32);
					continue;
				}
				#endif

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: new commond, channel = %d, type = %d\n", FUN, LINE, channel, vava_common.comtype);

				switch(vava_common.comtype)
				{
					case VAVA_MSGTYPE_HEARTBEAT:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}

						wifisig = (VAVA_WifiSigle *)vava_common.data;

						#ifdef BATTEY_INFO
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Wifi Heart: [ch - %d] [sig - %d] [bat - %d] [vol - %d] [tmp - %d] [ele - %d] [power - %d] [tf - %d]\n", 
						                                FUN, LINE, channel, wifisig->sig, wifisig->batLevel, wifisig->voltage, wifisig->temperature, wifisig->electricity,
						                                 wifisig->adapter, wifisig->tfcard);

						#else
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Wifi Heart: [ch - %d] [sig - %d] [bat - %d] [vol - %d] [power - %d] [tf - %d]\n", 
						                                FUN, LINE, channel, wifisig->sig, wifisig->batLevel, wifisig->voltage, wifisig->adapter, wifisig->tfcard);
						#endif

						g_camerainfo_dynamic[channel].signal = wifisig->sig;
						g_camerainfo[channel].powermode = wifisig->adapter;

						if(wifisig->batLevel >= 0 && wifisig->batLevel <= 100)
						{
							g_online_flag[channel].battery = wifisig->batLevel;
							g_online_flag[channel].voltage = wifisig->voltage;
							#ifdef BATTEY_INFO
							g_online_flag[channel].temperature = wifisig->temperature;
							g_online_flag[channel].electricity = wifisig->electricity;
							#endif

							g_camerainfo_dynamic[channel].battery = wifisig->batLevel;
							g_camerainfo_dynamic[channel].voltage = wifisig->voltage;
							#ifdef BATTEY_INFO
							g_camerainfo_dynamic[channel].temperature = wifisig->temperature;
							g_camerainfo_dynamic[channel].electricity = wifisig->electricity;
							#endif
						}

						if(wifisig->tfcard == 1)
						{
							g_tfstatus[channel] = 1;
							g_tfcheck[channel] = 0;
						}
						else if(wifisig->tfcard == 2)
						{
							g_tfstatus[channel] = 0;
							g_tfcheck[channel] = 0;
						}

						g_online_flag[channel].online = 1;
						g_camerainfo[channel].heart_status = 0;

						if(g_camerainfo_dynamic[channel].online == 0)
						{
							g_camerainfo_dynamic[channel].online = 1;
							g_camerainfo[channel].first_flag = 0;
							g_devinfo_update = 1;
						}

						if(g_camerainfo[channel].wakeup_flag == 0 && g_camerainfo[channel].alarm_flag == 0 
							&& g_camerainfo[channel].cloud_flag == 0 && g_camerainfo[channel].config_flag == 0 
							&& g_camerainfo[channel].up_flag == 0)
						{
							VAVAHAL_CmdResp_Heartbeat(clientfd, 1);
							
							g_camerainfo[channel].sleep_flag = 1;
							VAVAHAL_SleepCamera_Ex(channel);
						}
						else
						{
							VAVAHAL_CmdResp_Heartbeat(clientfd, 0);
						}

						break;
					case VAVA_MSGTYPE_PAIR_REQ:
						tmppairstatus = -1;
						tmppairchannel = -1;
						tmppairip = -1;
						memset(&vava_pair, 0, sizeof(VAVA_Pair_Resp));
						
						pairreq = (VAVA_Pair_Req *)vava_common.data;

#ifdef FREQUENCY_OFFSET
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel = %d, pairreq->channel = %d, sn = %s, index = %d\n", 
							                            FUN, LINE, channel, pairreq->channel, pairreq->sn, pairreq->index);
#else 
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel = %d, pairreq->channel = %d, sn = %s\n", 
							                            FUN, LINE, channel, pairreq->channel, pairreq->sn);
#endif

						memset(pairaddr, 0, 32);
						sprintf(pairaddr, "[pair] addr = %x", g_gatewayinfo.rfaddr);
						Err_Info(pairaddr);

						//检测配对通道是否合法
						if(pairreq->channel < 0 || pairreq->channel >= MAX_CHANNEL_NUM)
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: pair channel check err, channel = %d\n", FUN, LINE, pairreq->channel);
							break;
						}

						//查询是否已配过对
						for(i = 0; i < MAX_CHANNEL_NUM; i++)
						{
							if(strcmp(clientmac, g_pair[i].mac) == 0)
							{
								tmppairchannel = i;

								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: find repair, channel = %d, prechannel = %d\n", 
									                            FUN, LINE, tmppairchannel, pairreq->channel);
								break;
							}
						}

						if(i < MAX_CHANNEL_NUM)
						{
							//已配过对
							tmppairip = clientip;
							tmppairstatus = 0;

							vava_pair.result = 0;
							vava_pair.addr = g_gatewayinfo.rfaddr;
#ifdef FREQUENCY_OFFSET
							vava_pair.index = g_pair[i].index;
#endif
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel = %d\n", FUN, LINE, pairreq->channel);
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: nstat = %d\n", FUN, LINE, g_pair[pairreq->channel].nstat);
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: lock = %d\n", FUN, LINE, g_pair[pairreq->channel].lock);
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: sn check [%s][%s]\n", FUN, LINE, g_pair[pairreq->channel].id, pairreq->sn);
							
							//未配过对
							if(g_pair[pairreq->channel].nstat == 0 
								&& g_pair[pairreq->channel].lock == 1
							    && strcmp(g_pair[pairreq->channel].id, pairreq->sn) == 0)
							{
#ifdef FREQUENCY_OFFSET
								if(pairreq->flag == 0 || (pairreq->flag == 1 && g_pair[pairreq->channel].index == pairreq->index))
								{
#endif
									tmppairchannel = pairreq->channel;
									tmppairip = clientip;
									tmppairstatus = 0;

									vava_pair.result = 0;
									vava_pair.addr = g_gatewayinfo.rfaddr;
#ifdef FREQUENCY_OFFSET
									vava_pair.index = g_pair[pairreq->channel].index;

									if(pairreq->flag == 0)
									{
										g_pair[pairreq->channel].index = 0;
									}
								}
								else
								{
									vava_pair.result = 1;
									vava_pair.addr = 0xFFFFFFFF;
									vava_pair.index = 0;
								}
#endif
							}
							else
							{
								vava_pair.result = 1;
								vava_pair.addr = 0xFFFFFFFF;
#ifdef FREQUENCY_OFFSET
								vava_pair.index = 0;
#endif
							}
						}

#ifdef FREQUENCY_OFFSET
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: pair result = %d, addr = %x[%x], gwaddr = %x, channel = %d\n", 
									                    FUN, LINE, vava_pair.result, vava_pair.addr, vava_pair.index, g_gatewayinfo.rfaddr, tmppairchannel);
#else
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: pair result = %d, addr = %x[%x], channel = %d\n", 
									                    FUN, LINE, vava_pair.result, vava_pair.addr, g_gatewayinfo.rfaddr, tmppairchannel);
#endif

						//经常出现基站短波地址为1情况
						if(vava_pair.addr == 1)
						{
							//重置基站短波地址
							VAVAHAL_BuildGwRfAddr();
						}

						//回复摄像机配对结果
						VAVAHAL_CmdResp_PairResp(clientfd, &vava_pair);
						
						if(tmppairstatus != 0)
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: pair info check fail, clear lock info\n", FUN, LINE);
							
							//VAVAHAL_ClearPairLock(); 
							g_pair[pairreq->channel].lock = 0;

							g_pair_result = 0;

							if(g_keyparing == 1)
							{
								g_keyparing = 0;

								//恢复灯状态
								VAVAHAL_ResetLedStatus();
							}

							if(g_pairmode == 1)
							{
								g_pairmode = 0;
								VAVAHAL_PlayAudioFile("/tmp/sound/sync_fail.opus");
							}
						}
						
						break;
					case VAVA_MSGTYPE_PAIR_ACK:
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======================== PAIR ACK ========================\n", FUN, LINE);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: clientip = %x [PAIR: ip - %x, channel - %d, status - %d]\n", 
							                            FUN, LINE, clientip, tmppairip, tmppairchannel, tmppairstatus);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ==========================================================\n", FUN, LINE);

						if(tmppairip == clientip)
						{
							if(g_wakeup_withpair_addr == g_pair[tmppairchannel].addr)
							{
								g_wakeup_withpair_status = 0;
							}
			
							if(tmppairstatus == 0 && tmppairchannel != -1)
							{
								//创建对应关系
								strcpy(g_pair[tmppairchannel].mac, clientmac);
								g_pair[tmppairchannel].ipaddr = clientip;
								g_pair[tmppairchannel].nstat = 1;
								g_pair[tmppairchannel].lock = 0;

								strcpy((char *)g_camerainfo[tmppairchannel].sn, g_pair[tmppairchannel].id);

								//更新channel信息
								channel = tmppairchannel;

								//写配置文件
								VAVAHAL_WritePairInfo();

								//更新RF信息
#ifdef FREQUENCY_OFFSET
								RF_Ack_Pair(g_pair[tmppairchannel].addr, g_pair[tmppairchannel].index, tmppairchannel);
#else
								RF_Ack_Pair(g_pair[tmppairchannel].addr, tmppairchannel);
#endif

								g_camerainfo[channel].first_flag = 1;
								g_firsttag = 1;
								g_cloudflag[channel] = 0;
								g_cloudrecheck[channel] = 1;
								
								VAVAHAL_SearchList_Remove(g_pair[tmppairchannel].addr);

								g_pir_sensitivity[channel] = VAVA_PIR_SENSITIVITY_MIDDLE;
								VAVAHAL_WritePirSensitivity();

								g_camerainfo[channel].v_quality = VAVA_VIDEO_QUALITY_AUTO;
								VAVAHAL_WriteVideoQuality();

								//清除布防信息
								VAVAHAL_ClearArmInfo_v1(channel);

								//清除摄像机推送配置
								g_pushflag[channel].push = 1;
								g_pushflag[channel].email = 0;
								VAVAHAL_WritePushConfig();
								VAVAHAL_WriteEmailConfig();

								if(g_keyparing == 1)
								{
									ret = VAVASERVER_AddCamera_Blind(g_pair[channel].id, channel);
								}
								else
								{
									ret = VAVASERVER_AddCamera(g_pair[channel].id, channel);
								}

								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: pair mode = %d\n", FUN, LINE, g_pairmode);
								
								if(g_pairmode == 1)
								{
									g_pairmode = 0;

									if(ret == 0)
									{
										VAVAHAL_PlayAudioFile("/tmp/sound/sync_success.opus");
										g_cameraattr_sync_flag[channel] = 1;
									}
									else
									{
										VAVAHAL_PlayAudioFile("/tmp/sound/sync_fail.opus");
									}
								}
							}
						}

						//清除锁定信息
						VAVAHAL_ClearPairLock();

						tmppairstatus = -1;
						tmppairchannel = -1;
						tmppairip = -1;

						if(g_keyparing == 1)
						{
							g_keyparing = 0;

							//恢复灯状态
							VAVAHAL_ResetLedStatus();
						}
						break;
					case VAVA_MSGTYPE_FIRST_CONNECT:
						firstinfo = (VAVA_First_Connect *)vava_common.data;

						if(channel == -1)
						{
							channel = VAVAHAL_CheckPairWithSn((char *)firstinfo->sn);
							if(channel == -1)
							{
								VAVAHAL_CmdResp_NoPair(clientfd);
								break;
							}
							else
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel - %d, mac update [%s] -> [%s]\n", FUN, LINE, channel, g_pair[channel].mac, clientmac);

								memset(logstr, 0, 128);
								sprintf(logstr, "ch-%d macup %s - %s", channel, g_pair[channel].mac, clientmac);
								Err_Log(logstr);
								
								//更新设备MAC地址
								strcpy(g_pair[channel].mac, clientmac);
							}
						}

						g_camerainfo[channel].videocodec = firstinfo->videocodec;
						g_camerainfo[channel].audiocodec = firstinfo->audiocodec;

						g_camerainfo[channel].m_res = firstinfo->m_res;
						g_camerainfo[channel].m_fps = firstinfo->m_fps;
						g_camerainfo[channel].m_bitrate = firstinfo->m_bitrate;
						g_camerainfo[channel].s_res = firstinfo->s_res;
						g_camerainfo[channel].s_fps = firstinfo->s_fps;
						g_camerainfo[channel].s_bitrate = firstinfo->s_bitrate;
						g_camerainfo[channel].mirror = firstinfo->mirror;
						g_camerainfo[channel].irledmode = firstinfo->irledmode;

						g_camerainfo[channel].samprate = firstinfo->samprate;
						g_camerainfo[channel].a_bit = firstinfo->a_bit;
						g_camerainfo[channel].channel = firstinfo->channel;
						g_camerainfo[channel].a_fps = firstinfo->a_fps;

						g_camerainfo[channel].mdctrl = firstinfo->mdctrl;
						g_camerainfo[channel].md_startx = firstinfo->md_startx;
						g_camerainfo[channel].md_endx = firstinfo->md_endx;
						g_camerainfo[channel].md_starty = firstinfo->md_starty;
						g_camerainfo[channel].md_endy = firstinfo->md_endy;
						g_camerainfo[channel].md_sensitivity = firstinfo->md_sensitivity;

						strcpy((char *)g_camerainfo[channel].hardver, (char *)firstinfo->hardver);
						strcpy((char *)g_camerainfo[channel].softver, (char *)firstinfo->softver);
						strcpy((char *)g_camerainfo[channel].sn, (char *)firstinfo->sn);
						strcpy((char *)g_camerainfo[channel].rfver, (char *)firstinfo->rfver);

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===========================================================\n", FUN, LINE);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ FIRST CONNECT [channel - %d]\n", FUN, LINE, channel);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ videocodc = %d, audiocodec = %d\n", 
							                            FUN, LINE, g_camerainfo[channel].videocodec, g_camerainfo[channel].audiocodec);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ RecStream:  res - %d, fps - %d, bitrate - %d\n", 
							                            FUN, LINE, g_camerainfo[channel].m_res, g_camerainfo[channel].m_fps, g_camerainfo[channel].m_bitrate);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ LiveStream: res - %d, fps - %d, bitrate - %d\n", 
							                            FUN, LINE, g_camerainfo[channel].s_res, g_camerainfo[channel].s_fps, g_camerainfo[channel].s_bitrate);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ Audio: samprate - %d, fps - %d, bitrate - %d, channel - %d\n", 
							                            FUN, LINE, g_camerainfo[channel].samprate, g_camerainfo[channel].a_fps, g_camerainfo[channel].a_bit,
							                            g_camerainfo[channel].channel);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ mirror - %d, irledmode - %d\n", 
							                            FUN, LINE, g_camerainfo[channel].mirror, g_camerainfo[channel].irledmode);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ MDInfo: ctrl - %d, [%d,%d] -> [%d,%d], sensitivity - %d\n", 
							                            FUN, LINE, g_camerainfo[channel].mdctrl, g_camerainfo[channel].md_startx, g_camerainfo[channel].md_starty,
											            g_camerainfo[channel].md_endx, g_camerainfo[channel].md_endy, g_camerainfo[channel].md_sensitivity);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ [%s][%s][%s][%s]\n", FUN, LINE, g_camerainfo[channel].hardver, g_camerainfo[channel].softver, 
							                                                               g_camerainfo[channel].sn, g_camerainfo[channel].rfver);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$ -------- PIR alarm - %d -------\n", FUN, LINE, firstinfo->pirstatus);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===========================================================\n", FUN, LINE);


						if(g_camerainfo[channel].first_flag == 1)
						{
							g_camerainfo[channel].first_flag = 0;
							g_devinfo_update = 1;
						}

						if(firstinfo->pirstatus == 1) //PIR触发
						{
							if(g_rec_delaytime == 0)
							{
								g_camerainfo[channel].alarm_flag = 1;
								VAVAHAL_StartAVMemCahce(channel, VAVA_AVMEM_MODE_ALL);
								VAVAHAL_PirAlarm(channel);
								sleep(1);
							}
							else
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel - %d, wait rec delay time - %d\n", FUN, LINE, channel, g_rec_delaytime);
							}
						}

						VAVAHAL_CmdResp_FirstConnectAck(clientfd);

						#if 0
						if(g_camerainfo_dynamic[channel].online == 1)
						{
							VAVAHAL_SleepCamera_Ex(channel);
						}
						#endif
						break;
					case VAVA_MSGTYPE_STREAM_START:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}

						//VAVAHAL_CmdResp_TimeSync(clientfd);

#ifdef AUDIO_TALK_LO_TEST //开启音频对讲(测试接口)
						g_avmemchace[channel].ap_read = g_avmemchace[channel].ap_write;
						g_avmemchace[channel].ap_nstats = 1;
						g_camera_talk[channel] = 1;
#endif
						break;
					case VAVA_MSGTYPE_STREAM_STOP:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}
						
						break;
					case VAVA_CONFIG_SET_RESOLUTION:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}
						
						resresult = (VAVA_ResConfig *)vava_common.data;
						ret = VAVAHAL_CheckSession(resresult->sessionid);
						if(ret == 0)
						{
							if(resresult->stream == 0)
							{
								if(resresult->result == 0)
								{
									g_camerainfo[channel].m_res = resresult->res;
									g_camerainfo[channel].m_fps = resresult->fps;
									g_camerainfo[channel].m_bitrate = resresult->bitrate;

									VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$$$$$ channel - %d, resoult set: m_res = %d, m_fps = %d, m_bitrate = %d $$$$$$\n", 
										                            FUN, LINE, channel, resresult->res, resresult->fps, resresult->bitrate);
								}
								
								VAVAHAL_SendCameraSetAck(resresult->sessionid, channel, resresult->result, VAVA_CMD_SET_REC_QUALITY);
							}
							else 
							{
								if(resresult->result == 0)
								{
									g_camerainfo[channel].s_res = resresult->res;
									g_camerainfo[channel].s_fps = resresult->fps;
									g_camerainfo[channel].s_bitrate = resresult->bitrate;

									VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: $$$$$$ channel - %d, resoult set: s_res = %d, s_fps = %d, s_bitrate = %d $$$$$$\n", 
										                            FUN, LINE, channel, resresult->res, resresult->fps, resresult->bitrate);

									if(resresult->sessionid != -1) //由APP更改分辨率才切换模式
									{
										g_camerainfo[channel].v_quality = g_tmp_quality[channel];
										VAVAHAL_WriteVideoQuality();
									}
								}
								
								VAVAHAL_SendCameraSetAck(resresult->sessionid, channel, resresult->result, VAVA_CMD_SET_VIDEO_QUALITY);
							}
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: session is closed\n", FUN, LINE);
						}

						//添加到休眠检测
						g_camerainfo[channel].sleep_check = 1;
						break;
					case VAVA_CONFIG_SET_MIRROR:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}
						
						mirrresult = (VAVA_MirrConfig *)vava_common.data;
						ret = VAVAHAL_CheckSession(mirrresult->sessionid);
						if(ret == 0)
						{
							if(mirrresult->result == 0)
							{
								g_camerainfo[channel].mirror = mirrresult->param;
							}

							g_cameraattr_sync_flag[channel] = 1;

							VAVAHAL_SendCameraSetAck(mirrresult->sessionid, channel, mirrresult->result, VAVA_CMD_SET_MIRRORMODE);
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: session is closed\n", FUN, LINE);
						}

						//添加到休眠检测
						g_camerainfo[channel].sleep_check = 1;
						break;
					case VAVA_CONFIG_SET_IRLEDMODE:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}
						
						irledresult = (VAVA_IrLedConfig *)vava_common.data;
						ret = VAVAHAL_CheckSession(irledresult->sessionid);
						if(ret == 0)
						{
							if(irledresult->result == 0)
							{
								g_camerainfo[channel].irledmode = irledresult->param;
							}

							g_cameraattr_sync_flag[channel] = 1;
							
							VAVAHAL_SendCameraSetAck(irledresult->sessionid, channel, irledresult->result, VAVA_CMD_SET_IRMODE);
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: session is closed\n", FUN, LINE);
						}
						
						//添加到休眠检测
						g_camerainfo[channel].sleep_check = 1;
						break;
					case VAVA_CONFIG_SET_MDPARAM:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}
						
						mdresult = (VAVA_MdConfig *)vava_common.data;
						ret = VAVAHAL_CheckSession(mdresult->sessionid);
						if(ret == 0)
						{
							if(mdresult->result == 0)
							{
								g_camerainfo[channel].mdctrl = mdresult->enabel;
								g_camerainfo[channel].md_sensitivity = mdresult->sensitivity;
								g_camerainfo[channel].md_startx = mdresult->startx;
								g_camerainfo[channel].md_starty = mdresult->starty;
								g_camerainfo[channel].md_endx = mdresult->endx;
								g_camerainfo[channel].md_endy = mdresult->endy;
							}

							g_cameraattr_sync_flag[channel] = 1;
							
							VAVAHAL_SendCameraSetAck(mdresult->sessionid, channel, mdresult->result, VAVA_CMD_SET_MDPARAM);
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: session is closed\n", FUN, LINE);
						}

						//添加到休眠检测
						g_camerainfo[channel].sleep_check = 1;
						break;
					case VAVA_CONFIG_RESET_FACTORY:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}
						
						camerafactory = (VAVA_CameraFactory *)vava_common.data;
						ret = VAVAHAL_CheckSession(camerafactory->sessionid);
						if(ret == 0)
						{
							VAVAHAL_SendCameraSetAck(camerafactory->sessionid, channel, camerafactory->result, VAVA_CMD_CAMERA_RESET);
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: session is closed\n", FUN, LINE);
						}

						//添加到休眠检测
						g_camerainfo[channel].sleep_check = 1;
						break;
					case VAVA_CONFIG_UPDATE_RESP:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}

						if(g_update.current == channel)
						{
							g_update.wait = -1;
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: UPDATE_RESP channel faile, save = %d, current = %d\n", 
								                             FUN, LINE, g_update.current, channel);
						}
						break;
					case VAVA_CONFIG_UPDATE_TRANSACK:
					case VAVA_CONFIG_UPDATE_RESULT:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}

						if(g_update.current == channel)
						{
							upstatus = (VAVA_Upgrate_Status *)vava_common.data;

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: type = %d, status = %d\n", FUN, LINE, vava_common.comtype, upstatus->status);

							g_update.result = upstatus->status;
							g_update.wait = -1;
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: UPDATE TRANSACK or RESULT channel faile, save = %d, current = %d\n", 
								                             FUN, LINE, g_update.current, channel);
						}
						break;
					case VAVA_CONFIG_START_REC:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}

						recctrlresult = (VAVA_CameraRecCtrl *)vava_common.data;
						if(recctrlresult->sessionid != -1)
						{
							if(recctrlresult->result == 0)
							{
								VAVAHAL_StartManualRec(channel, recctrlresult->sessionid);
								VAVAHAL_CmdReq_InsertIframe(clientfd, VAVA_STREAM_MAIN);

								ntsamptime = 0;
								while(g_running)
								{
									if(ntsamptime++ >= 50)
									{
										break;
									}

									if(g_manaul_ntsamp[channel] > 0)
									{
										break;
									}
									
									usleep(100000);
								}

								if(ntsamptime >= 50)
								{
									VAVAHAL_StopManualRec(channel);
									VAVAHAL_SendRecStartResult(recctrlresult->sessionid, channel, VAVA_ERR_CODE_RECSTART_FAIL);
								}
								else
								{
									VAVAHAL_SendRecStartResult(recctrlresult->sessionid, channel, VAVA_ERR_CODE_SUCCESS);
								}
							}
							else
							{
								VAVAHAL_SendRecStartResult(recctrlresult->sessionid, channel, VAVA_ERR_CODE_MANUAL_REC_FAIL);
							}
						}

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --------  START REC  -------- [%d][%d]\n", FUN, LINE, channel, recctrlresult->channel);
						break;
					case VAVA_CONFIG_STOP_REC:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}

						recctrlresult = (VAVA_CameraRecCtrl *)vava_common.data;
						if(recctrlresult->result != 0)
						{
							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: rec stream not close, channel = %d\n", FUN, LINE, channel);
						}

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --------  STOP REC  -------- [%d][%d]\n", FUN, LINE, channel, recctrlresult->channel);
						break;
					case VAVA_MSGTYPE_MD_RESULT:
						mdstatus = (VAVA_MdResult *)vava_common.data;

						VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --> [ channel - %d,  modetect resutl = %d ] <-- \n", FUN, LINE, channel, mdstatus->result);
						VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
						
						if(mdstatus->result == 1) //确认告警
						{
							g_md_result[channel] = 1;
							
							if(g_cloudflag[channel] == 1)
							{
								g_cloudsend[channel] = 1;
							}
						}
						else
						{
							//停止录像
							g_alarmrec[channel].ctrl = 0;
							g_cloudrec[channel].ctrl = 0;
							
							g_camerainfo[channel].alarm_flag = 0;
							g_camerainfo[channel].cloud_flag = 0;
							g_cloudsend[channel] = 0;
							
							g_md_result[channel] = 0;
							
							VAVAHAL_SleepCamera_Ex(channel);
						}
						break;
					case VAVA_MSGTYPE_OTA_INFO:
						ipcotainfo = (VAVA_IPC_OtaInfo *)vava_common.data;
						memset(&g_ipc_otainfo[channel], 0, sizeof(VAVA_IPC_OtaInfo));
						strcpy(g_ipc_otainfo[channel].f_code, ipcotainfo->f_code);
						strcpy(g_ipc_otainfo[channel].f_secret, ipcotainfo->f_secret);
						strcpy(g_ipc_otainfo[channel].f_inver, ipcotainfo->f_inver);
						strcpy(g_ipc_otainfo[channel].f_outver, ipcotainfo->f_outver);

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------- channel %d -----------------------\n", FUN, LINE, channel);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Camera f_code: %s\n", FUN, LINE, g_ipc_otainfo[channel].f_code);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Camera f_secret: %s\n", FUN, LINE,  g_ipc_otainfo[channel].f_secret);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Camera f_inver: %s\n", FUN, LINE, g_ipc_otainfo[channel].f_inver);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Camera f_outver: %s\n", FUN, LINE, g_ipc_otainfo[channel].f_outver);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------------------------\n", FUN, LINE);
						break;
					case VAVA_MSGTYPE_RFHW_INFO:
						ipcrfhw = (VAVA_IPC_RFHW *)vava_common.data;
						strcpy((char *)g_camerainfo[channel].rfhw, ipcrfhw->version);

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------- channel %d -----------------------\n", FUN, LINE, channel);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Camera RFHardVer: %s\n", FUN, LINE, g_camerainfo[channel].rfhw);
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------------------------\n", FUN, LINE, channel);
						break;
					case VAVA_MSGTYPE_COM_CTRL:
						if(channel == -1)
						{
							VAVAHAL_CmdResp_NoPair(clientfd);
							break;
						}
						
						comresult = (VAVA_IPCComConfig *)vava_common.data;
						ret = VAVAHAL_CheckSession(comresult->sessionid);
						if(ret == 0)
						{
							VAVAHAL_SendCameraSetAck(mirrresult->sessionid, channel, comresult->result, VAVA_CMD_IPC_COM_CTRL);
						}
						else
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: session is closed\n", FUN, LINE);
						}

						//添加到休眠检测
						g_camerainfo[channel].sleep_check = 1;
						break;
					default:
						break;
				}
			}

			if(errnum >= 5)
			{
				break;
			}
		}
	}

	//VAVAHAL_WriteSocketId(0, channel, -1);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: client exit, channel - %d\n", FUN, LINE, channel);
	
	return NULL;
}

void *commonserver_pth(void *data)
{
	int sockid = -1;
	int port = SERVER_CMD_PORT;

	int ret;
	//int checktime;
	int acepfd;
	size_t addr_len;
	struct sockaddr_in client_addr;

	fd_set rdRd;
	struct timeval timeout;

	ServerParam s_param;

#if 0
	checktime = 30;
	while(g_running)
	{
		if(checktime-- <= 0 || g_router_link_status == VAVA_NETWORK_LINKOK)
		{
			break;
		}

		sleep(1);
	}
#endif

	sockid = tcp_create(port, BING_NET);
	if(sockid < 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create server err\n", FUN, LINE);

		Err_Log("commonserver create fail");
		g_running = 0;
		return NULL;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: server is running...\n", FUN, LINE);

	while(g_running)
	{
		FD_ZERO(&rdRd);
		FD_SET(sockid, &rdRd);

		timeout.tv_sec = 0;				
		timeout.tv_usec = 500000;

		ret = select(sockid + 1, &rdRd, NULL, NULL, &timeout);
		if(ret < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err\n", FUN, LINE);
			continue;
		}
		else if(ret == 0)
		{
			continue;  //timeout
		}

		if(FD_ISSET(sockid, &rdRd))
		{
			addr_len = sizeof(struct sockaddr_in);
			acepfd = accept(sockid, (struct sockaddr*)&client_addr, &addr_len);
			if(acepfd < 0)
			{
				continue;
			}

			if(g_avstop == 1)
			{
				close(acepfd);
				continue;
			}

			if(g_update.status != VAVA_UPDATE_IDLE && g_update.type == VAVA_UPDATE_TYPE_GATEWAY)
			{
				close(acepfd);
				continue;
			}

			s_param.clientfd = acepfd;
			s_param.clientip = inet_addr(inet_ntoa(client_addr.sin_addr));
			memset(s_param.mac, 0, 18);
			ret = getpeermac(s_param.clientfd, s_param.mac);
			if(ret == 0)
			{
				//创建客户端
				pthread_t recv_id;
				pthread_attr_t attr;

				VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: client connect, fd - %d, ip - %s [%s]\n", 
					                            FUN, LINE, acepfd, inet_ntoa(client_addr.sin_addr), s_param.mac);

				pthread_attr_init(&attr); 
				pthread_attr_setdetachstate(&attr, 1);
				pthread_attr_setstacksize(&attr, STACK_SIZE);
				ret = pthread_create(&recv_id, &attr, commonclient_pth, &s_param);
				pthread_attr_destroy(&attr);

				if(ret != 0)
				{
					Err_Log("create commonclient fail");
					close(acepfd);
				}
			}
			else
			{
				Err_Log("get commonclient mac fail");
				close(acepfd);
			}
		}
	}

	close(sockid);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: common server exit\n", FUN, LINE);

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}

//#define SAVE_AV
#ifdef SAVE_AV
FILE *videofd = NULL;
FILE *audiofd = NULL;
int videocount = 0;
int audiocount = 0;
int avflag = 0;
char avpath[128];
time_t avtt;
struct tm *avttinfo;
#endif

void *avclient_pth(void *data)
{
	ServerParam *sp = (ServerParam *)data;
	int clientfd; 
	int clientip;
	char clientmac[18];
	char ip_str[16];

	int i;
	int ret;
	int channel;
	int recvsize = 0;
	unsigned char *recvbuff = NULL;
	unsigned int buffsize = MEM_AV_RECV_SIZE;

	int m_fps = 0;
	int s_fps = 0;

	int flag = 0;
	int sec = 0;
	int m_video = 0;
	int s_video = 0;
	int audio = 0;
	struct timeval time_save;
	struct timeval time_current;

	unsigned char m_savenum = 0;
	unsigned char s_savenum = 0;

	unsigned char m_checkflag = 0;
	unsigned char m_lostflag = 0;
	unsigned char s_checkflag = 0;
	unsigned char s_lostflag = 0;

	unsigned char lastflag = 0;
	unsigned char lastdata[4];

	fd_set rdRd;
	struct timeval timeout;

	int ntp;
	int phototime;

	int savemainnum = 0;
	int savesubnum = 0;

	int lastcheck = 0;
	int iframecheck = 0;

	VAVA_Avhead *vava_avhead;
	
	clientfd = sp->clientfd;
	clientip = sp->clientip;
	memset(clientmac, 0, 18);
	strcpy(clientmac, sp->mac);

	memset(ip_str, 0, 16);
	sprintf(ip_str, "%d.%d.%d.%d", (clientip & 0x000000FF),
	                               (clientip & 0x0000FF00) >> 8,
	                               (clientip & 0x00FF0000) >> 16,
	                               (clientip & 0xFF000000) >> 24);

	channel = VAVAHAL_CheckPairWithMac(clientmac, clientfd);
	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		sleep(2);
		close(clientfd);
		return NULL;
	}
	
	VAVAHAL_WriteSocketId(1, channel, clientfd);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: new client, clientfd = %d, [channel - %d][%s][%s]\n", 
		                            FUN, LINE, clientfd, channel, ip_str, clientmac);

	recvbuff = g_avrecvbuff[channel].buff;
	memset(recvbuff, 0, buffsize);

	while(g_running && g_avstop == 0)
	{
		clientfd = VAVAHAL_ReadSocketId(1, channel);
		if(clientfd < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel [%d] is close, exit\n", FUN, LINE, channel);
			break;
		}
			
		FD_ZERO(&rdRd);
		FD_SET(clientfd, &rdRd);

		timeout.tv_sec = 0;				
		timeout.tv_usec = 100000;
		
		ret = select(clientfd + 1, &rdRd, NULL, NULL, &timeout);
		if(ret < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err, channel = %d\n", FUN, LINE, channel);
			break;
		}
		else if(ret == 0)
		{
			continue;  //timeout
		}

		if(FD_ISSET(clientfd, &rdRd))
		{
			ret = recv(clientfd, recvbuff + recvsize, buffsize - recvsize, 0);
			if(ret < 0)
			{
				if(errno == EAGAIN)
				{
					continue;
				}

				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: ret = %d, strerror(errno) = %s, channel = %d\n", 
					                             FUN, LINE, ret, strerror(errno), channel);
				break;
			}
			else if(ret == 0)
			{
				break;
			}

			g_camerainfo[channel].wifi_num = 3;

			recvsize += ret;
			if(recvsize < 100)
			{
				continue;
			}
			
			vava_avhead = (VAVA_Avhead *)recvbuff;
			if(vava_avhead->tag != VAVA_TAG_CAMERA_AV)
			{
				//串流直接关闭该连接
				if(vava_avhead->tag == VAVA_TAG_CAMERA_CMD)
				{
					break;
				}

				//查找头
				for(i = 0; i < recvsize - 100; i++)
				{
					if(recvbuff[i] == 0x02 && recvbuff[i + 1] == 0x00
						&& recvbuff[i + 2] == 0x00 && recvbuff[i + 3] == 0xEB 
						&& (recvbuff[i + 4] == VAVA_STREAM_TYPE_MAIN || recvbuff[i + 4] == VAVA_STREAM_TYPE_SUB)
						&& recvbuff[i + 6] == 0x1)
					{
						break;
					}
				}

				recvsize -= i;
				memcpy(recvbuff, recvbuff + i, recvsize);

				if(recvsize <= 100)
				{
					VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: not find head or Iframe, lost size = %d, channel = %d\n", FUN, LINE, i, channel);

					if(iframecheck++ >= 10)
					{
						break;
					}
					
					continue;
				}

				iframecheck = 0;
				
				vava_avhead = (VAVA_Avhead *)recvbuff;

				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: find tag = %x, recvsize = %d, framesize = %d, lostsize = %d, channel = %d\n", 
					                             FUN, LINE, vava_avhead->tag, recvsize, vava_avhead->size, i, channel);
			}

			if(vava_avhead->size <= 0 || vava_avhead->size >= 200000)
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: tag - [%x] stream - [%d], frame - [%d], size - [%d], channel = %d\n", 
					                             FUN, LINE, vava_avhead->tag, vava_avhead->streamtype, vava_avhead->frametype, vava_avhead->size, channel);

				memset(recvbuff, 0, buffsize);
				recvsize = 0;
				continue;
			}

			if(recvsize < sizeof(VAVA_Avhead) + vava_avhead->size)
			{
				continue;
			}

			if(flag == 0)
			{
				gettimeofday(&time_save, NULL);
				flag = 1;
			}

			lastflag = 0;
			if(vava_avhead->frametype == 0 || vava_avhead->frametype == 1)
			{
				if((vava_avhead->version & 0x01) == 1)
				{
					lastflag = 1;
					
					memcpy(lastdata, recvbuff + sizeof(VAVA_Avhead) + vava_avhead->size - 4, 4);
					if(lastdata[0] != 0xEB || lastdata[1] != 0x55 || lastdata[2] != 0x55 || lastdata[3] != 0xEB)
					{
						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: WARING, framecheck fail [%x %x %x %x], channel = %d\n", 
							                             FUN, LINE, lastdata[0], lastdata[1], lastdata[2], lastdata[3], channel);

						lastcheck++;
						if(lastcheck >= 5)
						{
							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: framecheck more than 5 sec, recreate socket, channel = %d\n", FUN, LINE, channel);
							break;
						}

						gettimeofday(&time_current, NULL);
						sec = time_current.tv_sec - time_save.tv_sec;
						if(sec > 0)
						{
							time_t t_time;
							struct tm *t_info;
							time(&t_time);
							t_info = localtime(&t_time);

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [%02d:%02d:%02d] [ch - %d] [sec - %d] [%d / %03d Kb/s] [%d / %03d Kb/s] [A %d B/s] [Alarm - %d] [Cloud - %d]\n", 
								                            FUN, LINE, t_info->tm_hour, t_info->tm_min, t_info->tm_sec,
								                            channel, sec, m_fps, m_video / 1024 / sec, s_fps, s_video / 1024 / sec, audio / sec, 
								                            g_camerainfo[channel].alarm_flag, g_camerainfo[channel].cloud_flag);

							m_video = 0;
							s_video = 0;
							audio = 0;

							time_save.tv_sec = time_current.tv_sec;
						}
			
						recvsize = recvsize - sizeof(VAVA_Avhead) - vava_avhead->size;
						if(recvsize > 0)
						{
							memcpy(recvbuff, recvbuff + sizeof(VAVA_Avhead) + vava_avhead->size, recvsize);
						}

						continue;
					}

					lastcheck = 0;
				}
			}

			if(vava_avhead->size <= recvsize - sizeof(VAVA_Avhead));
			{
				//写入缓冲区
				switch(vava_avhead->streamtype)
				{
					case VAVA_STREAM_TYPE_MAIN: //主码流
						if(m_checkflag == 0)
						{
							if(vava_avhead->frametype == 1)
							{
								m_checkflag = 1;
								m_lostflag = 0;
								m_savenum = 1;
							}
						}
						else
						{
							//增加丢帧检测
							m_savenum++;
							if(m_savenum > 45)
							{
								m_savenum = 1;
							}
							
							if(vava_avhead->framenum != m_savenum)
							{
								if(vava_avhead->frametype == 1)
								{
									m_savenum = 1;
								}
								else
								{
									VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [M] lost frame, save = %d, current = %d, channel = %d\n", 
										                             FUN, LINE, m_savenum, vava_avhead->framenum, channel);

									m_lostflag = 1;
									m_checkflag = 0;
								}
							}
						}

						if(m_lostflag == 0)
						{
							m_fps = vava_avhead->framerate;
						
							if(vava_avhead->size <= g_avmemchace[channel].pmvMemEnd - g_avmemchace[channel].pmvMemFree)
							{
								if(lastflag == 0)
								{
									memcpy(g_avmemchace[channel].pmvMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
								}
								else
								{
									memcpy(g_avmemchace[channel].pmvMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size - 4);
								}
								g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].offset = g_avmemchace[channel].pmvMemFree - g_avmemchace[channel].pmvMemBegin;
								if(lastflag == 0)
								{
									g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].size = vava_avhead->size;
								}
								else
								{
									g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].size = vava_avhead->size - 4;
								}
								g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ftype = vava_avhead->frametype;
								g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].fps = vava_avhead->framerate;
								g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ntsamp = vava_avhead->ntsamp;

								//更新缓存
								if(lastflag == 0)
								{
									g_avmemchace[channel].pmvMemFree += vava_avhead->size;
								}
								else
								{
									g_avmemchace[channel].pmvMemFree += vava_avhead->size - 4;
								}

								//更新写指针位置
								g_avmemchace[channel].mv_write += 1;
								if(g_avmemchace[channel].mv_write >= MEM_MAIN_QUEUE_NUM)
								{
									g_avmemchace[channel].mv_write = 0;
								}
							}
							else
							{
								if(lastflag == 0)
								{
									memcpy(g_avmemchace[channel].pmvMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
								}
								else
								{
									memcpy(g_avmemchace[channel].pmvMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size - 4);
								}
								g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].offset = 0;
								if(lastflag == 0)
								{
									g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].size = vava_avhead->size;
								}
								else
								{
									g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].size = vava_avhead->size - 4;
								}
								g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ftype = vava_avhead->frametype;
								g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].fps = vava_avhead->framerate;
								g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ntsamp = vava_avhead->ntsamp;

								//更新缓存
								if(lastflag == 0)
								{
									g_avmemchace[channel].pmvMemFree = g_avmemchace[channel].pmvMemBegin + vava_avhead->size;
								}
								else
								{
									g_avmemchace[channel].pmvMemFree = g_avmemchace[channel].pmvMemBegin + vava_avhead->size - 4;
								}
								
								//更新写指针
								g_avmemchace[channel].mv_write += 1;
								if(g_avmemchace[channel].mv_write >= MEM_MAIN_QUEUE_NUM)
								{
									g_avmemchace[channel].mv_write = 0;
								}
							}

							m_video += vava_avhead->size;
						}
						break;
					case VAVA_STREAM_TYPE_SUB: //子码流
						if(s_checkflag == 0)
						{
							if(vava_avhead->frametype == 1)
							{
								s_checkflag = 1;
								s_lostflag = 0;
								s_savenum = 1;
							}
						}
						else
						{
							//增加丢帧检测
							s_savenum++;
							if(s_savenum > 45)
							{
								s_savenum = 1;
							}
							
							if(vava_avhead->framenum != s_savenum)
							{
								if(vava_avhead->frametype == 1)
								{
									s_savenum = 1;
								}
								else
								{
									VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [S] lost frame, save = %d, current = %d, channel = %d\n", 
										                             FUN, LINE, s_savenum, vava_avhead->framenum, channel);

									s_lostflag = 1;
									s_checkflag = 0;
								}
							}
						}
					
						if(s_lostflag == 0)
						{
							s_fps = vava_avhead->framerate;

							#ifdef SAVE_AV
							if(channel == 0 && vava_avhead->frametype == 1 && videofd == NULL && videocount == 0 
								&& g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD && g_gatewayinfo.format_flag == 0)
							{
								if(avflag == 0)
								{
									VAVAHAL_SystemCmd("/bin/mkdir -p /mnt/sd0/avtest");
									avflag = 1;
								}

								time(&avtt);
								avttinfo = localtime(&avtt);
								
								memset(avpath, 0, 128);
								sprintf(avpath, "/mnt/sd0/avtest/test_%d%02d%02d%02d%02d%02d.h264", avttinfo->tm_year + 1900, 
									                                                                avttinfo->tm_mon + 1, 
									                                                                avttinfo->tm_mday,
				                                                                                    avttinfo->tm_hour, 
				                                                                                    avttinfo->tm_min, 
				                                                                                    avttinfo->tm_sec);
								videofd = fopen(avpath, "wb");

								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========== START VIDEO REC... ==========\n", FUN, LINE);
							}

							if(videofd != NULL && channel == 0)
							{
								if(g_gatewayinfo.sdstatus != VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.format_flag == 1)
								{
									fclose(videofd);
									videofd = NULL;
									videocount = 0;
								}
								else
								{
									if(vava_avhead->frametype == 0 || vava_avhead->frametype == 1)
									{
										fwrite(recvbuff + sizeof(VAVA_Avhead), vava_avhead->size, 1, videofd);
										videocount++;
										if(videocount >= 4500)
										{
											fclose(videofd);
											videofd = NULL;
											videocount = 0;

											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========== END VIDEO REC... ==========\n", FUN, LINE);
										}	
									}
								}
							}
							
							#endif

							VAVAHAL_CheckVideoFrame(channel, (char *)(recvbuff + sizeof(VAVA_Avhead)), vava_avhead->frametype, "A");

							if(vava_avhead->size <= g_avmemchace[channel].psvMemEnd - g_avmemchace[channel].psvMemFree)
							{
								if(lastflag == 0)
								{
									memcpy(g_avmemchace[channel].psvMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
								}
								else
								{
									memcpy(g_avmemchace[channel].psvMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size - 4);
								}
								g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].offset = g_avmemchace[channel].psvMemFree - g_avmemchace[channel].psvMemBegin;
								if(lastflag == 0)
								{
									g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].size = vava_avhead->size;
								}
								else
								{
									g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].size = vava_avhead->size - 4;

								}
								g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ftype = vava_avhead->frametype;
								g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].fps = vava_avhead->framerate;
								g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ntsamp = vava_avhead->ntsamp;

								//更新缓存
								if(lastflag == 0)
								{
									g_avmemchace[channel].psvMemFree += vava_avhead->size;
								}
								else
								{
									g_avmemchace[channel].psvMemFree += vava_avhead->size - 4;
								}

								//抓图
								#if 1
								if(vava_avhead->frametype == 1)
								{
									if(g_snapshot[channel].flag == 1 && g_snapshot[channel].sessionid != -1)
									{
										ntp = VAVAHAL_GetNtp();
										phototime = vava_avhead->ntsamp / 1000 - ntp * 3600;

										if(lastflag == 0)
										{
											VAVAHAL_SaveSnapShot(channel, g_snapshot[channel].sessionid,
												                 (char *)(recvbuff + sizeof(VAVA_Avhead)), vava_avhead->size, phototime);
										}
										else
										{
											VAVAHAL_SaveSnapShot(channel, g_snapshot[channel].sessionid,
												                 (char *)(recvbuff + sizeof(VAVA_Avhead)), vava_avhead->size - 4, phototime);
										}

										g_snapshot[channel].flag = 0;
										g_snapshot[channel].sessionid = -1;
									}
								}
								#endif

								//更新写指针
								g_avmemchace[channel].sv_write += 1;
								if(g_avmemchace[channel].sv_write >= MEM_SUB_QUEUE_NUM)
								{
									g_avmemchace[channel].sv_write = 0;
								}
							}
							else
							{
								if(lastflag == 0)
								{
									memcpy(g_avmemchace[channel].psvMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
								}
								else
								{
									memcpy(g_avmemchace[channel].psvMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size - 4);
								}
								g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].offset = 0;
								if(lastflag == 0)
								{
									g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].size = vava_avhead->size;
								}
								else
								{
									g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].size = vava_avhead->size - 4;
								}
								g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ftype = vava_avhead->frametype;
								g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].fps = vava_avhead->framerate;
								g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ntsamp = vava_avhead->ntsamp;

								//更新缓存
								if(lastflag == 0)
								{
									g_avmemchace[channel].psvMemFree = g_avmemchace[channel].psvMemBegin + vava_avhead->size;
								}
								else
								{
									g_avmemchace[channel].psvMemFree = g_avmemchace[channel].psvMemBegin + vava_avhead->size - 4;
								}

								//抓图
								#if 1
								if(vava_avhead->frametype == 1)
								{
									if(g_snapshot[channel].flag == 1 && g_snapshot[channel].sessionid != -1)
									{

										ntp = VAVAHAL_GetNtp();
										phototime = vava_avhead->ntsamp / 1000 - ntp * 3600;

										if(lastflag == 0)
										{
											VAVAHAL_SaveSnapShot(channel, g_snapshot[channel].sessionid,
												                 (char *)(recvbuff + sizeof(VAVA_Avhead)), vava_avhead->size, phototime);
										}
										else
										{
											VAVAHAL_SaveSnapShot(channel, g_snapshot[channel].sessionid,
												                 (char *)(recvbuff + sizeof(VAVA_Avhead)), vava_avhead->size - 4, phototime);
										}

										g_snapshot[channel].flag = 0;
										g_snapshot[channel].sessionid = -1;
									}
								}
								#endif

								//更新写指针
								g_avmemchace[channel].sv_write += 1;
								if(g_avmemchace[channel].sv_write >= MEM_SUB_QUEUE_NUM)
								{
									g_avmemchace[channel].sv_write = 0;
								}
							}

							s_video += vava_avhead->size;
						}
						break;
					case VAVA_STREAM_TYPE_AUDIO: //音频 需要同时写入主码流和子码流
						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: [#WARING#]  AUDIO TYPE - 2 (need 8 or 9)\n", FUN, LINE);
						break;
						
#ifndef AUDIO_TALK_LO_TEST 
						//测试  写到对讲BUFF
						//写入主码流
						if(vava_avhead->size <= g_avmemchace[channel].pmvMemEnd - g_avmemchace[channel].pmvMemFree)
						{
							memcpy(g_avmemchace[channel].pmvMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].offset = g_avmemchace[channel].pmvMemFree - g_avmemchace[channel].pmvMemBegin;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].size = vava_avhead->size;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].pmvMemFree += vava_avhead->size;
							g_avmemchace[channel].mv_write += 1;

							if(g_avmemchace[channel].mv_write >= MEM_MAIN_QUEUE_NUM)
							{
								g_avmemchace[channel].mv_write = 0;
							}
						}
						else
						{
							memcpy(g_avmemchace[channel].pmvMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].offset = 0;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].size = vava_avhead->size;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].pmvMemFree = g_avmemchace[channel].pmvMemBegin + vava_avhead->size;
							g_avmemchace[channel].mv_write += 1;

							if(g_avmemchace[channel].mv_write >= MEM_MAIN_QUEUE_NUM)
							{
								g_avmemchace[channel].mv_write = 0;
							}
						}

						//写入子码流
						if(vava_avhead->size <= g_avmemchace[channel].psvMemEnd - g_avmemchace[channel].psvMemFree)
						{
							memcpy(g_avmemchace[channel].psvMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].offset = g_avmemchace[channel].psvMemFree - g_avmemchace[channel].psvMemBegin;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].size = vava_avhead->size;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].psvMemFree += vava_avhead->size;
							g_avmemchace[channel].sv_write += 1;

							if(g_avmemchace[channel].sv_write >= MEM_SUB_QUEUE_NUM)
							{
								g_avmemchace[channel].sv_write = 0;
							}
						}
						else
						{
							memcpy(g_avmemchace[channel].psvMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].offset = 0;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].size = vava_avhead->size;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].psvMemFree = g_avmemchace[channel].psvMemBegin + vava_avhead->size;
							g_avmemchace[channel].sv_write += 1;

							if(g_avmemchace[channel].sv_write >= MEM_SUB_QUEUE_NUM)
							{
								g_avmemchace[channel].sv_write = 0;
							}
						}

						audio += vava_avhead->size;
#else
						if(vava_avhead->size <= g_avmemchace[channel].papMemEnd - g_avmemchace[channel].papMemFree)
						{
							memcpy(g_avmemchace[channel].papMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].offset = g_avmemchace[channel].papMemFree - g_avmemchace[channel].papMemBegin;
							g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].size = vava_avhead->size;
							g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].papMemFree += vava_avhead->size;
							g_avmemchace[channel].ap_write += 1;

							if(g_avmemchace[channel].ap_write >= MEM_AUDIO_QUEUE_NUM)
							{
								g_avmemchace[channel].ap_write = 0;
							}
						}
						else
						{
							memcpy(g_avmemchace[channel].papMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].offset = 0;
							g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].size = vava_avhead->size;
							g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].papMemFree = g_avmemchace[channel].papMemBegin + vava_avhead->size;
							g_avmemchace[channel].ap_write += 1;

							if(g_avmemchace[channel].ap_write >= MEM_AUDIO_QUEUE_NUM)
							{
								g_avmemchace[channel].ap_write = 0;
							}
						}
#endif
						break;
					case VAVA_STREAM_TYPE_AUDIO1:
						savemainnum++;
						if(savemainnum >= 24)
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [ <-- ] Audio Rec Stream [24] Frame Recive\n", FUN, LINE);
							savemainnum = 0;
						}
						
						//写入主码流
						if(vava_avhead->size <= g_avmemchace[channel].pmvMemEnd - g_avmemchace[channel].pmvMemFree)
						{
							memcpy(g_avmemchace[channel].pmvMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].offset = g_avmemchace[channel].pmvMemFree - g_avmemchace[channel].pmvMemBegin;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].size = vava_avhead->size;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].fps = vava_avhead->framerate;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].pmvMemFree += vava_avhead->size;
							g_avmemchace[channel].mv_write += 1;

							if(g_avmemchace[channel].mv_write >= MEM_MAIN_QUEUE_NUM)
							{
								g_avmemchace[channel].mv_write = 0;
							}
						}
						else
						{
							memcpy(g_avmemchace[channel].pmvMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].offset = 0;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].size = vava_avhead->size;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].fps = vava_avhead->framerate;
							g_avmemchace[channel].mvlists[g_avmemchace[channel].mv_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].pmvMemFree = g_avmemchace[channel].pmvMemBegin + vava_avhead->size;
							g_avmemchace[channel].mv_write += 1;

							if(g_avmemchace[channel].mv_write >= MEM_MAIN_QUEUE_NUM)
							{
								g_avmemchace[channel].mv_write = 0;
							}
						}

						audio += vava_avhead->size;
						break;
					case VAVA_STREAM_TYPE_AUDIO2:
						//写入子码流
						#ifdef SAVE_AV
						
						if(channel == 0 && audiofd == NULL && audiocount == 0 
							&& g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD && g_gatewayinfo.format_flag == 0)
						{
							if(avflag == 0)
							{
								VAVAHAL_SystemCmd("/bin/mkdir -p /mnt/sd0/avtest")
								avflag = 1;
							}
							
							time(&avtt);
							avttinfo = localtime(&avtt);

							memset(avpath, 0, 128);
							sprintf(avpath, "/mnt/sd0/avtest/test_%d%02d%02d%02d%02d%02d.g711", avttinfo->tm_year + 1900, 
								                                                                avttinfo->tm_mon + 1, 
								                                                                avttinfo->tm_mday,
			                                                                                    avttinfo->tm_hour, 
			                                                                                    avttinfo->tm_min, 
			                                                                                    avttinfo->tm_sec);
							audiofd = fopen(avpath, "wb");

							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: ========== START AUDIO REC... ==========\n", FUN, LINE);
						}

						if(audiofd != NULL && channel == 0)
						{
							if(g_gatewayinfo.sdstatus != VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.format_flag == 1)
							{
								fclose(audiofd);
								audiofd = NULL;
								audiocount = 0;
							}
							else
							{
								fwrite(recvbuff + sizeof(VAVA_Avhead), vava_avhead->size, 1, audiofd);
								audiocount++;
								if(audiocount >= 7500)
								{
									fclose(audiofd);
									audiofd = NULL;
									audiocount = 0;

									VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: ========== END AUDIO REC... ==========\n", FUN, LINE);
								}	
							}
						}
						
						#endif

						savesubnum++;
						if(savesubnum >= 24)
						{
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [ <-- ] Audio Live Stream [24] Frame Recive\n", FUN, LINE);
							savesubnum = 0;
						}
						
						if(vava_avhead->size <= g_avmemchace[channel].psvMemEnd - g_avmemchace[channel].psvMemFree)
						{
							memcpy(g_avmemchace[channel].psvMemFree, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].offset = g_avmemchace[channel].psvMemFree - g_avmemchace[channel].psvMemBegin;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].size = vava_avhead->size;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].fps = vava_avhead->framerate;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].psvMemFree += vava_avhead->size;
							g_avmemchace[channel].sv_write += 1;

							if(g_avmemchace[channel].sv_write >= MEM_SUB_QUEUE_NUM)
							{
								g_avmemchace[channel].sv_write = 0;
							}
						}
						else
						{
							memcpy(g_avmemchace[channel].psvMemBegin, recvbuff + sizeof(VAVA_Avhead), vava_avhead->size);
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].offset = 0;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].size = vava_avhead->size;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ftype = vava_avhead->frametype;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].fps = vava_avhead->framerate;
							g_avmemchace[channel].svlists[g_avmemchace[channel].sv_write].ntsamp = vava_avhead->ntsamp;

							g_avmemchace[channel].psvMemFree = g_avmemchace[channel].psvMemBegin + vava_avhead->size;
							g_avmemchace[channel].sv_write += 1;

							if(g_avmemchace[channel].sv_write >= MEM_SUB_QUEUE_NUM)
							{
								g_avmemchace[channel].sv_write = 0;
							}
						}

						audio += vava_avhead->size;
						break;
					default:
						break;
				}

				gettimeofday(&time_current, NULL);
				sec = time_current.tv_sec - time_save.tv_sec;
				if(sec > 0)
				{
					time_t t_time;
					struct tm *t_info;
					time(&t_time);
					t_info = localtime(&t_time);

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [%02d:%02d:%02d] [ch - %d] [sec - %d] [%d / %03d Kb/s] [%d / %03d Kb/s] [A %d B/s] [Alarm - %d] [Cloud - %d]\n", 
								                    FUN, LINE, t_info->tm_hour, t_info->tm_min, t_info->tm_sec,
								                    channel, sec, m_fps, m_video / 1024 / sec, s_fps, s_video / 1024 / sec, audio / sec, 
								                    g_camerainfo[channel].alarm_flag, g_camerainfo[channel].cloud_flag);

					m_video = 0;
					s_video = 0;
					audio = 0;

					time_save.tv_sec = time_current.tv_sec;
				}
			
				recvsize = recvsize - sizeof(VAVA_Avhead) - vava_avhead->size;
				if(recvsize > 0)
				{
					memcpy(recvbuff, recvbuff + sizeof(VAVA_Avhead) + vava_avhead->size, recvsize);
				}
			} 
		}
	}

	//VAVAHAL_WriteSocketId(1, channel, -1);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: client exit, channel - %d\n", FUN, LINE, channel);

	return NULL;
}

void *avserver_pth(void *data)
{
	int sockid = -1;
	int port = SERVER_AV_PORT;

	int ret;
	//int checktime;
	int acepfd;
	size_t addr_len;
	struct sockaddr_in client_addr;

	fd_set rdRd;
	struct timeval timeout;

	ServerParam s_param;

#if 0
	checktime = 30;
	while(g_running)
	{
		if(checktime-- <= 0 || g_router_link_status == VAVA_NETWORK_LINKOK)
		{
			break;
		}

		sleep(1);
	}
#endif

	sockid = tcp_create(port, BING_NET);
	if(sockid < 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create server err\n", FUN, LINE);
		
		Err_Log("avserver create fail");
		g_running = 0;
		return NULL;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: server is running...\n", FUN, LINE);

	while(g_running)
	{
		FD_ZERO(&rdRd);
		FD_SET(sockid, &rdRd);

		timeout.tv_sec = 0;				
		timeout.tv_usec = 500000;

		ret = select(sockid + 1, &rdRd, NULL, NULL, &timeout);
		if(ret < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err\n", FUN, LINE);
			continue;
		}
		else if(ret == 0)
		{
			continue;  //timeout
		}

		if(FD_ISSET(sockid, &rdRd))
		{
			addr_len = sizeof(struct sockaddr_in);
			acepfd = accept(sockid, (struct sockaddr*)&client_addr, &addr_len);
			if(acepfd < 0)
			{
				continue;
			}

			if(g_avstop == 1)
			{
				close(acepfd);
				continue;
			}

			if(g_update.status != VAVA_UPDATE_IDLE && g_update.type == VAVA_UPDATE_TYPE_GATEWAY)
			{
				close(acepfd);
				continue;
			}

			memset(&s_param, 0, sizeof(ServerParam));
			s_param.clientfd = acepfd;
			s_param.clientip = inet_addr(inet_ntoa(client_addr.sin_addr));
			memset(s_param.mac, 0, 18);
			ret = getpeermac(s_param.clientfd, s_param.mac);
			if(ret == 0)
			{
				//创建客户端
				pthread_t recv_id;
				pthread_attr_t attr;

				pthread_attr_init(&attr); 
				pthread_attr_setdetachstate(&attr, 1);
				pthread_attr_setstacksize(&attr, STACK_SIZE);
				ret = pthread_create(&recv_id, &attr, avclient_pth, &s_param);
				pthread_attr_destroy(&attr);

				if(ret != 0)
				{
					Err_Log("create avclient fail");
					close(acepfd);
				}
			}
			else
			{
				Err_Log("get avclient mac fail");
				close(acepfd);
			}
		}
	}

	close(sockid);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: av server exit\n", FUN, LINE);

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);
	
	return NULL;
}

//#define SUB_STREAM_SAVE
#ifdef SUB_STREAM_SAVE
FILE *testfd = NULL;
int testnum = 0;
#endif

//发送实时预览流 使用子码流发送
void *avsend_pth(void *data)
{
	int channel = *(int *)data;
	int i;
	int ret;
	unsigned int checksize;
	char str[128];
	int checknum = 0;
	int flag_360P = 0;
	int flag_720P = 0;

	//读指针
	int av_read;
	
	int offset;
	int size;
	int type;
	int fps;
	unsigned int ntsamp_sec;
	unsigned int ntsamp_usec;

	int sendaudionum = 0;

	VAVA_AV_HEAD avhead;
	VAVA_ResConfig resconfig;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: check channel fail, channel = %d\n", FUN, LINE, channel);
		
		Err_Log("input avsend channel err");
		g_running = 0;
		return NULL;
	}

	av_read = g_avmemchace[channel].sv_write;;

	while(g_running)
	{
		if(g_avmemchace[channel].sv_nstats == 0)
		{
			usleep(60000);
			continue;
		}

		if(g_avstop == 1)
		{
			sleep(1);
			continue;
		}

		if(av_read == g_avmemchace[channel].sv_write)
		{
			usleep(60000);
			continue;
		}

		offset = g_avmemchace[channel].svlists[av_read].offset;
		size = g_avmemchace[channel].svlists[av_read].size;
		type = g_avmemchace[channel].svlists[av_read].ftype;
		fps = g_avmemchace[channel].svlists[av_read].fps;
		ntsamp_sec = g_avmemchace[channel].svlists[av_read].ntsamp / 1000;
		ntsamp_usec = g_avmemchace[channel].svlists[av_read].ntsamp % 1000;

		for(i = 0; i < MAX_SESSION_NUM; i++)
		{
			if(g_session[i].id != -1 && g_session[i].camerachannel == channel)
			{
				if((type == 0 || type == 1) && g_session[i].videostatus == 1)
				{
					if(type == 1 && g_session[i].videoflag == 0)
					{
						g_session[i].videoflag = 1;
						g_session[i].framenum = 0;

						#ifdef SUB_STREAM_SAVE
						if(testfd == NULL && testnum == 0)
						{
							testfd = fopen("/mnt/sd0/test.h264", "wb");
							if(testfd == NULL)
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create test.h264 fail\n", FUN, LINE);
							}
							else
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ================ test start @@@@\n", FUN, LINE);
							}
						}
						#endif
					}

					if(g_session[i].videoflag == 1)
					{
						ret = PPCS_Check_Buffer(g_session[i].id, P2P_CHANNEL_AV, &checksize, NULL);
						if(ret == ERROR_PPCS_SUCCESSFUL)
						{
							if(type == 1)
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: sessionchannel = %d, chacesize = %d, checknum = %d, auto = %d\n",
									                            FUN, LINE, i, checksize, checknum, g_camerainfo[channel].v_quality);
							}

							if(g_camerainfo[channel].v_quality == VAVA_VIDEO_QUALITY_AUTO)
							{
								if(checksize < SYP2P_VIDEO_NORMAL_SIZE)
								{
									if(type == 1)
									{
										g_session[i].sendmode = P2P_SEND_MODE_NORMAL;
									}

									if(g_camerainfo[channel].s_res == VAVA_VIDEO_RESOULT_360P)
									{
										checknum += 1;
									}
								}
								else if(checksize >= SYP2P_AUTOVQ_CHANGE_SIZE && checksize < SYP2P_AUTOVQ_LOSTP)
								{	
									if(g_camerainfo[channel].s_res != VAVA_VIDEO_RESOULT_360P)
									{
										if(flag_360P == 0)
										{
											flag_360P = 1;
											flag_720P = 0;
											
											memset(&resconfig, 0, sizeof(VAVA_ResConfig));
											resconfig.stream = VAVA_STREAM_TYPE_SUB;
											resconfig.res = VAVA_VIDEO_RESOULT_360P;
											resconfig.fps = 15;
											resconfig.bitrate = 400;
											VAVAHAL_InsertCmdList(channel, -1, VAVA_CMD_SET_VIDEO_QUALITY, (void *)&resconfig, sizeof(VAVA_ResConfig));

											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======= Change Resoult [1080P or 720P -> 360P]\n", FUN, LINE);
										}
									}
									
									checknum = 0;
								}
								else if(checksize >= SYP2P_AUTOVQ_LOSTP && checksize < SYP2P_AUTOVQ_LOSTALL)
								{
									g_session[i].sendmode = P2P_SEND_MODE_LOSTP;

									if(type == 1) //减少点打印
									{
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Lost p..., channel = %d, checksize = %d\n", FUN, LINE, channel, checksize);
									}
								}
								else if(checksize >= SYP2P_AUTOVQ_LOSTALL)
								{
									g_session[i].sendmode = P2P_SEND_MODE_LOSTALL;

									if(type == 1) //减少点打印
									{
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Lost all..., channel = %d, checksize = %d\n", FUN, LINE, channel, checksize);
									}
								}

								if(checknum >= 300)
								{
									if(g_camerainfo[channel].s_res != VAVA_VIDEO_RESOULT_720P)
									{
										if(flag_720P == 0)
										{
											flag_720P = 1;
											flag_360P = 0;
											memset(&resconfig, 0, sizeof(VAVA_ResConfig));
											resconfig.stream = VAVA_STREAM_TYPE_SUB;
											resconfig.res = VAVA_VIDEO_RESOULT_720P;
											resconfig.fps = 15;
											resconfig.bitrate = 700;
											VAVAHAL_InsertCmdList(channel, -1, VAVA_CMD_SET_VIDEO_QUALITY, (void *)&resconfig, sizeof(VAVA_ResConfig));

											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ======= Change Resoult [360P -> 720P]\n", FUN, LINE);
										}
									}

									checknum = 0;
								}
							}
							else
							{
								if(checksize < SYP2P_VIDEO_NORMAL_SIZE) //可正常传输
								{
									if(type == 1)
									{
										g_session[i].sendmode = P2P_SEND_MODE_NORMAL;
									}
								}
								else if(checksize > SYP2P_VIDEO_LOSTP_SIZE && checksize <= SYP2P_VIDEO_LOSTALL_SIZE)
								{
									g_session[i].sendmode = P2P_SEND_MODE_LOSTP;

									if(type == 1) //减少点打印
									{
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Lost p..., channel = %d, checksize = %d\n", FUN, LINE, channel, checksize);
									}
								}
								else if(checksize > SYP2P_VIDEO_LOSTALL_SIZE)
								{
									g_session[i].sendmode = P2P_SEND_MODE_LOSTALL;

									if(type == 1) //减少点打印
									{
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Lost all..., channel = %d, checksize = %d\n", FUN, LINE, channel, checksize);
									}
								}
							}

							if(g_session[i].sendmode == P2P_SEND_MODE_LOSTP)
							{
								if(type == 0)
								{
									continue;
								}
							}
							else if(g_session[i].sendmode == P2P_SEND_MODE_LOSTALL)
							{
								continue;
							}

							memset(&avhead, 0, sizeof(VAVA_AV_HEAD));
							avhead.tag = VAVA_TAG_VIDEO;
							avhead.encodetype = g_camerainfo[channel].videocodec;
							avhead.frametype = type;
							avhead.framerate = fps;

							if(g_camerainfo[channel].v_quality == VAVA_VIDEO_QUALITY_AUTO)
							{
								avhead.res = VAVA_VIDEO_QUALITY_AUTO;
							}
							else
							{
								avhead.res = g_camerainfo[channel].s_res;
							}
							
							avhead.size = size;
							avhead.ntsamp_sec = ntsamp_sec;
							avhead.ntsamp_usec = ntsamp_usec;
							avhead.framenum = g_session[i].framenum++;

							//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: res = %d, fps = %d\n", FUN, LINE, avhead.res, avhead.framerate);

							ret = PPCS_Write(g_session[i].id, P2P_CHANNEL_AV, (char *)&avhead, sizeof(VAVA_AV_HEAD));
							if(ret == sizeof(VAVA_AV_HEAD))
							{
								#ifdef SUB_STREAM_SAVE
								if(testfd != NULL)
								{
									fwrite(g_avmemchace[channel].psvMemBegin + offset, size, 1, testfd);
									testnum ++;
									if(testnum >= 500)
									{
										fclose(testfd);
										testfd = NULL;

										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ================ test end @@@@\n", FUN, LINE);
									}
								}
								#endif

								VAVAHAL_CheckVideoFrame(channel, g_avmemchace[channel].psvMemBegin + offset, avhead.frametype, "B");
								
								ret = PPCS_Write(g_session[i].id, P2P_CHANNEL_AV, g_avmemchace[channel].psvMemBegin + offset, size);
								if(ret != size)
								{
									VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: PPCS_Write fail, videosize = %d, ret = %d\n", FUN, LINE, size, ret);

									memset(str, 0, 128);
									sprintf(str, "PPCS_Write video fail, channel = %d", channel);
									Err_Log(str);
								}
							}
							else
							{
								VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: PPCS_Write videohead, fail, ret = %d\n", FUN, LINE, ret);

								memset(str, 0, 128);
								sprintf(str, "PPCS_Write videohead fail, channel = %d", channel);
								Err_Log(str);	
							}
						}
						else
						{
							memset(str, 0, 128);
							sprintf(str, "PPCS_Check_Video_Buffer fail, channel = %d, ret = %d", channel, ret);
							Err_Log(str);
						}
					}
				}
				else if(type == 8 && g_session[i].audiostatus == 1 && g_camerainfo[channel].mic == 1)
				{
					if(g_session[i].videoflag == 0 || g_session[i].sendmode == P2P_SEND_MODE_LOSTALL)
					{
						continue;
					}

					sendaudionum++;
					if(sendaudionum >= 24)
					{
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [ --> ] Audio Live Stream [24] Frame Send\n", FUN, LINE);
						sendaudionum = 0;
					}

					memset(&avhead, 0, sizeof(VAVA_AV_HEAD));
					avhead.tag = VAVA_TAG_AUDIO;
					avhead.encodetype = g_camerainfo[channel].audiocodec;
					avhead.frametype = type;
					avhead.framerate = fps;
					avhead.size = size;
					avhead.ntsamp_sec = ntsamp_sec;
					avhead.ntsamp_usec = ntsamp_usec;
					avhead.framenum = g_session[i].framenum++;

					ret = PPCS_Write(g_session[i].id, P2P_CHANNEL_AV, (char *)&avhead, sizeof(VAVA_AV_HEAD));
					if(ret == sizeof(VAVA_AV_HEAD))
					{
						ret = PPCS_Write(g_session[i].id, P2P_CHANNEL_AV, g_avmemchace[channel].psvMemBegin + offset, size);
						if(ret != size)
						{
							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: PPCS_Write fail, audiosize = %d, ret = %d\n", FUN, LINE, size, ret);

							memset(str, 0, 128);
							sprintf(str, "PPCS_Write audio fail, channel = %d", channel);
							Err_Log(str);
						}
					}
					else
					{
						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: PPCS_Write audiohead fail, ret = %d\n", FUN, LINE, ret);

						memset(str, 0, 128);
						sprintf(str, "PPCS_Write audiohead fail, channel = %d", channel);
						Err_Log(str);
					}
				}
			}
		}

		if(type == 0 || type == 1)
		{
			usleep(5000);
		}

		av_read++;
		if(av_read >= MEM_SUB_QUEUE_NUM)
		{
			av_read = 0;
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

//#define AUDIO_PLAY_SAVE
#ifdef AUDIO_PLAY_SAVE
FILE *apaudio = NULL;
int apcount = 0;

FILE *apaudio1 = NULL;
int apcount1 = 0;
#endif
void *audiorecv_pth(void *data)
{
	int channel = *(int *)data;
	int ret;
	int tmptalk = -1;
	int talkstart = 0;
	int readsize;
	char recvbuff[1200];
	VAVA_AV_HEAD *avhead;
	VAVA_CMD_HEAD *cmdhead;

	int recvaudionum = 0;

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);
		
		Err_Log("input audiorecv channel err");
		g_running = 0;
		return NULL;
	}
	
	while(g_running)
	{
		if(g_camera_talk[channel] == -1)
		{
			tmptalk = -1;
				
			usleep(40000);
			continue;
		}

		if(g_avstop == 1)
		{
			sleep(1);
			continue;
		}

		if(tmptalk == -1)
		{
			tmptalk = g_camera_talk[channel];
			talkstart = 0;
		}
		
		memset(recvbuff, 0, 1200);
		readsize = sizeof(VAVA_AV_HEAD);
		ret = PPCS_Read(tmptalk, P2P_CHANNEL_AV, recvbuff, &readsize, 5000);
		if(ret == ERROR_PPCS_SUCCESSFUL && readsize == sizeof(VAVA_AV_HEAD))
		{
			#if 0
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: AudioPlayback head [%x], size = %d, channel = %d\n", 
			                                FUN, LINE, avhead->tag, avhead->size, channel);
			#endif
				
			avhead = (VAVA_AV_HEAD *)recvbuff;
			if(avhead->tag != VAVA_TAG_TALK)
			{
				if(avhead->tag == 0 && avhead->size == 0)
				{
					continue;
				}

				//解析音频传输头失败 需要重新创建会话
				memset(recvbuff, 0, 1200);
				cmdhead = (VAVA_CMD_HEAD *)recvbuff;
				cmdhead->sync_code = VAVA_TAG_APP_CMD;
				cmdhead->cmd_code = VAVA_CMD_PARSEERR;
				cmdhead->cmd_length = 0;

				pthread_mutex_lock(&mutex_session_lock);
				PPCS_Write(tmptalk, P2P_CHANNEL_CMD, recvbuff, sizeof(VAVA_CMD_HEAD));
				pthread_mutex_unlock(&mutex_session_lock);

				g_camera_talk[channel] = -1;
				continue;
			}

			//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: vavahal->framenum = %d\n", FUN, LINE, avhead->framenum);

			recvaudionum++;
			if(recvaudionum >= 24)
			{
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [ <-- ] Audio PlayBack Stream [24] Frame Send, channel = %d, talkstart = %d\n", 
			                                    FUN, LINE, channel, talkstart);
				
				recvaudionum = 0;
			}

			readsize = avhead->size;
			ret = PPCS_Read(tmptalk, P2P_CHANNEL_AV, recvbuff + sizeof(VAVA_AV_HEAD), &readsize, 0xFFFFFFFF);
			if(ret == ERROR_PPCS_SUCCESSFUL && readsize == avhead->size)
			{
				#if 0
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: AudioPlayback readdata, size = %d channel = %d\n",
				                                FUN, LINE, readsize, channel);
				#endif

				#ifdef AUDIO_PLAY_SAVE
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: AP 1: avhead->framenum = %d\n", FUN, LINE, avhead->framenum);
				#endif

				if(talkstart == 0 && avhead->framenum == 0)
				{
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [ *** AUDIO TALK START *** ] channel = %d\n", FUN, LINE, channel);

					talkstart = 1;

					#ifdef AUDIO_PLAY_SAVE

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========= AP 1 Send Start =========\n", FUN, LINE);
					
					if(apaudio == NULL && apcount == 0)
					{
						apaudio = fopen("/tmp/save.g711", "wb");
					}
					#endif
				}

				if(talkstart == 0)
				{
					continue;
				}

				#ifdef AUDIO_PLAY_SAVE
				if(apaudio != NULL)
				{
					fwrite(recvbuff + sizeof(VAVA_AV_HEAD), avhead->size, 1, apaudio);
					apcount++;
					if(apcount >= 24)
					{
						fclose(apaudio);
						apaudio = NULL;
						apcount = 0;

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========= Save AP Audio 1 Success =========\n", FUN, LINE);
					}
				}
				#endif
				
				//写入缓冲区
				if(avhead->size <= g_avmemchace[channel].papMemEnd - g_avmemchace[channel].papMemFree)
				{
					memcpy(g_avmemchace[channel].papMemFree, recvbuff + sizeof(VAVA_AV_HEAD), avhead->size);
					g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].offset = g_avmemchace[channel].papMemFree - g_avmemchace[channel].papMemBegin;
					g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].size = avhead->size;
					g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].framenum = avhead->framenum;

					g_avmemchace[channel].papMemFree += avhead->size;
					g_avmemchace[channel].ap_write += 1;

					if(g_avmemchace[channel].ap_write >= MEM_AUDIO_QUEUE_NUM)
					{
						g_avmemchace[channel].ap_write = 0;
					}
				}
				else
				{
					memcpy(g_avmemchace[channel].papMemBegin, recvbuff + sizeof(VAVA_AV_HEAD), avhead->size);
					g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].offset = 0;
					g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].size = avhead->size;
					g_avmemchace[channel].aplists[g_avmemchace[channel].ap_write].framenum = avhead->framenum;

					g_avmemchace[channel].papMemFree = g_avmemchace[channel].papMemBegin + avhead->size;
					g_avmemchace[channel].ap_write += 1;

					if(g_avmemchace[channel].ap_write >= MEM_AUDIO_QUEUE_NUM)
					{
						g_avmemchace[channel].ap_write = 0;
					}
				}
			}
			else
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: AudioPlayback readdata fail, needsize = %d, size = %d channel = %d\n",
				                                FUN, LINE, avhead->size, readsize, channel);
			}
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

void *apsend_pth(void *data)
{
	int channel = *(int *)data;
	int ret;
	int sockid;
	int framenum = 0;
	int sendstart = 0;

	VAVA_Avhead *audiohead;
	char sendbuff[500];

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);

		Err_Log("input apsend channel err");
		g_running = 0;
		return NULL;
	}
	
	while(g_running)
	{
		if(g_camera_talk[channel] == -1)
		{
			sendstart = 0;
			
			usleep(40000);
			continue;
		}

		if(g_avstop == 1)
		{
			sleep(1);
			continue;
		}

		if(g_avmemchace[channel].ap_nstats == 0)
		{
			usleep(40000);
			continue;
		}

		if(g_avmemchace[channel].ap_read == g_avmemchace[channel].ap_write)
		{
			usleep(40000);
			continue;
		}

		if(g_camerainfo[channel].speeker == 0)
		{
			g_avmemchace[channel].ap_read++;
			if(g_avmemchace[channel].ap_read >= MEM_AUDIO_QUEUE_NUM)
			{
				g_avmemchace[channel].ap_read = 0;
			}

			continue;
		}

		sockid = VAVAHAL_ReadSocketId(1, channel);
		if(sockid != -1)
		{
			framenum = g_avmemchace[channel].aplists[g_avmemchace[channel].ap_read].framenum;
			
			memset(sendbuff, 0, 500);
			audiohead = (VAVA_Avhead *)sendbuff;
			audiohead->tag = VAVA_TAG_CAMERA_AV;
			audiohead->streamtype = VAVA_STREAM_TYPE_AUDIO;
			audiohead->encodetype = g_camerainfo[channel].audiocodec;
			audiohead->frametype = 8;
			audiohead->framenum = framenum;
			audiohead->framerate = g_camerainfo[channel].a_fps;
			audiohead->size = g_avmemchace[channel].aplists[g_avmemchace[channel].ap_read].size;
			audiohead->ntsamp = g_avmemchace[channel].aplists[g_avmemchace[channel].ap_read].ntsamp;

			memcpy(sendbuff + sizeof(VAVA_Avhead), g_avmemchace[channel].papMemBegin
				   + g_avmemchace[channel].aplists[g_avmemchace[channel].ap_read].offset, audiohead->size);

#if 0
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: send audio play, sockid = %d, size = %d, read = %d, write = %d\n",
				                             FUN, LINE, sockid, audiohead->size, g_avmemchace[channel].ap_read, 
				                             g_avmemchace[channel].ap_write);
#endif		

			#ifdef AUDIO_PLAY_SAVE
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: AP 2: framenum = %d\n", FUN, LINE, framenum);
			#endif

			if(sendstart == 0 && framenum == 0)
			{
				sendstart = 1;

				#ifdef AUDIO_PLAY_SAVE
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: ========= AP 2 Send Start =========\n", FUN, LINE);
				
				if(apaudio1 == NULL && apcount1 == 0)
				{
					apaudio1 = fopen("/tmp/save1.g711", "wb");
				}
				#endif
			}

			if(sendstart == 1)
			{
				#ifdef AUDIO_PLAY_SAVE
				if(apaudio1 != NULL)
				{
					fwrite(sendbuff + sizeof(VAVA_AV_HEAD), audiohead->size, 1, apaudio1);
					apcount1++;
					if(apcount1 >= 24)
					{
						fclose(apaudio1);
						apaudio1 = NULL;
						apcount1 = 0;

						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: ========= Save AP Audio 2 Success =========\n", FUN, LINE);
					}
				}
				#endif
				
				ret = send(sockid, sendbuff, sizeof(VAVA_Avhead) + audiohead->size, 0);
				if(ret < 0)
				{
					if(errno != EAGAIN)
					{
						VAVAHAL_WriteSocketId(1, channel, -1);
					}
				}
				else if(ret == 0)
				{
					VAVAHAL_WriteSocketId(1, channel, -1);
				}
			}
		}

		g_avmemchace[channel].ap_read++;
		if(g_avmemchace[channel].ap_read >= MEM_AUDIO_QUEUE_NUM)
		{
			g_avmemchace[channel].ap_read = 0;
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}


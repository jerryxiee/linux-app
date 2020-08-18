#include "basetype.h"
#include "watchdog.h"
#include "vavahal.h"
#include "devdiscovery.h"

typedef struct _devsearch{
	int sign;						//同步头 0xEB000006 
	short cmd;						//命令字 0x100      
	short len;						//负载数据长度0
}VAVA_Dev_Search;

struct _camerainfo{
	int status;						//是否配对
	char id[32];					//摄像机序列号
}; 

typedef struct _devinfo{
	char uuid[32];					//基站序列号
	char ip[16];					//基站IP地址
	char devmod[16];				//基站硬件类型
	char sofrver[16];				//基站软件版本号
	int cameranum; 					//基站配对摄像机数
	struct _camerainfo camerainfo[MAX_CHANNEL_NUM];//摄像机信息
}VAVA_Dev_info;

void *devdiscovery_pth(void *data)
{
	int i;
	int ret;
	int sockfd;
	int port = 7499;
	int on = 1;
	int sendsize = 0;
	socklen_t len;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	VAVA_Dev_Search head;
	VAVA_Dev_info respond;

	char recvbuff[1024];
	char sendbuff[1024];

	while(g_running)
	{
		sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
		if(sockfd < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: socket create err, fd = %d\n", FUN, LINE, sockfd);

			sleep(5);
			continue;
		}

		//设置可重用和广播类型
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

		memset(&server_addr, 0, sizeof(struct sockaddr_in));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		server_addr.sin_port = htons(port);

		if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
		{
			close(sockfd);

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: bind err\n", FUN, LINE);
			
			sleep(5);
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------------------------------------\n", FUN, LINE);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:         DEV-SERARCH SERVICE OPEN       \n", FUN, LINE);
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ---------------------------------------\n", FUN, LINE);
		
		while(g_running)
		{
			len = sizeof(struct sockaddr_in);
			memset(recvbuff, 0, 1024);
			ret = recvfrom(sockfd, recvbuff, 1024, 0, (struct sockaddr *)&client_addr, &len);
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
				break;
			}
			else
			{
				//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: recv broadcast, size = %d\n", FUN, LINE, ret);
				
				if(ret >= sizeof(VAVA_Dev_Search))
				{
					memcpy(&head, recvbuff, sizeof(VAVA_Dev_Search));
					if(head.sign != VAVA_TAG_DEV_SEARCH)
					{
						continue;
					}

					#if 0
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------\n", FUN, LINE);
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: sign: 0x%x\n", FUN, LINE, head.sign);
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmd: 0x%x\n", FUN, LINE, head.cmd);
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: len: %d\n", FUN, LINE, head.len);
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------\n", FUN, LINE);
					#endif

					if(head.sign == VAVA_TAG_DEV_SEARCH && head.cmd == 0x100)
					{
						head.len = sizeof(VAVA_Dev_info);

						memset(&respond, 0, sizeof(VAVA_Dev_info));
						strcpy(respond.uuid, g_gatewayinfo.sn);
						strcpy(respond.ip, g_gatewayinfo.ipstr);
						strcpy(respond.devmod, g_gatewayinfo.hardver);
						strcpy(respond.sofrver, g_gatewayinfo.softver);
						respond.cameranum = 0;

						for(i = 0; i < MAX_CHANNEL_NUM; i++)
						{
							memset(&respond.camerainfo[i], 0, sizeof(struct _camerainfo));
							
							if(g_pair[i].nstat == 1)
							{
								respond.cameranum++;

								respond.camerainfo[i].status = 1;
								strcpy(respond.camerainfo[i].id, g_pair[i].id);
							}
						}

						memset(sendbuff, 0, 1024);
						sendsize = 0;
						memcpy(sendbuff, &head, sizeof(VAVA_Dev_Search));
						sendsize += sizeof(VAVA_Dev_Search);
						memcpy(sendbuff + sendsize, &respond, sizeof(VAVA_Dev_info));
						sendsize += sizeof(VAVA_Dev_info);

						ret = sendto(sockfd, sendbuff, sendsize, 0, (struct sockaddr *)&client_addr, len);

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Dev Search... [%s], ret = %d\n", FUN, LINE, respond.ip, ret);
					}
				}
			}
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}


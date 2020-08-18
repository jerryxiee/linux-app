#include "basetype.h"
#include "vavahal.h"
#include "watchdog.h"
#include "networkcheck.h"

#define WIRLESS_STATUS_FAILD		0x300
#define WIRLESS_STATUS_LINKING		0x400
#define WIRLESS_STATUS_SUCCESS		0x500
		
#define netmsg_key					0xEB01
#define netmsg_type					0x11
#define netsend_type				0x22

int g_router_link_type = -1;		//路由器连接方式
									//0 无线  1 有线
volatile unsigned char g_router_link_status = VAVA_NETWORK_LINKFAILD;
									//路由器连接状态
static int g_internetcheck = 0;
static int g_internetflag = 1;

void *wirlesscheck_pth(void *data)
{
	int status;
	int savestatus = WIRLESS_STATUS_FAILD;
	
	//延时启动
	sleep(5);
	
	while(g_running)
	{
		sleep(3);
		
		if(g_router_link_type != 0)
		{
			savestatus = WIRLESS_STATUS_FAILD;
			continue;
		}

		status = VAVAHAL_SystemCmd("/usr/sbin/netCheck");

		if(savestatus != status)
		{
			savestatus = status;
			
			switch(savestatus)
			{
				case WIRLESS_STATUS_FAILD:
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: wirless connect fail\n", FUN, LINE);
					
					if(g_apflag == 0)
					{
						VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
						VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_LIGHT);
					}
					
					break;
				case WIRLESS_STATUS_LINKING:
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: wirless conncting\n", FUN, LINE);
					
					if(g_apflag == 0)
					{
						VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
						VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);
					}
					
					break;
				case WIRLESS_STATUS_SUCCESS:
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: wirless connect success\n", FUN, LINE);
					
					if(g_apflag == 0)
					{
						VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_LIGHT);
						VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_SLAKE);
					}
					
					break;
				default:
					break;
			}	
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

void *netchange_pth(void *data)
{
	int ret;
	int msgid = -1;
	int savetype = -1;
	int changeflag = 0;
	VAVA_NetMsg msghead;
	
	struct timeval msgtime;
	struct timeval msgchange;

	while(g_running)
	{
		//创建消息
		msgid = msgget(netmsg_key, 0666);  
	    if(msgid < 0)
		{  
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: can't get msgid, 5 sec retry\n", FUN, LINE);

			sleep(5);
			continue;
	    }

		break;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: clear msg chach...\n", FUN, LINE);
	
	//取空消息(避免有冗余消息)
	while(g_running)
	{
		ret = msgrcv(msgid, (void *)&msghead, sizeof(struct _msgdata), netmsg_type, IPC_NOWAIT);
		if(ret < 0 && errno == ENOMSG)
		{
			break;
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: clear msg chach success, send read msg...\n", FUN, LINE);

	//发送读取指令
	while(g_running)
	{
		msghead.msg_type = netsend_type;
		msghead.msgdata.nettype = 2;
		msghead.msgdata.ntsamp = 0;
	
		ret = msgsnd(msgid, (void *)&msghead, sizeof(struct _msgdata), 0);
		if(ret < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: msgsend fail, ret = %d\n", FUN, LINE, ret);
			
			sleep(5);
			continue;
		}

		break;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: send read msg success\n", FUN, LINE);

	//循环处理消息
	while(g_running)
	{
		memset(&msghead, 0, sizeof(VAVA_NetMsg));
		ret = msgrcv(msgid, (void *)&msghead, sizeof(struct _msgdata), netmsg_type, IPC_NOWAIT);
		if(ret < 0)
		{
			if(errno == ENOMSG || ret == -1)
			{
				sleep(1);
				
				gettimeofday(&msgtime, NULL);
				if(msgtime.tv_sec - msgchange.tv_sec >= 3 && changeflag == 1)
				{
					changeflag = 0;
					
					memset(g_gatewayinfo.ipstr, 0, 16);
					strcpy(g_gatewayinfo.ipstr, "0.0.0.0");
					
					g_gatewayinfo.nettype = savetype;

					if(g_gatewayinfo.nettype == 0)
					{
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --- wird disconnect ---\n", FUN, LINE);
						
						VAVAHAL_SystemCmd("killall udhcpc");
						g_nas_net_check = 0;
						g_internetcheck = 0;
					}
					else if(g_gatewayinfo.nettype == 1)
					{
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: --- wird connect ---\n", FUN, LINE);
						
						VAVAHAL_SystemCmd("udhcpc -i eth0.1");

						VAVAHAL_GetLocalIP(g_gatewayinfo.ipstr);
						g_internetcheck = 1;
						g_nas_net_check = 1;
					}
				}
				
				continue;
			}

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: recv msg fail, ret = %d, errno = %d\n", FUN, LINE, ret, errno);
			break;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: recv new netchange msg, savetype = %d, nettype = %d\n", 
		                                FUN, LINE, savetype, msghead.msgdata.nettype);

		if(msghead.msgdata.nettype == 0)
		{
			g_nas_net_check = 0;
		}

		if(savetype != msghead.msgdata.nettype)
		{
			savetype = msghead.msgdata.nettype;
			gettimeofday(&msgchange, NULL);
			changeflag = 1;
			continue;
		}
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ================ MSG PTH EXIT =================\n", FUN, LINE);

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

void *internetcheck_pth(void *data)
{
	int internet;
	int savetype = -1;
	
	while(g_running)
	{
		if(g_internetflag == 0)
		{
			sleep(1);
			continue;
		}
		
		if(g_internetcheck == 0)
		{
			g_router_link_status = VAVA_NETWORK_LINKFAILD;
			
			if(savetype != VAVA_NETWORK_LINKFAILD)
			{
				if(g_keyparing == 0)
				{
					VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
					VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);
				}

				#if 0
				if(savetype != -1)
				{
					VAVAHAL_PlayAudioFile("/tmp/sound/net_connect_fail.opus");
				}
				#endif

				savetype = VAVA_NETWORK_LINKFAILD;
			}

			sleep(1);
			continue;
		}

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: <-- ** InterNet Check ** -->\n", FUN, LINE);

		internet = VAVAHAL_CheckServerStatus();
		//internet = VAVAHAL_PingStatus();
		if(internet == 0x500)
		{
			g_router_link_status = VAVA_NETWORK_LINKOK;

			if(savetype != VAVA_NETWORK_LINKOK)
			{
				if(g_keyparing == 0)
				{
					VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_LIGHT);
					VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_SLAKE);
				}
				
				savetype = VAVA_NETWORK_LINKOK;
				//VAVAHAL_PlayAudioFile("/tmp/sound/net_connect_success.opus");
			}
		}
		else
		{
			g_router_link_status = VAVA_NETWORK_LINKFAILD;
			
			if(savetype != VAVA_NETWORK_LINKFAILD)
			{
				if(g_keyparing == 0)
				{
					VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);
					VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
				}

				savetype = VAVA_NETWORK_LINKFAILD;
			}
		}

		sleep(10);
		continue;
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

#ifdef WIRLD_ONLY

void *nettypechange_pth(void *data)
{
	int savetype = -1;
	int sockfd;
	struct sockaddr_in *sin;
	struct ifreq if_ra0;
	
	while(g_running)
	{
		sleep(1);
		
		if(savetype != g_gatewayinfo.nettype)
		{
			memset(g_gatewayinfo.ipstr, 0, 16);
			strcpy(g_gatewayinfo.ipstr, "0.0.0.0");

			savetype = g_gatewayinfo.nettype;
			if(savetype == 0)
			{
				VAVAHAL_SystemCmd("killall udhcpc");
			}
			else if(savetype == 1)
			{
				g_router_link_type = 1;
				g_router_link_status = VAVA_NETWORK_LINKING;
				
				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------- [ RJ45 ] --------\n", FUN, LINE);
				
				VAVAHAL_SystemCmd("udhcpc -i eth0.1");

				strcpy(if_ra0.ifr_name, WIRED_NET);	

				sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
				if(sockfd >= 0)
				{
					if(ioctl(sockfd, SIOCGIFADDR, &if_ra0) >= 0)
					{
						sin = (struct sockaddr_in *)&(if_ra0.ifr_addr);
						strcpy(g_gatewayinfo.ipstr, inet_ntoa(sin->sin_addr));

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get ip = %s\n", FUN, LINE, g_gatewayinfo.ipstr);

						if(g_apflag == 0)
						{
							VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_LIGHT);
							VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_SLAKE);
						}
					}
					else
					{
						if(g_apflag == 0)
						{
							VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
							VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_LIGHT);
						}
					}

					close(sockfd);
				}

				g_router_link_status = VAVA_NETWORK_LINKOK;
			}
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

#else

void *nettypechange_pth(void *data)
{
	int savetype = -1;
	int sockfd;
	struct sockaddr_in *sin;
	struct ifreq if_ra0;

	int netstatus;
	int faildcount;
	int firstwifi = 0;

	while(g_running)
	{
		sleep(1);
		
		if(savetype != g_gatewayinfo.nettype)
		{
			memset(g_gatewayinfo.ipstr, 0, 16);
			strcpy(g_gatewayinfo.ipstr, "0.0.0.0");

			savetype = g_gatewayinfo.nettype;
			if(savetype == 0)
			{
				VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
				VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_FAST_FLASH);

				g_router_link_type = 0;
				g_router_link_status = VAVA_NETWORK_LINKING;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------- [ WIFI ] --------\n", FUN, LINE);

				if(firstwifi == 1)
				{
					VAVAHAL_SystemCmd("killall udhcpc");
				}

				faildcount = 0;
				netstatus = 0x300;
				while(g_running)
				{
					netstatus = VAVAHAL_GetWireleseStatus();
					if(netstatus == 0x500)
					{
						break;
					}
					else if(netstatus == 0x300)
					{
						netstatus = VAVAHAL_PingStatus();
						if(netstatus == 0x500)
						{
							break;
						}

						faildcount++;
						if(faildcount >= 10)
						{
							break;
						}
					}

					sleep(1);
				}

				if(netstatus == 0x500)
				{
					strcpy(if_ra0.ifr_name, WIRELESS_NET);	
					VAVAHAL_PlayAudioFile("/tmp/sound/wirelessrouterok.opus");
				}
				else
				{
					continue;
				}
			}
			else
			{
				g_router_link_type = 1;
				g_router_link_status = VAVA_NETWORK_LINKING;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------- [ RJ45 ] --------\n", FUN, LINE);
				
				VAVAHAL_SystemCmd("udhcpc -i eth0.1");

				strcpy(if_ra0.ifr_name, WIRED_NET);	

				VAVAHAL_PlayAudioFile("/tmp/sound/wiredrouterok.opus");
			}

			firstwifi = 1;

			sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
			if(sockfd >= 0)
			{
				if(ioctl(sockfd, SIOCGIFADDR, &if_ra0) >= 0)
				{
					sin = (struct sockaddr_in *)&(if_ra0.ifr_addr);
					strcpy(g_gatewayinfo.ipstr, inet_ntoa(sin->sin_addr));

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get ip = %s\n", FUN, LINE, g_gatewayinfo.ipstr);

					if(g_apflag == 0)
					{
						VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_LIGHT);
						VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_SLAKE);
					}
				}
				else
				{
					if(g_apflag == 0)
					{
						VAVAHAL_LedCtrl(VAVA_LED_WHITE, VAVA_LED_SLAKE);
						VAVAHAL_LedCtrl(VAVA_LED_RED, VAVA_LED_LIGHT);
					}
				}

				close(sockfd);
			}

			g_router_link_status = VAVA_NETWORK_LINKOK;
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}

#endif

void internetcheck_start()
{
	g_internetflag = 1;
}

void internetcheck_stop()
{
	g_internetflag = 0;
}


#include "basetype.h"
#include "vavahal.h"
#include "watchdog.h"
#include "apconfig.h"

void *Apclient_pth(void *data)
{
	int ret;
	int clientfd;
	fd_set rdRd;
	struct timeval timeout;

	struct ac_head head;
	struct ac_set setdata;
	struct ac_resp respdata;

	char getbuff[1024];
	char ssid[32];
	char pass[32];

	clientfd = *(int *)data;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: apclient fd = %d\n", FUN, LINE, clientfd);

	while(g_running)
	{
		FD_ZERO(&rdRd);
		FD_SET(clientfd, &rdRd);

		timeout.tv_sec = 0;				
		timeout.tv_usec = 500000;

		ret = select(clientfd + 1, &rdRd, NULL, NULL, &timeout);
		if(ret < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err\n", FUN, LINE);
		}
		else if(ret == 0)
		{
			//timeout
			continue;
		}
		else
		{
			if(FD_ISSET(clientfd, &rdRd))
			{
				memset(getbuff, 0, 1024);
				ret = recv(clientfd, getbuff, 1024, 0);
				if(ret > 0)
				{
					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: recv ret = %d\n", FUN, LINE, ret);
					
					if(ret < sizeof(struct ac_head))
					{
						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: not recive enough data( <head )\n", FUN, LINE);
						continue;
					}
					
					memcpy(&head, getbuff, sizeof(struct ac_head));

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: new message [0x%x][0x%x][%d]\n", 
						                            FUN, LINE, head.tag, head.cmd, head.lenght);

					if(head.tag != VAVA_TAG_AP)
					{
						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: message tag err, tag = 0x%x\n", FUN, LINE, head.tag);
						continue;
					}
					
					if(ret < sizeof(struct ac_head) + head.lenght)
					{
						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: not recive enough data( <data)\n", FUN, LINE);
						continue;
					}

					if(head.cmd == 0x101)
					{
						memset(&setdata, 0, sizeof(struct ac_set));
						memcpy(&setdata, getbuff + sizeof(struct ac_head), sizeof(struct ac_set));

						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ssid - [%s], pass - [%s]\n", FUN, LINE, setdata.ssid, setdata.pass);

						memset(ssid, 0, 32);
						memset(pass, 0, 32);
						strcpy(ssid, setdata.ssid);
						strcpy(pass, setdata.pass);

						head.result = 0;
						head.cmd = 0x101;
						head.lenght = sizeof(struct ac_resp);

						memset(&respdata, 0, sizeof(struct ac_resp));
						strcpy(respdata.sn, g_gatewayinfo.sn);
						
						memset(getbuff, 0, 1024);
						memcpy(getbuff, &head, sizeof(struct ac_head));
						memcpy(getbuff + sizeof(struct ac_head), &respdata, sizeof(struct ac_resp));

						ret = send(clientfd, getbuff, sizeof(struct ac_head) + sizeof(struct ac_resp), 0);
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
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: send resp success, ret = %d\n", FUN, LINE, ret);
						}
					}
					else if(head.cmd == 0x102)
					{
						VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: recv apconfig ack\n", FUN, LINE);
						
						//关闭AP配网
						VAVAHAL_CloseAPNetwork();

						//联网
						VAVAHAL_ConnectAp(ssid, pass);
						g_apflag = 0;

						//恢复灯状态
						VAVAHAL_ResetLedStatus();
					}
				}
				else if(ret == 0)
				{
					break;
				}
				else
				{
					if(errno == EAGAIN)
					{
						continue;
					}

					break;
				}
			}
		}
	}

	close(clientfd);

	return NULL;
}

void *apservers_pth(void *data)
{
	int sockid = -1;
	int port = SERVER_AP_PORT;
	int acceptid;
	int clientid;
	size_t addr_len;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	struct sockaddr_in *sin;

	int ret;
	int reuse = 1;
	struct ifreq if_ra0;

	pthread_t pth_id;
	pthread_attr_t attr;

	fd_set rdRd;
	struct timeval timeout;

	strcpy(if_ra0.ifr_name, BING_NET);

	while(g_running)
	{
		sockid = socket(PF_INET, SOCK_STREAM, 0);  //创建socket
		if(sockid < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: socket err, sockid = %d\n", FUN, LINE, sockid);
			
			sleep(2);
			continue;
		}

		ret = setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
		if(ret < 0)
		{
			close(sockid);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: SO_REUSEADDR err\n", FUN, LINE);

			sleep(2);
			continue;
		}

		ret = setsockopt(sockid, SOL_SOCKET, SO_BINDTODEVICE, (char *)&if_ra0, sizeof(if_ra0));
		if(ret < 0)
		{
			close(sockid);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: SO_BINDTODEVICE err\n", FUN, LINE);
			
			sleep(2);
			continue;
		}

		memset(&server_addr, 0x0, sizeof(struct sockaddr_in));
		if(ioctl(sockid, SIOCGIFADDR, &if_ra0) < 0)
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
		if(bind(sockid, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0)
		{
			close(sockid);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: bind err\n", FUN, LINE);
			
			sleep(2);
			continue;
		}

		//监听端口
		if(listen(sockid, 10) < 0)
		{
			close(sockid);

			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: listen err\n", FUN, LINE);
			
			sleep(2);
			continue;
		}

		while(g_running)
		{
			FD_ZERO(&rdRd);
			FD_SET(sockid, &rdRd);

			timeout.tv_sec = 0;				
			timeout.tv_usec = 500000;

			ret = select(sockid + 1, &rdRd, NULL, NULL, &timeout);
			if(ret < 0)
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err, ret = %d\n", FUN, LINE, ret);
			}
			else if(ret == 0) //timeout
			{
				continue; 
			}
			else
			{
				if(FD_ISSET(sockid, &rdRd))
				{
					addr_len = sizeof(struct sockaddr_in);
					acceptid = accept(sockid, (struct sockaddr*)&client_addr, &addr_len);
					if(acceptid < 0)
					{
						continue;
					}

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: new apconfig client connect, acceptid = %d\n", FUN, LINE, acceptid);
					
					clientid = acceptid;

					pthread_attr_init(&attr);
					pthread_attr_setdetachstate(&attr, 1);
					pthread_attr_setstacksize(&attr, STACK_SIZE);
					ret = pthread_create(&pth_id, &attr, Apclient_pth, &clientid);
					if(ret != 0)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: apconfig client create err, ret = %d\n", FUN, LINE, ret);
					}
					pthread_attr_destroy(&attr);
				}
			}
		}
	}

	//线程异常退出由看门狗重启
	WatchDog_Stop(__FUNCTION__);

	return NULL;
}


#include "basetype.h"
#include "vavahal.h"
#include "usboverload.h"

#define NETLINK_USER 	22
#define USER_MSG    	(NETLINK_USER + 1)
#define MSG_LEN 		100

struct usb_check_msg{
    struct nlmsghdr hdr;
    char data[MSG_LEN];
};

void *UsbOverLoadCheck_pth(void *data)
{
	int skfd;
	struct sockaddr_nl local;
	struct sockaddr_nl dest_addr;
	struct usb_check_msg info;

	int ret;
	int flags;
	socklen_t dest_addr_len;

	fd_set rd_set;
	struct timeval timeout;

	int testnum = 0;

	while(g_running)
	{
		skfd = socket(AF_NETLINK, SOCK_RAW, USER_MSG);
	    if(skfd == -1)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: create socket error\n", FUN, LINE);

			sleep(10);
			continue;
	    }

		local.nl_family = AF_NETLINK;
	    local.nl_pid = 50; 
	    local.nl_groups = 0;

		if(bind(skfd, (struct sockaddr *)&local, sizeof(local)) != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: bind error\n", FUN, LINE);
			
	        close(skfd);

			sleep(10);
			continue;
    	}

		flags = fcntl(skfd, F_GETFL, 0);  
	    fcntl(skfd, F_SETFL, flags|O_NONBLOCK); 
		
	    memset(&dest_addr, 0, sizeof(dest_addr));
	    dest_addr.nl_family = AF_NETLINK;
	    dest_addr.nl_pid = 0; // to kernel
	    dest_addr.nl_groups = 0;
		dest_addr_len = sizeof(dest_addr); 

		while(g_running)
		{
			FD_ZERO(&rd_set);
			FD_SET(skfd, &rd_set);
			
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			
			ret = select(skfd + 1, &rd_set, NULL, NULL, &timeout);
			if(ret < 0)
			{
				VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err\n", FUN, LINE);
			}
			else if(ret == 0)
			{
				testnum = 0;
				continue;
			}
			else
			{
				if(FD_ISSET(skfd, &rd_set))
				{
					ret = recvfrom(skfd, &info, sizeof(struct usb_check_msg), 0, (struct sockaddr *)&dest_addr, &dest_addr_len);
					if(ret == -1)
					{
						VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: recv data err\n", FUN, LINE);
						break;
					}

					VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: -------> Usb OverLoad Check:%s <-------\n", FUN, LINE, info.data);

					if(testnum++ % 16 == 0)
					{
						VAVAHAL_PlayAudioFile("/tmp/sound/usboverload.opus");
						testnum = 1;
					}
				}
			}
		}

		close(skfd);

		sleep(10);
		continue;
	}

	return NULL;
}

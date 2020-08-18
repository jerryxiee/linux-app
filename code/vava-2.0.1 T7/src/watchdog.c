#include "basetype.h"
#include "vavahal.h"
#include "errlog.h"
#include "watchdog.h"

static int g_watchdog_flag = 1;

int WatchDog_Report()
{
	VAVAHAL_SystemCmd("echo 1 > /tmp/Guard_main");
	return 0;
}

void *WatchDog_pth(void *data)
{
	WatchDog_Report();
	
	while(g_running && g_watchdog_flag)
	{
		//每10秒喂一次
		sleep(10);
		WatchDog_Report();
	}

	Err_Log("Restart with wathcdog");
	VAVAHAL_SystemCmd("sync");

	return NULL;
}

int WatchDog_Stop(const char *pthstr)
{
	char str[128];

	if(g_update.status != VAVA_UPDATE_IDLE)
	{
		return 0;
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: pth [%s] exit\n", FUN, LINE, pthstr);

	if(g_exitflag == 0)
	{
		memset(str, 0, 128);
		sprintf(str, "[%s], stop", pthstr);
		Err_Log(str);
	}
	
	g_watchdog_flag = 0;
	return 0;
}


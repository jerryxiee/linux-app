#include "basetype.h"
#include "vavahal.h"
#include "errlog.h"

int Err_Log(char *str)
{
	char cmd[256];
	time_t t_time;
	struct tm *t_info;

	if(str == NULL)
	{
		return 0;
	}

	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD && g_gatewayinfo.totol > 100)
	{
		time(&t_time);
		t_info = localtime(&t_time);

		memset(cmd, 0, 256);
		sprintf(cmd, "echo %d%02d%02d_%02d:%02d:%02d %s >> /mnt/sd0/%s/%s",
			         t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday,
			         t_info->tm_hour, t_info->tm_min, t_info->tm_sec, str, 
			         g_gatewayinfo.sn, LOG_FILE);
		VAVAHAL_SystemCmd(cmd);
	}

	return 0;
}

int Err_Info(char *str)
{
	char cmd[256];
	time_t t_time;
	struct tm *t_info;

	if(str == NULL)
	{
		return 0;
	}

	time(&t_time);
	t_info = localtime(&t_time);

	memset(cmd, 0, 256);
	sprintf(cmd, "echo %d%02d%02d_%02d:%02d:%02d %s >> %s",
		         t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday,
		         t_info->tm_hour, t_info->tm_min, t_info->tm_sec, str, LOG_MEM);
	VAVAHAL_SystemCmd(cmd);

	return 0;
}

#include "basetype.h"
#include "errlog.h"
#include "rfserver.h"
#include "vavahal.h"
#include "camerapair.h"

void *camerapair_pth(void *data)
{
	int ret;
	int channel;
	int timeout;
	unsigned int sessionid;
#ifdef FREQUENCY_OFFSET
	unsigned int index;
#endif
	unsigned int addr;
	VAVA_CameraPair *camerapair = NULL;

	camerapair = (VAVA_CameraPair *)data;
	sessionid = camerapair->sessionid;
	addr = camerapair->addr;
#ifdef FREQUENCY_OFFSET
	index = camerapair->index;
#endif
	channel = camerapair->channel;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ============== PAIR PTH START ==============\n", FUN, LINE);

	if(channel < 0 || channel >= MAX_CHANNEL_NUM)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: input channel err, channel = %d\n", FUN, LINE, channel);

		Err_Log("pair channel err");
		
		return NULL;
	}
	
	timeout = 3;
	
	while(g_running)
	{
		if(timeout-- <= 0)
		{
			g_pair[channel].lock = 0;
			g_keyparing = 0;

			if(sessionid != -1)
			{
				VAVAHAL_SendPairResp(sessionid, VAVA_ERR_CODE_PAIR_FAIL);
			}

			if(g_pairmode == 1)
			{
				g_pairmode = 0; 
				VAVAHAL_PlayAudioFile("/tmp/sound/sync_fail.opus");
			}
			
			VAVAHAL_ResetLedStatus();
			return NULL;
		}

#ifdef FREQUENCY_OFFSET
		ret = VAVAHAL_PairReq(channel, addr, index);
#else	
		ret = VAVAHAL_PairReq(channel, addr);
#endif
		if(ret == VAVA_ERR_CODE_SUCCESS)
		{
			break;
		}
	}

	timeout = 60;

	g_pair_result = 1;
	while(g_running)
	{
		if(timeout-- <= 0)
		{
			if(g_pairmode == 1)
			{
				g_pairmode = 0; 
				VAVAHAL_PlayAudioFile("/tmp/sound/sync_timeout.opus");
			}
			
			break;
		}

		if(g_pair[channel].nstat == 1)
		{
			if(sessionid != -1)
			{
				VAVAHAL_SendPairResp(sessionid, VAVA_ERR_CODE_SUCCESS);
			}

			return NULL;
		}

		if(g_pair_result == 0)
		{
			if(sessionid != -1)
			{
				VAVAHAL_SendPairResp(sessionid, VAVA_ERR_CODE_PAIR_FAIL);
			}
			
			g_wakeup_withpair_addr = 0xFFFFFFFF;
			g_pair_wifi_addr = 0xFFFFFFFF;
			
			return NULL;
		}

		sleep(1);
	}

	//超时退出
	g_pair[channel].lock = 0;
	g_keyparing = 0;
	g_pair_result = 0;
	VAVAHAL_ResetLedStatus();

	if(sessionid != -1)
	{
		VAVAHAL_SendPairResp(sessionid, VAVA_ERR_CODE_PAIR_TIMEOUT);
	}

	return NULL;	
}


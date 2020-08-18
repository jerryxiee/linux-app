#include "basetype.h"
#include "cjson.h"
#include "errlog.h"
#include "crypto.h"
#include "update.h"
#include "vavahal.h"
#include "watchdog.h"
#include "vavaserver.h"

#define SERVER_LANGUAGE					"en"

#define SERVER_RESULT_SUCCESS			200
#define SERVER_RESULT_PARAM_ERR			400
#define SERVER_RESULT_TOKEN_NOAUTH		401
#define SERVER_RESULT_SYSERR			500

#define SERVER_RESULT_NOUSER			216002
#define SERVER_RESULT_BINDBYSELF		216003
#define SERVER_RESULT_NOBIND			216005

#if 0
static pthread_mutex_t *lockarray;
#endif

int g_download_flag = 0;
int g_token_timeout = 0;

static unsigned char g_token_flag = 0;
static unsigned char g_sydid_flag = 0;
static unsigned char g_key_flag = 0;
static unsigned char g_push_flag = 0;
static unsigned char g_push_cloudup_flag = 0;
static unsigned char g_cloudstatus_flag = 0;
static unsigned char g_addstation_flag = 0;
static unsigned char g_cloudup_flag = 0;
static unsigned char g_serverlink_flag = 0;
static unsigned char g_pairsync_flag = 0;
static unsigned char g_addcamera_blind_flag = 0;
static unsigned char g_addcamera_flag = 0;
static unsigned char g_bsattr_flag = 0;
static unsigned char g_cameraattr_flag = 0;
static unsigned char g_statussycn_flag = 0;

static unsigned int g_station_locktime = 0;

#ifdef ALARM_PHOTO_IOT
static unsigned char g_imgname_flag = 0;
#endif

static char g_cloudtoken[1024];

#ifdef ALARM_PHOTO_IOT
static char g_tmpimgname[256];
#endif

#if 0
static void lock_callback(int mode, int type, char *file, int line)
{
	(void)file;
	(void)line;
	
	if(mode & CRYPTO_LOCK) 
	{
		pthread_mutex_lock(&(lockarray[type]));
	}
	else
	{
		pthread_mutex_unlock(&(lockarray[type]));
	}
}

static unsigned long thread_id(void)
{
	unsigned long ret;

	ret = (unsigned long)pthread_self();
	return ret;
}

int VAVASERVER_InitLocks()
{
	int i;
	
	/* Must initialize libcurl before any threads are started */ 
  	curl_global_init(CURL_GLOBAL_ALL);

	lockarray = (pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));

	for(i = 0; i < CRYPTO_num_locks(); i++) 
	{
	    pthread_mutex_init(&(lockarray[i]), NULL);
	}
 
	CRYPTO_set_id_callback((unsigned long (*)())thread_id);
	CRYPTO_set_locking_callback((void (*)())lock_callback);

	return 0;
}

void VAVASERVER_Killlocks(void)
{
	int i;
 
	CRYPTO_set_locking_callback(NULL);
	
	for(i = 0; i < CRYPTO_num_locks(); i++)
	{
		pthread_mutex_destroy(&(lockarray[i]));
	}

	OPENSSL_free(lockarray);
}
#endif

size_t token_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pData = NULL;
	cJSON *pAccessToken = NULL;
	cJSON *pExpiresIn = NULL;

	int code;
	int exin;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_PARAM_ERR)
			{
				g_token_flag = 2;
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pData = cJSON_GetObjectItem(pRoot, "data");
		if(pData == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find data\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pAccessToken = cJSON_GetObjectItem(pData, "access_token");
		if(pAccessToken == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find access_token\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pExpiresIn = cJSON_GetObjectItem(pData, "expires_in");
		if(pExpiresIn != NULL)
		{
			exin = pExpiresIn->valueint;
			if(exin >= 600)
			{
				g_token_timeout = exin;
			}
		}

		memset(g_gatewayinfo.token, 0, 64);
		strcpy(g_gatewayinfo.token, pAccessToken->valuestring);
		g_token_flag = 1;

		cJSON_Delete(pRoot);
	}
	
	return size * nmemb;     //必须返回这个大小, 否则只回调一次, 不清楚为何.
}

size_t did_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pData = NULL;
	cJSON *pSyDid = NULL;
	cJSON *pInitString = NULL;
	cJSON *pCrcKey = NULL;
	char tmpdid[32];

	char *tok = NULL;
	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_PARAM_ERR)
			{
				g_sydid_flag = 2;
			}
			else if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pData = cJSON_GetObjectItem(pRoot, "data");
		if(pData == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find data\n", FUN, LINE);

			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pSyDid = cJSON_GetObjectItem(pData, "didCode");
		if(pSyDid == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find syDid\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		memset(tmpdid, 0, 32);
		strcpy(tmpdid, pSyDid->valuestring);

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get sydid = %s\n", FUN, LINE, tmpdid);

		tok = strtok(tmpdid, ",");

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: tok = %s\n", FUN, LINE, tok);

		memset(g_gatewayinfo.sydid, 0, 32);
		strcpy(g_gatewayinfo.sydid, tok);

		tok = strtok( NULL, ",");

		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: tok = %s\n", FUN, LINE, tok);

		memset(g_gatewayinfo.apilisence, 0, 16);
		strcpy(g_gatewayinfo.apilisence, tok);

		pInitString = cJSON_GetObjectItem(pData, "initCode");
		if(pInitString == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find initString\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		memset(g_gatewayinfo.initstr, 0, 128);
		strcpy(g_gatewayinfo.initstr, pInitString->valuestring);

		pCrcKey = cJSON_GetObjectItem(pData, "crcKey");
		if(pCrcKey == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find crcKey\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		memset(g_gatewayinfo.crckey, 0, 16);
		strcpy(g_gatewayinfo.crckey, pCrcKey->valuestring);
		
		cJSON_Delete(pRoot);

		g_sydid_flag = 1;
	}

	return size * nmemb;     //必须返回这个大小, 否则只回调一次, 不清楚为何.
}

size_t key_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;

	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);
			
			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		g_key_flag = 1;
		cJSON_Delete(pRoot);
	}

	return size * nmemb;     //必须返回这个大小, 否则只回调一次, 不清楚为何.
}

size_t push_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pCloudData = NULL;
	cJSON *pCloudUpFlag = NULL;

	int code;
	int upflag;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		g_push_flag = 1;

		pCloudData = cJSON_GetObjectItem(pRoot, "data");
		if(pCloudData != NULL)
		{
			pCloudUpFlag = cJSON_GetObjectItem(pCloudData, "cloudSpaceUpdatedFlag");
			if(pCloudUpFlag != NULL)
			{
				upflag = atoi(pCloudUpFlag->valuestring);
				if(upflag == 1)
				{
					g_push_cloudup_flag = 1;
				}
			}
		}
		
		cJSON_Delete(pRoot);
	}

	return size * nmemb;     //必须返回这个大小, 否则只回调一次, 不清楚为何.
}

size_t update_callback(void *ptr, size_t size, size_t nmemb, FILE *stream)
{  
	int ret;

	ret = update_write((char *)ptr, size * nmemb);
	if(ret != 0)
	{
		g_update.status = VAVA_UPDATE_LOAD_ERR;
		VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, 0, g_update.current);
	}
	
	return size * nmemb;
}

int loading_callback(char *progress_data, double t, double d, double ultotal, double ulnow)  
{  
	int loading = (int)(d*100.0/t);
	int tmp;

	tmp = g_update.loading;
	g_update.loading = loading;

	if((tmp != loading) && (loading % 10 == 0))
	{
		VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, g_update.loading, g_update.current); 
	}
	
	return 0;  
}

size_t addstation_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pData = NULL;
	cJSON *pLockSecond = NULL;
	
	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pData = cJSON_GetObjectItem(pRoot, "data");
		if(pData == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find data\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pLockSecond = cJSON_GetObjectItem(pData, "lockSeconds");
		if(pLockSecond != NULL)
		{
			g_station_locktime = pLockSecond->valueint;
		}

		cJSON_Delete(pRoot);

		g_addstation_flag = 1;
	}

	return size * nmemb;     //必须返回这个大小, 否则只回调一次, 不清楚为何.
}

size_t cloudstatus_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pCloudData = NULL;
	cJSON *pCloudToken = NULL;

	char *statecode;
	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		
		statecode = pStateCode->valuestring;
		code = atoi(statecode);
		if(code != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == 40002 || code == 200001 || code == 40000)
			{
				if(code == 40002)
				{
					g_token_timeout = 0; //token超时
				}
				
				g_cloudstatus_flag = 2;
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pCloudData = cJSON_GetObjectItem(pRoot, "data");
		if(pCloudData == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get data fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pCloudToken = cJSON_GetObjectItem(pCloudData, "token");
		if(pCloudToken == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get cloud token fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		
		strcpy(g_cloudtoken, pCloudToken->valuestring);

		cJSON_Delete(pRoot);

		g_cloudstatus_flag = 1;
	}

	return size * nmemb;     //必须返回这个大小, 否则只回调一次, 不清楚为何.
}

size_t cloudupflag_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pCloudUpData = NULL;
	cJSON *pCloudUpflag = NULL;

	char *statecode;
	int code;
	int upflag;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		
		statecode = pStateCode->valuestring;
		code = atoi(statecode);
		if(code != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == 40002 || code == 200001 || code == 40000)
			{
				if(code == 40002)
				{
					g_token_timeout = 0; //token超时
				}
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pCloudUpData = cJSON_GetObjectItem(pRoot, "data");
		if(pCloudUpData == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get data fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pCloudUpflag = cJSON_GetObjectItem(pCloudUpData, "cloudSpaceUpdatedFlags");
		if(pCloudUpflag == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get cloud up flag fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		upflag = atoi(pCloudUpflag->valuestring);
		if(upflag == 1)
		{
			g_cloudup_flag = 1;
		}

		cJSON_Delete(pRoot);
	}

	return size * nmemb;     //必须返回这个大小, 否则只回调一次, 不清楚为何.
}

size_t pairsync_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pData = NULL;
	cJSON *pIntervalSec = NULL;
	cJSON *pCameraList = NULL;
	cJSON *pCameraItem = NULL;
	cJSON *pChannel = NULL;
	cJSON *pSn = NULL;
	
	int intervalsec;
	int listnum;
	int i;
	int code;
	int channel;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		g_pairsync[i].nstat = 0;
		memset(g_pairsync[i].sn, 0, 32);
	}

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pData = cJSON_GetObjectItem(pRoot, "data");
		if(pData == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find data\n", FUN, LINE);

			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pIntervalSec = cJSON_GetObjectItem(pData, "intervalSeconds");
		if(pIntervalSec == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find pIntervalSec\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		intervalsec = pIntervalSec->valueint;

		pCameraList = cJSON_GetObjectItem(pData, "cameraList");
		if(pCameraList == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find cameraList\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		listnum = cJSON_GetArraySize(pCameraList);
		for(i = 0; i < listnum; i++)
		{
			pCameraItem = cJSON_GetArrayItem(pCameraList, i);
			if(pCameraItem == NULL)
			{
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find cameraitem\n", FUN, LINE);
			
				cJSON_Delete(pRoot);
				return size * nmemb;
			}

			pChannel = cJSON_GetObjectItem(pCameraItem, "channel");
			if(pChannel == NULL)
			{
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find channel\n", FUN, LINE);
				cJSON_Delete(pRoot);
				return size * nmemb;
			}

			channel = pChannel->valueint;
			if(channel < 0 || channel >= MAX_CHANNEL_NUM)
			{
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: channel chekck fail, channel = %d\n", FUN, LINE, channel);
				cJSON_Delete(pRoot);
				return size * nmemb;
			}

			pSn = cJSON_GetObjectItem(pCameraItem, "cameraSn");
			if(pSn == NULL)
			{
				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find cameraSn\n", FUN, LINE);
			
				cJSON_Delete(pRoot);
				return size * nmemb;
			}

			g_pairsync[channel].nstat = 1;
			strcpy(g_pairsync[channel].sn, pSn->valuestring);
		}

		cJSON_Delete(pRoot);

		g_pair_sync_time = intervalsec;
		g_pairsync_flag = 1;
	}

	return size * nmemb;     //必须返回这个大小, 否则只回调一次, 不清楚为何.
}

size_t addcamera_blind_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;

	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			else if(code == SERVER_RESULT_NOUSER || code == SERVER_RESULT_BINDBYSELF)
			{
				g_addcamera_blind_flag = 1;
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		cJSON_Delete(pRoot);

		g_addcamera_blind_flag = 1;
	}

	return size * nmemb;
}

size_t addcamera_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;

	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			else if(code == SERVER_RESULT_BINDBYSELF)
			{
				g_addcamera_flag = 1;
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		cJSON_Delete(pRoot);

		g_addcamera_flag = 1;
	}

	return size * nmemb;
}

size_t updatebsattr_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;

	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			else if(code == SERVER_RESULT_NOBIND)
			{
				g_bsattr_flag = 1;
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		cJSON_Delete(pRoot);

		g_bsattr_flag = 1;
	}

	return size * nmemb;
}

size_t updateipcattr_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;

	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		cJSON_Delete(pRoot);

		g_cameraattr_flag = 1;
	}

	return size * nmemb;
}

size_t updatestatus_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pData = NULL;
	cJSON *pIntervalSec = NULL;
	cJSON *pIntervalSecSlow = NULL;

	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}
		code = pStateCode->valueint;
		if(code != SERVER_RESULT_SUCCESS)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == SERVER_RESULT_TOKEN_NOAUTH)
			{
				g_token_timeout = 0; //token超时
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pData = cJSON_GetObjectItem(pRoot, "data");
		if(pData == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find data\n", FUN, LINE);

			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pIntervalSec = cJSON_GetObjectItem(pData, "intervalSeconds");
		if(pIntervalSec == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find pIntervalSec\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		g_status_sync_time = pIntervalSec->valueint;

		pIntervalSecSlow = cJSON_GetObjectItem(pData, "slowIntervalSeconds");
		if(pIntervalSecSlow == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: not find slowIntervalSeconds\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		g_status_sync_time_slow = pIntervalSecSlow->valueint;

		cJSON_Delete(pRoot);

		g_statussycn_flag = 1;
	}

	return size * nmemb;
}

#ifdef ALARM_PHOTO_IOT
static size_t photo_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{   
	cJSON *pRoot = NULL;
	cJSON *pStateCode = NULL;
	cJSON *pData = NULL;
	cJSON *pImagName = NULL;

	char *statecode;
	int code;
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	pRoot = cJSON_Parse((char *)ptr);
	if(pRoot != NULL)
	{
		pStateCode = cJSON_GetObjectItem(pRoot, "stateCode");
		if(pStateCode == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get stateCode fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		statecode = pStateCode->valuestring;
		code = atoi(statecode);
		if(code != 0)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: post err, statecode = %d\n", FUN, LINE, code);

			if(code == 40002 || code == 200001)
			{
				if(code == 40002)
				{
					g_token_timeout = 0; //token超时
				}
			}
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pData = cJSON_GetObjectItem(pRoot, "data");
		if(pData == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get data fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		pImagName = cJSON_GetObjectItem(pData, "imageName");
		if(pImagName == NULL)
		{
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: get imgname fail\n", FUN, LINE);
			
			cJSON_Delete(pRoot);
			return size * nmemb;
		}

		memset(g_tmpimgname, 0, 256);
		strcpy(g_tmpimgname, pImagName->valuestring);

		cJSON_Delete(pRoot);

		g_imgname_flag = 1;
	}
	
	return size * nmemb;
}
#endif

size_t serverlink_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	char buff[2048];

	memset(buff, 0, 2048);
	VAVAHAL_PrintUnformatted((char *)ptr, buff);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: size = %d\n", FUN, LINE, size * nmemb);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, buff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: *******************************************\n", FUN, LINE);

	g_serverlink_flag = 1;
	
	return size * nmemb;
}

int VAVASERVER_GetSyDid()
{
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;

	int ret;
	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	g_sydid_flag = 0;

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/p2p/get-did?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/p2p/get-did?access_token=%s&timestamp=%lld&lang=%s",
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}

	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, "");
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, did_callback);
    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	if(g_sydid_flag == 1)
	{
		return 0;
	}
	else if(g_sydid_flag == 2)
	{
		return 1;
	}

	return -1;
}

int VAVASERVER_GetToken()
{
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	int ret;

	if(g_token_timeout > 1000)
	{
		return 0;
	}

	g_token_flag = 0;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/oauth/login", g_gatewayinfo.domain_user);
	}
	else
	{
		sprintf(url, "https://%s/oauth/login", g_gatewayinfo.domain_factory);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	#if 0
	cJSON_AddStringToObject(pJsonRoot, "client_id", VAVA_GATEWAY_F_CODE);
	cJSON_AddStringToObject(pJsonRoot, "client_secret", VAVA_GATEWAY_F_SECRET);
	#else
	cJSON_AddStringToObject(pJsonRoot, "client_id", TEST_CLIENT_ID);
	cJSON_AddStringToObject(pJsonRoot, "client_secret", TEST_CLIENT_SECRECT);
	#endif
	cJSON_AddStringToObject(pJsonRoot, "scope", "all");
	cJSON_AddStringToObject(pJsonRoot, "grant_type", "password");
	cJSON_AddStringToObject(pJsonRoot, "sn", g_gatewayinfo.sn);
	cJSON_AddStringToObject(pJsonRoot, "auth_type", "sn_password");

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}
	
	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, token_callback);
    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);

	if(g_token_flag == 1)
	{
		return 0;
	}
	else if(g_token_flag == 2)
	{
		return 1;
	}

	return -1;
}

int VAVASERVER_AuthKey(char *key)
{
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	int ret;
	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	g_key_flag = 0;

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		//APP更改接口
		sprintf(url, "https://%s/ipc/p2p/check-session-key?access_token=%s&timestamp=%lld&lang=%s", 
		              g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		//APP更改接口
		sprintf(url, "https://%s/ipc/p2p/check-session-key?access_token=%s&timestamp=%lld&lang=%s", 
		              g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "sessionKey", key);

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}
	
	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, key_callback);
    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;
	
	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);

	if(g_key_flag == 1) //正常获取
	{
		return 0;
	}

	return -1;
}

void *DownloadTimeOut_pth(void *data)
{
	int timeout = 10;
	int saveload = 0;
	
	g_download_flag = 1;

	while(g_download_flag)
	{
		sleep(1);

		if(saveload != g_update.loading)
		{
			timeout = 10;
			saveload = g_update.loading;
		}
		else
		{
			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: download timeout\n", FUN, LINE, timeout);
			
			if(timeout-- <= 0)
			{
				g_update.status = VAVA_UPDATE_LOAD_ERR;
				VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, g_update.loading, g_update.current);

				//开启看门狗
				VAVAHAL_SystemCmd("reboot");
				break;
			}
		}
	}

	return NULL;
}

int VAVASERVER_DownLoad()
{
	int ret;
	CURL *m_curl = NULL;
	char *progress_data = "update: ";

	pthread_t pth_id;
	pthread_attr_t attr; 

	pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, 1);
	pthread_attr_setstacksize(&attr, STACK_SIZE);
	ret = pthread_create(&pth_id, &attr, DownloadTimeOut_pth, NULL);
	pthread_attr_destroy(&attr);
	
	g_update.status = VAVA_UPDATE_LOADING;
	VAVAHAL_SendUpStats(g_update.sessionid, g_update.status, g_update.loading, g_update.current);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);

		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}
	
	curl_easy_setopt(m_curl, CURLOPT_URL, g_update.url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, NULL);  
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, update_callback);  
	curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0);  
	curl_easy_setopt(m_curl, CURLOPT_PROGRESSFUNCTION, loading_callback);  
	curl_easy_setopt(m_curl, CURLOPT_PROGRESSDATA, progress_data); 
	ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	g_download_flag = 0;

	return ret;
}

int VAVASERVER_GetCloudStatus(int channel, char *cloudtoken)
{
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;
	
	int ret;
	time_t tt;
	struct tm *tt_info;

	return -1;

	g_cloudstatus_flag = 0;

	memset(g_cloudtoken, 0, 1024);
	
	time(&tt);
	tt_info = localtime(&tt);
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/mi/ipc/cloud/token/station?accessToken=%s&timeStamp=%d%02d%02d%02d%02d%02d&clientType=B998&clientChannel=D998&clientVer=%s&deviceType=P02",
					 g_gatewayinfo.domain_user, g_gatewayinfo.token, tt_info->tm_year + 1900, tt_info->tm_mon + 1, tt_info->tm_mday, tt_info->tm_hour, tt_info->tm_min, 
					 tt_info->tm_sec, VAVA_GATEWAY_F_APPVER_IN);
	}
	else
	{
		sprintf(url, "https://%s/mi/ipc/cloud/token/station?accessToken=%s&timeStamp=%d%02d%02d%02d%02d%02d&clientType=B998&clientChannel=D998&clientVer=%s&deviceType=P02",
					 g_gatewayinfo.domain_factory, g_gatewayinfo.token, tt_info->tm_year + 1900, tt_info->tm_mon + 1, tt_info->tm_mday, tt_info->tm_hour, tt_info->tm_min, 
					 tt_info->tm_sec, VAVA_GATEWAY_F_APPVER_IN);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "cameraSn", g_pair[channel].id);
	cJSON_AddStringToObject(pJsonRoot, "stationSn", g_gatewayinfo.sn);
	
	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}
	
	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);

	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, cloudstatus_callback);
    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;
	
	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);

	if(g_cloudstatus_flag == 1)
	{
		strcpy(cloudtoken, g_cloudtoken);
		return 0;
	}
	else if(g_cloudstatus_flag == 2)
	{
		return 1;
	}
		
	return -1;
}

int VAVASERVER_GetCloudUpFlag(int channel)
{
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;
	
	int ret;
	time_t tt;
	struct tm *tt_info;

	return -1;

	g_cloudup_flag = 0;

	time(&tt);
	tt_info = localtime(&tt);
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/mi/ipc/cloud/space/get/updated-flag?accessToken=%s&timeStamp=%d%02d%02d%02d%02d%02d&clientType=B998&clientChannel=D998&clientVer=%s&deviceType=P02",
					 g_gatewayinfo.domain_user, g_gatewayinfo.token, tt_info->tm_year + 1900, tt_info->tm_mon + 1, tt_info->tm_mday, tt_info->tm_hour, tt_info->tm_min, 
					 tt_info->tm_sec, VAVA_GATEWAY_F_APPVER_IN);
	}
	else
	{
		sprintf(url, "https://%s/mi/ipc/cloud/space/get/updated-flag?accessToken=%s&timeStamp=%d%02d%02d%02d%02d%02d&clientType=B998&clientChannel=D998&clientVer=%s&deviceType=P02",
					 g_gatewayinfo.domain_factory, g_gatewayinfo.token, tt_info->tm_year + 1900, tt_info->tm_mon + 1, tt_info->tm_mday, tt_info->tm_hour, tt_info->tm_min, 
					 tt_info->tm_sec, VAVA_GATEWAY_F_APPVER_IN);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "cameraSns", g_pair[channel].id);
	cJSON_AddStringToObject(pJsonRoot, "stationSn", g_gatewayinfo.sn);
	
	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}
	
	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);

	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, cloudupflag_callback);
    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);

	if(g_cloudup_flag == 1)
	{
		return 0;
	}
		
	return -1;
}

int VAVASERVER_PushAlarm(int channel, int pushtype, int pushtime, int nstamp, int pushfiletype, 
	                     char *dirname, char *filename, char *msg)
{
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	cJSON *pExtRoot = NULL;
	char *pstr = NULL;

	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	char ch_str[2];
	char ntsamp_str[32];
	char ntp_str[4];
	char time_str[15];

	int ret;
	int ntp;
	time_t tt;
	struct tm *tt_info;
	
	g_push_flag = 0;
	g_push_cloudup_flag = 0;
	
	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;

	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/msg/notice/add?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/msg/notice/add?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "cameraSn", g_pair[channel].id);
	cJSON_AddNumberToObject(pJsonRoot, "noticeType", pushtype);

	pExtRoot = cJSON_CreateObject();
	if(pExtRoot == NULL)
	{
		cJSON_Delete(pJsonRoot);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	memset(ch_str, 0, 2);
	sprintf(ch_str, "%d", channel);

	ntp = VAVAHAL_GetNtp();

	memset(ntp_str, 0, 4);
	sprintf(ntp_str, "%d", ntp);

	memset(ntsamp_str, 0, 32);
	sprintf(ntsamp_str, "%d", nstamp - ntp * 3600);

	memset(time_str, 0, 15);
	tt = nstamp - ntp * 3600;
	tt_info = localtime(&tt);
	sprintf(time_str, "%d%02d%02d%02d%02d%02d", tt_info->tm_year + 1900, tt_info->tm_mon + 1, tt_info->tm_mday, tt_info->tm_hour, tt_info->tm_min, tt_info->tm_sec);

	cJSON_AddNumberToObject(pExtRoot, "duration", pushtime);
	cJSON_AddNumberToObject(pExtRoot, "fileType", pushfiletype);
	cJSON_AddStringToObject(pExtRoot, "channel", ch_str);
	cJSON_AddStringToObject(pExtRoot, "timestamp", ntsamp_str);
	cJSON_AddStringToObject(pExtRoot, "timezone", ntp_str);
	cJSON_AddStringToObject(pExtRoot, "deviceTime", time_str);
	cJSON_AddStringToObject(pExtRoot, "fileName", filename);
	cJSON_AddStringToObject(pExtRoot, "fileDate", dirname);
	cJSON_AddStringToObject(pExtRoot, "msg", msg);
	cJSON_AddNumberToObject(pExtRoot, "msgSwitch", g_pushflag[channel].push);
	cJSON_AddNumberToObject(pExtRoot, "mailSwitch", g_pushflag[channel].email);

#ifdef ALARM_PHOTO_IOT
	if(g_imgflag[channel] == 1)
	{
		g_imgflag[channel] = 0;
		cJSON_AddStringToObject(pJsonRoot, "imageName", g_imgname[channel]);
	}
#endif

	cJSON_AddItemToObject(pJsonRoot, "extJson", pExtRoot);
		
	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}
	
	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);

	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, push_callback);
    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;
	
	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);

	if(g_push_cloudup_flag == 1)
	{
		g_cloudupcheck[channel] = 1;
	}

	if(g_push_flag == 1)
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: push success, channel = %d, pushtype = %d, pushfile = %d, ntsamp = %d\n",
		                                FUN, LINE, channel, pushtype, pushfiletype, nstamp);
		return 0;
	}
	else
	{
		VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: push fail, channel = %d, pushtype = %d, pushfile = %d, ntsamp = %d\n",
		                                FUN, LINE, channel, pushtype, pushfiletype, nstamp);
		return -1;
	}
}

#ifdef ALARM_PHOTO_IOT
int VAVASERVER_PushPhoto(int channel)
{
	int ret;
	CURL *m_curl = NULL;
	struct curl_httppost *formpost = 0;
	struct curl_httppost *lastptr  = 0;

	char filename[12];

	g_imgname_flag = 0;

	memset(filename, 0, 12);
	strcpy(filename, "/tmp/pic0");

	switch(channel)
	{
		case 0:
			strcpy(filename, "/tmp/pic0");
			break;
		case 1:
			strcpy(filename, "/tmp/pic1");
			break;
		case 2:
			strcpy(filename, "/tmp/pic2");
			break;
		case 3:
			strcpy(filename, "/tmp/pic3");
			break;
		default:
			break;
		
	}

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);

		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}
	
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "file", CURLFORM_FILE, filename, CURLFORM_FILENAME, "upfile", CURLFORM_END);   

	curl_easy_setopt(m_curl, CURLOPT_URL, "https://ifttt.vavafood.com/ifttt/v1/upload");
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(m_curl, CURLOPT_HTTPPOST, formpost);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, photo_callback);

	ret = curl_easy_perform(m_curl);
	if(ret != CURLE_OK)
    {
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
    }

	curl_formfree(formpost);

	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);
	
	if(g_imgname_flag == 1)
	{
		strcpy(g_imgname[channel], g_tmpimgname);
		g_imgflag[channel] = 1;
		
		return 0;
	}

	return -1;
}
#endif

int VAVASERVER_AddStationOn(int *timeout)
{
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;

	int ret;
	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	g_addstation_flag = 0;
	g_station_locktime = 60;

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/device/station/lock?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/device/station/lock?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}
	
	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, "");

	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, addstation_callback);
    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;
	
	pthread_mutex_unlock(&mutex_curl_lock);

	if(g_addstation_flag == 1)
	{
		*timeout = g_station_locktime;
		return 0;
	}

	return -1;
}

int VAVASERVER_CheckServerStatus()
{
	int ret;
	char url[256];
	CURL *m_curl = NULL;

	static int nolink = 0;
	int internet;
	
	g_serverlink_flag = 0;

	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ping", g_gatewayinfo.domain_user);
	}
	else
	{
		sprintf(url, "https://%s/ping", g_gatewayinfo.domain_factory);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}

	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, serverlink_callback);

    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}
	else
	{
		nolink = 0;
	}

	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	if(ret == 6)
	{
		nolink++;
		if(nolink >= 3)
		{
			nolink = 0;

			VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: check ping status\n", FUN, LINE);
			
			internet = VAVAHAL_PingStatus();
			if(internet == 0x500)
			{
				Err_Log("dns parse fail");
				VAVAHAL_SystemCmd("sync");
				VAVAHAL_SystemCmd("reboot");
			}
		}
	}
	else if(ret == 35)
	{
		return 1;
	}

	if(g_serverlink_flag == 1)
	{
		return 0;
	}

	return -1;
}

int VAVASERVER_GetPairList()
{
	int ret;
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;

	g_pairsync_flag = 0;

	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	memset(url, 0, 256);

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/device/camera/list-for-station?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/device/camera/list-for-station?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);

	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}

	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, "");
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, pairsync_callback);

    ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_easy_cleanup(m_curl);
	m_curl = NULL;
	
	pthread_mutex_unlock(&mutex_curl_lock);

	if(g_pairsync_flag == 1)
	{
		return 0;
	}

	return -1;
}

int VAVASERVER_AddCamera_Blind(char *sn, int channel)
{
	int ret;
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	g_addcamera_blind_flag = 0;

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/device/camera/add-blind?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/device/camera/add-blind?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "cameraSn", sn);
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);
	
	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}

	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, addcamera_blind_callback);

	ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;
	
	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);

	if(g_addcamera_blind_flag == 1)
	{
		return 0;
	}

	return -1;
}

int VAVASERVER_AddCamera(char *sn, int channel)
{
	int ret;
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	char *pstr = NULL;

	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	g_addcamera_flag = 0;

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/device/camera/add?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/device/camera/add?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pJsonRoot, "cameraSn", sn);
	cJSON_AddNumberToObject(pJsonRoot, "channel", channel);

	if(g_cameraname[channel].nstate == 1)
	{
		cJSON_AddStringToObject(pJsonRoot, "cameraName", g_cameraname[channel].name);
	}

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);
	
	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}

	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, addcamera_callback);

	ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);
	
	if(g_addcamera_flag == 1)
	{
		return 0;
	}

	return -1;
}

int VAVASERVER_UpdateBsAttr()
{
	int ret;
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	cJSON *pStationAttr = NULL;
	char *pstr = NULL;

	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	g_bsattr_flag = 0;

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/device/station/report-attr?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/device/station/report-attr?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pStationAttr = cJSON_CreateObject();
	if(pStationAttr == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pStationAttr, "hardver", g_gatewayinfo.hardver);
	cJSON_AddStringToObject(pStationAttr, "softver", g_gatewayinfo.softver);
	cJSON_AddStringToObject(pStationAttr, "sn", g_gatewayinfo.sn);
	cJSON_AddStringToObject(pStationAttr, "mac", g_gatewayinfo.mac);
	cJSON_AddStringToObject(pStationAttr, "rfver", g_gatewayinfo.rfver);
	cJSON_AddStringToObject(pStationAttr, "rfhw", g_gatewayinfo.rfhw);
	cJSON_AddStringToObject(pStationAttr, "f_code", VAVA_GATEWAY_F_CODE);
	cJSON_AddStringToObject(pStationAttr, "f_secret", VAVA_GATEWAY_F_SECRET);
	cJSON_AddStringToObject(pStationAttr, "f_firmnum", VAVA_GATEWAY_F_FIRMNUM);
	cJSON_AddStringToObject(pStationAttr, "f_appversionin", VAVA_GATEWAY_F_APPVER_IN);
	cJSON_AddStringToObject(pStationAttr, "f_appversionout", VAVA_GATEWAY_F_APPVER_OUT);
	cJSON_AddStringToObject(pStationAttr, "h_code", VAVA_GATEWAY_H_CODE);
	cJSON_AddStringToObject(pStationAttr, "h_secret", VAVA_GATEWAY_H_SECRET);
	cJSON_AddStringToObject(pStationAttr, "h_devicenum", VAVA_GATEWAY_H_DEVICNUM);
	cJSON_AddStringToObject(pStationAttr, "h_appversionin", VAVA_GATEWAY_H_APPVER_IN);
	cJSON_AddStringToObject(pStationAttr, "h_appversionout", VAVA_GATEWAY_H_APPVER_OUT);
	cJSON_AddNumberToObject(pStationAttr, "signal", g_gatewayinfo.signal);
	cJSON_AddNumberToObject(pStationAttr, "language", g_gatewayinfo.language);
	cJSON_AddNumberToObject(pStationAttr, "nas_ctrl", g_nas_config.ctrl);
	cJSON_AddStringToObject(pStationAttr, "nas_ip", g_nas_config.ip);
	cJSON_AddStringToObject(pStationAttr, "nas_path", g_nas_config.path);
	cJSON_AddNumberToObject(pStationAttr, "alarmrecord", g_rectime.alarm_time);
	cJSON_AddNumberToObject(pStationAttr, "ntp", g_ntpinfo.ntp);
	cJSON_AddStringToObject(pStationAttr, "ntp_describe", g_ntpinfo.str);

	cJSON_AddItemToObject(pJsonRoot, "stationAttrObject", pStationAttr);

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);
	
	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}

	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, updatebsattr_callback);

	ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);
	
	if(g_bsattr_flag == 1)
	{
		return 0;
	}

	return -1;
}

int VAVASERVER_UpdateCameraAttr(int channel)
{
	int ret;
	int cameranum;
	int armlist;
	int weekday;

	char timestr[9];
	char weekstr[2];
	char days[8];
	
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	cJSON *pCameraAttr = NULL;
	cJSON *pArmList = NULL;
	cJSON *pArmdata[MAX_ARM_LIST_NUM];
	char *pstr = NULL;

	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	cameranum = 0;
	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		if(g_pair[channel].nstat == 1)
		{
			cameranum++;
			break;
		}
	}
	if(cameranum <= 0)
	{
		return 0;
	}

	g_cameraattr_flag = 0;

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/device/station/report-attr?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/device/station/report-attr?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pCameraAttr = cJSON_CreateObject();
	if(pCameraAttr == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	cJSON_AddStringToObject(pCameraAttr, "cameraSn", (char *)g_camerainfo[channel].sn);
	cJSON_AddNumberToObject(pCameraAttr, "channel", channel);
	cJSON_AddNumberToObject(pCameraAttr, "armingtype", g_camera_arminfo_v1[channel].type);
	
	pArmList = cJSON_CreateArray();
	if(pArmList == NULL)
	{
		cJSON_Delete(pJsonRoot);
		cJSON_Delete(pCameraAttr);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);

		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	for(armlist = 0; armlist < MAX_ARM_LIST_NUM; armlist++)
	{
		if(g_camera_arminfo_v1[channel].armdata[armlist].nstat == 0)
		{
			break;
		}

		pArmdata[armlist] = cJSON_CreateObject();
		if(pArmdata[armlist] == NULL)
		{
			cJSON_Delete(pJsonRoot);
			cJSON_Delete(pCameraAttr);
			cJSON_Delete(pArmList);
			
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);

			Err_Log("json malloc fail");
			g_running = 0;
			return -1;
		}

		//起始时间
		memset(timestr, 0, 9);
		sprintf(timestr, "%02d%02d%02d", g_camera_arminfo_v1[channel].armdata[armlist].s_h,
			                             g_camera_arminfo_v1[channel].armdata[armlist].s_m,
			                             g_camera_arminfo_v1[channel].armdata[armlist].s_s);
		cJSON_AddStringToObject(pArmdata[armlist], "starttime", timestr);

		//结束时间
		memset(timestr, 0, 9);
		sprintf(timestr, "%02d%02d%02d", g_camera_arminfo_v1[channel].armdata[armlist].e_h,
			                             g_camera_arminfo_v1[channel].armdata[armlist].e_m,
			                             g_camera_arminfo_v1[channel].armdata[armlist].e_s);
		cJSON_AddStringToObject(pArmdata[armlist], "endtime", timestr);

		memset(days, 0, 8);
		for(weekday = 0; weekday < VAVA_WEEKDAYS; weekday++)
		{
			if(g_camera_arminfo_v1[channel].armdata[armlist].weekday[weekday] == 1)
			{
				memset(weekstr, 0, 2);
				sprintf(weekstr, "%d", weekday);
				strcat(days, weekstr);
			}
		}

		cJSON_AddStringToObject(pArmdata[armlist], "days", days);
		cJSON_AddItemToArray(pArmList, pArmdata[armlist]);
	}

	cJSON_AddItemToObject(pCameraAttr, "arminglist", pArmList);

	cJSON_AddNumberToObject(pCameraAttr, "micstatus", g_camerainfo[channel].mic);
	cJSON_AddNumberToObject(pCameraAttr, "speakerstatus", g_camerainfo[channel].speeker);
	cJSON_AddStringToObject(pCameraAttr, "f_code", g_ipc_otainfo[channel].f_code);
	cJSON_AddStringToObject(pCameraAttr, "f_secret", g_ipc_otainfo[channel].f_secret);
	cJSON_AddStringToObject(pCameraAttr, "f_firmnum", VAVA_CAMERA_F_FIRMNUM);
	cJSON_AddStringToObject(pCameraAttr, "f_appversionin", g_ipc_otainfo[channel].f_inver);
	cJSON_AddStringToObject(pCameraAttr, "f_appversionout", g_ipc_otainfo[channel].f_outver);
	cJSON_AddStringToObject(pCameraAttr, "h_code", VAVA_CAMERA_H_CODE);
	cJSON_AddStringToObject(pCameraAttr, "h_secret", VAVA_CAMERA_H_SECRET);
	cJSON_AddStringToObject(pCameraAttr, "h_devicenum", VAVA_CAMERA_H_DEVICNUM);
	cJSON_AddStringToObject(pCameraAttr, "h_appversionin", VAVA_CAMERA_H_APPVER_IN);
	cJSON_AddStringToObject(pCameraAttr, "h_appversionout", VAVA_CAMERA_H_APPVER_OUT);
	cJSON_AddNumberToObject(pCameraAttr, "videocodec", g_camerainfo[channel].videocodec);
	cJSON_AddNumberToObject(pCameraAttr, "m_res", g_camerainfo[channel].m_res);
	cJSON_AddNumberToObject(pCameraAttr, "m_fps", g_camerainfo[channel].m_fps);
	cJSON_AddNumberToObject(pCameraAttr, "m_bitrate", g_camerainfo[channel].m_bitrate);
	cJSON_AddNumberToObject(pCameraAttr, "s_res", g_camerainfo[channel].s_res);
	cJSON_AddNumberToObject(pCameraAttr, "s_fps", g_camerainfo[channel].s_fps);
	cJSON_AddNumberToObject(pCameraAttr, "s_bitrate", g_camerainfo[channel].s_bitrate);
	cJSON_AddNumberToObject(pCameraAttr, "mirrormode", g_camerainfo[channel].mirror);
	cJSON_AddNumberToObject(pCameraAttr, "irmode", g_camerainfo[channel].irledmode);
	cJSON_AddNumberToObject(pCameraAttr, "pirsensitivity", g_pir_sensitivity[channel]);
	cJSON_AddNumberToObject(pCameraAttr, "audiocodec", g_camerainfo[channel].audiocodec);
	cJSON_AddNumberToObject(pCameraAttr, "audiorate", g_camerainfo[channel].samprate);
	cJSON_AddNumberToObject(pCameraAttr, "audiobitper", g_camerainfo[channel].a_bit);
	cJSON_AddNumberToObject(pCameraAttr, "audiochannel", g_camerainfo[channel].channel);
	cJSON_AddNumberToObject(pCameraAttr, "audioframerate", g_camerainfo[channel].a_fps);
	cJSON_AddNumberToObject(pCameraAttr, "md_enable", g_camerainfo[channel].mdctrl);
	cJSON_AddNumberToObject(pCameraAttr, "md_startx", g_camerainfo[channel].md_startx);
	cJSON_AddNumberToObject(pCameraAttr, "md_endx", g_camerainfo[channel].md_endx);
	cJSON_AddNumberToObject(pCameraAttr, "md_starty", g_camerainfo[channel].md_starty);
	cJSON_AddNumberToObject(pCameraAttr, "md_endy", g_camerainfo[channel].md_endy);
	cJSON_AddNumberToObject(pCameraAttr, "md_sensitivity", g_camerainfo[channel].md_sensitivity);
	cJSON_AddStringToObject(pCameraAttr, "hardver", (char *)g_camerainfo[channel].hardver);
	cJSON_AddStringToObject(pCameraAttr, "softver", (char *)g_camerainfo[channel].softver);
	cJSON_AddStringToObject(pCameraAttr, "rfver", (char *)g_camerainfo[channel].rfver);
	cJSON_AddStringToObject(pCameraAttr, "rfhw", (char *)g_camerainfo[channel].rfhw);
	cJSON_AddStringToObject(pCameraAttr, "mac", g_pair[channel].mac);

	cJSON_AddItemToObject(pJsonRoot, "cameraAttrObject", pCameraAttr);

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);
	
	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}

	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, updateipcattr_callback);

	ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);
	
	if(g_cameraattr_flag == 1)
	{
		return 0;
	}

	return -1;
}

int VAVASERVER_UpdateStatus()
{
	int ret;
	char url[256];
	CURL *m_curl = NULL;
	struct curl_slist *plist = NULL;
	cJSON *pJsonRoot = NULL;
	cJSON *pStationStatus = NULL;
	cJSON *pCameraStatusList = NULL;
	cJSON *pCameraItem[MAX_CHANNEL_NUM];
	char *pstr = NULL;

	int channel;
	int cameranum;
	int sessionnum;
	int wakeupnum;
	int videonum;
	
	struct timeval t_ntsamp;
	unsigned long long ntsamp = 0;

	cameranum = 0;
	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		if(g_pair[channel].nstat == 1)
		{
			cameranum++;
			break;
		}
	}
	if(cameranum <= 0)
	{
		return 0;
	}

	g_statussycn_flag = 0;

	gettimeofday(&t_ntsamp, NULL);
	ntsamp += (unsigned long long)t_ntsamp.tv_sec * 1000 + (unsigned long long)t_ntsamp.tv_usec / 1000;
	
	memset(url, 0, 256);

	if(g_domaintype == SY_DID_TYPE_USER)
	{
		sprintf(url, "https://%s/ipc/device/station/report-status?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_user, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}
	else
	{
		sprintf(url, "https://%s/ipc/device/station/report-status?access_token=%s&timestamp=%lld&lang=%s", 
			          g_gatewayinfo.domain_factory, g_gatewayinfo.token, ntsamp, SERVER_LANGUAGE);
	}

	VAVAHAL_Print_NewLine(LOG_LEVEL_DEBUG);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, url);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pJsonRoot = cJSON_CreateObject();
	if(pJsonRoot == NULL)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pStationStatus = cJSON_CreateObject();
	if(pStationStatus == NULL)
	{
		cJSON_Delete(pJsonRoot);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	pCameraStatusList = cJSON_CreateArray();
	if(pCameraStatusList == NULL)
	{
		cJSON_Delete(pJsonRoot);
		cJSON_Delete(pStationStatus);

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateArray err\n", FUN, LINE);
		
		Err_Log("json malloc fail");
		g_running = 0;
		return -1;
	}

	for(channel = 0; channel < MAX_CHANNEL_NUM; channel++)
	{
		if(g_pair[channel].nstat == 1)
		{
			pCameraItem[channel] = cJSON_CreateObject();
			if(pCameraItem[channel] == NULL)
			{
				cJSON_Delete(pJsonRoot);
				cJSON_Delete(pStationStatus);
				cJSON_Delete(pCameraStatusList);

				VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cJSON_CreateObject err\n", FUN, LINE);
				
				Err_Log("json malloc fail");
				g_running = 0;
				return -1;
			}

			cJSON_AddStringToObject(pCameraItem[channel], "cameraSn", (char *)g_camerainfo[channel].sn);
			cJSON_AddNumberToObject(pCameraItem[channel], "channel", channel);
			cJSON_AddNumberToObject(pCameraItem[channel], "online", VAVAHAL_GetCameraOnlineStatus(channel));
			cJSON_AddNumberToObject(pCameraItem[channel], "signal", g_camerainfo_dynamic[channel].signal);
			cJSON_AddNumberToObject(pCameraItem[channel], "lever", g_camerainfo_dynamic[channel].battery);
			cJSON_AddNumberToObject(pCameraItem[channel], "voltage", g_camerainfo_dynamic[channel].voltage);
			cJSON_AddNumberToObject(pCameraItem[channel], "powermode", g_camerainfo[channel].powermode);
			cJSON_AddNumberToObject(pCameraItem[channel], "armingstatus", g_camera_arminfo_v1[channel].status);
			cJSON_AddNumberToObject(pCameraItem[channel], "cloud", g_cloudflag[channel]);
			VAVAHAL_GetChannel_ConnectNum(channel, &wakeupnum, &videonum);
			cJSON_AddNumberToObject(pCameraItem[channel], "wakeup", wakeupnum);
			cJSON_AddNumberToObject(pCameraItem[channel], "video", videonum);
			if(g_update.status != VAVA_UPDATE_IDLE && g_update.type == VAVA_UPDATE_TYPE_CAMERA && g_update.current == channel)
			{
				cJSON_AddNumberToObject(pCameraItem[channel], "upstatus", g_update.status);
				if(g_update.status == VAVA_UPDATE_LOADING)
				{
					cJSON_AddNumberToObject(pCameraItem[channel], "loaddata", g_update.loading);
				}
			}
			else
			{
				cJSON_AddNumberToObject(pCameraItem[channel], "upstatus", VAVA_UPDATE_IDLE);
			}

			cJSON_AddItemToArray(pCameraStatusList, pCameraItem[channel]);
		}
	}

	cJSON_AddItemToObject(pJsonRoot, "cameraStatusObjectList", pCameraStatusList);

	if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_NOCRAD)
	{
		cJSON_AddNumberToObject(pStationStatus, "sdstatus", g_gatewayinfo.sdstatus);
		cJSON_AddNumberToObject(pStationStatus, "totolsize", 0);
		cJSON_AddNumberToObject(pStationStatus, "usedsize", 0);
		cJSON_AddNumberToObject(pStationStatus, "freesize", 0);
	}
	else if(g_gatewayinfo.sdstatus == VAVA_SD_STATUS_HAVECARD || g_gatewayinfo.sdstatus == VAVA_SD_STATUS_FULL)
	{
		if(g_gatewayinfo.format_flag == 1)
		{
			cJSON_AddNumberToObject(pStationStatus, "sdstatus", VAVA_SD_STATUS_NEEDFORMAT);
		}
		else
		{
			cJSON_AddNumberToObject(pStationStatus, "sdstatus", g_gatewayinfo.sdstatus);
		}

		cJSON_AddNumberToObject(pStationStatus, "totolsize", g_gatewayinfo.totol);
		cJSON_AddNumberToObject(pStationStatus, "usedsize", g_gatewayinfo.used);
		cJSON_AddNumberToObject(pStationStatus, "freesize", g_gatewayinfo.free);
	}

	#ifdef NAS_NFS
	cJSON_AddNumberToObject(pStationStatus, "nasstatus", g_nas_status);
	if(g_nas_status == VAVA_NAS_STATUS_SYNC || g_nas_status == VAVA_NAS_STATUS_LACKOF_SPACE)
	{
		cJSON_AddNumberToObject(pStationStatus, "nas_totolsize", g_gatewayinfo.nas_totol);
		cJSON_AddNumberToObject(pStationStatus, "nas_usedsize", g_gatewayinfo.nas_used);
		cJSON_AddNumberToObject(pStationStatus, "nas_freesize", g_gatewayinfo.nas_free);
	}
	else
	{
		cJSON_AddNumberToObject(pStationStatus, "nas_totolsize", 0);
		cJSON_AddNumberToObject(pStationStatus, "nas_usedsize", 0);
		cJSON_AddNumberToObject(pStationStatus, "nas_freesize", 0);
	}
	#endif

	if(g_update.status != VAVA_UPDATE_IDLE && g_update.type == VAVA_UPDATE_TYPE_GATEWAY)
	{
		cJSON_AddNumberToObject(pStationStatus, "upstatus", g_update.status);
		if(g_update.status == VAVA_UPDATE_LOADING)
		{
			cJSON_AddNumberToObject(pStationStatus, "loaddata", g_update.loading);
		}
	}
	else
	{
		cJSON_AddNumberToObject(pStationStatus, "upstatus", VAVA_UPDATE_IDLE);
	}

	VAVAHAL_GetSessionNum(&sessionnum);

	cJSON_AddNumberToObject(pStationStatus, "session", sessionnum);
	cJSON_AddNumberToObject(pStationStatus, "buzzer", g_buzzer_flag);

	cJSON_AddItemToObject(pJsonRoot, "stationStatusObject", pStationStatus);

	pstr = cJSON_PrintUnformatted(pJsonRoot);
	cJSON_Delete(pJsonRoot);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, pstr);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===================================\n", FUN, LINE);

	pthread_mutex_lock(&mutex_curl_lock);
	
	m_curl = curl_easy_init();
	if(m_curl == NULL)
	{
		free(pstr);
		
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: init_crul fail\n", FUN, LINE);
		pthread_mutex_unlock(&mutex_curl_lock);
		return -1;
	}

	curl_easy_setopt(m_curl, CURLOPT_URL, url);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

	// 设置http发送的内容类型为JSON  
    plist = curl_slist_append(NULL, "Content-Type:application/json;charset=UTF-8");  
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, plist);  
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, pstr);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, updatestatus_callback);

	ret = curl_easy_perform(m_curl);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: curl_easy_perform err, ret = %d\n", FUN, LINE, ret);
	}

	curl_slist_free_all(plist);
	curl_easy_cleanup(m_curl);
	m_curl = NULL;

	pthread_mutex_unlock(&mutex_curl_lock);

	free(pstr);
	
	if(g_statussycn_flag == 1)
	{
		return 0;
	}

	return -1;
}


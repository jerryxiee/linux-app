#include "basetype.h"
#include "errlog.h"
#include "vavahal.h"
#include "watchdog.h"
#include "rfserver.h"

#define RF_TAG_REQ				0x7B
#define RF_TAG_RESP				0x7E

volatile unsigned char g_rftest_flag = 0;
volatile int g_rftest_session = -1;

static int g_rffd = -1;
static unsigned char g_rf_flag = 1;

static int RF_FormatRate(int rate)
{
	int outrate = 0;
	
	switch(rate)
	{
		case 1200:      
			outrate = B1200; 
			break;
		case 1800:
			outrate = B1800;
			break;
		case 2400:
			outrate = B2400;
			break;
		case 4800:
			outrate = B4800;
			break;
		case 9600:
			outrate = B9600;
			break;
		case 19200:
			outrate = B19200;
			break;
		case 38400:
			outrate = B38400;
			break;
		case 115200:
			outrate = B115200;
			break;
		default:
			outrate = B115200;
			break;		
	}
	
	return outrate;
}

static int RF_SetRate(int outrate)
{
	struct termios Opt;
	
	if(g_rffd == -1)
	{
		return -1;
	}
	
	tcgetattr(g_rffd, &Opt);   
	cfsetispeed(&Opt, outrate);      
	cfsetospeed(&Opt, outrate);
	tcsetattr(g_rffd, TCSAFLUSH, &Opt);  
	
	return 0;
}

static int RF_SetParam(unsigned int Rate, unsigned char DataBit, unsigned char StopBit, unsigned char CheckBit)
{
	struct termios options;

	if(g_rffd == -1)
	{
		return -1;
	}

	tcgetattr(g_rffd, &options);
	options.c_oflag &= ~OPOST;  
	options.c_cflag &= ~CSIZE;

	switch(DataBit)
	{
		case 5:
			options.c_cflag |= CS5;
			break;
		case 6:
			options.c_cflag |= CS6;
			break;
		case 7:
			options.c_cflag |= CS7;
			break;
		case 8:
			options.c_cflag |= CS8;
			break;
		default:
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: DataBit param invalid\n", FUN, LINE);
			return -1;
	}

	switch(StopBit)
	{
		case 1:
			options.c_cflag &= ~(CSTOPB);
			break;
		case 2:
			options.c_cflag |= CSTOPB;
			break;
		default:
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: StopBit param invalid\n", FUN, LINE);
			return -1;
	}

	switch(CheckBit)
	{
		case 0:
			options.c_iflag &= ~(INPCK | ISTRIP);
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~PARODD;	
			break;
		case 1:
			options.c_iflag |= (INPCK | ISTRIP);
			options.c_cflag |= PARENB;            //有校验位
			options.c_cflag |= PARODD;		      //奇校验
			break;
		case 2:
			options.c_iflag |= (INPCK | ISTRIP);  
			options.c_cflag |= PARENB; 
			options.c_cflag &= ~PARODD;           //偶校验
			break;
		default:
			VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: CheckBit param invalid\n", FUN, LINE);
			return -1;
	}

	options.c_lflag = 0; 
	options.c_iflag = 0;
	options.c_oflag = 0;

	if(tcsetattr(g_rffd, TCSANOW, &options) < 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: tcsetattr err\n", FUN, LINE);
		return -1;
	}
	
	if(RF_SetRate(RF_FormatRate(Rate)) < 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: set rate err\n", FUN, LINE);
		return -1;
	}	

	return 0;
}

static void RF_DeInit()
{
	if(g_rffd != -1)
	{
		close(g_rffd);
		g_rffd = -1;	
	}

	return;
}

static int RF_Init(int num, unsigned int rate)
{
	int ret;
	char uart_name[12];

	memset(uart_name, 0, 12);
	sprintf(uart_name, "/dev/ttyS%d", num);

	g_rffd = open(uart_name, O_RDWR | O_NOCTTY | O_NDELAY, 0);
	if(g_rffd < 0)
	{
		return -1;
	}

	fcntl(g_rffd, F_SETFL, 0);

	ret = RF_SetParam(rate, 8, 1, 0);
	if(ret < 0)
	{
		RF_DeInit();

		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: RF_SetParam fail\n", FUN, LINE);
		return -1;
	}

	return 0;
}

static int RF_recv(unsigned char *databuff, int len)
{
	int ret = 0;
	int needlen = len;
	int recv = 0;

	while(g_running)
	{
		ret = read(g_rffd, databuff + recv, needlen);
		if(ret == needlen)
		{
			break;
		}

		recv += ret;
		needlen -= ret;
	}

	return recv;
}

static int RF_write(unsigned char *writebuff, int len)
{
	int ret;
	static struct timeval t_last;
	static struct timeval t_current;
	unsigned long long difftime;
	int sleeptime;
	
#ifdef FREQUENCY_OFFSET
	int i;
	char tmpstr[4];
	char tmpbuff[128];
#endif
	
	if(g_rffd == -1)
	{
		return -1;
	}

	gettimeofday(&t_current, NULL);
	difftime = (t_current.tv_sec - t_last.tv_sec) * 1000 + t_current.tv_usec / 1000 - t_last.tv_usec / 1000;
	if(difftime > 0 && difftime < 250)
	{
		sleeptime = 250 - difftime;
		usleep(sleeptime);
	}

#ifdef FREQUENCY_OFFSET
	memset(tmpbuff, 0, 128);

	for(i = 0; i < len; i++)
	{
		memset(tmpstr, 0, 4);
		sprintf(tmpstr, "%x ", writebuff[i]);
		strcat(tmpbuff, tmpstr);
	}

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========================\n", FUN, LINE);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: %s\n", FUN, LINE, tmpbuff);
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========================\n", FUN, LINE);
#endif
	
	pthread_mutex_lock(&mutex_rf_lock);
	ret = write(g_rffd, writebuff, len);
	pthread_mutex_unlock(&mutex_rf_lock);

	return ret;
}

#if 0
static int RF_Resp_IdCheck()
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];
	
	VAVA_RF_Head *rfhead;
	VAVA_RF_Checkresp *rfcheckresp;

	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rfcheckresp = (VAVA_RF_Checkresp *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_RESP;
	rfhead->cmd = CC1310_IDCHECK;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_Checkresp) + 1;

	rfcheckresp->id = 0;
	rfcheckresp->addr = g_gatewayinfo.rfaddr;
	
	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}
	
	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}
#endif

static int RF_IdCheck()
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];
	
	VAVA_RF_Head *rfhead;
	VAVA_RF_Checkresp *rfcheckresp;

	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rfcheckresp = (VAVA_RF_Checkresp *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_IDCHECK;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_Checkresp) + 1;

	rfcheckresp->id = 0;
	rfcheckresp->addr = g_gatewayinfo.rfaddr;

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}
	
	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

static int RF_GetVersion()
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];
	
	VAVA_RF_Head *rfhead;

	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_GETVERSION;
	rfhead->len = sizeof(VAVA_RF_Head) + 1;

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}
	
	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);
	
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

static int RF_GetHWVersion()
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];
	
	VAVA_RF_Head *rfhead;

	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_GETHWVER;
	rfhead->len = sizeof(VAVA_RF_Head) + 1;

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;

	sendlen = rfhead->len;
	ret = write(g_rffd, sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

#if 0
static int RF_Resp_SendSn()
{
	return RF_Resp_FeedBack(CC1310_SENDSN);
}

static int RF_Resp_RemoveSn()
{
	return RF_Resp_FeedBack(CC1310_REMOVESN);
}

static int RF_Resp_Pir()
{
	return RF_Resp_FeedBack(CC1310_PIR);
}

static int RF_Resp_LowPlower()
{
	return RF_Resp_FeedBack(CC1310_BATTERYLOW);
}

static int RF_Resp_FullPower()
{
	return RF_Resp_FeedBack(CC1310_BATTERYFULL);
}

static int RF_Resp_ExPower()
{
	return RF_Resp_FeedBack(CC1310_EXPORTPOWER);
}
#endif

int RF_FirstConnect(unsigned int addr)
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	VAVA_RF_Head *rfhead;
	VAVA_RF_FirstConnect *rffirst;
	
	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;
	
	rfhead = (VAVA_RF_Head *)sendbuff;
	rffirst = (VAVA_RF_FirstConnect *)(sendbuff + sizeof(VAVA_RF_Head));
	
	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_FIRSTCONNECT;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_FirstConnect) + 1;

	rffirst->addr = addr;

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}
	
	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

#ifdef FREQUENCY_OFFSET
int RF_PirGet(unsigned int addr, unsigned int index)
#else
int RF_PirGet(unsigned int addr)
#endif
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	VAVA_RF_Head *rfhead;
	VAVA_RF_Pirreq *rfpirreq;
	
	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;
	
	rfhead = (VAVA_RF_Head *)sendbuff;
	rfpirreq = (VAVA_RF_Pirreq *)(sendbuff + sizeof(VAVA_RF_Head));
	
	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_PIRGET;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_Pirreq) + 1;

	rfpirreq->addr = addr;
#ifdef FREQUENCY_OFFSET
	rfpirreq->index = index;
#endif

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

#ifdef FREQUENCY_OFFSET
int RF_PirSet(unsigned int addr, unsigned int index, int val)
#else
int RF_PirSet(unsigned int addr, int val)
#endif
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	VAVA_RF_Head *rfhead;
	VAVA_RF_Pirreq *rfpirreq;

	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;
	
	rfhead = (VAVA_RF_Head *)sendbuff;
	rfpirreq = (VAVA_RF_Pirreq *)(sendbuff + sizeof(VAVA_RF_Head));
	
	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_PIRSET;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_Pirreq) + 1;

	rfpirreq->addr = addr;
#ifdef FREQUENCY_OFFSET
	rfpirreq->index = index;
#endif
	rfpirreq->val = (unsigned int)val;

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

#ifdef FREQUENCY_OFFSET
int RF_Req_Pair(unsigned int addr, unsigned int index, unsigned char channel, char *ssid, char *pass)
#else
int RF_Req_Pair(unsigned int addr, unsigned char channel, char *ssid, char *pass)
#endif
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[128];
	
	VAVA_RF_Head *rfhead;
	VAVA_RF_Pairreq *rfpair;

	if(ssid == NULL || pass == NULL)
	{
		return -1; 
	}

	memset(sendbuff, 0, 128);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rfpair = (VAVA_RF_Pairreq *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_PAIRREQ;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_Pairreq) + 1;

	rfpair->addr = addr;
#ifdef FREQUENCY_OFFSET
	rfpair->index = 0;
	rfpair->preindex = index;
#endif
	rfpair->channel = channel;
	strcpy(rfpair->ssid, ssid);
	strcpy(rfpair->pass, pass);
	
	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

#ifdef FREQUENCY_OFFSET
int RF_WakeUP(unsigned int addr, unsigned int index, unsigned char mode, unsigned char netch)
#else
int RF_WakeUP(unsigned int addr, unsigned char mode, unsigned char netch)
#endif
{
	int i;
	int ret;
	int ntp;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	struct timeval timesync;

	VAVA_RF_Head *rfhead;
	VAVA_RF_Wakeup *rfwake;

	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rfwake = (VAVA_RF_Wakeup *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_WAKEUP;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_Wakeup) + 1;

	rfwake->addr = addr;
#ifdef FREQUENCY_OFFSET
	rfwake->index = index;
#endif

	ntp = VAVAHAL_GetNtp();
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get ntp = %d\n", FUN, LINE, ntp);

	gettimeofday(&timesync, NULL);
	rfwake->ntsamp = timesync.tv_sec + ntp * 3600;
	rfwake->mode = mode;
	rfwake->netch = netch;
	
	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

#ifdef FREQUENCY_OFFSET
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: addr = %x, index = %d, rfcheck = %x, ntsamp = %d, mode = %d, netch = %d, ret = %d\n", 
	                                FUN, LINE, addr, index, rfcheck, rfwake->ntsamp, rfwake->mode, rfwake->netch, ret);
#else
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: addr = %x, rfcheck = %x, ntsamp = %d, mode = %d, netch = %d, ret = %d\n", 
	                                FUN, LINE, addr, rfcheck, rfwake->ntsamp, rfwake->mode, rfwake->netch, ret);
#endif

	return 0;
}

#ifdef FREQUENCY_OFFSET
int RF_Sleep(unsigned int addr, unsigned int index)
#else
int RF_Sleep(unsigned int addr)
#endif
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	VAVA_RF_Head *rfhead;
	VAVA_RF_Sleep *rfsleep;
	
	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rfsleep = (VAVA_RF_Sleep *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_SLEEP;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_Sleep) + 1;
	
	rfsleep->addr = addr;
#ifdef FREQUENCY_OFFSET
	rfsleep->index = index;
#endif

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

#ifdef FREQUENCY_OFFSET
int RF_HeartCheck(unsigned int addr, unsigned int index, unsigned char netch)
#else
int RF_HeartCheck(unsigned int addr, unsigned char netch)
#endif
{
	int i;
	int ret;
	int sendlen;
	int ntp;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	VAVA_RF_Head *rfhead;
	VAVA_RF_HeartReq *rfheart;

	struct timeval timesync;
	
	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rfheart = (VAVA_RF_HeartReq *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_HARTBEAT;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_HeartReq) + 1;

	ntp = VAVAHAL_GetNtp();
	gettimeofday(&timesync, NULL);
	
	rfheart->addr = addr;
#ifdef FREQUENCY_OFFSET
	rfheart->index = index;
#endif
	rfheart->data.ntsamp = timesync.tv_sec + ntp * 3600;
	rfheart->data.netch = netch;
	rfheart->data.rectime = g_rectime.alarm_time;

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ntp - %d, netch - %d, rectime - %d, ntsamp = %d\n", 
	                                FUN, LINE, ntp, rfheart->data.netch, rfheart->data.rectime, rfheart->data.ntsamp);

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: addr = %x, rfcheck = %x, ret = %d\n", FUN, LINE, addr, rfcheck, ret);
	
	return 0;
}

#ifdef FREQUENCY_OFFSET
int RF_TFSet(unsigned int addr, unsigned int index, unsigned char status)
#else
int RF_TFSet(unsigned int addr, unsigned char status)
#endif
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	VAVA_RF_Head *rfhead;
	VAVA_RF_TfSet *rftfset;
	
	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rftfset = (VAVA_RF_TfSet *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_TFSET;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_TfSet) + 1;

	rftfset->addr = addr;
#ifdef FREQUENCY_OFFSET
	rftfset->index = index;
#endif
	rftfset->data.tfstatus = status;

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

#ifdef FREQUENCY_OFFSET
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: addr = %x, index = %d, rfcheck = %x, ret = %d\n", FUN, LINE, addr, index, rfcheck, ret);
#else
	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: addr = %x, rfcheck = %x, ret = %d\n", FUN, LINE, addr, rfcheck, ret);
#endif

	return 0;
}

#ifdef FREQUENCY_OFFSET
int RF_Ack_Pair(unsigned int addr, unsigned int index, unsigned int channel)
#else
int RF_Ack_Pair(unsigned int addr, unsigned int channel)
#endif
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	VAVA_RF_Head *rfhead;
	VAVA_RF_Pairack *rfpairack;

	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rfpairack = (VAVA_RF_Pairack *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_PAIRACK;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_Pairack) + 1;

	rfpairack->addr = addr;
#ifdef FREQUENCY_OFFSET
	rfpairack->index = index;
#endif
	rfpairack->channel = channel;

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

int RF_TestCtrl(int ctrl)
{
	int i;
	int ret;
	int sendlen;
	unsigned char rfcheck;
	unsigned char sendbuff[20];

	VAVA_RF_Head *rfhead;
	VAVA_RF_TestCtrl *rfctrl;
	
	memset(sendbuff, 0, 20);
	sendlen = 0;
	rfcheck = 0;

	rfhead = (VAVA_RF_Head *)sendbuff;
	rfctrl = (VAVA_RF_TestCtrl *)(sendbuff + sizeof(VAVA_RF_Head));

	rfhead->tag = RF_TAG_REQ;
	rfhead->cmd = CC1310_TEST;
	rfhead->len = sizeof(VAVA_RF_Head) + sizeof(VAVA_RF_TestCtrl) + 1;

	rfctrl->ctrl = (unsigned int)ctrl;

	for(i = 0; i < rfhead->len - 1; i++)
	{
		rfcheck ^= sendbuff[i];
	}

	sendbuff[rfhead->len - 1] = rfcheck;
	sendlen = rfhead->len;

	ret = RF_write(sendbuff, sendlen);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: rfcheck = %x, ret = %d\n", FUN, LINE, rfcheck, ret);

	return 0;
}

int RF_StopUart()
{
	g_rf_flag = 0;

	sleep(2);

	RF_DeInit();

	return 0;
}

int getpairnum()
{
	int i;
	int num = 0;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		if(g_pair[i].nstat == 1)
		{
			num = 1;
			break;
		}
	}

	return num;
}
	
void *rfserver_pth(void *data)
{
	int i;
	int ret;
	int testnum;

	int channel;
	int datalen;
	int checklen;
	
	unsigned char recvbuff[120];

	int versionflag = 1;
	int hwversionflag = 1;
	int heartflag = 0;
	int heartnum = 0;
	int noheart = 0;
	int savemodeflag[MAX_CHANNEL_NUM];

	unsigned char rfcheck;
	VAVA_RF_Head *rfhead;
	VAVA_RF_heart *rfheart;
	VAVA_RF_Camerasn *rfcamerasn;
	VAVA_RF_Cameraremove *rfcameraremove;
	VAVA_RF_Cameractrlresp *rfcameraresp;
	VAVA_RF_Pairresp *rfcamerapairresp;
	VAVA_RF_Exportpower *rfexportpower;
	VAVA_RF_TestData *rftestdata;
	VAVA_RF_FirstConnect *rffirstconnect;
	VAVA_RF_Pirresp *rfpirresp;
	VAVA_RF_Pir *rfpiralarm;
	VAVA_RF_PowerOff *rfpoweroff;
	VAVA_RF_Version *rfversion;
	VAVA_RF_Hardver *rfhardver;
	VAVA_RF_TFGet *rftfget;
	VAVA_RF_TfSet *rftfset;

	VAVA_RF_Log *rflog;
	
	fd_set rdRd;
	struct timeval timeout;

	testnum = -1;

	for(i = 0; i < MAX_CHANNEL_NUM; i++)
	{
		savemodeflag[i] = -1;
	}
	
	ret = RF_Init(1, 9600);
	if(ret != 0)
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: RF_Init fail\n", FUN, LINE);
		
		Err_Log("RF init fail");

		g_running = 0;
		return NULL;
	}

#if 0
	//设置IDCHECK
	//(一定要设置两次 要不然概率性没写入)
	RF_IdCheck();
	sleep(1);
	RF_IdCheck();
#endif

	sleep(1);

	VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ========= RF Start =========\n", FUN, LINE);
	
	RF_IdCheck();

	g_rf_flag = 1;

	while(g_running && g_rf_flag)
	{
		FD_ZERO(&rdRd);
		FD_SET(g_rffd, &rdRd);

		timeout.tv_sec = 0;				
		timeout.tv_usec = 500000;

		noheart++;
		if(noheart >= 1800) //10分钟没心跳则重启RF
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: RF norespond, reset\n", FUN, LINE);

			Err_Log("RF norespond");
			
			VAVAHAL_ResetRf();
			break;
		}

		heartnum++;
		if(heartnum >= 360) //3分钟检测一次心跳
		{
			heartnum = 0;
			heartflag = 1;
		}

		if(versionflag == 1 || heartflag == 1)
		{
			heartflag = 0;
			RF_GetVersion();
		}

		if(hwversionflag == 1)
		{
			RF_GetHWVersion();
		}

		ret = select(g_rffd + 1, &rdRd, NULL, NULL, &timeout);
		if(ret < 0)
		{
			VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err\n", FUN, LINE);
			continue;
		}
		else if(ret == 0)
		{
			//timeout
			continue;
		}
		else
		{
			if(FD_ISSET(g_rffd, &rdRd))
			{
				heartnum = 0;
				noheart = 0;
				
				memset(recvbuff, 0, 120);

				//收取头
				while(g_running)
				{
					read(g_rffd, &recvbuff[0], 1);
					if(recvbuff[0] != RF_TAG_REQ && recvbuff[0] != RF_TAG_RESP)
					{
						VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: read uart head fail, recvbuff[0] = 0x%x\n", FUN, LINE, recvbuff[0]);
						continue;
					}

					read(g_rffd, &recvbuff[1], 1);
					read(g_rffd, &recvbuff[2], 1);
					break;
				}
				
				rfhead = (VAVA_RF_Head *)recvbuff;

				VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [tag = %x] [cmd = %x] [len = %d]\n", 
				                                FUN, LINE, rfhead->tag, rfhead->cmd, rfhead->len);

				datalen = rfhead->len - sizeof(VAVA_RF_Head);
				checklen = rfhead->len - 1;

				RF_recv(recvbuff + sizeof(VAVA_RF_Head), datalen);

				//计算头校验值
				rfcheck = 0;
				
				for(i = 0; i < sizeof(VAVA_RF_Head); i++)
				{
					rfcheck ^= recvbuff[i];
				}

				if(rfhead->tag == RF_TAG_REQ)
				{
					switch(rfhead->cmd)
					{
						case CC1310_HARTBEAT:	//心跳
							rfheart = (VAVA_RF_heart *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
				                                                 FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								
								break;
							}

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Heart, addr - [%x]\n", FUN, LINE, rfheart->addr);

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 0)
								{
									continue;
								}
								
								if(g_pair[i].addr == rfheart->addr)
								{
									g_online_flag[i].online = rfheart->status;
									g_online_flag[i].battery = rfheart->battery;
									g_online_flag[i].voltage = rfheart->voltage;
									#ifdef BATTEY_INFO
									g_online_flag[i].temperature = rfheart->temperature;
									g_online_flag[i].electricity = rfheart->electricity;
									#endif
									
									g_camerainfo[i].powermode = rfheart->expower;
									g_camerainfo[i].heart_status = 0;

									if(rfhead->len == sizeof(VAVA_RF_heart) + sizeof(VAVA_RF_Head) + 1)
									{
										#ifdef BATTEY_INFO
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Heart, [ch - %d] [%x] [bat - %d] [vol - %d] [tmp - %d] [ele - %d] [power - %d] [online - %d] [tf - %d]\n", 
											                            FUN, LINE, i, rfheart->addr, rfheart->battery, rfheart->voltage, rfheart->temperature, rfheart->electricity, 
											                            rfheart->expower, rfheart->status, rfheart->tfcard);
										#else
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Heart, [ch - %d] [%x] [bat - %d] [vol - %d] [power - %d] [online - %d] [tf - %d]\n", 
											                            FUN, LINE, i, rfheart->addr, rfheart->battery, rfheart->voltage, rfheart->expower, rfheart->status, rfheart->tfcard);
										#endif

										if(rfheart->tfcard == 1)
										{
											g_tfstatus[i] = 1;
											g_tfcheck[i] = 0;
										}
										else if(rfheart->tfcard == 2)
										{
											g_tfstatus[i] = 0;
											g_tfcheck[i] = 0;
										}
									}
									else
									{
										#ifdef BATTEY_INFO
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Heart, [ch - %d] [%x] [bat - %d] [vol - %d] [tmp - %d] [ele - %d] [power - %d] [online - %d] [tf - x]\n", 
											                            FUN, LINE, i, rfheart->addr, rfheart->battery, rfheart->voltage, rfheart->temperature, rfheart->electricity, 
											                            rfheart->expower, rfheart->status);
										#else
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Heart, [ch - %d] [%x] [bat - %d] [vol - %d] [power - %d] [online - %d] [tf - x]\n", 
											                            FUN, LINE, i, rfheart->addr, rfheart->battery, rfheart->voltage, rfheart->expower, rfheart->status);
										#endif
									}

									break;
								}
							}
							break;
						case CC1310_SENDSN:		//上报摄像机列表
							rfcamerasn = (VAVA_RF_Camerasn *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cmd %d check fail, [%x][%x]\n", FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							#if 0
							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 1 && g_pair[i].addr == rfcamerasn->addr)
								{
									g_online_flag[i].online = 1;
									break;
								}
							}
							#endif

							if(strlen(rfcamerasn->sn) != 25 
								|| (strncmp(rfcamerasn->sn, "P0201", strlen("P0201")) != 0
								     && strncmp(rfcamerasn->sn, "64XIJE", strlen("64XIJE")) != 0))
							{
								VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: Sn is NULL or not Auth [%s]\n", FUN, LINE, rfcamerasn->sn);
								break;
							}

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Camera Send DevInfo, [addr - %x], [sn - %s]\n", 
							                                FUN, LINE, rfcamerasn->addr, rfcamerasn->sn);
							
							VAVAHAL_SearchList_Insert(rfcamerasn->addr, rfcamerasn->sn);
							break;
						case CC1310_REMOVESN:
							rfcameraremove = (VAVA_RF_Cameraremove *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
							                                  FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}
							
							VAVAHAL_SearchList_Remove(rfcameraremove->addr);
							break;
						case CC1310_PIR:		//PIR报警
							rfpiralarm = (VAVA_RF_Pir *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}
							
							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
							                                  FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							pthread_mutex_lock(&mutex_pir_lock);

							channel = -1;
							
							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 1 && g_pair[i].addr == rfpiralarm->addr)
								{
									channel = i;
									break;
								}
							}

							if(channel == -1)
							{
								pthread_mutex_unlock(&mutex_pir_lock);
								break;
							}

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: channel = %d, wakeup - %d, alarm - %d, cloud - %d, first - %d, armstatus - %d\n", 
							                                FUN, LINE, channel, g_camerainfo[channel].wakeup_flag, g_camerainfo[channel].alarm_flag, 
										                    g_camerainfo[channel].cloud_flag, g_camerainfo[channel].first_flag, 
										                    g_camera_arminfo_v1[channel].status);

							if(g_camerainfo[channel].wakeup_flag == 1 || g_camerainfo[channel].alarm_flag == 1 || g_camerainfo[channel].cloud_flag == 1
								|| g_camerainfo[channel].first_flag == 1 || g_camera_arminfo_v1[channel].status == 0)
							{
								pthread_mutex_unlock(&mutex_pir_lock);
								break;
							}

							pthread_mutex_unlock(&mutex_pir_lock);

							VAVAHAL_PirAlarmNoTF(channel);
							break;
						case CC1310_BATTERYLOW:	//低电量报警
						case CC1310_BATTERYFULL://满电提示
							break;
						case CC1310_EXPORTPOWER://外部电源状态
							rfexportpower = (VAVA_RF_Exportpower *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}
							
							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
							                                  FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}
							
							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 0)
								{
									continue;
								}
								
								if(g_pair[i].addr == rfexportpower->addr)
								{
									g_camerainfo[i].powermode = rfexportpower->status;

									if(savemodeflag[i] != g_camerainfo[i].powermode)
									{
										if(g_camerainfo[i].powermode == 0)
										{
											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========================================\n", FUN, LINE);
											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:         [EXPOWER OUTPUT] channel - %d    \n", FUN, LINE, i);
											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========================================\n", FUN, LINE);
										}
										else if(g_camerainfo[i].powermode == 1)
										{
											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========================================\n", FUN, LINE);
											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:         [EXPOWER INPUT] channel - %d     \n", FUN, LINE, i);
											VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========================================\n", FUN, LINE);
										}

										g_devinfo_update = 1;
										savemodeflag[i] = g_camerainfo[i].powermode;
									}

									break;
								}
							}
							
							break;
						case CC1310_PRINTF_LOG:
							rflog = (VAVA_RF_Log *)(recvbuff + sizeof(VAVA_RF_Head));
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [RFLog]: %s\n", FUN, LINE, rflog->log);
							break;
						default:
							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: cmd not req cmd, cmd = %x\n", FUN, LINE, rfhead->cmd);
							break;
					}
				}
				else
				{
					switch(rfhead->cmd)
					{
						case CC1310_IDCHECK:	//身份确认
							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                              FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ID-CHECK SUCCESS\n", FUN, LINE);
							break;
						case CC1310_GETVERSION:
							rfversion = (VAVA_RF_Version *)(recvbuff + sizeof(VAVA_RF_Head));
							
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: **** -> RF Version [%s] <- ****\n", FUN, LINE, rfversion->version);

							if(strncmp(rfversion->version, "GW-", strlen("GW-")) == 0)
							{
								strcpy(g_gatewayinfo.rfver, rfversion->version);

								//设置短波地址
								RF_IdCheck();
								versionflag = 0;
							}
							break;
						case CC1310_GETHWVER:
							rfhardver = (VAVA_RF_Hardver *)(recvbuff + sizeof(VAVA_RF_Head));
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: **** -> RF Hard Version [%s] <- ****\n", FUN, LINE, rfhardver->version);

							strcpy(g_gatewayinfo.rfhw, rfhardver->version);
							hwversionflag = 0;
							break;
						case CC1310_PAIRREQ:	//配对接收结果
							rfcamerapairresp = (VAVA_RF_Pairresp *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                              FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].addr != 0xFFFFFFFF)
								{
									VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [PAIR RESP] channel - %d, addr - %x, recvaddr - %x\n", 
								                                    FUN, LINE, i, g_pair[i].addr, rfcamerapairresp->addr);

									if(g_pair[i].addr == rfcamerapairresp->addr)
									{
										g_pair_status[i] = 0;
										g_pair_wifi_addr = 0xFFFFFFFF;
										break;
									}
								}
							}
							break;
						case CC1310_WAKEUP:		//唤醒结果
							rfcameraresp = (VAVA_RF_Cameractrlresp *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                              FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: WakeUp Resp [%x]\n", FUN, LINE, rfcameraresp->addr);

							if(g_clear_addr == rfcameraresp->addr)
							{
								g_clear_addr = 0;
								g_clear_status = 0;
							}

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: [TEST] ADDR = [%x], CURRENT = [%x]\n", FUN, LINE, rfcameraresp->addr, g_pair[i].addr);
								
								if(g_wakeup_withpair_addr == rfcameraresp->addr)
								{
									g_wakeup_withpair_status = 0;
									g_wakeup_withpair_addr = 0xFFFFFFFF;
								}
								
								if(g_pair[i].addr == rfcameraresp->addr)
								{
									if(g_camerainfo[i].wakeup_status == 1)
									{
										g_wakeup_result[i] = rfcameraresp->status;
										g_camerainfo[i].wakeup_status = 0;

										#ifdef WAKE_TIME_USER_TEST
										gettimeofday(&g_testval_2, NULL);

										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------------------------------\n", FUN, LINE);
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:   WakeUp [%x] Success [channel - %d, time - %d Ms, result - %d]\n", 
											                            FUN, LINE, rfcameraresp->addr, i, 
											                            (int)((g_testval_2.tv_sec - g_testval_1.tv_sec) * 1000 + g_testval_2.tv_usec / 1000 - g_testval_1.tv_usec / 1000),
											   							g_wakeup_result[i]);
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ----------------------------------------------------------------\n", FUN, LINE);
										
										g_camerainfo[i].sleep_flag = 1;
										#endif
									}

									break;
								}
							}
							break;
						case CC1310_SLEEP:		//休眠结果
							rfcameraresp = (VAVA_RF_Cameractrlresp *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                              FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: Sleep Resp [%x]\n", FUN, LINE, rfcameraresp->addr);

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_sleep_withpair_addr == rfcameraresp->addr)
								{
									g_sleep_withpair_status = 0;
								}
								
								if(g_pair[i].addr == rfcameraresp->addr)
								{
									if(g_camerainfo[i].sleep_status == 1)
									{
										g_sleep_result[i] = rfcameraresp->status;
										g_camerainfo[i].sleep_status = 0;

										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------\n", FUN, LINE);
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]:       Sleep [%x] Success [channel - %d]   \n", 
											                            FUN, LINE, rfcameraresp->addr, i);
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ------------------------------------------\n", FUN, LINE);

										g_camerainfo[i].sleep_flag = 0;
									}

									break;
								}
							}

							break;
						case CC1310_FIRSTCONNECT:
							rffirstconnect = (VAVA_RF_FirstConnect *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                                FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 1 && g_pair[i].addr == rffirstconnect->addr)
								{
									VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: =========>> CHANNEL - %d [FIRST RESP]\n", FUN, LINE, i);
								}
							} 
							break;
						case CC1310_PIRGET:
							rfpirresp = (VAVA_RF_Pirresp *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                                FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 0)
								{
									continue;
								}
								
								if(g_pair[i].addr == rfpirresp->addr)
								{
									g_camerainfo[i].pirget_status = 0;

									if(rfpirresp->val != VAVA_PIR_SENSITIVITY_OFF)
									{
										g_pir_sensitivity[i] = rfpirresp->val;

										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===>> CHANNEL - %d [PIR RESP] pir = %d <<===\n",
										                                FUN, LINE, i, g_pir_sensitivity[i]);
									}
									else
									{
										VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===>> CHANNEL - %d [PIR RESP] pir = 0 <<===\n", FUN, LINE, i);
									}
									
									break;
								}
							}
							break;
						case CC1310_PIRSET:
							rfpirresp = (VAVA_RF_Pirresp *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                                FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 0)
								{
									continue;
								}
								
								if(g_pair[i].addr == rfpirresp->addr)
								{
									g_camerainfo[i].pirset_status = 0;

									if(rfpirresp->val != VAVA_PIR_SENSITIVITY_OFF)
									{
										g_pir_sensitivity[i] = rfpirresp->val;
									}

									VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===>> CHANNEL - %d [PIR SET RESP] pir = %d <<===\n", 
									                                FUN, LINE, i, rfpirresp->val);
									break;
								}
							}
							break;
						case CC1310_TFGET:
							rftfget = (VAVA_RF_TFGet *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                                FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 0)
								{
									continue;
								}
								
								if(g_pair[i].addr == rftfget->addr)
								{
									VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===>> CHANNEL - %d [TF Status Get] <<===\n", FUN, LINE, i);
									
									g_tfstatus[i] = -1;
									g_camerainfo[i].poweroff_flag = 0;
									break;
								}
							}
							break;
						case CC1310_TFSET:
							rftfset = (VAVA_RF_TfSet *)(recvbuff + sizeof(VAVA_RF_Head));

							//计算数据校验值
							for(i = sizeof(VAVA_RF_Head); i < rfhead->len - 1; i++)
							{
								rfcheck ^= recvbuff[i];
							}

							if(rfcheck != recvbuff[checklen])
							{
								VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: cmd %d check fail, [%x][%x]\n", 
								                                FUN, LINE, rfhead->cmd, rfcheck, recvbuff[checklen]);
								break;
							}

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 0)
								{
									continue;
								}
								
								if(g_pair[i].addr == rftfset->addr)
								{
									g_camerainfo[i].tfset_status = 0;
									g_tfstatus[i] = rftfset->data.tfstatus;
									g_tfcheck[i] = 0;
									
									VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ===>> CHANNEL - %d [TF SET RESP] status = %d <<===\n",
									                                FUN, LINE, i, rftfset->data.tfstatus);
									break;
								}
							}
							break;
						case CC1310_POWEROFF:
							rfpoweroff = (VAVA_RF_PowerOff *)(recvbuff + sizeof(VAVA_RF_Head));

							for(i = 0; i < MAX_CHANNEL_NUM; i++)
							{
								if(g_pair[i].nstat == 0)
								{
									continue;
								}

								if(g_pair[i].addr == rfpoweroff->addr)
								{
									VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: ====== Power Off ======, addr = %x\n", FUN, LINE, rfpoweroff->addr);

									VAVAHAL_SearchList_Remove(rfpoweroff->addr);

									g_camerainfo_dynamic[i].online = 0;
									g_camerainfo_dynamic[i].signal = 0;
									//g_camerainfo_dynamic[i].battery = 0;
									//g_camerainfo_dynamic[i].voltage = 0;

									g_camerainfo[i].poweroff_flag = 1;

									g_devinfo_update = 1;
								}
							}
							break;
						case CC1310_TEST: 
							rftestdata = (VAVA_RF_TestData *)(recvbuff + sizeof(VAVA_RF_Head));
							
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: get 1310 test, num = %d\n", FUN, LINE, rftestdata->num);
							
							if(testnum == -1)
							{
								testnum = rftestdata->num;
							}
							else
							{
								if(testnum + 1 != rftestdata->num)
								{
									VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: lost rf packet, testnum = %d, recvnum = %d\n", 
									                                 FUN, LINE, testnum, rftestdata->num);

									//上报APP端显示
									if(g_rftest_session != -1)
									{
										VAVAHAL_SendRfTestErr(g_rftest_session, testnum, rftestdata->num);
									}
								}
								
								testnum = rftestdata->num;
							}
							break;
						case CC1310_TEST_START:
							testnum = -1;
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: >>> @@ RF Test Start @@ <<<\n", FUN, LINE);
							break;
						case CC1310_TEST_STOP:
							testnum = -1;
							VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: >>> @@ RF Test Stop @@ <<<\n", FUN, LINE);
							break;
						default:
							VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: cmd not resp cmd, cmd = %x\n", FUN, LINE, rfhead->cmd);
							break;
					}
				}
			}
		}
	}

	RF_DeInit();

	if(g_update.status == VAVA_UPDATE_IDLE)
	{
		//线程异常退出由看门狗重启
		WatchDog_Stop(__FUNCTION__);
	}
	
	return NULL;
}


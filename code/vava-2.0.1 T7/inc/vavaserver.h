#ifndef _VAVA_SERVER_H_
#define _VAVA_SERVER_H_

int VAVASERVER_InitLocks();
void VAVASERVER_Killlocks();
int VAVASERVER_GetToken();
int VAVASERVER_GetSyDid();
int VAVASERVER_AuthKey(char *key);
int VAVASERVER_DownLoad();
int VAVASERVER_GetCloudStatus(int channel, char *cloudtoken);
int VAVASERVER_GetCloudUpFlag(int channel);
int VAVASERVER_PushAlarm(int channel, int pushtype, int pushtime, int nstamp, int pushfiletype, 
	                     char *dirname, char *filename, char *msg);
#ifdef ALARM_PHOTO_IOT
int VAVASERVER_PushPhoto(int channel);
#endif

int VAVASERVER_AddStationOn(int *timeout);
int VAVASERVER_CheckServerStatus();

int VAVASERVER_GetPairList();
int VAVASERVER_AddCamera_Blind(char *sn, int channel);
int VAVASERVER_AddCamera(char *sn, int channel);
int VAVASERVER_UpdateBsAttr();
int VAVASERVER_UpdateCameraAttr(int channel);
int VAVASERVER_UpdateStatus();

#endif

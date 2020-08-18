#ifndef _NETWORK_H_
#define _NETWORK_H_

int Nas_SyncFile(char *dirname, char *filename, char *path);
void *Nas_InitMp4Encoder(char *mp4path, int fps);
int Nas_GetSpsPps(unsigned char *buff, unsigned char *spsbuff, unsigned char *ppsbuff, 
	              unsigned char *setbuff, int *spssize, int *ppssize, int *setsize);
int Nas_RecFile_Verification(char *recfilename);
int Nas_CloseMp4Encoder(void *handle);
int Nas_stop();

void *FTP_Manage_pth(void *data);
void *Nas_Manage_pth(void *data);
void *Nas_Sync_pth(void *data);
int Nas_GetCapacity(char *path);

#endif
#ifndef _RECORD_H_
#define _RECORD_H_

void *Rec_Alarm_pth(void *data);
void *Rec_FullTime_pth(void *data);
void *Rec_Manaul_pth(void *data);

#ifdef CLOUDE_SUPPORT
void *Rec_Cloud_pth(void *data);
#endif

void *Rec_PlayBack(void *data);

int Rec_CheckDir(char *dirname);
int Rec_Create_Fulltime_Recfile(int channel, VAVA_RecInfo *recinfo);
int Rec_Close_Fulltime_Recfile(int channel, VAVA_RecInfo *recinfo, int totoltime);
int Rec_Cteate_Manual_Recfile(int channel, VAVA_RecInfo *recinfo);
int Rec_Close_Manual_Recfile(int channel, VAVA_RecInfo *recinfo, int totoltime);
int Rec_Cteate_Alarm_Recfile(int channel, VAVA_RecInfo *recinfo);
int Rec_Close_Alarm_Recfile(int channel, VAVA_RecInfo *recinfo, int totoltime, int pushflag, int ntsamp);

int Rec_InserDirToIdx(char *dirname);
int Rec_InserFileToIdx(int channel, char *dirname, char *filename, int rectype, int rectime);

//循环删除 每次删除一天录像
int Rec_DelFirstRecDir();

//需要在函数调用前加锁
char *Rec_ImportDateIdx();
int Rec_DelRecDate(char *datelist, char *dirname);
int Rec_ReleaseDateIdx(char *datelist);

//需要在函数调用前加锁
char *Rec_ImportFileIdx(char *dirname);
int Rec_DelRecFile(char *filelist, char *filename);
int Rec_ReleaseFileIdx(char *filelist, char *dirname);

#endif


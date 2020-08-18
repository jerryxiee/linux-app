#ifndef _VAVA_HAL_H_
#define _VAVA_HAL_H_

#include "cjson.h"
#include "quelist.h"

//打印信息
void VAVAHAL_Print(int level, const char *format, ...);
void VAVAHAL_Print_NewLine(int level);

//读取基站信息
int VAVAHAL_ReadGateWayInfo();

//初始化摄像机信息
int VAVAHAL_InitCameraInfo(int channel);

//获取和设置基站提示音语言
int VAVAHAL_ReadLanguage();
int VAVAHAL_SetLanguage(int language);

//开启和关闭AP配网网卡
int VAVAHAL_OpenAPNetwork();
int VAVAHAL_CloseAPNetwork();

//初始化各通道socket句柄信息
int	VAVAHAL_InitSocket();

//读取软硬件版本号序列号和MAC地址
int VAVAHAL_ReadSn();
int VAVAHAL_ReadSoftVer();
int VAVAHAL_ReadReleaseVer();
int VAVAHAL_ReadMac();

//生成基站短波地址
int VAVAHAL_BuildGwRfAddr();

//读取和更新SOCKET句柄
int VAVAHAL_ReadSocketId(int type, int channel);
int VAVAHAL_WriteSocketId(int type, int channel, int fd);

//初始化升级
int VAVAHAL_InitUpdate();

//读取和更新P2P服务器信息
int VAVAHAL_ReadSyInfo();
int VAVAHAL_WriteSyInfo();

//读取和更新认证服务器信息
int VAVAHAL_ReadDoman();
int VAVAHAL_BuildDomanWithSn(char *sn);
int VAVAHAL_WriteDoman();

//读取和更新配对文件
int VAVAHAL_ReadPairInfo();
int VAVAHAL_WritePairInfo();

//读取和更新麦克风信息
int VAVAHAL_ReadMicInfo();
int VAVAHAL_WriteMicInfo();

//读取和更新喇叭信息
int	VAVAHAL_ReadSpeakerInfo();
int VAVAHAL_WriteSpeakerInfo();

//读取和更新自动视频质量
int VAVAHAL_ReadVideoQuality();
int VAVAHAL_WriteVideoQuality();

//读取和更新PIR灵敏度
int VAVAHAL_ReadPirSensitivity();
int VAVAHAL_WritePirSensitivity();

//申请和释放录像搜索空间
int VAVAHAL_InitSearchBuff();
int VAVAHAL_DeInitSearchBuff();

//读取和更新录像时长信息
int VAVAHAL_ReadRecTime();
int VAVAHAL_WriteRecTime();
 
//获取基站热点信息
int VAVAHAL_GetSSIDInfo();

//获取NAS配置信息
int VAVAHAL_ReadNasConfig();
int VAVAHAL_WriteNasConfig();

//获取尚云DID
int VAVAHAL_GetP2Pdid();

//检测mac是否已配对
int VAVAHAL_CheckPairWithMac(char *mac, int clientfd);

//检测SN是否已配对
int VAVAHAL_CheckPairWithSn(char *sn);

//清空缓存(开启和结束视频时调用)
int VAVAHAL_StartAVMemCahce(int channel, int mode);
int VAVAHAL_StopAVMemCahce(int channel);

//获取时区
int VAVAHAL_GetNtp();

//播放基站提示音
int VAVAHAL_PlayAudioFile(char *path);

//LED灯控制
int VAVAHAL_LedCtrl(VAVA_LED_ID led, VAVA_LED_CTRL val);

//蜂鸣器控制
int VAVAHAL_BuzzerCtrl(VAVA_BUZZER_TYPE val);

//在配对或配网完成后恢复LED灯状态
int VAVAHAL_ResetLedStatus();

//////////////////////////////////////////////////////////////////
//日期合法性验证
int VAVAHAL_Date_Verification(char *date);
//检测日期当天文件个数
int VAVAHAL_Date_CheckFile(char *date);
//检测是否为当天日期
int VAVAHAL_Date_CurrentCheck(char *date);
//时间合法性验证
int VAVAHAL_Time_Verification(char *checktime);
int VAVAHAL_Time_Verification_Ex(char *checktime, int *out_hour, int *out_min, int *out_sec);
//录像文件名合法性验证
int VAVAHAL_RecFile_Verification(char *recfilename);
int VAVAHAL_RecFile_Verification_Ex(char *recfilename, int *filetype, int *filechannel);
//布防时间合法性验证
int VAVAHAL_Armtime_Verification(char *timelist, int *status,
	                             int *s_h, int *s_m, int *s_s,
                                 int *e_h, int *e_m, int *e_s);
int VAVAHAL_Armtime_Verification_v1(char *listtime, int *s_h, int *s_m, int *s_s, int *e_h, int *e_m, int *e_s);
//IP地址合法性验证
int VAVAHAL_CheckIP(char *ip);
//LINUX路径全法性检测
int VAVAHAL_CheckLinuxPath(char *path);

//获取星期简称
char *VAVAHAL_GetWeekStr(int weekday);

//读取和更新布防配置参数 1.1样式
int VAVAHAL_ReadArmInfo_v1();
int VAVAHAL_WriteArmInfo_v1(int channel);
int VAVAHAL_PrintArmInfo_v1(int channel);
int VAVAHAL_ClearArmInfo_v1(int channel);

//读取和更新时区信息
int VAVAHAL_ReadNtpInfo();
int VAVAHAL_WriteNtpInfo(int ntp, char *ntpstr);
int VAVAHAL_ClearNtp();

//获取和更新推送配置信息
int VAVAHAL_ReadPushConfig();
int VAVAHAL_WritePushConfig();
int VAVAHAL_ReadEmailConfig();
int VAVAHAL_WriteEmailConfig();

//信令,图片和队列(插入)
int VAVAHAL_InsertCmdList(int channel, int sessionid, int cmd_code, void *data, int size);
int VAVAHAL_InsertRecList(int type, char *dirname, char *filename);
int VAVAHAL_InsertImgList(int channel, int sessionid, char *dirname, char *filename);
int VAVAHAL_InsertPushList(int channel, int pushtype, int filetype, char *dirname, char *filename, char *msg, int time, int ntsamp);

//生成CRC32校验值
int VAVAHAL_Crc32Gen(char *buff, int size);

//将数字与星期转换
int VAVAHAL_NumToWeekStr(int num, char *str);
int VAVAHAL_WeekStrToNum(int *num, char *str);

//检测会话是否有效
int VAVAHAL_CheckSession(int sessionid);

//检测文件是否存在
int VAVAHAL_FindFile(char *dirname, char *filename);

//格式化SD卡
int VAVAHAL_ForMatSD();

//检测SD卡(录像前)
int VAVAHAL_CheckSDStatus();
int VAVAHAL_PopTFCard();
int VAVAHAL_CheckTFRWStatus();

//开启和关闭手动录像
//int VAVAHAL_StartManaulRec(int channel);
//int VAVAHAL_StopManaulRec(int channel);

//开启和关闭录像功能
int VAVAHAL_OpenRec(int type, int channel);
int VAVAHAL_CloseRec(int type, int channel);

//开启和关闭云存录像功能
int VAVAHAL_OpenCloudRec(int type, int channel);
int VAVAHAL_CloseCloudRec(int type, int channel);

//开启报警录像
int VAVAHAL_StartAlarmRec(int channel);
int VAVAHAL_StopAlarmRec(int channel);

//开启云存录像
int VAVAHAL_StartAlarmCloud(int channel);
int VAVAHAL_StopAlarmCloud(int channel);

//开启和关闭自动录像(全时录像)
int VAVAHAL_StartFullTimeRec(int channel);
int VAVAHAL_StopFullTimeRec(int channel);

//开启和关闭手动录像
int VAVAHAL_StartManualRec(int channel, int session);
int VAVAHAL_StopManualRec(int channel);

//新录像接口,有流则录像
//int VAVAHAL_StartRec(int channel, int type);
//int VAVAHAL_StopRec(int channel);

//保存报警图片
int VAVAHAL_SaveAlarmPhoto(int channel, int offset, int size, int ntsamp, int pushflag, char *tmpdir, char *tmpfile);
int VAVAHAL_SavePhotoInfo(int channel, int ntsamp, int *flag, char *dirname, char *filename);
int VAVAHAL_SaveSnapShot(int channel, int session, char *buff, int size, int ntsamp);
int VAVAHAL_SavePhoto(int channel, int offset, int size);

//加密和解密日期
int VAVAHAL_EncryptionDateStr(char *indirname, char *outdirname, int random);
int VAVAHAL_ParseDateStr(char *indirname, char *outdirname, int random);

//加密和解密文件名
int VAVAHAL_EncryptionFileStr(char *infilename, char *outfilename, int random);
int VAVAHAL_ParseFileStr(char *infilename, char *outfilename, int random);

//基站搜索到的摄像机列表操作
int VAVAHAL_SearchList_Init();
int VAVAHAL_SearchList_Insert(unsigned int addr, char *sn);
int VAVAHAL_SearchList_Remove(unsigned int addr);
int VAVAHAL_SearchList_Update();
int VAVAHAL_SearchList_Find(char *sn, unsigned int *addr);
int VAVAHAL_SearchList_FindOne(char *sn, unsigned int *addr);

//删除文件和文件夹
int VAVAHAL_DelRecFile(char *dirname, char *filename);
int VAVAHAL_DelRecDir(char *dirname);

//初次连接
int VAVAHAL_GetPirStatus(int channel);
int VAVAHAL_SetPirStatus(int channel, int sensitivity);

//摄像机配对
#ifdef FREQUENCY_OFFSET
int VAVAHAL_PairReq(int channel, unsigned int addr, unsigned int index);
#else
int VAVAHAL_PairReq(int channel, unsigned int addr);
#endif
int VAVAHAL_PairCamera(cJSON *pJsonRoot, int sessionid, unsigned int addr, char *sn, char *name, int channel);
int VAVAHAL_ClearPairLock();

//唤醒休眠
int VAVAHAL_WakeupCamera(int channel, unsigned char mode);
int VAVAHAL_WakeupCamera_Ex(int channel, unsigned char mode);
#ifdef FREQUENCY_OFFSET
int VAVAHAL_WakeupCamera_Ext(unsigned int addr, unsigned int index);
#else
int VAVAHAL_WakeupCamera_Ext(unsigned int addr);
#endif
int VAVAHAL_WakeupCamera_Ex1(int channel, unsigned char mode);
int VAVAHAL_SleepCamera(int channel);
int VAVAHAL_SleepCamera_Ex(int channel);
int VAVAHAL_WakeupCamera_WithSet(int channel, int *cmdfd);
int VAVAHAL_WakeupCamera_WithSnapshort(int channel);
#ifdef FREQUENCY_OFFSET
int VAVAHAL_WakeupCamera_WithPair(unsigned int addr, unsigned int index);
int VAVAHAL_SleepCamera_WithPair(unsigned int addr, unsigned int index);
#else
int VAVAHAL_WakeupCamera_WithPair(unsigned int addr);
int VAVAHAL_SleepCamera_WithPair(unsigned int addr);
#endif

//心跳检测 
int VAVAHAL_HeartCheck(int channel);

//TF卡状态同步到摄像机
int VAVAHAL_TfSet(int channel, unsigned char status);

//连接无线网络
int VAVAHAL_ConnectAp(char *ssid, char *pass);

//获取无线连接状态
int VAVAHAL_GetWireleseStatus();
int VAVAHAL_PingStatus();
int VAVAHAL_CheckServerStatus();

//获取wifi信道
int VAVAHAL_GetNetChannel();

//获取会话数和视频观看人数
int VAVAHAL_GetSessionNum(int *sessionnum);
int VAVAHAL_GetChannel_ConnectNum(int channel, int *wakeupnum, int *videonum);

//打印状态
void VAVAHAL_PrintOnline();
void VAVAHAL_PrintPush();
void VAVAHAL_PrintEmail();

//阻塞获取AP连接状态，30秒超时
void VAVAHAL_GetApStatus(char *ssid);

//PIR报警
int VAVAHAL_PirAlarmNoTF(int channel);
int VAVAHAL_PirAlarm(int channel);

//推送消息
int VAVAHAL_PushAlarm(int channel, int pushtype, int pushtime, int ntsamp, int pushfiletype, 
                      char *dirname, char *filename, char *msg);
//云存推送
int VAVAHAL_CloudPush(int channel, int ntsamp);

//恢复出厂设置
int VAVAHAL_ResetGateWay();
int VAVAHAL_ResetCamera(int channel);

//获取本机IP
int VAVAHAL_GetLocalIP(char *ipstr);

//获取IP地址连通状态
int VAVAHAL_PingIPStatus(char *ip);

//检查当前系统是否空闲
int VAVAHAL_CheckSysIdle();

//检查NAS文件夹
int VAVAHAL_CheckNasDir(char *dirname);
int VAVAHAL_UpdateNasDir(char *dirname);

//转换NAS文件路径
int VAVAHAL_BuildNasPath(char *path, char *dirname, char *filename);

//升级短波
int VAVAHAL_UpgradeRf();

//重启短波
int VAVAHAL_ResetRf();

//升级超时进度
int VAVAHAL_CreateUpgradingTimeout(int upnum);

//系统调用
int VAVAHAL_SystemCmd(char *str);
int VAVAHAL_SystemCmd_Ex(char *cmd, char *buff, int len);
int VAVAHAL_SystemCmd_Enx(char *cmd);

//刷新云存状态
int VAVAHAL_RefreshCloud();

//获取IPC在线状态
int VAVAHAL_GetCameraOnlineStatus(int channel);

//检测帧
int VAVAHAL_CheckVideoFrame(int channel, char *frame, int type, char *str);

//释放录像缓存
int VAVAHAL_FreeAvMem();

//关闭AV服务器
int VAVAHAL_StopAvServer();

#ifdef FREQUENCY_OFFSET
//获取空闲配对频段
int VAVAHAL_GetPairFreeIndex();
#endif

//重启网络升级服务器
int VAVAHAL_ResetUpServer();

//去掉冗余信息
int VAVAHAL_PrintUnformatted(char *str, char *buff);

//////////////////////////////////////////////////////////////////
//向APP发送消息
int VAVAHAL_SendUpStats(int sessionid, int status, int loading, int channel);
int VAVAHAL_SendDevInfo(int sessionid, int sessionchannel);
int VAVAHAL_SendPaseErr(int sessionid);
int VAVAHAL_SendErrInfo(int sessionid, int errnum, int cmdcode, void *data);
int VAVAHAL_SendSearchRecDate(int sessionid);
int VAVAHAL_SendNoRecDate(int sessionid);
int VAVAHAL_SendRecStop(int sessionid, int channel, int type);
int VAVAHAL_SendRecDatePack(int sessionid, int endflag, int datenum, cJSON *pDateList);
int VAVAHAL_SendSearchRecFile(int sessionid, char *dirname, int type);
int VAVAHAL_SendNoRecFile(int sessionid, char *dirname);
int VAVAHLA_SendRecFilePack(int sessionid, char *dirname, int endflag, int filenum, cJSON *pFileList);
int VAVAHAL_SendSearchRecShareDate(int sessionid, struct _recsharedatelist list[]);
int VAVAHAL_SendNoRecShareDate(int sessionid);
int VAVAHAL_SendRecShareDatePack(int sessionid, int endflag, int datenum, cJSON *pDateList);
int VAVAHAL_CheckRecDateWithChannel(char *dirname, int channel);
int VAVAHAL_SendSearchRecShareList(int sessionid, char *dirname, int type, int channel[]);
int VAVAHAL_SendNoRecShareList(int sessionid, char *dirname);
int VAVAHAL_SendRecShareListPack(int sessionid, char *dirname, int endflag, int filenum, cJSON *pFileList);
int VAVAHAL_SendPairResp(int sessionid, int errnum);
int VAVAHAL_SendRfTestErr(int sessionid, int savenum, int currentnum);
int VAVAHAL_SendCameraSetAck(int sessionid, int channel, int result, int cmd);
int VAVAHAL_SendCameraSetTimeout(int sessionid, int cmd);
int VAVAHAL_SendCameraNotReady(int sessionid, int cmd);
int VAVAHAL_SendRecDelResult(int sessionid, int type, char *dirname, char *filename, int result);
int VAVAHAL_SendSnapShotResult(int sessionid, int channel, char *dirname, char *filename, int result);
int VAVAHAL_SendCameraNoWakeup(int sessionid, int cmd);
int VAVAHAL_SendRecStartResult(int sessionid, int channel, int result);

//////////////////////////////////////////////////////////////////
//向摄像机发送命令
int VAVAHAL_UpdateCamera(int cmdfd, int cmdtype, int param1, int param2);
int VAVAHAL_CmdResp_NoPair(int cmdfd);
int VAVAHAL_CmdResp_Heartbeat(int cmdfd, int sleep);
int VAVAHAL_CmdResp_PairResp(int cmdfd, VAVA_Pair_Resp *pairinfo);
int VAVAHAL_CmdResp_FirstConnectAck(int cmdfd);
int VAVAHAL_CmdResp_TimeSync(int cmdfd);
int VAVAHAL_CmdReq_InsertIframe(int cmdfd, unsigned int stream);
int VAVAHAL_CmdReq_SetRes(int cmdfd, qCmdData cmddata);
int VAVAHAL_CmdReq_SetMirr(int cmdfd, qCmdData cmddata);
int VAVAHAL_CmdReq_SetIrMode(int cmdfd, qCmdData cmddata);
int VAVAHAL_CmdReq_SetMdParam(int cmdfd, qCmdData cmddata);
int VAVAHAL_CmdReq_CameraReset(int cmdfd, qCmdData cmddata);
int VAVAHAL_CmdReq_OpenRec(int cmdfd, qCmdData cmddata);
int VAVAHAL_CmdReq_CloseRec(int cmdfd, qCmdData cmddata);
int VAVAHAL_CmdReq_SetComCtrl(int cmdfd, qCmdData cmddata);

//////////////////////////////////////////////////////////////////
//获取会话参数
int VAVAHAL_ParamParse_SessionAuth(cJSON *pRoot, int *random, char *md5, char *authkey);
int VAVAHAL_ParamParse_Channel(cJSON *pRoot, int *channel);
int VAVAHAL_ParamParse_Ctrl(cJSON *pRoot, int *channel, int *ctrl);
int VAVAHAL_ParamParse_VideoQuality(cJSON *pRoot, int *channel, int *quality);
int VAVAHAL_ParamParse_MirrMode(cJSON *pRoot, int *channel, int *mirrmode);
int VAVAHAL_ParamParse_IrLedMode(cJSON *pRoot, int *channel, int *irledmode);
int VAVAHAL_ParamParse_PushConfig(cJSON *pRoot);
int VAVAHAL_ParamParse_EmailConfig(cJSON *pRoot);
int VAVAHAL_ParamParse_PirSsty(cJSON *pRoot, int *channel);
int VAVAHAL_ParamParse_ArmInfo_V1(cJSON *pRoot, int *channel);
int VAVAHAL_ParamParse_MdInfo(cJSON *pRoot, int *channel, VAVA_MdConfig *mdconfig);
int VAVAHAL_ParamParse_RecTimeInfo(cJSON *pRoot);
int VAVAHAL_ParamParse_RecSearch(cJSON *pRoot, VAVA_RecSearch *recsearch);
int VAVAHAL_ParamParse_RecShareDate(cJSON *pRoot, VAVA_RecShareDateSearch *recsharedate);
int VAVAHAL_ParamParse_RecShareList(cJSON *pRoot, VAVA_RecShareListSearch *recsharelist);
int VAVAHAL_ParamParse_RecImg(cJSON *pRoot, int *channel, VAVA_RecImgGet *recimgget);
int VAVAHAL_ParamParse_RecDelList(cJSON *pRoot, cJSON *pJsonRoot, int sessionid);
int VAVAHAL_ParamParse_GateWayTime(cJSON *pRoot);
int VAVAHAL_ParamParse_Language(cJSON *pRoot);
int VAVAHAL_ParamParse_WifiConfig(cJSON *pRoot);
int VAVAHAL_ParamParse_Camerapair(cJSON *pRoot, int *channel, char *sn, char *name);
int VAVAHAL_ParamParse_ClearPair(cJSON *pRoot);
int VAVAHAL_ParamParse_NewVersion(cJSON *pRoot, int sessionid);
int VAVAHAL_ParamParse_Doman(cJSON *pRoot);
int VAVAHAL_ParamParse_P2PServer(cJSON *pRoot);
int VAVAHAL_Param_Parse_NasServer(cJSON *pRoot);
int VAVAHAL_ParamParse_RecPlay(cJSON *pRoot, int sessionid, int *channel, int *recchannel);
int VAVAHAL_ParamParse_RecPlayCtrl(cJSON *pRoot, int sessionid, int *channel, int *ctrl);
int VAVAHAL_ParamParse_RfTest(cJSON *pRoot, int sessionid);
int VAVAHAL_ParamParse_Sn(cJSON *pRoot);
int VAVAHAL_ParamParse_Buzzertype(cJSON *pRoot);
int VAVAHAL_ParamParse_CloudDebugLevel(cJSON *pRoot);
int VAVAHAL_ParamParse_ComCtrl(cJSON *pRoot);
int VAVAHAL_ParamParse_IPCComCtrl(cJSON *pRoot, int *channel, int *ctrl);
int VAVAHAL_ParamParse_LogLevel(cJSON *pRoot);

//组织会话参数
int VAVAHAL_ParamBuild_Auth(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_Channel(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_Ctrl(cJSON *pJsonRoot, int channel, int ctrl);
int VAVAHAL_ParamBuild_Status(cJSON * pJsonRoot, int channel, int status);
int VAVAHAL_ParamBuild_VideoInfo(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_AudioOpen(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_GateWayInfo(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_SDInfo(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_BatteryInfo(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_CameraInfo(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_CameraConfig(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_VideoQuality(cJSON *pJsonRoot, int channel, int streamtype);
int VAVAHAL_ParamBuild_MirrMode(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_IrMode(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_PushConfig(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_EmailConfig(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_CameraArmInfo_v1(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_PirSensitivity(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_MDInfo(cJSON *pJsonRoot, int channel);
int VAVAHAL_ParamBuild_RecTimeInfo(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_GateWayTime(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_UpStatus(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_SyDid(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_Doman(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_P2PServer(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_NasServer(cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_RecFileSize(cJSON *pRoot, cJSON *pJsonRoot);
int VAVAHAL_ParamBuild_RecPlay(cJSON *pJsonRoot, int channel, int recchannel);

#endif 


#ifndef _BASE_TYPE_H_
#define _BASE_TYPE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <netdb.h>
#include <termios.h> 
#include <semaphore.h>
#include <syslog.h>
#include <pthread.h>
#include <netdb.h>
#include <setjmp.h>
#include <sys/msg.h> 
#include <sys/ioctl.h>
#include <sys/ipc.h>   
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/vfs.h>
#include <sys/reboot.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <asm/types.h>
#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/sysinfo.h>
#include <linux/netlink.h>

#include "mps_clds_dev.h"
#include "curl.h"
#include "easy.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define PROGRAM_VER  				"V2.0.0.1 T7"

#define LOG_BUF_SIZE				2048
#define LOG_LEVEL_OFF				0
#define LOG_LEVEL_ERR				1
#define LOG_LEVEL_WARING  			2
#define LOG_LEVEL_DEBUG				3

#define FUN							__FUNCTION__
#define LINE						__LINE__
#define RFUP_PRINT					printf

#if 1
//环境域名
//测试环境指向集成环境
#define DOMAIN_DEV					"iot-api-dev.sunvalleycloud.com"	//开发
#define DOMAIN_SIT 					"iot-api-sit.sunvalleycloud.com"	//集成
#define DOMAIN_TEST					DOMAIN_SIT
#define DOMAIN_DEMO					"iot-api-demo.sunvalleycloud.com"	//演示
#define DOMAIN_UAT					"iot-api-uat.sunvalleycloud.com"	//公测
#define DOMAIN_USER					"iot-api.sunvalleycloud.com"		//正式
#else
//环境域名
#define DOMAIN_DEV					"app-dev.vavafood.com"				//开发
#define DOMAIN_TEST					"app-test.vavafood.com"				//测试
#define DOMAIN_SIT					"app-ptest.vavafood.com"			//集成
#define DOMAIN_DEMO					"app.vavafood.com"					//演示
#define DOMAIN_UAT					"app-ptest.vavafood.com"			//公测
#define DOMAIN_USER					"app.vavasmart.com"					//正式
#endif

#if 1
#define WIRLD_ONLY					//仅支持有线
#endif

#if 0
#define FTP_SERVER					//是否支持FTPSERVER(SD卡目录)
#endif

#if 1
#define DEV_DISCOVERY				//是否支持设备发现协议
#endif

#if 1
#define NAS_NFS						//是否支持NAS服务自动同步(NFS协议)
#endif

#if 1
#define WAKE_TIME_USER_TEST			//打印唤醒流程耗时
#endif

#if 0
#define ALARM_PHOTO					//报警抓图(媒体库)
#endif

#if 0
#define ALARM_PHOTO_IOT				//报警抓图(IOT)
#endif

#if 1
#define ARM_INFO_V1					//使用1.1样式
#endif

#if 1
#define CLOUDE_SUPPORT				//支持云服务
#endif

#if 1
#define DEBUG_LEVER_ERR				//错误打印
#define DEBUG_LEVER_INFO			//信息打印
#define DEBUG_LEVER_DEBUG			//调试打印
#endif

#if 0
#define FRAME_COUNT					//调试缓存
#endif

#if 1
#define BATTEY_INFO					//电池信息
#endif

#if 0
#define DEVELOPMENT_MODE			//开发版模式
#endif

#if 1
#define FREQUENCY_OFFSET			//频偏模式
#endif

#define GATEWAY_PRIVEDATA
#ifdef GATEWAY_PRIVEDATA
/*                         基站内部数据结构                       */
//栈大小
#define STACK_SIZE					50000

//短波指令间隔时长
#ifdef FREQUENCY_OFFSET
#define RF_CMD_HEART_TIME			60000 * 5
#else
#define RF_CMD_HEART_TIME			60000
#endif
#define RF_CMD_SLEEP_TIME			RF_CMD_HEART_TIME

//============================= 基站固件版本 =========================
#if 0 //合作版本(已废)
#define VAVA_GATEWAY_F_CODE			"294346a2f36bd76cb3a43856bcd0e5fe"
#define VAVA_GATEWAY_F_SECRET		"18c6d873f1df0fe339e1c38129cb66d2"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010000"
#define VAVA_GATEWAY_F_APPVER_OUT	"010000"
#endif

#if 0 //v1.0.0版本
#define VAVA_GATEWAY_F_CODE			"5a0783265151b96f290354e1970d0f86"
#define VAVA_GATEWAY_F_SECRET		"4c694b5dc6e029b72e96c95f25e8a4ea"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010000"
#define VAVA_GATEWAY_F_APPVER_OUT	"010000"
#endif

#if 0 //v1.0.1版本
#define VAVA_GATEWAY_F_CODE			"8f84a58a6b0e48d2c93e7d98d208243f"
#define VAVA_GATEWAY_F_SECRET		"704ac11a5d01e5cef106df07052dff30"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010001"
#define VAVA_GATEWAY_F_APPVER_OUT	"010001"
#endif

#if 0 //v1.1.1版本
#define VAVA_GATEWAY_F_CODE			"4292e35fe68903bdbcdfc4009956a7ff"
#define VAVA_GATEWAY_F_SECRET		"40172bf39cc6f21fbb56a0d912448e0b"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010101"
#define VAVA_GATEWAY_F_APPVER_OUT	"010101"
#endif

#if 0 //v1.1.2版本
#define VAVA_GATEWAY_F_CODE			"bdbd4954472dbe130215fb65683fe3cb"
#define VAVA_GATEWAY_F_SECRET		"0597d11cf6094b1f7be5cd813afca3f9"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010102"
#define VAVA_GATEWAY_F_APPVER_OUT	"010102"
#endif

#if 0 //v1.2.0版本
#define VAVA_GATEWAY_F_CODE			"3e74a2c6dbeaa9d523fa1f1f3d18fcfa"
#define VAVA_GATEWAY_F_SECRET		"6320fae2dc6663e65b8a326c8f4b7bef"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010200"
#define VAVA_GATEWAY_F_APPVER_OUT	"010200"
#endif

#if 0 //v1.2.1版本
#define VAVA_GATEWAY_F_CODE			"bb5b42a8649a4d1c8b57cb6ba1563dc9"
#define VAVA_GATEWAY_F_SECRET		"541f87bbc4d1e038be0cc1879f598f8f"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010201"
#define VAVA_GATEWAY_F_APPVER_OUT	"010201"
#endif

#if 0 //v1.2.2版本
#define VAVA_GATEWAY_F_CODE			"e6d64a200b138a3c9dc3255e1be4b598"
#define VAVA_GATEWAY_F_SECRET		"970b9ec64a6ba5302d91b44c9d0a7ecb"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010202"
#define VAVA_GATEWAY_F_APPVER_OUT	"010202"
#endif

#if 0 //v1.2.3版本
#define VAVA_GATEWAY_F_CODE			"ce3269d3b504b68c2b1628c12e7338d4"
#define VAVA_GATEWAY_F_SECRET		"065156819ac9d8f6d8fe7ff662b4cf5f"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010203"
#define VAVA_GATEWAY_F_APPVER_OUT	"010203"
#endif

#if 1 //v1.2.4版本
#define VAVA_GATEWAY_F_CODE			"c66f26ddfc6de7d920d4412ce4a7b570"
#define VAVA_GATEWAY_F_SECRET		"e67643503d0077cd987725cd086a2446"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010204"
#define VAVA_GATEWAY_F_APPVER_OUT	"010204"
#endif

#if 0 //v1.2.5版本
#define VAVA_GATEWAY_F_CODE			"3485bdcf829fafd7aceda6388afc6b62"
#define VAVA_GATEWAY_F_SECRET		"6f624c21bf39b7d3175c1f97c3bf7325"
#define VAVA_GATEWAY_F_FIRMNUM		"P020201"
#define VAVA_GATEWAY_F_APPVER_IN	"010205"
#define VAVA_GATEWAY_F_APPVER_OUT	"010205"
#endif

#if 1
#define TEST_CLIENT_ID 				"9c33e08830c243c597246c71e3c2f458"
#define TEST_CLIENT_SECRECT 		"237e61fdc48a46908736c499685e9f34"
#endif

//====================================================================

//============================= 基站硬件版本 =========================
#if 0 //合作版本(已报废)
#define VAVA_GATEWAY_H_CODE			"ef895ec94a856c9f7b5ac2b5512956fb"
#define VAVA_GATEWAY_H_SECRET		"12ad7dba28e46a386532fa70934e1b33"
#define VAVA_GATEWAY_H_DEVICNUM		"P020201"
#define VAVA_GATEWAY_H_APPVER_IN	"010000"
#define VAVA_GATEWAY_H_APPVER_OUT	"010000"
#endif

#if 1 //v1.0.0版本
#define VAVA_GATEWAY_H_CODE			"84ae6a434949d557c9cb0a62a6ded241"
#define VAVA_GATEWAY_H_SECRET		"951c76f433735365cdf6cee988f4bb58"
#define VAVA_GATEWAY_H_DEVICNUM		"P020201"
#define VAVA_GATEWAY_H_APPVER_IN	"010000"
#define VAVA_GATEWAY_H_APPVER_OUT	"010000"
#endif
//====================================================================


//============================ 摄像机固件版本  =======================
#if 0  //合作版本(已废)
#define VAVA_CAMERA_F_CODE			"3b5f1b8ebc182765cfc20df994322b41"
#define VAVA_CAMERA_F_SECRET		"36d2653cffcbc36ee92d87c440deb21d"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010000"
#define VAVA_CAMERA_F_APPVER_OUT	"010000"
#endif

#if 0 //v1.0.0版本
#define VAVA_CAMERA_F_CODE			"908247603ab810981587070e01a3c850"
#define VAVA_CAMERA_F_SECRET		"1d8e903ee7e46483a43b16677bc904c1"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010000"
#define VAVA_CAMERA_F_APPVER_OUT	"010000"
#endif

#if 0 //v1.0.1版本
#define VAVA_CAMERA_F_CODE			"bde3eb0ca7c3f7a7953a218adf2ec091"
#define VAVA_CAMERA_F_SECRET		"6b56588ec2f462305c2fa80636c99ad0"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010001"
#define VAVA_CAMERA_F_APPVER_OUT	"010001"
#endif

#if 1 //v1.1.1版本
#define VAVA_CAMERA_F_CODE			"678436a7e1fbc0d965665eb23bf20ccd"
#define VAVA_CAMERA_F_SECRET		"11b0da1162962ecd605c7b296e564599"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010101"
#define VAVA_CAMERA_F_APPVER_OUT	"010101"
#endif

#if 0 //v1.1.2版本
#define VAVA_CAMERA_F_CODE			"47691540408a8226e7d7353f00ebc580"
#define VAVA_CAMERA_F_SECRET		"7ee10ea28799faad15372a8dc950faa5"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010102"
#define VAVA_CAMERA_F_APPVER_OUT	"010102"
#endif

#if 0 //v1.2.0版本
#define VAVA_CAMERA_F_CODE			"8968e510382b2dd4ac3d05052cdc4146"
#define VAVA_CAMERA_F_SECRET		"1d8052b8183648bdc3acef706c25d569"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010200"
#define VAVA_CAMERA_F_APPVER_OUT	"010200"
#endif

#if 0 //v1.2.1版本
#define VAVA_CAMERA_F_CODE			"16b737898b2d89d2c70af5a89842683b"
#define VAVA_CAMERA_F_SECRET		"4630c5c1083d4485783ee5c0db27cab8"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010201"
#define VAVA_CAMERA_F_APPVER_OUT	"010201"
#endif

#if 0 //v1.2.2版本
#define VAVA_CAMERA_F_CODE			"34d63eef0c721a161652a1257547fca9"
#define VAVA_CAMERA_F_SECRET		"f95de78914954bb9ed31bc7ba200c146"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010202"
#define VAVA_CAMERA_F_APPVER_OUT	"010202"
#endif

#if 0 //v1.2.3版本
#define VAVA_CAMERA_F_CODE			"b031b4855fab7fb7f920b018c4b34a62"
#define VAVA_CAMERA_F_SECRET		"c465dfaa8291c5213218d1f0a3be2bf1"
#define VAVA_CAMERA_F_FIRMNUM		"P020101"
#define VAVA_CAMERA_F_APPVER_IN		"010203"
#define VAVA_CAMERA_F_APPVER_OUT	"010203"
#endif

#if 0 //v1.2.4版本
#define VAVA_CAMERA_F_CODE			"c66f26ddfc6de7d920d4412ce4a7b570"
#define VAVA_CAMERA_F_SECRET		"e67643503d0077cd987725cd086a2446"
#define VAVA_CAMERA_F_FIRMNUM		"P020201"
#define VAVA_CAMERA_F_APPVER_IN		"010204"
#define VAVA_CAMERA_F_APPVER_OUT	"010204"
#endif

#if 0 //v1.2.5版本
#define VAVA_CAMERA_F_CODE			"b93e69a26c332ecf9215782d981ae5d1"
#define VAVA_CAMERA_F_SECRET		"d48b472da92ce2d1bba8ed2af9efd791"
#define VAVA_CAMERA_F_FIRMNUM		"P020201"
#define VAVA_CAMERA_F_APPVER_IN		"010205"
#define VAVA_CAMERA_F_APPVER_OUT	"010205"
#endif
//====================================================================

//============================ 摄像机硬件版本  =======================
#if 0 //合作版本(已废)
#define VAVA_CAMERA_H_CODE			"0b22c4d6eced3c3f7b806b8741ffa549"
#define VAVA_CAMERA_H_SECRET		"063c9308de9aa967e941ae81ba0e6b65"
#define VAVA_CAMERA_H_DEVICNUM		"P020101"
#define VAVA_CAMERA_H_APPVER_IN		"010000"
#define VAVA_CAMERA_H_APPVER_OUT	"010000"
#endif

#if 1 //v1.0.0版本
#define VAVA_CAMERA_H_CODE			"61fcc44078246b6bb49b186adebd1efa"
#define VAVA_CAMERA_H_SECRET		"cf5966f03e14de5f7ef3ad143b721926"
#define VAVA_CAMERA_H_DEVICNUM		"P020101"
#define VAVA_CAMERA_H_APPVER_IN		"010000"
#define VAVA_CAMERA_H_APPVER_OUT	"010000"
#endif
//====================================================================

#define SY_DID_TYPE_FACTORY			0
#define SY_DID_TYPE_USER			1

//文件路径
//基站配置相关
#define LANGUAGE_FILE_DEFAULT		"/etc_ro/language"			//语言默认配置

//摄像机配置相关(恢复出厂后清除)
#define P2PSERVER_FILE				"/tmp/config/p2pconf"		//p2p服务器配置文件
#define DOMAN_FILE					"/tmp/config/domain"		//认证服务器配置文件
#define RECTIME_FILE				"/tmp/config/rectime"		//录像时长配置文件
#define LANGUAGE_FILE				"/tmp/config/language"		//语言配置
#define NTPINFO_FILE				"/tmp/config/ntpinfo"		//NTP配置信息
#define NASCONFIG_FILE				"/tmp/config/nasconfig"		//NAS配置文件

//摄像机配置相关(恢复出厂后不清除)
#ifdef FREQUENCY_OFFSET
#define PAIR_FILE_OLD				"/tmp/factorytest/pairconf" //摄像机配对文件
#define PAIR_FILE					"/tmp/factorytest/pairconfex"//摄像机配对文件
#else
#define PAIR_FILE					"/tmp/factorytest/pairconf" //摄像机配对文件
#endif
#define MIC_FILE					"/tmp/factorytest/mic"  	//MIC配置文件
#define SPEAKER_FILE				"/tmp/factorytest/speaker"  //喇叭配置文个
#define VIDEOQUALITY_FILE			"/tmp/factorytest/videoquality"	//视频自动质量配置文件
#define PIRSENSITIVITY_FILE			"/tmp/factorytest/pirsensitivity"//PIR灵敏度文件
#define ARMINFO_FILE				"/tmp/factorytest/armconfig" //布防参数配置文件 1.0样式
#define ARMINFO_FILE_V1				"/tmp/factorytest/armconfig_v1"	//布防参数配置文件 1.1样式
#define PUSH_FLAG_FILE				"/tmp/factorytest/pushconfig" //消息推送配置文件
#define EMAIL_FLAG_FILE				"/tmp/factorytest/msgconfig" //邮件推送配置文件

//升级包路径
#define UPDATE_FILE1				"/tmp/update/img1.bin"		//升级包1
#define UPDATE_FILE2				"/tmp/update/img2.bin"		//升级包2

#define RECIDX_FILE					"vava.idx"					//录像索引文件
#define NAS_SYNC_FILE				"vava.sync"					//Nas服务同步文件
#define LOG_FILE					"log"						//日志文件
#define LOG_MEM						"/tmp/memlog"				//内存日志

#define VAVA_TAG_CAMERA_CMD			0xEB000001					//摄像机与基站信令传输
#define VAVA_TAG_CAMERA_AV			0xEB000002					//摄像机与基站音视频图片传输
#define VAVA_TAG_APP_CMD			0xEB000003					//基站与APP信令传输
#define VAVA_TAG_TALK				0xEB000004					//对讲流
#define VAVA_TAG_APP_IMG			0xEB000005					//基站与APP图片传输
#define VAVA_TAG_DEV_SEARCH			0xEB000006					//基站发现协议
#define VAVA_TAG_AP					0xEB000007					//基站AP配网
#define VAVA_TAG_OTA				0xEB000008					//OTA升级头
#define VAVA_TAG_AP_VIDEO			0xEB000009					//回放录像(视频)
#define VAVA_TAG_AP_AUDIO			0xEB000010					//回放录像(音频)
#define VAVA_TAG_VIDEO				0xEB000011					//视频流
#define VAVA_TAG_AUDIO				0xEB000022					//音频流
#define VAVA_REC_HEAD				0xEB0000AA					//录像头
#define VAVA_IDX_HEAD				0xEB0000BB					//索引文件头
#define VAVA_SYNC_HEAD				0x351A408B					//NAS同步头

#define VAVA_REC_ENCRYPT_FREE		0							//录像加密标志
#define VAVA_REC_ENCRYPT_AES		0x1A						//录像加密标志

#define WIRELESS_NET				"apcli0"
#define WIRED_NET					"eth0.1"
#if 1
#define BING_NET					"br-lan"
#else
#define BING_NET					"eth0.1"
#endif

#define VAVA_STREAM_MAIN			0
#define VAVA_STREAM_SUB				1

#define VAVA_ENCODE_H264			0
#define VAVA_ENCODE_H265			1

//低电量阀值
#define CAMERA_LOW_POWER			10
#define CAMERA_UPDATE_LOW_POWER		15

//录像延时开关
#define REC_DELAY_TIME				30

//测试音频和对讲 将收到的音频数据写入对进BUFF
//由stream_start信令开启
#if 0 
#define AUDIO_TALK_LO_TEST
#endif

//支持最大通道数
//#define MAX_CHANNEL_NUM				6
#define MAX_CHANNEL_NUM				4

//支持最大会话数
#define MAX_SESSION_NUM				6

//支持最大搜索摄像机保存数
#define MAX_SEARCH_CAMERA_NUM		10

//录像分包条数
#define REC_PACKAGE_NUM				500

#define P2P_CHANNEL_CMD				0
#define P2P_CHANNEL_AV				1
#define P2P_CHANNEL_RECAV			2
#define P2P_CHANNEL_IMG				3

//尚云缓冲BUFF大小
#define SYP2P_CMD_BUFF_SIZE			50000
#define SYP2P_VIDEO_NORMAL_SIZE		200000
#define SYP2P_VIDEO_LOSTP_SIZE		350000
#define SYP2P_VIDEO_LOSTALL_SIZE	500000
#define SYP2P_AUDIO_BUFF_SIZE		50000

#define SYP2P_AUTOVQ_NOMARL_SIZE	100000
#define SYP2P_AUTOVQ_CHANGE_SIZE	250000
#define SYP2P_AUTOVQ_LOSTP			350000
#define SYP2P_AUTOVQ_LOSTALL		500000

//每日布防时间条数
#define MAX_ARM_TIME_LIST			4
#define MAX_ARM_LIST_NUM			10 //7天 x 4条 最多10条

typedef enum{
	VAVA_STREAM_TYPE_MAIN=0,		//(0)主码流
	VAVA_STREAM_TYPE_SUB,			//(1)子码流
	VAVA_STREAM_TYPE_AUDIO,			//(2)音频流
	VAVA_STREAM_TYPE_IMG,			//(3)图片流
	VAVA_STREAM_TYPE_UPGRATE,		//(4)升级流
	VAVA_STREAM_TYPE_AUDIO1=8,		//(8)录像音频
	VAVA_STREAM_TYPE_AUDIO2			//(9)实时音频
}VAVA_STREAM_TYPE;

//星期
typedef enum{
	VAVA_WEEKDAY_SUN,				//(0)星期天
	VAVA_WEEKDAY_MON,				//(1)星期一
	VAVA_WEEKDAY_TUE,				//(2)星期二
	VAVA_WEEKDAY_WED,				//(3)星期三
	VAVA_WEEKDAY_THU,				//(4)星期四
	VAVA_WEEKDAY_FRI,				//(5)星期五
	VAVA_WEEKDAY_SAT,				//(6)星期六
	VAVA_WEEKDAYS
}VAVA_WEEKDAY;

//语言
typedef enum{
	VAVA_LANGUAGE_ENGLIST = 0,		//(0)英语
	VAVA_LANGUAGE_CHINESE			//(1)中文
}VAVA_LANGUAGE;

#if 0
//按键状态
typedef enum{
	VAVA_BUTTON_KEY_VOLUME = 0,		//(0)SYNC键
	VAVA_BUTTON_KEY_RING,			//(1)WIFI键
	VAVA_BUTTON_KEY_DOUBLE			//(2)同时按下
}VAVA_BUTTON_KEY;
#else
typedef enum{
	VAVA_BUTTON_KEY_SYNC = 0,		//(0)SYNC键
	VAVA_BUTTON_KEY_RESET,			//(1)WIFI键
	VAVA_BUTION_DOUBLE				//(2)同时按下
}VAVA_BUTTON_KEY;
#endif

//LED灯
typedef enum{
	VAVA_LED_RED = 0,				//红色
	VAVA_LED_WHITE					//白色
}VAVA_LED_ID;

typedef enum{
	VAVA_LED_SLAKE = 0,				//(0)熄灭
	VAVA_LED_LIGHT,					//(1)点亮
	VAVA_LED_FAST_FLASH				//(3)快闪
}VAVA_LED_CTRL;

//蜂鸣器
typedef enum{
	VAVA_BUZZER_TYPE_CLOSE=0,		//(0)不鸣叫
	VAVA_BUZZER_TYPE_OPEN,			//(1)持续鸣叫
	VAVA_BUZZER_TYPE_INTERVAL		//(2)间隔性鸣叫
}VAVA_BUZZER_TYPE;

#ifdef FREQUENCY_OFFSET
//摄像机与基站配对信息
typedef struct _pairinfo_old{
	int nstat;						//摄像机配对状态
	int lock;						//锁定
	int ipaddr;						//摄像机IP地址
	unsigned int addr;				//摄像机短波地址
	char mac[18];					//摄像机MAC地址
	char id[32];					//摄像机序列号
}VAVA_Pair_info_old;
#endif

//摄像机与基站配对信息(新版本)
typedef struct _pairinfo{
	int nstat;						//摄像机配对状态
	int lock;						//锁定
	int ipaddr;						//摄像机IP地址
	unsigned int addr;				//摄像机短波地址
	char mac[18];					//摄像机MAC地址
	char id[32];					//摄像机序列号
#ifdef FREQUENCY_OFFSET
	unsigned int index;				//摄像机频段下标
#endif
}VAVA_Pair_info;

//基站同步列表信息
typedef struct _pairsync{
	int nstat;						//状态
	int type;						//配对方式
	char sn[32];					//序列号
}VAVA_Pair_Sync;

//音视频缓冲buff
#define MEM_MAIN_QUEUE_NUM			200 //视频15帧 单频8帧 (约8秒缓存)
#define MEM_SUB_QUEUE_NUM			80 	//视频15帧 音频8帧 (约3秒缓存)
#define MEM_AUDIO_QUEUE_NUM			75  //音频25帧 (约3秒缓存)

#define MEM_AUDIO_SIZE       		10000
#define MEM_MAIN_VIDEO_SIZE			1200000
#define MEM_SUB_VIDEO_SIZE       	400000

#define MEM_AV_SIZE					250000
#define MEM_AV_RECV_SIZE			MEM_AV_SIZE
#define MEM_AV_CLOUD_SIZE			MEM_AV_SIZE
#define MEM_AV_NAS_SIZE				MEM_AV_SIZE
#define MEM_AV_PLAY_SIZE			MEM_AV_SIZE
#define MEM_AV_IMG_SIZE				MEM_AV_SIZE

struct _avunit{
	int offset;
	int size;
	int ftype;
	int fps;
	int framenum;
#ifdef FRAME_COUNT
	int count;
#endif
	unsigned long long ntsamp;
};

typedef struct av_queue_unit{
	struct _avunit mvlists[MEM_MAIN_QUEUE_NUM];	//录像流
	struct _avunit svlists[MEM_SUB_QUEUE_NUM];	//预览流
	struct _avunit aplists[MEM_AUDIO_QUEUE_NUM];//对讲流

	short mv_read;	//I帧位置
	short sv_read;	//I帧位置
	short ap_read;
	
  	short mv_write;
	short sv_write;
	short ap_write;
	
	short mv_nstats;
	short sv_nstats;
	short ap_nstats;
	
 	char *pmvMemBegin, *pmvMemEnd, *pmvMemFree;
	char *psvMemBegin, *psvMemEnd, *psvMemFree;
	char *papMemBegin, *papMemEnd, *papMemFree;
}VAVA_AVmemchace;

typedef struct av_recvmem{
	unsigned char *buff;
}VAVA_AVRecvMem;

typedef struct av_cloudmem{
	unsigned char *buff;
}VAVA_AVCloudMem, VAVA_AVNasMem, VAVA_AVPlayMem;

//基站信息
typedef struct _gateway_info{
	int nettype;					//网络连接状态
	int signal;						//与路由器连接信号
	int language;					//语言
	int sdstatus;					//SD卡状态
	int format_flag;				//SD卡需要格式化标记
	int totol;						//SD卡总容量(MB)
	int used;						//SD卡已使用空间(MB)
	int free;						//SD卡剩余空间(MB)
#ifdef NAS_NFS
	int nas_totol;					//Nas总容量(MB)
	int nas_used;					//Nas已使用空间(MB)
	int nas_free;					//Nas剩余空间(MB)
#endif
	unsigned int rfaddr;			//短波芯片地址		
	int netch;						//wifi信道
	
	char sn[32];					//序列号
	char releasedate[12];			//发布日期
	char hardver[16];				//硬件版本号
	char softver[16];				//软件版本号
	char mac[18];					//MAC地址
	char ipstr[16];					//IP地址
	char ssid[16];					//基站SSID
	char pass[16];					//基站密码
	char rfver[16];					//短波版本号
	char rfhw[16];					//短波硬件版本号
	char domain_user[64];			//服务器域名(用户)
	char domain_factory[64];		//服务器域名(生产)
	char token[64];					//服务器token
	char sydid[32];					//尚云did
	char apilisence[16];			//尚云APIlisence
	char crckey[16];				//尚云厂商校验码
	char initstr[128];				//尚云服务器初始化字串
}VAVA_GateWayInfo;

typedef struct _camera_info{
	unsigned char videocodec;		//视频编码方式(0 all-H264  1-rec 264 live265)
	unsigned char audiocodec;		//音频编码方式(0 PCM 1 G711A  2 G711U  3 AAC  4 OPUS)

	unsigned char mic;				//MIC状态	
	unsigned char speeker;			//喇叭状态

	unsigned char m_res;			//主码流分辨率(录像)
	unsigned char m_fps;			//主码流帧率  (录像)
	unsigned char s_res;			//次码流分辨率(预览)
	unsigned char s_fps;			//子码流帧率  (预览)
	
	unsigned char a_fps;			//音频帧率
	unsigned char a_bit;			//音频采样位宽
	unsigned char channel;			//音频声道(1 单声道  2 立体声)
	unsigned char reserv_1;			//保留对齐
	
	unsigned char mirror;			//视频翻转
	unsigned char irledmode;		//红外灯模式
	unsigned char osdmode;			//osd模式
	unsigned char v_quality;		//视频质量(预览)

	unsigned char powermode;		//电源类型(电池或外接电源)
	unsigned char pirstatus;		//PIR状态
	unsigned char mdctrl;			//移动侦测开关
	unsigned char md_sensitivity;	//移动侦测灵敏度
	
	unsigned char md_startx;		//水平方向起始位置
	unsigned char md_endx;			//水平方向结束位置
	unsigned char md_starty;		//垂直方向起始位置
	unsigned char md_endy;			//垂直方向结束位置

	unsigned char wakeup_status;	//唤醒状态
	unsigned char sleep_status;		//休眠状态
	unsigned char tfset_status;		//T卡状态
	unsigned char sleep_check;		//休眠检测

	unsigned char pirget_status;	//pir获取状态
	unsigned char pirset_status;	//pir设置状态
	unsigned char heart_status;		//心跳状态
	unsigned char wifi_heart;		//wifi心跳

	unsigned char first_flag;		//初次连接标记
	unsigned char wakeup_flag;		//唤唤标志
	unsigned char sleep_flag;		//休眠标志
	unsigned char alarm_flag;		//报警标志 

	unsigned char cloud_flag;		//云存标志
	unsigned char config_flag;		//配置标志
	unsigned char poweroff_flag;	//关机标志
	unsigned char up_flag;			//升级标志

	unsigned char wifi_num;			//wifi心跳数
	unsigned char wake_fail;		//唤配失败次数
	unsigned char reserv_2[2];		//保留
	
	unsigned short m_bitrate;		//主码流码率
	unsigned short s_bitrate;		//次码流码率
		
	unsigned int samprate;			//音频采样率
	
	unsigned char softver[16];		//软件版本号		
	unsigned char hardver[16]; 		//硬件版本号
	unsigned char sn[32];			//摄像机序列号
	unsigned char rfver[16];		//短波版本号
	unsigned char rfhw[16];			//短波硬件版本号
}VAVA_CameraInfo;

typedef struct _camera_name{
	int nstate;
	char name[32];
}VAVA_CameraName;

//录像时长信息
typedef struct _rectime{
	int full_time;					//全时录像(计划录像)
	int alarm_time;					//报警录像
	int manaua_time;				//手动录像
	unsigned char fulltime_ctrl[MAX_CHANNEL_NUM]; //全时录像开关
}VAVA_RecTime;

//录像回放管理
typedef struct _recplay{
	FILE *fd;
	int sesssion;
	int token;
	int type;
	int ctrl;
	int v_encode;
	int a_encode;
	int fps;
	int res;
	int encrypt;
	char dirname[9];
	char filename[11];
}VAVA_RecPlay;

//录像文件信息
typedef struct _recinfo{
	char tag;						//0 录像创建  1录像完成
	char v_encode;					//视频编码格式
	char a_encode;					//音频编码格式
	char res;						//分辨率
	char fps;						//帧率
	char encrypt;					//加密方式 0 不加密 0x1A AES加密
	unsigned short vframe;			//视频帧数
	int size;						//录像大小
	int time;						//录像时长
}VAVA_RecInfo;

//分辨率配置
typedef struct _resconfig{
	unsigned int stream;			//码流类型(0 主码流 1 子码流)
	unsigned int res;				//分辨率
	unsigned int fps;				//帧率
	unsigned int bitrate;			//码率
	unsigned int sessionid;			//会话ID
	unsigned int channel;			//通道号
	unsigned int result;			//返回值
}VAVA_ResConfig;

//视频翻转状态配置
//红外模式配置
//PIR开关配置
//抓图
typedef struct _mirrconfig{
	int param;						//翻转状态/红外灯模式/PIR开关/PIR灵敏度
	unsigned int sessionid;			//会话ID
	unsigned int channel;			//通道号
	unsigned int result;			//返回值
}VAVA_MirrConfig, VAVA_IrLedConfig, VAVA_PirConfig;

typedef struct _ipccomconfig{
	unsigned int ctrl;				//控制命令
	unsigned int sessionid;			//会话ID
	unsigned int channel;			//通道号
	unsigned int result;			//返回值
}VAVA_IPCComConfig;

typedef struct _mdconfig{
	unsigned int enabel;			//移动侦测开关
	unsigned int startx;			//水平方向起始位置
	unsigned int endx;				//水平方向结束位置
	unsigned int starty;			//垂直方向起始位置
	unsigned int endy;				//垂直方向结束位置
	unsigned int sensitivity;		//移动侦测灵敏度
	unsigned int sessionid;			//会话ID
	unsigned int channel;			//通道号
	unsigned int result;			//返回值
}VAVA_MdConfig;

typedef struct _snapshot{
	unsigned int flag;
	unsigned int sessionid;
}VAVA_Snapshot;

typedef struct _clearcamera{
	unsigned int addr;
#ifdef FREQUENCY_OFFSET
	unsigned int index;
#endif	
}VAVA_ClearCamera;

typedef struct _recsearch{
	char date[9];					//搜索日期
	int type;						//搜索类型
}VAVA_RecSearch;

struct _recsharedatelist{
	int channel;					//搜索通道
	char startdate[9];				//搜索起始日期
};

typedef struct _recsharedate{
	struct _recsharedatelist list[MAX_CHANNEL_NUM];
}VAVA_RecShareDateSearch;

typedef struct _recsharelist{
	char date[9];					//搜索日期
	int type;						//搜索类型
	int channel[MAX_CHANNEL_NUM];	//搜索通道
}VAVA_RecShareListSearch;

typedef struct _recimgget{
	char dirname[9];				//文件日期
	char filename[11];				//文件名
}VAVA_RecImgGet;

typedef struct _wificonfig{
	char ssid[32];					//路由器SSID
	char pass[32];					//路由器密码
}VAVA_WifiConfig;

typedef struct _rechead{
	unsigned int tag;				//同步头 0xEB0000AA
	unsigned int size;				//帧大小
	unsigned int type;				//帧类型 0 P帧 1 I帧 8音频帧
	unsigned int fps;				//实时帧率
	unsigned int time_sec;			//时间戳(秒)
	unsigned int time_usec;			//时间戳(毫秒)
}VAVA_RecHead;

typedef struct _recdel{
	int type;						//删除类型
	char dirname[9];				//文件夹
	char filename[11];				//文件名
}VAVA_RecDel;

typedef struct _sysupdate{
	int sessionid;					//会话ID
	int status;						//升级状态
	unsigned int type;				//升级包类型
	                                //11 基站 
	                                //12 摄像机
	int loading;					//下载进度
	int current;					//当前升级的摄像机通道
	int wait;						//等待摄像机回应
	int result;						//摄像机升级结果 (0 成功 非0失败)
	int upchannel[MAX_CHANNEL_NUM]; //需升级的摄像机通道
	char url[256];					//URL地址
}VAVA_Update;

typedef struct _camerafactory{
	unsigned int sessionid;			//会话ID
	unsigned int channel;			//通道号
	unsigned int result;			//返回值
}VAVA_CameraFactory, VAVA_CameraRecCtrl;

typedef struct _camerapair{
	unsigned int sessionid;			//会话ID
	unsigned int addr;				//RF地址
	int channel;					//通道号
#ifdef FREQUENCY_OFFSET
	unsigned int index;				//摄像机频段下标
#endif
}VAVA_CameraPair;

#define OTA_HEAD_SIZE				16
#define OTA_FWINFO_SIZE				16
#define OTA_UPMEM_SIZE				50000

#if 0
typedef struct _otahead{
	int tag;						//同步头 0xEB000008
	int type;						//升级类型
	int filesize;					//文件大小
	int crc32;						//crc校验值
}VAVA_OtaHead;
#else
typedef struct _otahead{
	int tag;						//同步头 0xEB000008
	int otatype;					//升级类型(基站和摄像机)
	int totolsize;					//总大小
	int upnum;						//升级固件个数
}VAVA_OtaHead;

typedef struct _frhead{
	int tag;						//同步头 0xEB000008
	int fwtype;						//固件包类型(区分固件和短波)
	int filesize;					//固件包大小
	int crc32;						//固件包校验值
}VAVA_FwHead;
#endif

typedef struct _otainfo{
	int init_flag;
	
	int headsize;
	int fw1infosize;
	int fw2infosize;

	int fw1wsize;
	int fw2wsize;

	int rfflag;
	int bsflag;
	int memsize;

	char headbuff[OTA_HEAD_SIZE];
	char fwinfo_1[OTA_FWINFO_SIZE];
	char fwinfo_2[OTA_FWINFO_SIZE];

	FILE *upfd1;
	FILE *upfd2;

	char *upmem;
}VAVA_OtaInfo;

typedef enum{
	VAVA_REC_STATUS_IDLE=0,			//(0)空闲
	VAVA_REC_STATUS_RECODING,		//(1)录像中
	VAVA_REC_STATUS_STOP			//(2)停止录像
}VAVA_REC_STATUS;

//录像管理
typedef struct _recmanage{
	int status;						//录像状态  VAVA_REC_STATUS
	int ctrl;						//控制命令
	int start;						//开始标记
	int session;					//会话名柄
	FILE *fd;						//文件句柄
	char dirname[9];
	char filename[11];
}VAVA_Rec_Manage;

typedef struct _recmanage_ex{
	int status;						//录像状态
	int ctrl;						//控制命令
	int type;						//录像类型
	int start;						//起始标记
	int read;						//read节点
	FILE *fd;						//文件句柄
	char dirname[9];
	char filename[11]; 
}VAVA_Rec_Manage_Ex;

typedef struct _idxhead{
	int tag;						//同步头 0xEB0000BB
	int totol;						//总目录数
	int first;
}VAVA_Idx_Head;

typedef struct _idxdate{
	char random;					//随机填充码 1-100 + 0x1A
	char dirname[11];				//文件夹
}VAVA_Idx_Date;

typedef struct _idxfile{
	char random;					//随机填充码 1-100 + 0x1A
	char type;						//类型
	char channel;					//通道
	char filename[11];				//文件名
	short rectime;					//时长
}VAVA_Idx_File;

//网络切换消息队列
struct _msgdata{
	int nettype;					//网络类型
	int ntsamp;						//时间戳
};

typedef struct _netmsg{  
    long msg_type;  				//消息类型
   	struct _msgdata msgdata;  		//消息数据
}VAVA_NetMsg; 

//基站搜索到的处于配对状态的摄像机列表
struct _searchdata{
	unsigned int flag;				//0 未使用 1 已使用
	unsigned int addr;				//RF地址
	unsigned int time;				//添加时间
	char sn[32];					//序列号
};

typedef struct _searchcameralist{
	unsigned int totol;				//总数
	unsigned int flush;				//清空flag
	struct _searchdata data[MAX_SEARCH_CAMERA_NUM]; 
}VAVA_SearchCamera;

typedef struct _onlinestatus{
	int online;
	int battery;
	int voltage;
#ifdef BATTEY_INFO
	int temperature;			//电池温度
	int electricity;			//电池电流
#endif
}VAVA_OnlineStatus;

typedef struct _toolupdate{
	int tag;
	int cmd;
	int totol;
	int size;
	int num;
	int crc;
}VAVA_ToolUpdataHead;

typedef enum{
	VAVA_TOOLUPDATE_CMD_BEGIN = 10,
	VAVA_TOOLUPDATE_CMD_DATA,
	VAVA_TOOLUPDATE_CMD_END,
	VAVA_TOOLUPDATE_CMD_UPGRADING,
	VAVA_TOOLUPDATE_CMD_UPSUCCESS,
	VAVA_TOOLUPDATE_CMD_UPFAIL
}VAVA_TOOLUPDATE_CMD;

typedef enum{
	VAVA_TOOLUPDATE_ERRCODE_SUCCESS = 0,//成功
	VAVA_TOOLUPDATE_ERRCODE_NOIDLE,		//已经处于升级模式
	VAVA_TOOLUPDATE_ERRCODE_OPENFIAL,	//创建升级文件失败
	VAVA_TOOLUPDATE_ERRCODE_NOUPGRATE,	//未处于升级模式
	VAVA_TOOLUPDATE_ERRCODE_NUMFAIL,	//升级包序号检测失败
	VAVA_TOOLUPDATE_ERRCODE_CRCFAIL,	//升级包CRC校验失败
	VAVA_TOOLUPDATE_ERRCODE_WRITEFAIL,	//升级包写入失败
	VAVA_TOOLUPDATE_ERRCODE_UPGRADING,	//升级中
	VAVA_TOOLUPDATE_ERRCODE_UPSUCCESS,	//升级成功
	VAVA_TOOLUPDATE_ERRCODE_UPFAIL		//升级失败
}VAVA_TOOLUPDATE_ERRCODE;

typedef enum{
	VAVA_AVMEM_MODE_ALL=0,				//全部
	VAVA_AVMEM_MODE_REC,				//仅录像
	VAVA_AVMEM_MODE_LIVE				//仅实时流
}VAVA_AVMEM_MODE;

//nas服务器参数
typedef struct _nasconfig{
	int ctrl;
	char ip[16];
	char path[64];
}VAVA_NasConfig;

typedef struct _nassync{
	int tag;
	int sync;
}VAVA_NasSync;

//推送信息
typedef struct _pushflag{
	int push;
	int email;
}VAVA_PushFlag;

//时区参数
typedef struct _ntpinfo{
	int ntp;
	char str[256];
}VAVA_NtpInfo;
/*                         基站内部数据结构                       */
#endif

#define GATEWAY_CAMERADATA
#ifdef GATEWAY_CAMERADATA
/*                        基站与摄像机交互                        */
//socket定义
#define SERVER_CMD_PORT				16154 
#define SERVER_AV_PORT				18154
#define SERVER_AP_PORT				27499

#define WAKE_UP_MODE_APP			0
#define WAKE_UP_MODE_PIR			1
#define WAKE_UP_CLEAR_IPC			2

//命令头定义
#define CMD_DATA_SIZE				116

typedef struct _vava_msg{
	unsigned int tag;				//同步头 0xEB000001
	unsigned int crc32;				//校验值(对命令字+参数进行共124字节进行校验)
	unsigned int comtype;			//命令字
	char data[CMD_DATA_SIZE];		//参数
}VAVA_Msg;

//音视频传输头定义
typedef struct _vava_avhead{
	int tag;						//同步头 0xEB000002
		
	unsigned char streamtype;		//流类型 0 主码流 1 子码流 2 音频 3 图片 4升级数据
	unsigned char encodetype;		//音视频编码类型
                     				//视频：0 H264  1 H265
									//音频：0 PCM  1 G711A  
									//      2 G711U  3 AAC  4 OPUS
	unsigned char frametype;		//帧类型 0 P帧  1 I帧  8音频帧
	unsigned char framerate;		//帧率

	unsigned char res;				//分辨率
	unsigned char framenum;			//帧号(I帧为0 P帧累加)
	unsigned char version;			//最低位为1时需要检测帧
	unsigned char reserv;			//保留
			
	unsigned int size;				//帧大小
	unsigned long long ntsamp;		//时间戳(毫秒)
}VAVA_Avhead;

//命令字定义
typedef enum{
	VAVA_MSGTYPE_HEARTBEAT = 0,		//(0)心跳
	VAVA_MSGTYPE_PAIR_REQ ,			//(1)摄像机请求配对
	VAVA_MSGTYPE_PAIR_RESP,			//(2)基站回复配对所需要信息
	VAVA_MSGTYPE_PAIR_ACK,			//(3)摄像机配对成功确认
	VAVA_MSGTYPE_NOPAIR_RESP,		//(4)摄像机没有与基站配对
	VAVA_MSGTYPE_FIRST_CONNECT,		//(5)摄像机初次连接基站
	VAVA_MSGTYPE_CONNECT_ACK,		//(6)摄像机连接后基站确认
	VAVA_MSGTYPE_TIME_SYNC,			//(7)时间同步
	VAVA_MSGTYPE_STREAM_START,		//(8)开始音视频流
	VAVA_MSGTYPE_STREAM_STOP,		//(9)停止音视频流
	VAVA_CONFIG_SET_RESOLUTION,		//(10)设置分辨率
	VAVA_CONFIG_SET_MIRROR,			//(11)设置视频翻转
	VAVA_CONFIG_SET_IRLEDMODE,		//(12)设置红外灯状态
	VAVA_CONFIG_SET_PIRCTRL,		//(13)设置PIR开关
	VAVA_CONFIG_SET_MDPARAM,		//(14)设置移动侦测参数
	VAVA_CONFIG_SET_OSDMODE,		//(15)设置OSD模式
	VAVA_CONFIG_SET_POWERFREQ,		//(16)设置电源模式
	VAVA_CONFIG_SET_PIRSITIVITY,	//(17)设置PIR灵敏度
	VAVA_CONFIG_SET_AUDIO,			//(18)设置音频状态
	VAVA_CONFIG_SET_SPEAKER,		//(19)设置嗽叭状态	
	VAVA_CONFIG_INSERT_IFRAME,		//(20)请求I帧
	VAVA_CONFIG_SNAPSHORT,			//(21)抓图
	VAVA_CONFIG_RESET_FACTORY,		//(22)恢复出厂
	VAVA_CONFIG_UPDATE_REQ,			//(23)升级请求
	VAVA_CONFIG_UPDATE_RESP,		//(24)升级确认
	VAVA_CONFIG_UPDATE_TRANSEND,	//(25)发送完成
	VAVA_CONFIG_UPDATE_TRANSFAIL,	//(26)发送失败
	VAVA_CONFIG_UPDATE_TRANSACK,	//(27)完成确认
	VAVA_CONFIG_UPDATE_RESULT,		//(28)升级结果回复
	VAVA_CONFIG_START_REC=100,		//(100)开启录像
	VAVA_CONFIG_STOP_REC,			//(101)关闭录像
	VAVA_MSGTYPE_MD_RESULT=200,		//(200)移动侦测结果返回
	VAVA_MSGTYPE_OTA_INFO,			//(201)OTA信息
	VAVA_MSGTYPE_RFHW_INFO,			//(202)RF硬件版本号
	VAVA_MSGTYPE_COM_CTRL = 300,	//(300)串口控制
}VAVA_MSGTYPE;

//配对请求
typedef struct _pair_req{
	int channel;
	char sn[32];
#ifdef FREQUENCY_OFFSET
	unsigned int flag;				//新旧版本  0 旧版本 1 新版本
	unsigned int index;				//摄像机频段下标
#endif
}VAVA_Pair_Req;

//配对回复
typedef struct _pair_resp{
	int result;						//配对结果 0 成功 非0失败
	unsigned int addr;				//基站短波芯片地址
#ifdef FREQUENCY_OFFSET
	unsigned int index;				//摄像机频段下标
#endif
}VAVA_Pair_Resp;

//基站热点信息
typedef struct _hotpot_info{
	unsigned char ssid[32];			//基站SSID
	unsigned char pass[32];			//基站密码
	int type;	
}VAVA_HotPot_Info;

//初次连接基站
typedef struct _first_connect{
	unsigned char videocodec;		//视频编码方式(0 all-H264  1-rec 264 live265)
	unsigned char audiocodec;		//音频编码方式
	unsigned char reserve_1;		//保留
	unsigned char mdctrl;			//移动侦测开关
	
	unsigned char m_fps;			//主码流帧率(录像)
	unsigned char s_fps;			//子码流帧率(预览)
	unsigned char a_fps;			//音频帧率
	unsigned char a_bit;			//音频采样位宽

	unsigned char m_res;			//主码流分辨率(录像)
	unsigned char s_res;			//次码流分辨率(预览)
	unsigned char channel;			//音频声道(1 单声道  2 立体声)
	unsigned char reserve_2;		//保留

	unsigned char mirror;			//视频翻转
	unsigned char irledmode;		//红外灯模式
	unsigned char osdmode;			//osd模式
	unsigned char powerfreq;		//电源频率 50Hz or 60Hz
	
	unsigned char reserve_3;		//保留
	unsigned char md_sensitivity;	//移动侦测灵敏度
	unsigned char powermode;		//电源类型(电池或外接电源)
	unsigned char pirstatus;		//PIR触发状态
	
	unsigned char md_startx;		//水平方向起始位置
	unsigned char md_endx;			//水平方向结束位置
	unsigned char md_starty;		//垂直方向起始位置
	unsigned char md_endy;			//垂直方向结束位置

	unsigned short m_bitrate;		//主码流码率
	unsigned short s_bitrate;		//次码流码率
		
	unsigned int samprate;			//音频采样率
	
	unsigned char softver[16];		//软件版本号		
	unsigned char hardver[16]; 		//硬件版本号
	unsigned char sn[32];			//摄像机序列号
	unsigned char rfver[16];		//短波版本号
}VAVA_First_Connect;

//摄像机短波硬件版本号
typedef struct _ipcrfhw{
	char version[16];
}VAVA_IPC_RFHW;

typedef struct _ipcotainfo{
	char f_code[36];		//固件code
	char f_secret[36];		//固件sercret;
	char f_inver[8];		//固件对内版本号
	char f_outver[8];		//固件对外版本号
}VAVA_IPC_OtaInfo;

//摄像机动态信息
typedef struct _dynamic_info{
	unsigned short battery;			//空载电量
	unsigned short batteryload;		//满载电量
	short online;					//在线状态
	short signal;					//信号
	int voltage;					//电池电压
#ifdef BATTEY_INFO
	int temperature;				//电池温度
	int electricity;				//电池电流
#endif
}VAVA_Camera_Dynamic;

//时间同步
typedef struct _time_sync{
	int sec;						//秒
	int usec;						//微秒
	unsigned char flag;				//更新标志 0xEE
	unsigned char sleep;			//休眠标志
	unsigned char reserv[2];		//保留对齐
}VAVA_Time_Sync;

//升级回复
typedef struct _upgrate_status{
	int status;						//结果 0 成功 非0失败
	int size;						//文件大小
}VAVA_Upgrate_Status, VAVA_Upgrate_Crc32;

//升级回复状态
typedef enum{
	VAVA_CAMERA_UPDATE_SUCCESS=0,	//升级成功
	VAVA_CAMERA_UPGRATING,			//升级
	VAVA_CAMERA_UPDATE_CRCFAIL,		//升级包校验失败	
	VAVA_CAMERA_UPDATE_FAIL			//升级失败
}VAVA_CAMERA_UPDATE_STATUS;

//移动侦测结果
typedef struct _mdresult{
	int result;
}VAVA_MdResult;

//强插I帧
typedef struct _insertiframe{
	unsigned int stream;			//0 主码流 1子码流
	unsigned int result;			//返回值
}VAVA_Insertiframe;

//WIFI心跳
typedef struct _wifisigle{
	int sig;						//信号强度
	unsigned char batLevel;			//电量
	unsigned char adapter;			//充电状态
	unsigned char reserv[2];		//保留
	int voltage;					//电压
#ifdef BATTEY_INFO
	int temperature;				//电池温度
	int electricity;				//电池电流
#endif
	int tfcard;						////0:旧版本 1:有卡 2 无卡
}VAVA_WifiSigle;
/*                        基站与摄像机交互                        */
#endif

#define GATEWAY_P2PDATA
#ifdef GATEWAY_P2PDATA
/*                          基站与APP交互                         */
//信令头
typedef struct _sy_head{
	unsigned int sync_code; 		//同步码  0xEB000003
	unsigned int cmd_code;			//信令码 
	unsigned int cmd_length;		//信令内容数据长度(JSON数据长度)
                                    //无参数填0且不传JSON数据
}VAVA_CMD_HEAD;		

//音视频传输头
typedef struct _avhead{
	int tag;						//同步头 
									//实时视频流同步头 0xEB000011
									//实时音频流同步头 0xEB000022
									//对讲音频流同步头 0xEB000004
									//回放视频同步头   0xEB000009
									//回放音频同频头   0xEB000010

	unsigned char encodetype; 		//编码类型
									//视频: 0 H264  1 H265
									//音频: 0 PCM  1 G711A  2 G711U  3 AAC  4 OPUS
	unsigned char frametype;  		//帧类型 0 P帧  1 I帧  8 音频帧
	unsigned char framerate;		//帧率
	unsigned char res; 				//分辨率(仅视频有效)
	unsigned int size; 				//帧大小
	unsigned int ntsamp_sec;		//时间戳(秒)
	unsigned int ntsamp_usec;		//时间戳(毫秒)
	unsigned int framenum;			//帧序号
}VAVA_AV_HEAD;

//图片传输头
typedef struct _imgheads{
	int tag;						//同步码 0xEB000005
	int channel;					//摄像机通道号
	unsigned int type;				//类型 0(I帧数据)  1(图片)
	unsigned int size;				//数据大小
	unsigned int result;			//0 成功 非0失败
	char date[9];					//日期(年月日)
	char file[11]; 					//时间(时分秒)
}VAVA_IMG_HEAD;

//命令字
typedef enum{
	VAVA_CMD_SESSION_AUTH=0,		//(0)会话认证
	VAVA_CMD_WAKE_UP,				//(1)唤醒摄像机
	VAVA_CMD_SLEEP,					//(2)休眠摄像机
	VAVA_CMD_OPENVIDEO, 			//(3)开启视频流
	VAVA_CMD_CLOSEVIDEO, 			//(4)半闭视频流
	VAVA_CMD_AUDIO_OPEN,			//(5)开启音频流
	VAVA_CMD_AUDIO_CLOSE,			//(6)关闭音频流
	VAVA_CMD_TALK_OPEN,				//(7)开启对讲
	VAVA_CMD_TALK_CLOSE,			//(8)关闭对讲
	VAVA_CMD_MIC_OPEN,				//(9)开启麦克风
	VAVA_CMD_MIC_CLOSE,				//(10)关闭麦克风
	VAVA_CMD_SPEAKER_OPEN,			//(11)开启喇叭
	VAVA_CMD_SPEAKER_CLOSE,			//(12)关闭喇叭
	VAVA_CMD_BUZZER_OPEN,			//(13)开启蜂鸣器
	VAVA_CMD_BUZZER_CLOSE,			//(14)关闭蜂鸣器

	VAVA_CMD_GATEWAYINFO=100,		//(100)获取基站信息
	VAVA_CMD_CAMERAINFO,			//(101)获取摄像机信息
	VAVA_CMD_SDINFO,				//(102)获取SD卡信息
	VAVA_CMD_GET_BATTERY,			//(103)获取电池电量
	VAVA_CMD_GET_ARMINGSTATUS,		//(104)获取摄像机布防状态
	VAVA_CMD_GET_DEVINFO,			//(105)获取心跳信息
	VAVA_CMD_GET_CAMERACONFIG,		//(106)获取摄像机配置信息

	VAVA_CMD_GET_VIDEO_QUALITY=201,	//(201)获取视频质量
	VAVA_CMD_SET_VIDEO_QUALITY,		//(202)设置视频质量
	VAVA_CMD_GET_MIRRORMODE,		//(203)获取视频翻转状态
	VAVA_CMD_SET_MIRRORMODE,		//(204)设置视频翻转
	VAVA_CMD_GET_IRMODE,			//(205)获取夜视模式
	VAVA_CMD_SET_IRMODE,			//(206)设置夜视模式
	VAVA_CMD_GET_PUSHCONFIG,		//(207)获取消息推送配置
	VAVA_CMD_SET_PUSHCONFIG,		//(208)设置消息推送配置
	VAVA_CMD_GET_PIR_SENSITIVITY,	//(209)获取PIR灵敏度
	VAVA_CMD_SET_PIR_SENSITIVITY,	//(210)设置PIR灵敏度
#ifndef ARM_INFO_V1
	VAVA_CMD_GET_ARMINGINFO,		//(211)获取布防撤防配置信息(废弃)
	VAVA_CMD_SET_ARMINGINFO,		//(212)设置布防撤防参数(废弃)
	VAVA_CMD_ARMING_EXCLUDE,		//(213)定时布防忽略指定日期(废弃)
#else
	VAVA_CMD_RESERV211,				//(211)保留
	VAVA_CMD_RESERV212,				//(212)保留
	VAVA_CMD_RESERV213,				//(213)保留
#endif
	VAVA_CMD_RESERV214,				//(214)保留
	VAVA_CMD_GET_MDPARAM,			//(215)获取移动侦测参数
	VAVA_CMD_SET_MDPARAM,			//(216)设置移动侦测参数
	VAVA_CMD_GET_ARMINGINFO_V1,		//(217)获取布防撤防配置信息(v1.1版)
	VAVA_CMD_SET_ARMINGINFO_V1,		//(218)设置布防撤防配置信息(v1.1版)
	VAVA_CMD_GET_EMAILCONFIG,		//(219)获取邮件推送配置
	VAVA_CMD_SET_EMAILCONFIG,		//(220)设置邮件推送配置

	VAVA_CMD_GET_REC_QUALITY=301,	//(301)获取录像质量
	VAVA_CMD_SET_REC_QUALITY,		//(302)设置录像质量
	VAVA_CMD_GET_RECTIME,			//(303)获取录像时长配置信息
	VAVA_CMD_SET_RECTIME,			//(304)设置录像时长
	VAVA_CMD_GET_FULLTIMEREC_CTRL,	//(305)获取全时录像开关状态
	VAVA_CMD_SET_FULLTIMEREC_CTRL,	//(306)设置全时录像开关
	VAVA_CMD_RECORDDATE_SEARCH, 	//(307)搜索存在录像的日期
	VAVA_CMD_RECORDLIST_SEARCH,		//(308)搜索指定日期的录像 
	VAVA_CMD_RECSHAREDATE_SEARCH,	//(309)搜索存在录像的日期(分享专用)
	VAVA_CMD_RECSHARELIST_SEARCH,	//(310)搜索指定日期的录像(分享专用)
	VAVA_CMD_RECORD_IMG,			//(311)获取录像缩略图 
	VAVA_CMD_RECORD_IMG_STOP,		//(312)停止获取缩略图
	VAVA_CMD_RECORD_DEL,			//(313)删除录像文件
	VAVA_CMD_RECOCD_GETSIZE,		//(314)获取录像文件大小
	VAVA_CMD_RECORD_PLAY, 			//(315)录像回放 
	VAVA_CMD_RECORD_PLAY_CTRL,		//(316)回放控制
	VAVA_CMD_SNAPSHOT,				//(317)抓图
	VAVA_CMD_RESERV318,				//(318)保留
	VAVA_CMD_START_REC,				//(319)开始手动录像
	VAVA_CMD_STOP_REC,				//(320)停止手动录像
	
	VAVA_CMD_GET_TIME=401,			//(401)获取基站时间
	VAVA_CMD_SET_TIME,				//(402)设置基站时间
	VAVA_CMD_SET_LANGUAGE,			//(403)设置提示时语言
	VAVA_CMD_FORMAT_STORAGE,		//(404)格式化SD卡
	VAVA_CMD_WIFI_CONFIG,			//(405)切换上级路由器
	VAVA_CMD_CAMERA_PAIR,			//(406)配对摄像机
	VAVA_CMD_GET_UPDATE_STATUS,		//(407)获取升级状态
	VAVA_CMD_CLEARMATCH,			//(408)清除摄像机配对信息
	VAVA_CMD_CAMERA_RESET,			//(409)恢复摄像机默认参数
	VAVA_CMD_RESET_FACTORY,			//(410)恢复基站默认参数(完成后会重启)
	VAVA_CMD_SYSTEM_NEWVESION,		//(411)新版本更新
	VAVA_CMD_POP_TF,				//(412)弹出TF卡

	VAVA_CMD_GET_SYDID=501,			//(501)获取尚云DID信息
	VAVA_CMD_CLEAR_SYDID,			//(502)清除尚云DID信息
	VAVA_CMD_GET_DOMAN,				//(503)获取当前服务器域名
	VAVA_CMD_SET_DOMAN,				//(504)设置当前服务器域名
	VAVA_CMD_GET_P2PSERVER,			//(505)获取P2P服务器信息
	VAVA_CMD_SET_P2PSERVER,			//(506)设置P2P服务器信息
	VAVA_CMD_GET_NASSERVER,			//(507)获取NAS服务器信息
	VAVA_CMD_SET_NASSERVER,			//(508)设置NAS服务器信息
	VAVA_CMD_RESERV_509,			//(509)保留
	VAVA_CMD_CLOUD_REFRESH,			//(510)刷新云存

	VAVA_CMD_BATTERY_STATUS=601,	//(601)电池状态
	VAVA_CMD_RESERV602,				//(602)保留
	VAVA_CMD_PARSEERR,				//(603)信令解析失败基站主动断开会话
	VAVA_CMD_PLAYBACK_END,			//(604)回放结束
	VAVA_CMD_DEV_INFO,				//(605)设备动态信息
	VAVA_CMD_RECORD_DEL_RESP,		//(606)录像文件删除结果
	VAVA_CMD_SNAPSHOT_RESP,			//(607)抓图结果返回
	VAVA_CMD_UPGRATE_STATUS,		//(608)返回升级状态
	VAVA_CMD_PAIR_RESP,				//(609)配对结果返回
	VAVA_CMD_RF_TEST_WARING,		//(610)RF稳定性测试异常
	VAVA_CMD_MANUAL_REC_STOP,		//(611)手动录像止消息

	VAVA_CMD_RF_TEST=701,			//(701)RF稳定性测试
	VAVA_CMD_CHANGE_SN,				//(702)更改序列号
	VAVA_CMD_RESET_RF,				//(703)重启短波
	VAVA_CMD_CLOUDE_DEBUG,			//(704)云存debug开关
	VAVA_CMD_COM_CTRL,				//(705)基站串口控制
	VAVA_CMD_RESET_SYSTEM,			//(706)重启基站
	VAVA_CMD_IPC_COM_CTRL,			//(707)摄像机串口控制
	VAVA_CMD_RESET_UPSERVER,		//(708)重启网络升级服务器
	VAVA_CMD_LOG_LEVER,				//(709)日志等级
}VAVA_CMD_CODE;

//错误码
typedef enum{
	VAVA_ERR_CODE_SUCCESS=0,		//(0)成功
	VAVA_ERR_CODE_MEM_MALLOC_FIAL,	//(1)申请内存资源失败
	VAVA_ERR_CODE_JSON_PARSE_FAIL,	//(2)JSON解析失败
	VAVA_ERR_CODE_JSON_NODE_NOFOUND,//(3)JSON节点未找到
	VAVA_ERR_CODE_JSON_MALLOC_FIAL,	//(4)JSON资源申请失败
	VAVA_ERR_CODE_SESSION_AUTH_FAIL,//(5)会话验证失败
	VAVA_ERR_CODE_AUTH_KEY_TIMEOUT,	//(6)会话KEY超时
	VAVA_ERR_CODE_UNAUTHORIZED,		//(7)会话未授权
	VAVA_ERR_CODE_PARAM_INVALID,	//(8)参数无效
	VAVA_ERR_CODE_CHANNEL_NOCAMERA,	//(9)通道无摄像机
	VAVA_ERR_CODE_CHANNEL_USED,		//(10)通道已使用
	VAVA_ERR_CODE_CAMERA_OFFLINE,	//(11)摄像机离线
	VAVA_ERR_CODE_CAMERA_DORMANT,	//(12)摄像机未唤醒
	VAVA_ERR_CODE_CAMERA_NOREADY,	//(13)摄像机未准备好
	VAVA_ERR_CODE_MIC_CLOSED,		//(14)麦克被关闭
	VAVA_ERR_CODE_SPEAKER_CLOSED,	//(15)喇叭被关闭
	VAVA_ERR_CODE_CMD_NOSUPPORT,	//(16)不支持的信令
	VAVA_ERR_CODE_CONFIG_FAIL,		//(17)摄像机配置失败
	VAVA_ERR_CODE_CONFIG_TIMEOUT,	//(18)摄像机配置超时
	VAVA_ERR_CODE_RECFILE_NOTFOUND,	//(19)录像文件未找到
	VAVA_ERR_CODE_TIME_SET_ERR,		//(20)时间配置失败
	VAVA_ERR_CODE_SD_NOFOUND,		//(21)未找到SD卡
	VAVA_ERR_CODE_SD_NEED_FORMAT,	//(22)SD卡需要格式化
	VAVA_ERR_CODE_SD_FORMAT_FAIL,	//(23)SD卡格式化失败
	VAVA_ERR_CODE_SD_MOUNT_FAIL,	//(24)SD卡挂载失败
	VAVA_ERR_CODE_IDX_OPEN_FAIL,	//(25)打开索引文件失败
	VAVA_ERR_CODE_IDX_HEAD_ERR,		//(26)索引文件头检测失败
	VAVA_ERR_CODE_IDX_OPERAT_FAIL,	//(27)索引文件操作失败
	VAVA_ERR_CODE_UPDATE_NOIDLE,	//(28)当前处于升级状态
	VAVA_ERR_CODE_UPDATE_TYPE_ERR,	//(29)升级类型不支持
	VAVA_ERR_CODE_INTO_UPDATE_FAIL,	//(30)进入升级模式失败
	VAVA_ERR_CODE_MANUAL_REC_NOIDEL,//(31)当前通道正在手动录像
	VAVA_ERR_CODE_MANUAL_REC_FAIL,	//(32)开启手动录像失败
	VAVA_ERR_CODE_NOT_PAIRMODE,		//(33)摄像机不处于配对模式
	VAVA_ERR_CODE_PAIR_FAIL,		//(34)配对失败
	VAVA_ERR_CODE_PAIR_TIMEOUT,		//(35)配对超时
	VAVA_ERR_CODE_REPAIR,			//(36)当前摄像机已配过对
	VAVA_ERR_CODE_CHANNEL_FULL,		//(37)当前基站通道已满
	VAVA_ERR_CODE_WAKEUP_FAIL,		//(38)唤醒摄像机失败
	VAVA_ERR_CODE_WAKEUP_TIMEOUT,	//(39)唤醒摄像机超时
	VAVA_ERR_CODE_SLEEP_FAIL,		//(40)休眠摄像机失败
	VAVA_ERR_CODE_SLEEP_TIMEOUT,	//(41)休眠摄像机超时
	VAVA_ERR_CODE_RECPLAY_REREQ,	//(42)当前会话已开启一路回放
	VAVA_ERR_CODE_RECPLAY_NOIDLE,	//(43)无空闲回放通道
	VAVA_ERR_CODE_RECPLAY_FAIL,		//(44)录像回放失败
	VAVA_ERR_CODE_RECFILE_DAMAGE,	//(45)录像文件损坏
	VAVA_ERR_CODE_TOKEN_FAIL,		//(46)录像回放控制token校验失败
	VAVA_ERR_CODE_RFTEST_BESTART,	//(47)RF测试正在进行中
	VAVA_ERR_CODE_RECSTART_ALARM,	//(48)报警录像中无法开启手动录像
	VAVA_ERR_CODE_RECSTART_NOIDLE,	//(49)有其它会话正在手动录像
	VAVA_ERR_CODE_RECSSTOP_NOOPEN,	//(50)手动录像未开启
	VAVA_ERR_CODE_IP_NOTCONNECT,	//(51)IP地址无法连接
	VAVA_ERR_CODE_PAHT_INVALID,		//(52)路径不合法
	VAVA_ERR_CODE_POWER_LOW,		//(53)电池电量低
	VAVA_ERR_CODE_INSET_LIST_FAIL,	//(54)插入队列失败
	VAVA_ERR_CODE_RECSTART_FAIL,	//(55)手动录像开启失败
	VAVA_ERR_CODE_PAIRNG,			//(56)其它会话正在配对
	VAVA_ERR_CODE_NOSUPPORT=8888,	//(8888)不支持该指令
	VAVA_ERR_CODE_NOAUSH,			//(8889)无权限
	VAVA_ERR_CODE_NORETURN=9999		//(9999)不返回(命令执行结束后返回)
}VAVA_ERR_CODE;

//视频编码类型
typedef enum{
	VAVA_VIDEO_CODEC_H264=0,		//(0)H264
	VAVA_VIDEO_CODEC_H265			//(1)H265
}VAVA_VIDEO_CODEC;

//音频编码类型
typedef enum{
	VAVA_AUDIO_CODEC_PCM=0,			//(0)PCM
	VAVA_AUDIO_CODEC_G711A,			//(1)G711A
	VAVA_AUDIO_CODEC_G711U,			//(2)G711U
	VAVA_AUDIO_CODEC_AAC,			//(3)AAC
	VAVA_AUDIO_CODEC_OPUS			//(4)OPUS
}VAVA_AUDIO_CODEC;

//SD卡状态
typedef enum{
	VAVA_SD_STATUS_NOCRAD=0,		//(0)无卡
	VAVA_SD_STATUS_HAVECARD,		//(1)有卡
	VAVA_SD_STATUS_BADCARD,			//(2)坏卡
	VAVA_SD_STATUS_NEEDFORMAT,		//(3)卡需要格式化
	VAVA_SD_STATUS_FULL				//(4)卡满
}VAVA_SD_STATUS;

//控制类型
typedef enum{
	VAVA_CTRL_DISABLE=0,			//(0)关闭
	VAVA_CTRL_ENABLE,				//(1)开启
}VAVA_CTRL_TYPE;

//PIR灵敏度
typedef enum{
	VAVA_PIR_SENSITIVITY_OFF=0,		//(0)关闭
	VAVA_PIR_SENSITIVITY_HIGH,		//(1)高
	VAVA_PIR_SENSITIVITY_MIDDLE,	//(2)中
	VAVA_PIR_SENSITIVITY_LOW		//(3)低
}VAVA_PIR_SENSITIVITY;

//布防类型及状态
typedef enum{
	VAVA_ARMING_STATUS_DISABLE=0,	//(0)撤防
	VAVA_ARMING_STATUS_ENABLE,		//(1)布防
	VAVA_ARMING_STATUS_ONTIME_ON,	//(2)定时布防开启
	VAVA_ARMING_STATUS_ONTIME_OFF	//(3)定时布防关闭
}VAVA_ARMING_STATUS;

//连接上级路由器状态
typedef enum{
	VAVA_NETWORK_LINKOK=0,			//(0)网络连接成功
	VAVA_NETWORK_LINKING,			//(1)网络连接中
	VAVA_NETWORK_LINKFAILD,			//(2)网络连接失败
	VAVA_NETWORK_UNKNOW				//(3)网络连接状态未知
}VAVA_NETWORK_STATUS;

//视频质量
typedef enum{
	VAVA_VIDEO_QUALITY_BEST=0,		//(0)最佳画质 (1080P 1000bps)
	VAVA_VIDEO_QUALITY_HIGH,		//(1)高清画质 (720P 700bps)
	VAVA_VIDEO_QUALITY_RENEWAL,		//(2)最佳续航 (360P 400bps)
	VAVA_VIDEO_QUALITY_AUTO,		//(3)自动模式 (360P 400bps~600bps)
}VAVA_VIDEO_QUALITY;

//视频分辨率
typedef enum{
	VAVA_VIDEO_RESOULT_1080P=0,		//(0)1080P
	VAVA_VIDEO_RESOULT_720P,		//(1)720P
	VAVA_VIDEO_RESOULT_360P,		//(2)360P
}VAVA_VIDEO_RESOULT;

//视频翻转状态
typedef enum{
	VAVA_MIRROR_TYPE_NORMAL=0,		//(0)不翻转
	VAVA_MIRROR_TYPE_HORIZONTALLY,	//(1)水平翻转
	VAVA_MIRROR_TYPE_VERTICALLY,	//(2)垂直翻转
	VAVA_MIRROR_TYPE_BOTH			//(3)双向都翻转
}VAVA_MIRROR_TYPE;

//夜视模式
typedef enum{
	VAVA_IRLED_MODE_CLOSE=0,		//(0)关闭红外灯
	VAVA_IRLED_MODE_AUTO,			//(1)自动检测
	VAVA_IRLED_MODE_OPEN			//(2)开启红外灯
}VAVA_IRLED_MODE;

//录像文件类型
typedef enum{
	VAVA_RECFILE_TYPE_FULLTIME=0, 	//(0)全时录像 
	VAVA_RECFILE_TYPE_ALARM,  		//(1)报警录像
	VAVA_RECFILE_TYPE_IMG,			//(2)自动抓图
	VAVA_RECFILE_TYPE_MANAUL,		//(3)手动录像
	VAVA_RECFILE_TYPE_SNAPSHOT,		//(4)手动抓图
	VAVA_RECFILE_TYPE_ALL=9			//(9)全部录像
}VAVA_RECFILE_TYPE;

//手动录像停止类型
typedef enum{
	VAVA_REC_STOP_NAMAL=0,			//(0)正常停止
	VAVA_REC_STOP_ALARM,			//(1)报警中止
	VAVA_REC_STOP_TIMEOUT			//(2)超时中止
}VAVA_REC_STOP_TYPE;

//录像回放控制类型
typedef enum{
	VAVA_RECFILE_PLAY_CTRL_CONTINUE=0,//(0)继续回放
	VAVA_RECFILE_PLAY_CTRL_PAUSE,	//(1)暂停回放
	VAVA_RECFILE_PLAY_CTRL_STOP		//(2)停止回放
}VAVA_RECFILE_PLAY_CTRL;

//录像回放传输模式
typedef enum{
	VAVA_RECFILE_TRANSPORT_NORMA=0,	//(0)正常传输
	VAVA_RECFILE_TRANSPORT_FAST		//(1)快速传输
}VAVA_RECFILE_TRANSPORT;

//批量删除录像模式
typedef enum{
	VAVA_RECFILE_DEL_NORMAL=0,		//(0)正常模式
	VAVA_RECFILE_DEL_ALLDIR			//(1)文件夹删除
}VAVA_RECFILE_DEL_FLAG;

//清除摄像机配对信息类型
typedef enum{
	VAVA_PAIR_CLEAR_CHANNEL=0,		//(0)清除指定摄像机
	VAVA_PAIR_CLEAR_ALL				//(1)清除全部摄像机
}VAVA_PAIR_CLEAR;

//升级状态
typedef enum{
	VAVA_UPDATE_IDLE=0,				//(0)空闲状态
	VAVA_UPDATE_START,				//(1)进入升级模式
	VAVA_UPDATE_LOADING,			//(2)升级包下载中
	VAVA_UPDATE_LOAD_ERR,			//(3)升级包下载失败或超时
	VAVA_UPDATE_LOAD_FINISH,		//(4)升级包下载完成
	VAVA_UPDATE_UPFILE_NOSUPPORT,	//(5)非VAVA升级包
	VAVA_UPDATE_CRCERR,				//(6)升级包校验失败
	VAVA_UPDATE_TYPE_ERR,			//(7)升级包类型错误
	VAVA_UPDATE_FILE_OPEN_FAIL,		//(8)升级包打开失败
	VAVA_UPDATE_REQ_FAIL,			//(9)发送升级请求失败(摄像机)
	VAVA_UPDATE_RESP_TIMEOUT,		//(10)等待摄像机响应超时(摄像机)
	VAVA_UPDATE_TRANSMITTING,		//(11)正在传输升级数据(摄像机)
	VAVA_UPDATE_TRANS_FIAL,			//(12)升级数据传输失败(摄像机)
	VAVA_UPDATE_CHECK_TIMEOUT,		//(13)校验升级包超时(摄像机)
	VAVA_UPDATE_CHECK_FAIL,			//(14)校验升级包失败(摄像机)
	VAVA_UPDATE_TIMEOUT,			//(15)升级超时(摄像机)
	VAVA_UPDATE_UPGRADING,			//(16)升级中
	VAVA_UPDATE_SUCCESS,			//(17)升级成功
	VAVA_UPDATE_FAIL,				//(18)升级失败
	VAVA_UPDATE_NOPAIR,				//(19)升级通道无摄像机
	VAVA_UPDATE_POWERLOW			//(20)升级通道摄像机电量低
}VAVA_UPDATE_STATUS;

//升级包类型
typedef enum{
	VAVA_UPDATE_TYPE_GATEWAY=11,	//(11)基站
	VAVA_UPDATE_TYPE_CAMERA,		//(12)摄像机升级
	VAVA_UPDATE_TYPE_RF				//(13)RF
}VAVA_UPDATE_TYPE;

//推送文件类型
typedef enum{
	VAVA_PUSH_FILE_TYPE_TEXT=0,		//(0)文本消息
	VAVA_PUSH_FILE_TYPE_VIDEO,		//(1)视频消息
	VAVA_PUSH_FILE_TYPE_IMG			//(2)图片
}VAVA_PUSH_FILE_TYPE;

//推送消息类型
typedef enum{
	VAVA_PUSH_TYPE_FW=1,			//(1)固件报警
	VAVA_PUSH_TYPE_LOWPOWER,		//(2)低电量报警
	VAVA_PUSH_SYSTEM_MSG,			//(3)系统消息
	VAVA_PUSH_SHARE,				//(4)分享
	VAVA_PUSH_CAMERA_OFFLINE		//(5)摄像机离线
}VAVA_PUSH_TYPE;

typedef enum{
	VAVA_NAS_STATUS_IDLE,			//(0)空闲
	VAVA_NAS_STATUS_CONFIGING,		//(1)正在配置NAS
	VAVA_NAS_STATUS_SYNC,			//(2)自动同步中
	VAVA_NAS_STATUS_SD_FAIL,		//(3)SD卡检测失败
	VAVA_NAS_STATUS_PARAM_FAIL,		//(4)NAS服务器参数无效
	VAVA_NAS_STATUS_MOUNT_FAIL,		//(5)NAS服务器挂载失败
	VAVA_NAS_STATUS_NOWRITE,		//(6)NAS服务器不可写
	VAVA_NAS_STATUS_CONNECT_FAIL,	//(7)NAS服务器连接失败
	VAVA_NAS_STATUS_LACKOF_SPACE,	//(8)NAS空间不足
}VAVA_NAS_STATUS;

typedef struct _session_list{
	short id;
	short camerachannel;
	short wakeupstatus;
	short videostatus;
	short videoflag;
	short audiostatus;
	short sendmode;
	short recimgstop;
	unsigned int framenum;
	unsigned int debugauth;
}SessionList;

//ap配网
struct ac_head{
    int tag;						//0xEB000007
    int cmd;						//0x101 配网
    								//0x102	配网确认
    int lenght;						//sizeof(struct ac_set)
    int result;						//固定填0
};

struct ac_set{
    char ssid[32];					//路由器SSID
    char pass[32];					//路由器密码	
};

struct ac_resp{
	char sn[32];					//基站序列号
};

//布防参数及状态 1.1样式
typedef struct _armdata_v1{
	int nstat;		//条目是否有效
	int s_h;		//开始时间(时分秒)
	int s_m;		
	int s_s;		
	int e_h;		//结束时间(时分秒)
	int e_m;
	int e_s;
	int weekday[VAVA_WEEKDAYS]; //0 为星期日
}VAVA_ArmData_v1;

typedef struct _arminfo_v1{
	int type;		//类型 0 关闭 1 开启 2 定时开启 3 定时关闭 
	int status;		//布防状态 0 撤防 1 布防
	VAVA_ArmData_v1 armdata[MAX_ARM_LIST_NUM]; //最多支持10条
}VAVA_ArmInfo_v1;

/*                          基站与APP交互                         */
#endif

#define GATEWAY_RFDATA
#ifdef GATEWAY_RFDATA
/*                          短波数据结构                          */
//短波协议头
typedef struct _rfhead{
	unsigned char tag; 			//0x7B: 主动发送   0x7E: RESP
	unsigned char len;			//长度(包含协议头+参数数据+校验字节的大小)
	unsigned char cmd;			//cmd
}VAVA_RF_Head;

//短波命令字
typedef enum{
	CC1310_HARTBEAT = 0x0, 		//心跳(带电池电量)
	CC1310_IDCHECK,				//身份确认(摄像机还是基站)
	CC1310_PAIRACK,				//配对完成后确认(摄像机短波记录基站ADDR)
	CC1310_RESEND,     			//重发
	CC1310_GETVERSION,			//获取版本号
	CC1310_GETNTSAMP,			//获取时间戳(摄像机专用)
	CC1310_GETPAIRINFO,         //获取配对信息(摄像机专用)

	CC1310_ASKSN = 0x10,		//向摄像机请求序列号 
	CC1310_FIRSTCONNECT,		//初次连接(摄像机需要上报所有参数
	CC1310_PIRGET,      		//PIR获取
	CC1310_PIRSET,      		//PIR设置
	CC1310_TFGET,				//TF状态获取
	CC1310_TFSET,				//TF状态设置
	CC1310_POWEROFF,     		//摄像机关机
	CC1310_SETHWVER,     		//设置硬件版本号
  	CC1310_GETHWVER,     		//获取硬件版本号

	CC1310_DEVRFID = 0x20,		//通知基站短波芯片地址
	CC1310_SENDSN,    			//通知基站摄像机短波地址和序列号
	CC1310_REMOVESN,			//通知基站摄像机退出配对模式
	CC1310_PAIRREQ,				//基站短波向摄像机短波发起配对请求
	CC1310_PIR,
	CC1310_BATTERYLOW,			//电量低
	CC1310_BATTERYFULL,			//电池充满
	CC1310_WAKEUP,				//唤醒摄像机
	CC1310_SLEEP,				//休眠摄像机
	CC1310_EXPORTPOWER,			//外部电源状态

	CC1310_DEFAULT = 0x30,	
	CC1310_BUTTON,				//按键
	CC1310_LED0,				//LED灯  
	CC1310_IDBROADCAST,			//广播摄像机地址和序列号
	CC1310_TEST,				//测试指令
	CC1310_TEST_START,			//测试开始
	CC1310_TEST_STOP,			//测试结束

	CC1310_IO_1 = 0x40,
	CC1310_IO_2,

	CC1310_FEEDBACK = 0x99,		//回复
	CC1310_PRINTF_LOG = 0xEE
}VAVA_RF_CMD;

typedef struct _rfheart_data{
	unsigned int ntsamp;		//时间戳
	unsigned char rectime;		//录像时长
	unsigned char netch;		//网络信道
	unsigned char reserv[2];
}VAVA_RF_HeartData;

//短波心跳
typedef struct _rfheart_req{
	unsigned int addr;			//摄像机短波地址
	VAVA_RF_HeartData data;
#ifdef FREQUENCY_OFFSET
	unsigned int index;			//摄像机频段下标
#endif
}VAVA_RF_HeartReq;

typedef struct _rfheart{
	unsigned int addr;			//RF地址
	unsigned char battery;		//电池电量(100精度)
	unsigned char battery_ex;	//电池电量(256精度)
	unsigned char status;		//在线状态 0 不在线 1 在线
	unsigned char expower;		//外部电源
	int voltage; 				//电池电压
#ifdef BATTEY_INFO
	int temperature;			//电池温度
	int electricity;			//电池电流
#endif
	int tfcard;					//0:旧版本 1:有卡 2 无卡
}VAVA_RF_heart;

//短波身份
typedef enum{
	VAVA_RF_ID_GATEWAY = 0,		//基站
	VAVA_RF_ID_CAMERA			//摄像机
}VAVA_RF_ID_TYPE;

typedef struct _rfcheckresp{
	unsigned int id;			//0 基站  1 摄像机
	unsigned int addr;			//短波芯片地址
}VAVA_RF_Checkresp;

//完成配对确认
typedef struct _rfpairack{
	unsigned int addr;			//基站或摄像机短波地址
	unsigned int channel;		//通道号
#ifdef FREQUENCY_OFFSET
	unsigned int index;			//摄像机频段下标
#endif
}VAVA_RF_Pairack;

//摄像机短波信息
typedef struct _rfcamerasn{
	unsigned int addr;			//摄像机RF地址
	char sn[32];				//摄像机序列号
}VAVA_RF_Camerasn;

//摄像机退出配对信息
typedef struct _rfcameraremove{
	unsigned int addr;			//摄像机RF地址
}VAVA_RF_Cameraremove;

//配对请求
typedef struct _rfpairreq{
	unsigned int addr;			//摄像机RF地址
	unsigned int channel;		//摄像机预分配通道号
	char ssid[16];				//基站热点ssid
	char pass[12];				//基站热点密码
#ifdef FREQUENCY_OFFSET
	unsigned int index;			//摄像机频段下标
	unsigned int preindex;		//摄像机预分配频段下标
#endif
}VAVA_RF_Pairreq;

//唤醒和休眠
typedef struct _rfcameractrl_1{
	unsigned int addr;			//摄像机RF地址
	unsigned int ntsamp;		//校时
	unsigned char mode;			//模式 //0 正常唤醒 1 PIR触发唤醒
	unsigned char netch;		//信道
	unsigned char reserv[2];	//保留
#ifdef FREQUENCY_OFFSET
	unsigned int index;			//摄像机频段下标
#endif
}VAVA_RF_Wakeup;

typedef struct _rfcameractrl{
	unsigned int addr;			//摄像机RF地址
#ifdef FREQUENCY_OFFSET
	unsigned int index;			//摄像机频段下标
#endif
}VAVA_RF_Sleep, VAVA_RF_Pairresp;

//报警信息
typedef struct _alarmmsg{
	unsigned int addr;			//摄像机RF地址
}VAVA_RF_Pir, VAVA_RF_BatteryLow, VAVA_RF_BatteryFull, VAVA_RF_PowerOff;

//唤醒和休眠结果回复
typedef struct _rfcameractrlresp{
	unsigned int addr;			//摄像机RF地址
	unsigned int status;		//结果 0 成功 非0失败
}VAVA_RF_Cameractrlresp; 

//初次连接
typedef struct _firstconnect{
	unsigned int addr;			//摄像机RF地址
}VAVA_RF_FirstConnect;

typedef struct _rflog{
	char log[30];
}VAVA_RF_Log;

//PIR灵敏度获取与配置
typedef struct _pirreq{
	unsigned int addr;		//摄像机RF地址
	unsigned int val;
#ifdef FREQUENCY_OFFSET
	unsigned int index;		//摄像机频段下标
#endif
}VAVA_RF_Pirreq, VAVA_RF_Pirresp;

//外部电源状态
typedef struct _rfexportpower{
	unsigned int addr;		//摄像机RF地址
	unsigned char status;  	//状态 0 外部电源断开 1 外部电源接入
	unsigned char type;    	//类型
	unsigned char reserv[2];//保留
}VAVA_RF_Exportpower;

//测试指令
typedef struct _rftest{
	unsigned int ctrl;
}VAVA_RF_TestCtrl;

typedef struct _testdata{
	int num;
}VAVA_RF_TestData;

//短波版本号
typedef struct _rfversion{
	char version[16]; 
}VAVA_RF_Version;

//硬件版本号
typedef struct _rfHwVer{
	int result;						//0 :success -1: failed
	char version[16];  
}VAVA_RF_Hardver;

typedef struct _rftfsetdata{
	unsigned char tfstatus;	//基站TF卡状态 0 无卡或卡异常 1 有卡
	unsigned char reserv[3];
}VAVA_RF_TFSetData;

typedef struct _rftfset{
	unsigned int addr;		//摄像机RF地址
	VAVA_RF_TFSetData data;
#ifdef FREQUENCY_OFFSET
	unsigned int index;		//摄像机频段下标
#endif
}VAVA_RF_TfSet, VAVA_RF_TFGet;
/*                          短波数据结构                          */
#endif

extern volatile unsigned char g_running;
extern volatile unsigned char g_exitflag;
extern volatile unsigned char g_waitflag;
extern volatile unsigned char g_debuglevel;
extern volatile int g_domaintype;
extern volatile unsigned char g_pir_sensitivity[MAX_CHANNEL_NUM];

extern unsigned int g_channel[MAX_CHANNEL_NUM];
extern volatile unsigned int g_wakeup_withpair_addr;
extern volatile unsigned int g_sleep_withpair_addr;
extern volatile unsigned int g_pair_wifi_addr;
extern volatile unsigned int g_clear_addr;

extern volatile unsigned char g_keyparing;
extern volatile unsigned char g_pair_result;
extern volatile unsigned char g_addstation;
extern volatile unsigned char g_wakeup_withpair_status;
extern volatile unsigned char g_sleep_withpair_status;
extern volatile unsigned char g_clear_status;

extern volatile unsigned char g_wakeup_result[MAX_CHANNEL_NUM];
extern volatile unsigned char g_sleep_result[MAX_CHANNEL_NUM];
extern volatile unsigned char g_pair_status[MAX_CHANNEL_NUM];

extern volatile char g_tfcheck[MAX_CHANNEL_NUM];	//TF卡状态更新
extern volatile char g_tfstatus[MAX_CHANNEL_NUM];	//短波TF卡状态

extern volatile unsigned char g_buzzer_flag; //蜂鸣器开关
extern volatile unsigned char g_buzzer_type; //蜂鸣器鸣叫类型
extern volatile unsigned char g_devinfo_update;//心跳包更新标记
extern volatile VAVA_Snapshot g_snapshot[MAX_CHANNEL_NUM]; //抓图不带报警
extern volatile VAVA_Snapshot g_snapshot_alarm[MAX_CHANNEL_NUM]; //抓图带报警
extern struct timeval g_salarm[MAX_CHANNEL_NUM];
extern volatile unsigned char g_update_sd_nochek;
extern volatile unsigned char g_wakenoheart[MAX_CHANNEL_NUM];
extern volatile unsigned char g_sleepnoheart[MAX_CHANNEL_NUM];
extern volatile unsigned long long g_manaul_ntsamp[MAX_CHANNEL_NUM];

extern volatile unsigned char g_pairmode; //配对状态

#ifdef ALARM_PHOTO_IOT
extern char g_imgname[MAX_CHANNEL_NUM][256];
extern volatile unsigned char g_imgflag[MAX_CHANNEL_NUM];
#endif

#ifdef WAKE_TIME_USER_TEST
extern struct timeval g_testval_1;
extern struct timeval g_testval_2;
#endif

extern pthread_mutex_t mutex_config_lock;
extern pthread_mutex_t mutex_cmdsend_lock;
extern pthread_mutex_t mutex_session_lock;
extern pthread_mutex_t mutex_cmdlist_lock;
extern pthread_mutex_t mutex_reclist_lock;
extern pthread_mutex_t mutex_imglist_lock;
extern pthread_mutex_t mutex_pushlist_lock;
extern pthread_mutex_t mutex_upgrate_lock;
extern pthread_mutex_t mutex_idx_lock;
extern pthread_mutex_t mutex_rf_lock;
extern pthread_mutex_t mutex_search_camera_lock;
extern pthread_mutex_t mutex_pair_lock;
extern pthread_mutex_t mutex_recplay_lock;
extern pthread_mutex_t mutex_format_lock;
extern pthread_mutex_t mutex_pir_lock;
extern pthread_mutex_t mutex_power_lock[MAX_CHANNEL_NUM];
extern pthread_mutex_t mutex_ping_lock;
extern pthread_mutex_t mutex_curl_lock;

#ifdef FREQUENCY_OFFSET
extern VAVA_Pair_info_old g_pair_old[MAX_CHANNEL_NUM];
#endif

extern VAVA_Pair_info g_pair[MAX_CHANNEL_NUM];
extern VAVA_ArmInfo_v1 g_camera_arminfo_v1[MAX_CHANNEL_NUM];
extern VAVA_RecTime g_rectime;
extern VAVA_Update g_update;
extern VAVA_SearchCamera g_searchcamera;
extern VAVA_RecPlay g_recplay[MAX_CHANNEL_NUM];
extern VAVA_Camera_Dynamic g_camerainfo_dynamic[MAX_CHANNEL_NUM];
extern VAVA_IPC_OtaInfo g_ipc_otainfo[MAX_CHANNEL_NUM];
extern VAVA_AVmemchace g_avmemchace[MAX_CHANNEL_NUM];
extern VAVA_AVRecvMem g_avrecvbuff[MAX_CHANNEL_NUM];
extern VAVA_AVNasMem g_avnasbuff;
extern VAVA_AVPlayMem g_avplaybuff;
extern SessionList g_session[MAX_SESSION_NUM];
extern VAVA_GateWayInfo g_gatewayinfo;
extern VAVA_CameraInfo g_camerainfo[MAX_CHANNEL_NUM];
extern VAVA_CameraName g_cameraname[MAX_CHANNEL_NUM];
extern VAVA_OnlineStatus g_online_flag[MAX_CHANNEL_NUM];
extern VAVA_PushFlag g_pushflag[MAX_CHANNEL_NUM];
extern VAVA_NtpInfo g_ntpinfo;

#ifdef CLOUDE_SUPPORT
extern VAVA_AVCloudMem g_avcloudbuff[MAX_CHANNEL_NUM];
#endif

extern volatile unsigned char g_firsttag;
extern volatile unsigned char g_sdformat;
extern volatile unsigned char g_sdformating;  //正在格式化 禁止检测T卡
extern volatile unsigned char g_sdpoping;	 //主动弹出SD卡 禁止检测T卡
extern volatile unsigned char g_md_result[MAX_CHANNEL_NUM];

extern volatile int g_camera_talk[MAX_CHANNEL_NUM];
extern volatile unsigned char g_tmp_quality[MAX_CHANNEL_NUM];

extern volatile unsigned char g_router_link_status;
extern volatile unsigned char g_apflag;
extern volatile unsigned char g_avstop;
extern int g_token_timeout;

extern VAVA_Rec_Manage g_manaulrec[MAX_CHANNEL_NUM];
extern VAVA_Rec_Manage g_alarmrec[MAX_CHANNEL_NUM];
extern VAVA_Rec_Manage g_fulltimerec[MAX_CHANNEL_NUM];
extern VAVA_Rec_Manage g_cloudrec[MAX_CHANNEL_NUM];

extern volatile unsigned char g_rftest_flag;
extern volatile int g_rftest_session;

extern volatile unsigned char g_nas_status;	//NAS工作状态
extern volatile unsigned char g_nas_change;	//NAS参数变更
extern volatile unsigned char g_nas_net_check;//NAS网络状态检测
extern VAVA_NasConfig g_nas_config;	//NAS配置

//云服务
extern struct mps_clds_dev_channel *clds_chl[MAX_CHANNEL_NUM];
extern volatile unsigned char g_cloudflag[MAX_CHANNEL_NUM];
extern volatile unsigned char g_cloudctrl[MAX_CHANNEL_NUM];
extern volatile unsigned char g_cloudsend[MAX_CHANNEL_NUM];
extern volatile unsigned char g_cloudlink[MAX_CHANNEL_NUM];
extern volatile unsigned char g_cloudrecheck[MAX_CHANNEL_NUM];
extern volatile unsigned char g_cloudupcheck[MAX_CHANNEL_NUM];
extern volatile unsigned char g_clouddebugflag;
extern volatile unsigned char g_clouddebuglever;

extern volatile unsigned char g_rec_delaytime; //录像延时开启开关

extern VAVA_FwHead *g_fw1; //升级类型为基站时有两个 升级类型为摄像机时只有一个
extern VAVA_FwHead *g_fw2;

extern int g_pair_sync_time;
extern int g_status_sync_time;
extern int g_status_sync_time_slow;
extern int g_bsattr_sync_flag;
extern int g_cameraattr_sync_flag[MAX_CHANNEL_NUM];
extern VAVA_Pair_Sync g_pairsync[MAX_CHANNEL_NUM];
extern VAVA_Pair_Sync g_pairupdate[MAX_CHANNEL_NUM];

//下载超时检测
extern int g_download_flag;

#if defined (__cplusplus)
}
#endif

#endif


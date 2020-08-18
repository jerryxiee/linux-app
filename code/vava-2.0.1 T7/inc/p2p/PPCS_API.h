#ifndef _PPCS_API___INC_H_
#define _PPCS_API___INC_H_

#include <netinet/in.h>
#include "PPCS_Error.h"

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct{	
	char bFlagInternet;		// Internet Reachable? 1: YES, 0: NO
	char bFlagHostResolved;	// P2P Server IP resolved? 1: YES, 0: NO
	char bFlagServerHello;	// P2P Server Hello? 1: YES, 0: NO
	char NAT_Type;			// NAT type, 0: Unknow, 1: IP-Restricted Cone type,   2: Port-Restricted Cone type, 3: Symmetric 
	char MyLanIP[16];		// My LAN IP. If (bFlagInternet==0) || (bFlagHostResolved==0) || (bFlagServerHello==0), MyLanIP will be "0.0.0.0"
	char MyWanIP[16];		// My Wan IP. If (bFlagInternet==0) || (bFlagHostResolved==0) || (bFlagServerHello==0), MyWanIP will be "0.0.0.0"
} st_PPCS_NetInfo;

typedef struct{	
	int Skt;					    // Sockfd
	struct sockaddr_in RemoteAddr;	// Remote IP:Port
	struct sockaddr_in MyLocalAddr;	// My Local IP:Port
	struct sockaddr_in MyWanAddr;	// My Wan IP:Port
	unsigned int ConnectTime;	    // Connection build in ? Sec Before
	char DID[24];					// Device ID
	char bCorD;						// I am Client or Device, 0: Client, 1: Device
	char bMode;						// Connection Mode: 0: P2P, 1:Relay Mode
	char Reserved[2];				
} st_PPCS_Session;

unsigned int PPCS_GetAPIVersion(void);
int PPCS_QueryDID(const char* DeviceName, char *DID, int DIDBufSize);
int PPCS_Initialize(char *Parameter);
int PPCS_DeInitialize(void);
int PPCS_NetworkDetect(st_PPCS_NetInfo *NetInfo, unsigned short UDP_Port);
int PPCS_NetworkDetectByServer(st_PPCS_NetInfo *NetInfo, unsigned short UDP_Port, char *ServerString);
int PPCS_Share_Bandwidth(char bOnOff);
int PPCS_Listen(const char *MyID, const unsigned int TimeOut_Sec, unsigned short UDP_Port, 
	            char bEnableInternet, const char* APILicense);
int PPCS_Listen_Break(void);
int PPCS_LoginStatus_Check(char *bLoginStatus);
int PPCS_Connect(const char *TargetID, char bEnableLanSearch, unsigned short UDP_Port);
int PPCS_ConnectByServer(const char *TargetID, char bEnableLanSearch, unsigned short UDP_Port, char *ServerString);
int PPCS_Connect_Break();
int PPCS_Check(int SessionHandle, st_PPCS_Session *SInfo);
int PPCS_Close(int SessionHandle);
int PPCS_ForceClose(int SessionHandle);
int PPCS_Write(int SessionHandle, unsigned char Channel, char *DataBuf, int DataSizeToWrite);
int PPCS_Read(int SessionHandle, unsigned char Channel, char *DataBuf, int *DataSize, unsigned int TimeOut_ms);
int PPCS_Check_Buffer(int SessionHandle, unsigned char Channel, unsigned int *WriteSize, unsigned int *ReadSize);
//// Ther following functions are available after ver. 2.0.0
int PPCS_PktSend(int SessionHandle, unsigned char Channel, char *PktBuf, int PktSize); //// Available after ver. 2.0.0
int PPCS_PktRecv(int SessionHandle, unsigned char Channel, char *PktBuf, int *PktSize, unsigned int TimeOut_ms); //// Available after ver. 2.0.0

#ifdef __cplusplus
}
#endif

#endif 

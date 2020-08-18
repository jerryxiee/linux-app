#ifndef _RF_SERVER_H_
#define _RF_SERVER_H_

int RF_FirstConnect(unsigned int addr);
#ifdef FREQUENCY_OFFSET
int RF_PirGet(unsigned int addr, unsigned int index);
int RF_PirSet(unsigned int addr, unsigned int index, int val);
int RF_Req_Pair(unsigned int addr, unsigned int index, unsigned char channel, char *ssid, char *pass);
int RF_WakeUP(unsigned int addr, unsigned int index, unsigned char mode, unsigned char netch);
int RF_Sleep(unsigned int addr, unsigned int index);
int RF_HeartCheck(unsigned int addr, unsigned int index, unsigned char netch);
int RF_TFSet(unsigned int addr, unsigned int index, unsigned char stauts);
int RF_Ack_Pair(unsigned int addr, unsigned int index, unsigned int channel);
#else
int RF_PirGet(unsigned int addr);
int RF_PirSet(unsigned int addr, int val);
int RF_Req_Pair(unsigned int addr, unsigned char channel, char *ssid, char *pass);
int RF_WakeUP(unsigned int addr, unsigned char mode, unsigned char netch);
int RF_Sleep(unsigned int addr);
int RF_HeartCheck(unsigned int addr, unsigned char netch);
int RF_TFSet(unsigned int addr, unsigned char stauts);
int RF_Ack_Pair(unsigned int addr, unsigned int channel);
#endif
int RF_TestCtrl(int ctrl);
int RF_StopUart();
void *rfserver_pth(void *data);

#endif

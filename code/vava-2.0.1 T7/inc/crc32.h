#ifndef _CRC32_H_
#define _CRC32_H_

#if defined (__cplusplus)
extern "C" {
#endif

int CRC32_Init(void);
int CRC32_Calculate(unsigned char *pbuff,int len);
unsigned int CRC32_GetResult(void);

int CRC32_OTA_Init();
int CRC32_OTA_Calculate(unsigned char *pbuff, int len);
unsigned int CRC32_OTA_GetResult(void);

#if defined (__cplusplus)
}
#endif

#endif

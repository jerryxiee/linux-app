#ifndef _SD_CHECK_H_
#define _SD_CHECK_H_

int SD_CheckDev(char *devname);
int SD_TFCheck(int *status, int *format);

void *SDPnp_pth(void *data);
void *SDSus_pth(void *data);

#endif

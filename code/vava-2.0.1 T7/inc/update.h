#ifndef _UPDATE_H_
#define _UPDATE_H_

//OTA升级
void *update_pth(void *data);
void *UpgringTimeout_pth(void *data);

int update_init();
int update_write(char *ptr, int size);
int update_finish();
int update_checkpack(int fwnum);
int update_deinit();
int update_sendtocamera(int cmdfd, int avfd, int filesize);
int update_wait(int *wait);

#endif


#ifndef _WATCH_DOG_H_
#define _WATCH_DOG_H_

void *WatchDog_pth(void *data);
int WatchDog_Stop(const char *pthstr);

#endif
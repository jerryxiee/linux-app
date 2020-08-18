#ifndef _SERVER_H_
#define _SERVER_H_

void *commonserver_pth(void *data);
void *avserver_pth(void *data);
void *avsend_pth(void *data);
void *audiorecv_pth(void *data);
void *apsend_pth(void *data);

#endif

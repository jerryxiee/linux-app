#ifndef _NETWORK_CHECK_H_
#define _NETWORK_CHECK_H_

void *wirlesscheck_pth(void *data);
void *netchange_pth(void *data);
void *internetcheck_pth(void *data);
void *nettypechange_pth(void *data);

void internetcheck_start();
void internetcheck_stop();

#endif

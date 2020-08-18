#ifndef _MUTEX_H_
#define _MUTEX_H_

#if defined (__cplusplus)
extern "C" {
#endif

int MutexInit(pthread_mutex_t *CriticalSection);
void MutexDeInit(pthread_mutex_t *CriticalSection);

#if defined (__cplusplus)
}
#endif

#endif


#include "basetype.h"
#include "vavahal.h"
#include "mutex.h"

int MutexInit(pthread_mutex_t *CriticalSection)
{
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	if(pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ADAPTIVE_NP))
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: \n", FUN, LINE);
		perror("Err in pthread_mutexattr_settype: ");
	}
	
	if(pthread_mutex_init(CriticalSection, &mattr))
	{
		VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: \n", FUN, LINE);
		perror("Err in pthread_mutex_init: ");
		return -1;                          
	}
	else
	{
		return 0;
	}
}

void MutexDeInit(pthread_mutex_t *CriticalSection)
{
	pthread_mutex_destroy(CriticalSection);
}

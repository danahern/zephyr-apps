#include <eai_osal/critical.h>
#include <pthread.h>

/*
 * POSIX critical section — global recursive mutex.
 * No IRQ disable on Linux/macOS; this just provides mutual exclusion.
 */

static pthread_mutex_t osal_critical_lock;
static unsigned int osal_critical_nesting;
static bool osal_critical_init_done;

static void ensure_init(void)
{
	if (!osal_critical_init_done) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&osal_critical_lock, &attr);
		pthread_mutexattr_destroy(&attr);
		osal_critical_init_done = true;
	}
}

eai_osal_critical_key_t eai_osal_critical_enter(void)
{
	ensure_init();
	pthread_mutex_lock(&osal_critical_lock);
	return osal_critical_nesting++;
}

void eai_osal_critical_exit(eai_osal_critical_key_t key)
{
	(void)key;
	osal_critical_nesting--;
	pthread_mutex_unlock(&osal_critical_lock);
}

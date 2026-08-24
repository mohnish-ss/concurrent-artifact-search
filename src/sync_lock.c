#include "search_internal.h"

#include <errno.h>

int search_lock_init(SearchLock *lock)
{
    if (lock == NULL) {
        return EINVAL;
    }

#if defined(__APPLE__)
    return pthread_mutex_init(&lock->primitive, NULL);
#else
    return sem_init(&lock->primitive, 0, 1) == 0 ? 0 : errno;
#endif
}

int search_lock_acquire(SearchLock *lock)
{
    if (lock == NULL) {
        return EINVAL;
    }

#if defined(__APPLE__)
    return pthread_mutex_lock(&lock->primitive);
#else
    int result;
    do {
        result = sem_wait(&lock->primitive);
    } while (result != 0 && errno == EINTR);
    return result == 0 ? 0 : errno;
#endif
}

int search_lock_release(SearchLock *lock)
{
    if (lock == NULL) {
        return EINVAL;
    }

#if defined(__APPLE__)
    return pthread_mutex_unlock(&lock->primitive);
#else
    return sem_post(&lock->primitive) == 0 ? 0 : errno;
#endif
}

int search_lock_destroy(SearchLock *lock)
{
    if (lock == NULL) {
        return EINVAL;
    }

#if defined(__APPLE__)
    return pthread_mutex_destroy(&lock->primitive);
#else
    return sem_destroy(&lock->primitive) == 0 ? 0 : errno;
#endif
}

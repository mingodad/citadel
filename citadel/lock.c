/*
 * read/write locks.
 * this is just a thin wrapper around POSIX rwlocks to provide recursive
 * write-locking.
 * by nathan bryant <nbryant@optonline.net>
 *
 * pthread_rwlock_* directly maps to lock_*, using lock_t instead of
 * pthread_rwlock_t, except lock_init doesn't take an attribute parameter
 *
 * There is no initializer macro; use lock_init().
 *
 * $Id$
 */

#define _XOPEN_SOURCE 500 /* Unix98 */

#include "lock.h"

int lock_init(lock_t *lock) {
	int ret;
	
        if ((ret = pthread_key_create(&lock->count, NULL)))
		return ret;

	return pthread_rwlock_init(&lock->lock, NULL);
}

int lock_wrlock(lock_t *lock) {
	int ret;
	long count;

	if ((count = (long)pthread_getspecific(lock->count)) > 0)
		return pthread_setspecific(lock->count, (void *)(count + 1));

	ret = pthread_rwlock_wrlock(&lock->lock);
	if (!ret) {
		ret = pthread_setspecific(lock->count, (void *)1);

		if (ret)
			pthread_rwlock_unlock(&lock->lock);
	}

	return ret;
}

int lock_trywrlock(lock_t *lock) {
        int ret;
	long count;

        if ((count = (long)pthread_getspecific(lock->count)) > 0)
                return pthread_setspecific(lock->count, (void *)(count + 1));

        ret = pthread_rwlock_trywrlock(&lock->lock);
        if (!ret) {
                ret = pthread_setspecific(lock->count, (void *)1);

		if (ret)
			pthread_rwlock_unlock(&lock->lock);
	}

        return ret;
}

int lock_unlock(lock_t *lock) {
	int ret;
	long count;

	if ((count = (long)pthread_getspecific(lock->count)) > 0) {
		if (count > 1)
			return pthread_setspecific(lock->count,
						   (void *)(count - 1));

		ret = pthread_rwlock_unlock(&lock->lock);
		if (ret)
			return ret;

		return pthread_setspecific(lock->count, (void *)0);
	}

	return pthread_rwlock_unlock(&lock->lock);
}

int lock_destroy(lock_t *lock) {
	int ret1, ret2;

	ret1 = pthread_key_delete(lock->count);
	ret2 = pthread_rwlock_destroy(&lock->lock);

	if (ret2)
		return ret2;

	return ret1;
}

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

#ifndef LOCK_H
#define LOCK_H

#include <pthread.h>

typedef struct {
	pthread_rwlock_t lock;
	pthread_key_t count;
} lock_t;

/* The following are implemented in lock.c */

int lock_init(lock_t *lock);
int lock_wrlock(lock_t *lock);
int lock_trywrlock(lock_t *lock);
int lock_unlock(lock_t *lock);
int lock_destroy(lock_t *lock);

/* The remaining functions are actually implemented here as macros:

int lock_rdlock(lock_t *lock);
int lock_tryrdlock(lock_t *lock);

*/

#define lock_rdlock(lock) (pthread_rwlock_rdlock(&(lock)->lock))
#define lock_tryrdlock(lock) (pthread_rwlock_tryrdlock(&(lock)->lock))

#endif /* LOCK_H */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_KEY 100000000U

typedef struct list_node_s {
	int data;
	struct list_node_s *next;
} list_node_t;

static list_node_t *head = NULL;

typedef struct {
	pthread_mutex_t mutex;
	pthread_cond_t readers_cv;
	pthread_cond_t writers_cv;
	unsigned active_readers;
	unsigned waiting_readers;
	unsigned waiting_writers;
	int writer_active;
} my_rwlock_t;

static int my_rwlock_init(my_rwlock_t *lock) {
	int rc;

	if (lock == NULL) return EINVAL;
	memset(lock, 0, sizeof(*lock)); // обнулить байты лока

	rc = pthread_mutex_init(&lock->mutex, NULL);
	if (rc != 0) return rc;

	rc = pthread_cond_init(&lock->readers_cv, NULL);
	if (rc != 0) {
		pthread_mutex_destroy(&lock->mutex);
		return rc;
	}

	rc = pthread_cond_init(&lock->writers_cv, NULL);
	if (rc != 0) {
		pthread_cond_destroy(&lock->readers_cv);
		pthread_mutex_destroy(&lock->mutex);
		return rc;
	}

	return 0;
}

static int my_rwlock_destroy(my_rwlock_t *lock) {
	int rc1, rc2, rc3;

	if (lock == NULL) return EINVAL;
	rc1 = pthread_cond_destroy(&lock->readers_cv);
	rc2 = pthread_cond_destroy(&lock->writers_cv);
	rc3 = pthread_mutex_destroy(&lock->mutex);
	if (rc1 != 0) return rc1;
	if (rc2 != 0) return rc2;
	
	return rc3;
}

static int my_rwlock_rdlock(my_rwlock_t *lock) {
	int rc;
	
	rc = pthread_mutex_lock(&lock->mutex);
	if (rc != 0) return rc;

	lock->waiting_readers++;
	while (lock->writer_active || lock->waiting_writers > 0) {
		rc = pthread_cond_wait(&lock->readers_cv, &lock->mutex); // поток отпустит mutex, чтобы другие могли завершиться (например читатели отпустить блокировку)
		if (rc != 0) {
			lock->waiting_readers--;
			pthread_mutex_unlock(&lock->mutex);
			return rc;
		}
	}

	lock->waiting_readers--;
	lock->active_readers++;

	return pthread_mutex_unlock(&lock->mutex);
}

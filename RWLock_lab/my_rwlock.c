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
		rc = pthread_cond_wait(&lock->readers_cv, &lock->mutex); // поток отпустит mutex, чтобы другие могли завершиться (например писатели отпустить блокировку)
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

static int my_rwlock_wrlock(my_rwlock_t *lock) {
	int rc;

	rc = pthread_mutex_lock(&lock->mutex);
	if (rc != 0) return rc;

	lock->waiting_writers++;

	while (lock->writer_active || lock->active_readers > 0) {
		rc = pthread_cond_wait(&lock->writers_cv, &lock->mutex);
		// после пробуждения, поток еще раз проверит условия while
		

		if (rc != 0) {
			lock->waiting_writers--;
			pthread_mutex_unlock(&lock->mutex);
			return rc;
		}
	}

	lock->waiting_writers--;
	lock->writer_active = 1;

	return pthread_mutex_unlock(&lock->mutex);
}

static int my_rwlock_unlock(my_rwlock_t *lock) {
	int rc;

	rc = pthread_mutex_lock(&lock->mutex);
	if (rc != 0) return rc;

	if (lock->writer_active) {
		lock->writer_active = 0;
	} else if (lock->active_readers > 0) {
		lock->active_readers--;
	} else {
		pthread_mutex_unlock(&lock->mutex);
		return EPERM;
	}

	if (!lock->writer_active && lock->active_readers == 0 &&
		lock->waiting_writers > 0) {
		rc = pthread_cond_signal(&lock->writers_cv); // signal пробудит только одного писателя
	} else if (!lock->writer_active && lock->waiting_writers == 0) {
		rc = pthread_cond_broadcast(&lock->readers_cv); // разбудит всех ожидающих читателей
	} else {
		rc = 0;
	}

	{
		int unlock_rc = pthread_mutex_unlock(&lock->mutex);
		return rc != 0 ? rc : unlock_rc;
	}
}

typedef enum {
	LOCK_PTHREAD,
	LOCK_CUSTOM
} lock_kind_t;

static pthread_rwlock_t system_lock;
static my_rwlock_t custom_lock;
static lock_kind_t selected_lock;

static int read_lock(void) {
	return selected_lock == LOCK_CUSTOM
		? my_rwlock_rdlock(&custom_lock)
		: pthread_rwlock_rdlock(&system_lock);
}

static int write_lock(void) {
	return selected_lock == LOCK_CUSTOM
		? my_rwlock_wrlock(&custom_lock)
		: pthread_rwlock_wrlock(&system_lock);
}

static int rw_unlock(void) {
	return selected_lock == LOCK_CUSTOM
		? my_rwlock_unlock(&custom_lock)
		: pthread_rwlock_unlock(&system_lock);
}

static int lock_init(lock_kind_t kind) {
	selected_lock = kind;
	return kind == LOCK_CUSTOM
		? my_rwlock_init(&custom_lock)
		: pthread_rwlock_init(&system_lock, NULL);
}

static int lock_destroy(lock_kind_t kind) {
	return kind == LOCK_CUSTOM
		? my_rwlock_destroy(&custom_lock)
		: pthread_rwlock_destroy(&system_lock);
}

static uint32_t rng_next(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 0xA341316CU;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static double rng_double(uint32_t *state) {
    return (double)rng_next(state) / 4294967296.0;
}



// -1 => ошибка памяти
// 0 => неуспешное выполнение
// 1 => успешное выполнение



static int list_insert(int value) {
    list_node_t *curr = head;
    list_node_t *pred = NULL;
    list_node_t *node;

    while (curr != NULL && curr->data < value) {
        pred = curr;
        curr = curr->next;
    }

    if (curr != NULL && curr->data == value) return 0;

    node = malloc(sizeof(*node));
    if (node == NULL) return -1;
    node->data = value;
    node->next = curr;
    if (pred == NULL) head = node;
    else pred->next = node;
    return 1;
}

static int list_member(int value) {
    list_node_t *curr = head;
    while (curr != NULL && curr->data < value) curr = curr->next;
    return curr != NULL && curr->data == value;
}

static int list_delete(int value) {
    list_node_t *curr = head;
    list_node_t *pred = NULL;

    while (curr != NULL && curr->data < value) {
        pred = curr;
        curr = curr->next;
    }
    if (curr == NULL || curr->data != value) return 0;

    if (pred == NULL) head = curr->next;
    else pred->next = curr->next;
    free(curr);
    return 1;
}

static void list_free(void) {
    list_node_t *curr = head;
    while (curr != NULL) {
        list_node_t *next = curr->next;
        free(curr);
        curr = next;
    }
    head = NULL;
}

static uint64_t list_size_and_validate(int *valid) {
    list_node_t *curr = head;
    uint64_t count = 0;
    int previous = -1;
    *valid = 1;

    while (curr != NULL) {
        if (curr->data < 0 || (count > 0 && curr->data <= previous)) {
            *valid = 0;
            break;
        }
        previous = curr->data;
        count++;
        curr = curr->next;
    }
    return count;
}

static int populate_initial_list(uint64_t wanted, uint64_t *inserted) {
	uint32_t seed = 1;
	uint64_t attempts = 0;
	uint64_t max_attempts = wanted * 2;
	*inserted = 0;

	while (*inserted < wanted && attempts < max_attempts) {
		int key = (int)(rng_next(&seed) % MAX_KEY);
		int rc = list_insert(key);
		if (rc < 0) return -1;
		if (rc == 1) (*inserted)++;
		attempts++;
	}

	return 0;
}

typedef struct {
	uint64_t member_ops;  // кол-во операций поиска
        uint64_t insert_ops;
        uint64_t delete_ops;
        uint64_t member_hits;  // кол-во успешных поисков
        uint64_t inserted;
        uint64_t deleted;
        uint64_t allocation_errors;
} thread_result_t;

typedef struct {
	long rank;
	uint64_t ops;
	double search_fraction;
	double insert_fraction;
	thread_result_t result;
} thread_arg_t;



static void *thread_work(void *arg_ptr) {
	thread_arg_t *arg = (thread_arg_t *)arg_ptr;
	uint32_t seed = (uint32_t)arg->rank + 1U;
	uint64_t i;

	memset(&arg->result, 0, sizeof(arg->result));
	for (i = 0; i < arg->ops; i++) {
		double which_op = rng_double(&seed);
		int value = (int)(rng_next(&seed) % MAX_KEY);
		int rc;

		if (which_op < arg->search_fraction) {
			rc = read_lock();
			if (rc != 0) {
				arg->result.allocation_errors++;
				continue;
			}
			arg->result.member_hits += (uint64_t)list_member(value);
			rc = rw_unlock();
			if (rc != 0) arg->result.allocation_errors++;
			arg->result.member_ops++;
		} else if (which_op < arg->search_fraction + arg->insert_fraction) {
			rc = write_lock();
			if (rc != 0) {
				arg->result.allocation_errors++;
				continue;
			}
			rc = list_insert(value);
			if (rc == 1) arg->result.inserted++;
			else if (rc < 0) arg->result.allocation_errors++;
			if (rw_unlock() != 0) arg->result.allocation_errors++;
			arg->result.insert_ops++;
		} else {
			rc = write_lock();
			if (rc != 0) {
				arg->result.allocation_errors++;
				continue;
			}
			arg->result.deleted += (uint64_t)list_delete(value);
			if (rw_unlock() != 0) arg->result.allocation_errors++;
			arg->result.delete_ops++;
		}
	}

	return NULL;
}

typedef struct {
	double elapsed_seconds;
	uint64_t member_ops, insert_ops, delete_ops;
	uint64_t member_hits, inserted, deleted, errors;
	uint64_t initial_inserted, final_size;
	int list_valid;
} bench_result_t;

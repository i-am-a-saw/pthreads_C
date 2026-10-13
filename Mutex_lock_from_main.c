#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

pthread_mutex_t mutex;

void *routine(void *arg) {
	(void)arg;
	printf("Unlocking mutex from thread\n");
	int rc = pthread_mutex_unlock(&mutex);
	if (rc != 0) fprintf(stderr, "Error in thread: %s (код %d)\n", strerror(rc), rc);
	return NULL;
}


int main() {
	pthread_mutexattr_t attr;
	int rc = pthread_mutexattr_init(&attr);
	if (rc != 0) return 1;

	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	if (rc != 0) return 1;

	rc = pthread_mutex_init(&mutex, &attr);
	if (rc != 0) return 1;

	rc = pthread_mutexattr_destroy(&attr);
	if (rc != 0) return 1;
	
	printf("Locking mutex from main\n");
	rc = pthread_mutex_lock(&mutex);
	
	pthread_t thread;
	int err = pthread_create(&thread, NULL, routine, NULL);
	if (err != 0) return 1;

	err = pthread_join(thread, NULL);
	if (err != 0) printf("Error with join: %d\n", err);
	
	rc = pthread_mutex_unlock(&mutex);

	printf("Mutex is free\n");

	return EXIT_SUCCESS;
	
}

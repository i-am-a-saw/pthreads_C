#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct Task {long rank; const char *title;};

static void *routine(void *arg) {
	const struct Task *task = arg;
	printf("rank=%ld title=%s\n", task->rank, task->title);
	return NULL;
}

int main() {
	enum {N = 3};
	pthread_t threads[N];
	struct Task tasks[N];

	int state =  pthread_setcancelstate(
		PTHREAD_CANCEL_ENABLE,
		PTHREAD_CANCEL_ENABLE		
	);

	int type = pthread_setcanceltype(
		PTHREAD_CANCEL_ASYNCHRONOUS,
		PTHREAD_CANCEL_DEFERRED		
	);
	(void)state;
	(void)type;

	for (long i = 0; i < N; ++i) {
		tasks[i].rank = i;
		tasks[i].title = "worker";
		int err = pthread_create(&threads[i], NULL, routine, &tasks[i]);
		if (err != 0) {
			fprintf(stderr, "pthread_create: error %d\n", err);
			return EXIT_FAILURE;
		}
	}

	for (long i = 0; i < N; ++i) {
		pthread_join(threads[i], NULL);
	}

	return EXIT_SUCCESS;
}

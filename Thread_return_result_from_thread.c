#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct Task { long rank; const char *title; };
struct Result { long rank; const char *title; long answer; };

static void *routine(void* arg) {
	const struct Task *task = arg;
	struct Result *result = malloc(sizeof *result);
	if (result == NULL) return NULL;

	result->rank = task->rank;
	result->title = "return";
	result->answer = task->rank*10;
	printf("thread %ld: %s\n", task->rank, task->title);
	return result;
}

int main(void) {
	enum {N = 3};
	pthread_t threads[N];
	struct Task tasks[N];

	int created = 0;
	for(long i = 0; i < N; ++i) {
		tasks[i].rank = i;
		tasks[i].title = "Title";
		int err = pthread_create(&threads[i], NULL, routine, &tasks[i]);
		if (err != 0) {
			fprintf(stderr, "pthread_create: %d\n", err);
			break;
		}
		++created;
	}

	for (int i = 0; i < N; ++i) {
		void *raw_result = NULL;
		int err = pthread_join(threads[i], &raw_result);
		if (err != 0) {
			fprintf(stderr, "pthread_join: %d\n", err);
			continue;
		}
		struct Result *result = raw_result;
		if (result == NULL) {
			fprintf(stderr, "thread %d returned no result", i);
			continue;
		}
		printf("Return: rank=%ld title=%s answer=%ld\n", result->rank, result->title, result->answer);
		free(result);
	}

	return created == N ? EXIT_SUCCESS : EXIT_FAILURE;

}

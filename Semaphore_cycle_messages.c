#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#define MSG_SIZE 100

typedef struct { int rank; } thread_arg;

static int thread_count;
static sem_t *sems;
static char **messages;

static void *routine(void *arg) {
	int rank = ((thread_arg *)arg)->rank;
	int dest = (rank + 1) % thread_count;
	char *msg = malloc(MSG_SIZE);
	if (msg == NULL) {
		fprintf(stderr, "malloc failed in thread %d\n", rank);
		messages[dest] = NULL;
	} else {
		snprintf(msg, MSG_SIZE, "Hello from %d to %d", rank, dest);
		messages[dest] = msg;
	}

	if (sem_post(&sems[dest]) == -1) {
		perror("sem_post");
		abort();
	}

	while (sem_wait(&sems[rank]) == -1) {
		if (errno == EINTR) continue; // пришел запрещающий сигнал, значит ждем, когда семафор увеличится
		perror("sem_wait");
		abort();
	}

	if (messages[rank] != NULL)
		printf("Thread %d -> %s\n", rank, messages[rank]);
	else
		fprintf(stderr, "Thread %d recieved no message (sender alloc failure)\n", rank);

	return NULL;
}

int main(int argc, char **argv) {
	if (argc != 2){
		fprintf(stderr, "Usage: %s THREAD_COUNT (>=1)\n", argv[0]);
		return 1;
	}

	char *end;
	long n = strtol(argv[1], &end, 10);
	if (*argv[1] == '\0' || *end != '\0' || n < 1 || n > 10000) {
		fprintf(stderr, "THREAD_COUNT must be in [1,10000]\n");
		return 1;
	}

	thread_count = (int)n;

	pthread_t *threads = malloc((size_t)thread_count * sizeof *threads);
	thread_arg *args = malloc((size_t)thread_count * sizeof *args);
	sems = calloc((size_t)thread_count, sizeof(*sems));
	messages = calloc((size_t)thread_count, sizeof(*messages));

	if (!threads || !args || !sems || !messages) {
		perror("calloc");
		free(threads); free(messages); free(sems); free(args);
		return 1;
	}

	int initialized = 0;
	for (; initialized < thread_count; ++initialized) {
		if (sem_init(&sems[initialized], 0, 0) == -1) {
			perror("sem_init");
			for (int j = 0; j < initialized; ++j) sem_destroy(&sems[j]);
			free(threads); free(args); free(messages); free(sems);
			return 1;
		}
	}

	int created = 0;
	for (int i = 0; i < thread_count; ++i) {
		args[i].rank = i;
		int rc = pthread_create(&threads[i], NULL, routine, &args[i]);
		if (rc != 0) {
			fprintf(stderr, "pthread_cretae: %s\n", strerror(rc));
			break;
		}

		++created;
	}

	if (created != thread_count) {
		for (int i = 0; i < thread_count; ++i) sem_post(&sems[i]);
	}

	for (int i = 0; i < created; ++i) {
		int rc = pthread_join(threads[i], NULL);
		if (rc != 0) fprintf(stderr, "join error: %s\n", strerror(rc));
	}

	for (int i = 0; i < created; ++i) {
		sem_destroy(&sems[i]);
		free(messages[i]);
	}

	free(messages); free(sems); free(threads); free(args);
	return created == thread_count ? 0 : 1;
}

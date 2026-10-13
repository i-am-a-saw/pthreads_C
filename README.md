# pthreads_C

A collection of small C examples for learning POSIX threads (`pthreads`) and thread synchronization on Linux, including WSL.

The programs are independent examples. Compile and run them separately; the repository dump does not include a build system such as a `Makefile`.

## Requirements

- Linux or WSL with a C compiler such as GCC
- POSIX threads and POSIX semaphores (available in a typical Linux environment)

Check that GCC is installed:

```bash
gcc --version
```

Compile the runnable examples with `-pthread`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -pthread SOURCE.c -o PROGRAM
```

## Examples

### `Threads_task_workers.c`

Creates three worker threads. Each thread receives a pointer to its own `Task` structure and prints its `rank` and `title`. The main thread joins the workers before exiting.

Build and run:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -pthread Threads_task_workers.c -o Threads_task_workers
./Threads_task_workers
```

The order of the three worker messages is not guaranteed because the threads execute concurrently.

**Note:** the calls that set the cancellation state and cancellation type are made by the main thread. The program does not call `pthread_cancel()`, so it does not actually cancel a thread.

### `Thread_return_result_from_thread.c`

Creates three threads. Each worker allocates a `Result` structure on the heap, fills it with the task rank, the string `"return"`, and `rank * 10`, then returns the structure pointer. The main thread obtains each result through `pthread_join()`, prints it, and frees the allocated memory.

Build and run:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -pthread Thread_return_result_from_thread.c -o Thread_return_result_from_thread
./Thread_return_result_from_thread
```

Worker output may appear in different orders. The result lines are printed as the main thread joins the thread handles.

### `Semaphore_cycle_messages.c`

Accepts a thread count from `1` to `10000`. Each thread sends a message to the next thread in a circular sequence, using a semaphore to signal that the message is ready. It then waits on its own semaphore and prints the message received from the previous thread. With one thread, the message is sent back to that same thread.

Build and run with four threads:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -pthread Semaphore_cycle_messages.c -o Semaphore_cycle_messages
./Semaphore_cycle_messages 4
```

Usage:

```text
./Semaphore_cycle_messages THREAD_COUNT
```

The output order is not guaranteed. `THREAD_COUNT` must be in the inclusive range `[1, 10000]`.

### `Mutex_lock_from_main.c`

Initializes a mutex with the type `PTHREAD_MUTEX_ERRORCHECK`. The main thread locks the mutex, then starts a worker that attempts to unlock it. Since the worker is not the thread that owns the mutex, the unlock attempt should fail with an ownership error. After joining the worker, the main thread unlocks the mutex itself.

Build and run:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -pthread Mutex_lock_from_main.c -o Mutex_lock_from_main
./Mutex_lock_from_main
```

This example demonstrates that a thread cannot unlock an error-checking mutex owned by another thread. The diagnostic text can vary by system and locale.

### `RWLock_lab/my_rwlock.c`

This file begins a custom read-write lock implementation. The shown code defines a lock structure containing an internal mutex, condition variables for readers and writers, reader/writer counters, and a `writer_active` flag. It implements initialization, destruction, and reader-lock acquisition. New readers wait while a writer is active **or waiting**, which prevents readers from continuously bypassing waiting writers.

**Current limitation:** in the supplied source, this file ends after `my_rwlock_rdlock()`. It does not include reader unlock, writer lock/unlock functions, or a `main()` test program. Therefore, the code shown is an incomplete implementation rather than a runnable read-write-lock example.

## Repository layout

```text
.
├── Mutex_lock_from_main.c
├── RWLock_lab/
│   └── my_rwlock.c
├── Semaphore_cycle_messages.c
├── Thread_return_result_from_thread.c
└── Threads_task_workers.c
```

## Notes

- Each top-level `.c` file is a separate program and should be compiled into its own executable.
- The examples are intended for experimenting with thread creation, joining, return values, mutex ownership, and semaphore-based coordination.
- In `Thread_return_result_from_thread.c`, the join loop iterates over all three slots even if thread creation fails partway through. Error handling could be improved to join only threads that were successfully created.
- The `RWLock_lab` source needs the missing lock/unlock operations and a test program before its behavior can be exercised as a complete read-write lock.


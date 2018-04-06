#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_THREADS 99

volatile unsigned long long NUM_THREADS;
volatile unsigned int COUNT[MAX_THREADS] = {0};
volatile unsigned long long in_cs = 0;

volatile unsigned int RUNNING = 0;

/*
 * atomic_cmpxchg
 * 
 * equivalent to atomic execution of this code:
 *
 * if (*ptr == old) {
 *   *ptr = new;
 *   return old;
 * } else {
 *   return *ptr;
 * }
 *
 */
static inline int atomic_cmpxchg (volatile int *ptr, int old, int new)
{
  int ret;
  asm volatile ("lock cmpxchgl %2,%1"
		: "=a" (ret), "+m" (*ptr)
		: "r" (new), "0" (old)
		: "memory");
  return ret;
}

struct spin_lock_t {
  int locked;
};

struct spin_lock_t S = {.locked = 0};

void spin_lock(struct spin_lock_t *s) {
  /* Spin on the lock while it is 1. If the lock becomes zero and this thread
     sees it, then it places the lock back into the locked state. As this
     operation is atomic, only one thread can do this. All other threads will
     witness the state change. */
  while (atomic_cmpxchg(&s->locked, 0, 1) == 1) {sched_yield();}
}
void spin_unlock (struct spin_lock_t *s) {
  s->locked = 0;
}

void*
worker(void* args) {
  int i = ((int*)args)[0];
  while (RUNNING) {
    spin_lock(&S);
    
    assert(in_cs==0);
    in_cs++;
    assert(in_cs==1);
    in_cs++;
    assert(in_cs==2);
    in_cs++;
    assert(in_cs==3);
    in_cs=0;
    
    COUNT[i] += 1;
    spin_unlock(&S);
  }
  return NULL;
}

int main(int argc, char** argv) {
  pthread_t threads[MAX_THREADS];
  unsigned int ids[MAX_THREADS];
  unsigned int i;
  NUM_THREADS = 5;
  long long sleep_time;
  
  if (argc != 3) {
    printf("Usage:\n%s num_threads run_time\n", argv[0]);
    return 1;
  }
  
  errno = 0;
  NUM_THREADS = strtoll(argv[1], NULL, 10);
  if (errno) {
    printf("Could not convert given thread count to a number\n");
    return 1;
  }

  errno = 0;
  sleep_time = strtoll(argv[2], NULL, 10);
  if (errno) {
    printf("Could not convert given running time to a number\n");
    return 1;
  }

  if (NUM_THREADS == 0 && NUM_THREADS > 99) {
    printf("Invalid number of threads\n");
    return 1;
  }

  if (sleep_time < 0) {
    printf("Invalid sleep time\n");
    return 1;
  }

  
  RUNNING = 1;
  for (i = 0; i < NUM_THREADS; ++i) {
    ids[i] = i;
    if (pthread_create(&threads[i], NULL, worker, &ids[i])) {
      printf("Could not create thread\n");
      exit(1);
    }
  }

  sleep(sleep_time);

  RUNNING = 0;
  for (i = 0; i < NUM_THREADS; ++i) {
    if (pthread_join(threads[i], NULL)) {
      printf("Could not join thread\n");
      exit(1);
    }
  }
  
  for (i = 0; i < NUM_THREADS; ++i) {
    printf("Thread %d was in critical section %d times\n", i, COUNT[i]);
  }
  return 0;
}

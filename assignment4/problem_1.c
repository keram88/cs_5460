#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_THREADS 99

volatile unsigned int ENTERING[MAX_THREADS] = {0};
volatile unsigned int NUMBER[MAX_THREADS] = {0};
volatile unsigned long long NUM_THREADS;
volatile unsigned int COUNT[MAX_THREADS] = {0};
volatile unsigned long long in_cs = 0;

volatile unsigned int RUNNING = 0;

static unsigned int
max_num() {
  unsigned int max = 0;
  size_t j = 0;
  for (; j < NUM_THREADS; ++j) {
    if (NUMBER[j] > max) {
      max = NUMBER[j];
    }
  }
  return max;
}

static void
spin_lock(unsigned int i) {
  unsigned int j;
  ENTERING[i] = 1;
  NUMBER[i] = 1+max_num();
  ENTERING[i] = 0;
  for (j = 0; j < NUM_THREADS; j++) {
    // Wait until thread j receives its number
    if (i == j) continue;
    while (ENTERING[j]) {}
    // Wait until all threads with smaller numbers or with the same number,
    // but with higher priority, finish their work
    while((NUMBER[j] != 0) &&
	  ((NUMBER[j] < NUMBER[i] || ((NUMBER[j] ==  NUMBER[i]) && (j < i))))) {}
  }
}

static void
spin_unlock(int i) {
  NUMBER[i] = 0;
}

void*
worker(void* args) {
  int i = ((int*)args)[0];
  while (RUNNING) {
    spin_lock(i);
    
    assert(in_cs==0);
    in_cs++;
    assert(in_cs==1);
    in_cs++;
    assert(in_cs==2);
    in_cs++;
    assert(in_cs==3);
    in_cs=0;
    
    COUNT[i] += 1;
    spin_unlock(i);
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

  if (NUM_THREADS > 99) {
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

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

void mfence (void) {
  asm volatile ("mfence" : : : "memory");
}

static void
spin_lock(unsigned int i) {
  /* The key insight comes from the discussion section from the linked Wikipedia
     article.
     "The necessity of the variable Entering might not be obvious as there is no
     'lock' around the loop. However, suppose the variable was removed and
     two processes computed the same NUMBER[i]. If the higher-priority process
     was preempted before setting NUMBER[i], the low-priority process will see
     that the other process has a number of zero, and enters the critical 
     section; later, the high-priority process will ignore equal Number[i] for
     lower-priority processes, and also enters the critical section. As a 
     result, two processes can enter the critical section at the same time. The
     bakery algorithm uses the Entering variable to make the assignment to
     NUMBER[i] look like it was atomic; process i will never see a number equal
     to zero for a process j that is going to pick the same number as i."
     
     This is what we are concerned with:
     ENTERING[i] = 1;
     NUMBER[i] = 1+max_num(); // Must appear atomic.
     ENTERING[i] = 0;

     Since there is a TSO and all other processors see stores from other
     processors in the same order (if seen in a timely manner), we must make 
     sure that processor i sees processor j set ENTERING[j] = 1, otherwise, it
     could skip over the while (ENTERING[j]) guard, and start looking at the
     NUMBER array. This necessitates the first fence. The second fence is
     necessary so that other cores will see NUMBER[i] and not continue into
     the critical section inappropriately. We don't need another fence since
     when thread i sets its ENTERING value to 0, it will eventually be seen
     by the other processors, but they only needed to know about the two prior
     stores.

     Essentially, the first fence blocks other readers wat while(ENTERING[i]),
     and the second fence ensures NUMBER[i] is up-to-date for other readers
     before the ENTERING[i] 'lock' is released.

     The reason this worked for running on one processor is within a single
     processor, memory is coherent, so no special guards are needed.
  */
  unsigned int j;
  ENTERING[i] = 1;
  mfence();
  NUMBER[i] = 1+max_num();
  mfence();
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

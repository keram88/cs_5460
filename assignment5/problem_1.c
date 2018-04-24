//
//  problem_1.c
//  PetPlayground
//
//  Created by Marek Baranowski on 4/23/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define check_error(x,msg) (errno=0, (x) ? 0 : (printf("%s: %s\n", msg, strerror(errno)), exit(255), 0))

size_t MAX_ANIMALS=99;
pthread_mutex_t playground_m;
pthread_cond_t cat_cv, dogbird_cv;
unsigned int n_cats, n_dogs, n_birds, cats, dogs, birds;
unsigned int cat_play, dog_play, bird_play;
volatile char STOP;

void
print_usage(const char * name) {
  printf("Usage:\n%s cat_threads[0-99] dog_threads[0-99] bird_threads[0-99]\n", name);
  exit(0);
}

void
play(void) {
  int i = 0;
  for (i=0; i<1000; i++) {
    assert(cats <= n_cats);
    assert(dogs <= n_dogs);
    assert(birds <= n_birds);
    assert(cats == 0 || dogs == 0);
    assert(cats == 0 || birds == 0);
  }
}

void
lock(void) {
  check_error(!pthread_mutex_lock(&playground_m), "Could not acquire lock");
}

void
unlock(void) {
  check_error(!pthread_mutex_unlock(&playground_m), "Could not release lock");
}

void
Wait(pthread_cond_t* cv) {
  check_error(!pthread_cond_wait(cv, &playground_m), "Invalid cond var or mutex");
}

void
Bcast(pthread_cond_t* cv) {
  check_error(!pthread_cond_broadcast(cv), "Invalid cond var");
}

void
cat_enter(void) {
  lock();
  while(dogs > 0 || birds > 0) {
    Wait(&dogbird_cv);
  }
  ++cat_play;
  ++cats;
  unlock();
}

void
cat_exit(void) {
  lock();
  --cats;
  if (cats == 0) {
    Bcast(&cat_cv);
  }
  unlock();
}

void*
cat(void* arg) {
  while(!STOP) {
    cat_enter();
    play();
    cat_exit();
  }
  return NULL;
}

void
dog_enter(void) {
  lock();
  while (cats > 0) {
    Wait(&cat_cv);
  }
  ++dog_play;
  ++dogs;
  unlock();
}

void dog_exit(void) {
  lock();
  --dogs;
  if (dogs == 0 && birds == 0) {
    Bcast(&dogbird_cv);
  }
  unlock();
}

void*
dog(void* arg) {
  while(!STOP) {
    dog_enter();
    play();
    dog_exit();
  }
  return NULL;
}

void
bird_enter(void) {
  lock();
  
  while(cats > 0) {
    Wait(&cat_cv);
  }
  ++bird_play;
  ++birds;
  unlock();
}

void
bird_exit(void) {
  lock();
  --birds;
  if (dogs == 0 && birds == 0) {
    Bcast(&dogbird_cv);
  }
  unlock();
}

void*
bird(void* arg) {
  while(!STOP) {
    bird_enter();
    play();
    bird_exit();
  }
  return NULL;
}

unsigned long long
checked_strtoull(const char* name, const char* arg, const char* var) {
  char* end;
  long long result;
  int old_errno = errno;
  errno = 0;
  result = strtoll(arg, &end, 10);
  if (errno || end == arg) {
    printf("Could not convert %s for %s: %s\n", arg, var, strerror(errno));
    print_usage(name);
  }
  if (result < 0 || result > (long long) MAX_ANIMALS) {
    printf("Argument %s out of range: %lld\n", var, result);
    print_usage(name);
  }
  errno = old_errno;
  return result;
}

int main(int argc, const char * argv[]) {
  if (argc != 4) {
    print_usage(argv[0]);
  }

  check_error(!pthread_mutex_init(&playground_m, NULL), "Could not initialize mutex");
  check_error(!pthread_cond_init(&cat_cv, NULL), "Could not initialize cond variable");
  check_error(!pthread_cond_init(&dogbird_cv, NULL), "Could not initialize cond variable");
  size_t i = 0;
  
  n_cats=checked_strtoull(argv[0], argv[1], "cat_threads");
  n_dogs=checked_strtoull(argv[0], argv[2], "dog_threads");
  n_birds=checked_strtoull(argv[0], argv[3], "bird_threads");
  
  pthread_t bird_th[MAX_ANIMALS];
  pthread_t cat_th[MAX_ANIMALS];
  pthread_t dog_th[MAX_ANIMALS];
  
  for (i = 0; i < n_cats; ++i) {
    check_error(!pthread_create(&cat_th[i], NULL, cat, NULL), "Could not create thread");
  }
  
  for (i = 0; i < n_birds; ++i) {
    check_error(!pthread_create(&bird_th[i], NULL, bird, NULL), "Could not create thread");
  }
  
  for (i = 0; i < n_dogs; ++i) {
    check_error(!pthread_create(&dog_th[i], NULL, dog, NULL), "Could not create thread");
  }
  
  STOP = 0;
  sleep(10);
  STOP = 1;

  for (i = 0; i < n_dogs; ++i) {
    check_error(!pthread_join(dog_th[i], NULL), "Could not join thread");
  }
  
  for (i = 0; i < n_birds; ++i) {
    check_error(!pthread_join(bird_th[i], NULL), "Could not join thread");
  }
  
  for (i = 0; i < n_cats; ++i) {
    check_error(!pthread_join(cat_th[i], NULL), "Could not join thread");
  }
  
  printf("cat play = %d, dog play = %d, bird play = %d\n", cat_play, dog_play, bird_play);
  
  check_error(!pthread_cond_destroy(&dogbird_cv), "Could not destroy cond variable");
  check_error(!pthread_cond_destroy(&cat_cv), "Could not destroy cond variable");
  check_error(!pthread_mutex_destroy(&playground_m), "Could not destroy mutex");
  return 0;
}

/* This program doesn't implement the monitor pattern correctly. *
 * This is evidenced by the code with the comment:               *
 * "Ewwww!!! gross and weird!"                                   *
 * However, it appears to not get dead-locked without this       *
 * because of spurious wake ups on the cv                        */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#define check_error(x,msg) ((x) ? (0) :	\
			    (printf("%s: %s\n", msg, strerror(errno)), exit(255), 0))

#define MAX_THREADS 99
pthread_mutex_t vec_m;
pthread_cond_t vec_cv;
pthread_cond_t cons_cv;
size_t num_threads;
size_t cons_exited;
volatile char done;

void
Lock(void) {
  check_error(!pthread_mutex_lock(&vec_m), "Could not acquire lock");
}

void
Unlock(void) {
  check_error(!pthread_mutex_unlock(&vec_m), "Could not release lock");
}

void
Wait(pthread_cond_t* cv) {
  check_error(!pthread_cond_wait(cv, &vec_m), "Invalid cond var or mutex");
}

void
Bcast(pthread_cond_t* cv) {
  check_error(!pthread_cond_broadcast(cv), "Invalid cond var");
}


typedef struct {
  char* name;
  char* cksum;
} pair;

typedef struct {
  pair* entries;
  size_t len;
  size_t cap;
} vector;

typedef struct {
  const char* dir;
  vector* files_in;
  vector* files_out;
} param_pack;

vector
mk_vec(void) {
  pair* entries;
  size_t cap = 1024;
  vector result;
  check_error((entries = calloc(cap, sizeof(pair))), "Unable to malloc");
  result.entries = entries;
  result.len = 0;
  result.cap = cap;
  return result;
}

void
vec_push(vector* v, char* n, char* cksum) {
  pair p;
  char* cpy;
  size_t len;
  size_t old_cap;
  size_t i;
  p.name = NULL;
  p.cksum = NULL;
  if (v->len == v->cap) {
    old_cap = v->cap;
    check_error((v->entries = realloc(v->entries, sizeof(pair)*v->cap*2)), "Could not expand vector");
    for (i = old_cap; i < v->cap*2; ++i) {
      v->entries[i] = p;
    }
    v->cap *= 2;
  }
  len = strlen(n);
  check_error((cpy = malloc(sizeof(char)*len+1)), "Unable to malloc");
  cpy = strcpy(cpy, n);
  p.name = cpy;

  if (cksum != NULL) {
    len = strlen(cksum);
    check_error((cpy = malloc(sizeof(char)*len+1)), "Unable to malloc");
    cpy = strcpy(cpy, cksum);
    p.cksum = cpy;
  }
  else {
    p.cksum = NULL;
  }
  v->entries[v->len] = p;
  v->len++;
}

/* Caller is responsible for freeing the result of this function */
pair
vec_pop(vector* v) {
  pair result;
  pair p = {NULL, NULL};
  if (v->len == 0) { return p; }
  result = v->entries[v->len-1];
  v->entries[v->len-1] = p;
  v->len--;
  return result;
}

void
vec_destroy(vector* v) {
  size_t i = 0;
  while (i < v->cap && (v->entries[i].name != NULL || v->entries[i].cksum != NULL)) {
    free(v->entries[i].name);
    free(v->entries[i].cksum);
    i += 1;
  }
  free(v->entries);
}

/* From                                                                      *
 * https://opensource.apple.com/source/xnu/xnu-1456.1.26/bsd/libkern/crc32.c *
 * all credit for the crc32 function goes to the authors.                    *
 */
static uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t
crc32(uint32_t crc, const void * buf, size_t size)
{
  const uint8_t *p;

  p = buf;
  crc = crc ^ ~0U;

  while (size--)
    crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

  return crc ^ ~0U;
}

void
print_usage(const char * name) {
  printf("Usage:\n%s directory threads[1-99]\n", name);
  exit(0);
}

int
paircmp(const void* a, const void* b) {
  const pair* pa = (const pair*) a;
  const pair* pb = (const pair*) b;
  const char* sa = pa->name;
  const char* sb = pb->name;
  return strcmp(sa, sb);
}

uint32_t*
run_cksum(FILE* file) {
  uint32_t* crc;
  char buf[4096];
  size_t num_read;
  check_error((crc = malloc(sizeof(uint32_t))), "Could not malloc");
  *crc = 0;
  while (1) {
    num_read = fread(buf, 1, sizeof(buf), file);
    *crc = crc32(*crc, buf, num_read);
    if (num_read < sizeof(buf)) {
      if (feof(file)) {
	break;
      }
      if (ferror(file)) {
	free(crc);
	return NULL;
      }
    } 
  }
  return crc;
}

void
cksum_file(const char* name, pair* p) {
  struct stat path_stat;
  if (stat(name, &path_stat) == -1) { return; };
  if (!S_ISREG(path_stat.st_mode)) {
    return;
  }
  char* result;
  FILE* file;
  uint32_t* cksum;
  errno = 0;
  if ((file = fopen(name, "r"))) {
    cksum = run_cksum(file);
    if (cksum) {
      check_error((result = malloc(8+1)), "Could not malloc");
      sprintf(result, "%08X", *cksum);
    }
    else {
      check_error((result = malloc(sizeof("ACCESS DENIED"))), "Could not malloc");
      strcpy(result, "ACCESS DENIED");
    }
    fclose(file);
    free(cksum);
  }
  else {
    check_error((result = malloc(sizeof("ACCESS DENIED"))), "Could not malloc");
    strcpy(result, "ACCESS DENIED");
  }
  p->cksum = result;
}

void
cksum_dir(const char* base, vector* files_in, vector* files_out) {
  pair p;
  char* path_name;
  size_t base_len = strlen(base);
  size_t file_len;
  while (files_in->len > 0) {
    p = vec_pop(files_in);
    file_len = strlen(p.name);
    check_error((path_name=malloc(base_len + file_len + 2)), "Could not malloc");
    sprintf(path_name, "%s/%s", base, p.name);
    cksum_file(path_name, &p);
    vec_push(files_out, p.name, p.cksum);
  }
}

void
list_dir(const char* dir, vector* files) {
  DIR* d;
  struct dirent* dents;

  check_error((d = opendir(dir)), "Error opening directory");
  errno = 0;
  while ((dents = readdir(d)) != NULL && errno == 0) {
    vec_push(files, dents->d_name, NULL);
  }
  if (errno != 0) {
    printf("Error while reading %s: %s\n", dir, strerror(errno));
    exit(255);
  }
  
  check_error(closedir(d) == 0, "Error closing directory");
}

unsigned long long
checked_strtoull(const char* name, const char* arg, const char* var) {
  char* end;
  long long result;
  errno = 0;
  result = strtoll(arg, &end, 10);
  if (errno || end == arg) {
    printf("Could not convert %s for %s: %s\n", arg, var, strerror(errno));
    print_usage(name);
  }
  if (result < 1 || result > MAX_THREADS) {
    printf("Argument %s out of range: %lld\n", var, result);
    print_usage(name);
  }
  return result;
}

void
put_file(vector* files_in, char* name) {
  Lock();
  vec_push(files_in, name, NULL);
  Bcast(&vec_cv);
  Unlock();
}

void*
dir_worker(void* params_v) {
  param_pack* params = (param_pack*) params_v;
  const char* dir = params->dir;
  vector* files_in = params->files_in;
  vector* files_out = params->files_out;
  DIR* d;
  struct dirent* dents;
  size_t total_files = 0;
  size_t consumed = 0;
  size_t exited_count = 0;
  check_error((d = opendir(dir)), "Error opening directory");
  errno = 0;
  while ((dents = readdir(d)) != NULL && errno == 0) {
    put_file(files_in, dents->d_name);
    total_files += 1;
  }
  if (errno != 0) {
    printf("Error while reading %s: %s\n", dir, strerror(errno));
    exit(255);
  }

  Lock();
  done = 1;
  consumed = files_out->len;
  exited_count = cons_exited;
  Unlock();

  check_error(closedir(d) == 0, "Error closing directory");

  // Wait for consumers...
  // Ewwww!!! gross and weird!
  while(consumed < total_files || exited_count != num_threads) {
    Lock();
    Bcast(&vec_cv);
    consumed = files_out->len;
    exited_count = cons_exited;
    if (consumed == total_files && exited_count == num_threads) {
      Unlock();
      break;
    }
    Wait(&cons_cv);
    consumed = files_out->len;
    exited_count = cons_exited;
    Unlock();
  }

  return NULL;
}

void
get_file(pair* my_pair, vector* files_in) {
  Lock();
  while (files_in->len == 0 && !done) {
    Wait(&vec_cv);
  }
  *my_pair = vec_pop(files_in);
  Unlock();
}

void
put_cksum(pair* my_pair, vector* files_out) {
  Lock();
  vec_push(files_out, my_pair->name, my_pair->cksum);
  Bcast(&cons_cv);
  Unlock();
}

void*
cksum_worker(void* params_v) {
  param_pack* params = (param_pack*) params_v;
  const char* base = params->dir;
  char* path_name;
  vector* files_in = params->files_in;
  vector* files_out = params->files_out;
  size_t base_len = strlen(base);
  size_t file_len;
  pair* my_pair;
  int finished;
  int todo;
  check_error((my_pair = malloc(sizeof(pair))), "Could not malloc");
  Lock();
  finished = done;
  todo = files_in->len;
  Unlock();
  while (todo > 0 || !finished) {
    get_file(my_pair, files_in);
    if (my_pair->name == NULL) {
      Lock();
      finished = done;
      todo = files_in->len;
      Unlock();
      continue;
    }
    // Do work
    file_len = strlen(my_pair->name);
    check_error((path_name=malloc(base_len + file_len + 2)), "Could not malloc");
    sprintf(path_name, "%s/%s", base, my_pair->name);
    cksum_file(path_name, my_pair);
    free(path_name);
    // Put back cksum
    put_cksum(my_pair, files_out);
    Lock();
    finished = done;
    todo = files_in->len;
    Unlock();
  }
  Lock();
  cons_exited++;
  Bcast(&cons_cv);
  Unlock();
  free(my_pair);
  return NULL;
}

int
main(int argc, const char * argv[]) {
  vector files_in = mk_vec();
  vector files_out = mk_vec();
  size_t i;
  pthread_t cksum_th[MAX_THREADS];
  pthread_t list_th;
  if (argc != 3) {
    print_usage(argv[0]);
  }
  
  check_error(!pthread_mutex_init(&vec_m, NULL), "Could not initialize mutex");
  check_error(!pthread_cond_init(&vec_cv, NULL), "Could not initialize cond variable");
  check_error(!pthread_cond_init(&cons_cv, NULL), "Could not initialize cond variable");
  done = 0;
  
  num_threads = checked_strtoull(argv[0], argv[2], "threads");

  param_pack params = {argv[1], &files_in, &files_out};
  
  check_error(!pthread_create(&list_th, NULL, dir_worker, (void*)&params), "Could not create thread");
  for (i = 0; i < num_threads; ++i) {
    check_error(!pthread_create(&cksum_th[i], NULL, cksum_worker, (void*)&params), "Could not create thread");
  }

  check_error(!pthread_join(list_th, NULL), "Could not join thread");

  for (i = 0; i < num_threads; ++i) {
    check_error(!pthread_join(cksum_th[i], NULL), "Could not join thread");
  }

  qsort(files_out.entries, files_out.len, sizeof(pair), paircmp);
  for (i = 0; i < files_out.len; ++i) {
    if (files_out.entries[i].cksum != NULL) {
      printf("%s %s\n", files_out.entries[i].name, files_out.entries[i].cksum);
    }
  }
  vec_destroy(&files_in);
  vec_destroy(&files_out);
  
  check_error(!pthread_cond_destroy(&cons_cv), "Could not destroy cond variable");
  check_error(!pthread_cond_destroy(&vec_cv), "Could not destroy cond variable");
  check_error(!pthread_mutex_destroy(&vec_m), "Could not destroy mutex");

  return 0;
}


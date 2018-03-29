/** program to test the kernel module **/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/wait.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

int main(void) {  
  int i;
  int j;
  int fd;
  int sleep_len;
  ssize_t r;
  const char* devs[] = {"/dev/sleepy0","/dev/sleepy1",
			"/dev/sleepy2","/dev/sleepy3",
			"/dev/sleepy4","/dev/sleepy5",
			"/dev/sleepy6","/dev/sleepy7",
			"/dev/sleepy8","/dev/sleepy9"};
  pid_t my_pid;
  for (i = 0; i < 10; ++i) {
    for (j = 0; j < 10; ++i) {
      if (fork() == 0) {
	/* writing to device i*/
	fd = open(devs[j], O_RDWR);
	assert(fd != -1);
	
	srand(time(NULL)+getpid());
	usleep(rand()%20);
	
	sleep_len = 12-j;
	r = write(fd, &sleep_len, sizeof sleep_len);
	assert(r >= 0);
	close(fd);
	
	return 0;
      }
    }
  }

  /* sleep for a second*/
  sleep(1);

  /* read from device 9*/
  fd = open("/dev/sleepy9", O_RDWR);
  assert(fd != -1);
  r = read(fd, NULL, 0);
  assert(r >= 0);
  close(fd);

  sleep(5); /* sleep for 7 seconds*/

  /* now read from device 0*/
  for (i = 0; i < 10; ++i) {
    fd = open(devs[i], O_RDWR);
    assert(fd != -1);
    r = read(fd, NULL, 0);
    assert(r >= 0);
    close(fd);
  }

  for (i = 0; i < 10*10; i++)
    wait(NULL);
  
  return 0;
}

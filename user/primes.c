#include "kernel/types.h"
#include "user/user.h"

void primes(int p_left[2]) {
  int prime;
  int p_right[2];
  int n;

  if (read(p_left[0], &prime, sizeof(prime)) == 0) {
    close(p_left[0]);
    exit(0);
  }

  printf("prime %d\n", prime);

  if (pipe(p_right) < 0) {
    fprintf(2, "pipe failed\n");
    close(p_left[0]);
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork failed\n");
    close(p_left[0]);
    close(p_right[0]);
    close(p_right[1]);
    exit(1);
  }

  if (pid == 0) {
    close(p_right[1]);   
    close(p_left[0]);    
    primes(p_right);     
  } else {
    close(p_right[0]);   
    while (read(p_left[0], &n, sizeof(n)) > 0) {
      if (n % prime != 0) {
        write(p_right[1], &n, sizeof(n));
      }
    }
    close(p_left[0]);
    close(p_right[1]);
    wait(0);
  }

  exit(0);
}

int
main(int argc, char *argv[])
{
  int p[2];
  if (pipe(p) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    close(p[1]);      
    primes(p);
  } else {
    close(p[0]);      
    for (int i = 2; i <= 35; i++) {
      write(p[1], &i, sizeof(i));
    }
    close(p[1]);
    wait(0);
  }

  exit(0);
}
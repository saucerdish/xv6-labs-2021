#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  int pid=fork();
  if(pid==0){//child
    char buf[2];
    read(p[0],buf,sizeof(buf));
    printf("%d: received ping\n",getpid());
    write(p[1],"1",2);
    close(p[0]);
    close(p[1]);
    exit(0);
  }else{
    char buf[2];
    write(p[1],"1",2);
    read(p[0],buf,sizeof(buf));
    printf("%d: received pong\n",getpid());
    close(p[0]);
    close(p[1]);
  }
  exit(0);
}

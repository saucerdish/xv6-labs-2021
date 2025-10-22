# lab0
## sleep
只是简单的调用sleep函数，熟悉整体项目代码。argv是命令行参数，例如`echo hello`传参就是argv=["echo","hello"]，argc=2。运用事先留好的atoi将字符串转化为int，再调用sleep。最后别忘记`exit(0)`保证程序正常退出。
```c
if(argc < 2){
    fprintf(2, "Usage: sleep number...\n");
    exit(1);
  }

  int num=atoi(argv[1]);
  sleep(num);
  exit(0);
```
## pingpong
==The parent should send a byte to the child; the child should print "<pid>: received ping", where <pid> is its process ID, write the byte on the pipe to the parent, and exit; the parent should read the byte from the child, print "<pid>: received pong", and exit.==
父亲发送一个字节，等待孩子接收并回答一个字节，父亲收到孩子发送的字节后打印提示信息并退出；孩子等待父亲发来的一个字节，打印提示信息，发送一个字节给父亲后退出。关键函数fork和pipe。

```c
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
  ```
## primes
实现素数筛（Prime Sieve）算法的进程通信版本。利用`pipe`与`fork`实现多个进程间的“筛选流水线”。每个进程筛除能被自己当前素数整除的数，并将剩余数传递给下一个进程。
**实验思路**
1. 父进程创建管道并写入初始数字 2 ~ 35。
2. 每个子进程：
    从管道读第一个数，作为当前素数 prime；打印该素数；创建新的管道；`fork`出下一个子进程；将不能被 prime 整除的数写入新管道；关闭文件描述符并退出。
3. 当管道为空时，子进程退出。
```c
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
```
## find
实现`find`命令：在目录树中查找所有名字为特定文件名的文件，打印其完整路径。
**实验思路**
1. 使用`open()`打开路径并调用`fstat()`判断类型；
2. 若为文件：比较文件名是否匹配；
3. 若为目录：
    遍历目录项`read(fd, &de, sizeof(de))`；
    跳过 `.` 与 `..`；
    拼接子路径后递归调用；
4. 最后关闭文件描述符并退出。
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  for(p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  if(strlen(p) >= DIRSIZ)
    return p;

  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;  // ✅ 去掉补空格，改为正常字符串结尾
  return buf;
}

void
find(char *path, char *filename)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    // printf("tfile filename:%s\n", fmtname(path));
    if(strcmp(fmtname(path), filename) == 0)
      printf("%s\n", path);     
    break;

  case T_DIR:
    // printf("dir filename:%s\n", path);
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      find(buf, filename);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(2, "Usage: find <directory> <filename>\n");
    exit(0);
  }

  find(argv[1], argv[2]);
  exit(0);
}
```
## xargs
实现简化版 xargs：从标准输入读取行内容，每读取一行，就将该行作为额外参数执行指定命令。
从标准输入读取内容直到换行；每行作为新的参数调用一次命令；通过`fork() + exec()`运行；父进程用`wait()`等待子进程完成；支持多参数命令，如`xargs echo bye`。
```c
if(argc < 2){
    fprintf(2, "Usage: xargs command [args...]\n");
    exit(1);
  }

  char buf[512];
  int n = 0;
  char c;

  // 从标准输入逐字读取
  while(read(0, &c, 1) == 1){
    if(c == '\n'){
      buf[n] = 0;

      char *args[MAXARG];
      for(int i = 1; i < argc; i++)
        args[i-1] = argv[i];   
      int argi = argc - 1;

      char *p = buf;
      while(*p){
        while(*p == ' ') p++;
        if(*p == 0) break;
        args[argi++] = p;
        while(*p && *p != ' ') p++;
        if(*p) *p++ = 0;
      }
      args[argi] = 0;

      if(fork() == 0){
        exec(argv[1], args);
        fprintf(2, "xargs: exec failed\n");
        exit(1);
      }
      wait(0);

      n = 0;
    } else {
      buf[n++] = c;
    }
  }
  exit(0);
```
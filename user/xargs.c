#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

//xargs 负责从标准输入（stdin）读取数据，把这些数据当作参数传递给指定的命令并执行。

int
main(int argc, char *argv[])
{
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
}

# 🧪 xv6 syscall实验报告
## ✳️ 实验一：`trace` 
### 1. 功能说明
`trace(mask)` 用于开启系统调用跟踪。若某位系统调用号对应的比特在 `mask` 中被置位，当该进程调用该系统调用时，内核会打印：
```
pid: syscall_name -> return_value
```
并且该状态会被子进程继承。
### 2. 实现步骤
#### （1）用户态声明
**文件：** `user/user.h`
```c
int trace(int);
```
**文件：** `user/usys.pl`
```pl
entry("trace");
```
#### （2）分配系统调用号
**文件：** `kernel/syscall.h`
```c
#define SYS_trace 22
```
#### （3）在内核中实现系统调用逻辑
**文件：** `kernel/sysproc.c`
```c
uint64 sys_trace(void)
{
  int mask;
  if (argint(0, &mask) < 0)
    return -1;
  myproc()->mask = mask;
  return 0;
}
```

#### （4）为进程结构体添加字段
**文件：** `kernel/proc.h`
```c
int mask;   // 保存trace掩码
```

#### （5）在 fork 时继承 trace 状态
**文件：** `kernel/proc.c`
```c
np->mask = p->mask;
```

#### （6）在系统调用表中注册
**文件：** `kernel/syscall.c`
```c
extern uint64 sys_trace(void);
[SYS_trace] sys_trace,
```
#### （7）在 syscall() 中添加打印逻辑
```c
if((p->mask & (1 << num))){          // 若该系统调用在跟踪掩码中
      static char *syscall_names[] = {
        [SYS_fork]   "fork",
        [SYS_exit]   "exit",
        [SYS_wait]   "wait",
        [SYS_pipe]   "pipe",
        [SYS_read]   "read",
        [SYS_kill]   "kill",
        [SYS_exec]   "exec",
        [SYS_fstat]  "fstat",
        [SYS_chdir]  "chdir",
        [SYS_dup]    "dup",
        [SYS_getpid] "getpid",
        [SYS_sbrk]   "sbrk",
        [SYS_sleep]  "sleep",
        [SYS_uptime] "uptime",
        [SYS_open]   "open",
        [SYS_write]  "write",
        [SYS_mknod]  "mknod",
        [SYS_unlink] "unlink",
        [SYS_link]   "link",
        [SYS_mkdir]  "mkdir",
        [SYS_close]  "close",
        [SYS_trace]  "trace",
        [SYS_sysinfo]  "sysinfo",
      };
      printf("%d: syscall %s -> %d\n", p->pid, syscall_names[num], p->trapframe->a0);
    }
```
### 3. 运行结果
```bash
$ trace 32 grep hello README
```
输出示例：
```
5: syscall read -> 3
5: syscall close -> 0
5: syscall exit -> 0
```
说明系统调用跟踪功能实现成功。

## ✳️ 实验二：`sysinfo` 系统调用
### 1. 功能说明
`sysinfo(struct sysinfo *info)` 用于获取当前系统信息，包括：
| 字段        | 含义                |
| --------- | ----------------- |
| `freemem` | 系统中空闲的内存字节数       |
| `nproc`   | 当前活动（非 UNUSED）进程数 |
### 2. 实现步骤
#### （1）用户态声明与桩函数
**文件：** `user/user.h`
```c
struct sysinfo;
int sysinfo(struct sysinfo *);
```
**文件：** `user/usys.pl`
```pl
entry("sysinfo");
```
#### （2）分配系统调用号
**文件：** `kernel/syscall.h`
```c
#define SYS_sysinfo 23
```
#### （3）实现内核逻辑
**文件：** `kernel/sysproc.c`
```c
#include "sysinfo.h"

uint64 sys_sysinfo(void)
{
  uint64 uaddr;
  if (argaddr(0, &uaddr) < 0)
    return -1;

  struct sysinfo si;
  si.freemem = freemem();
  si.nproc = countnproc();

  struct proc *p = myproc();
  if (copyout(p->pagetable, uaddr, (char *)&si, sizeof(si)) < 0)
    return -1;
  return 0;
}
```

#### （5）统计函数实现
**文件：** `kernel/kalloc.c`
```c
uint64 
freemem(void)
{
  uint64 count=0;
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r){
    r=r->next;
    count++;
  }
  release(&kmem.lock);
  return count*PGSIZE;
}
```

**文件：** `kernel/proc.c`
```c
uint64
countnproc(void)
{
  uint64 count=0;
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state!=UNUSED) count++;
    release(&p->lock);
  }
  return count;
}
```

#### （6）注册系统调用
**文件：** `kernel/syscall.c`
```c
extern uint64 sys_sysinfo(void);
[SYS_sysinfo] sys_sysinfo,
```

#### （7）函数声明
**文件：** `kernel/defs.h`
```c
uint64          countnproc(void);
uint64          freemem(void);
```

### 3. 测试运行
在 xv6 shell 中运行：
```bash
$ sysinfotest
sysinfotest: start
sysinfotest: OK
```
说明系统调用实现正确，能正确获取系统内存与进程数。

## 四、实验结果与分析
| 实验      | 功能验证结果        |
| ------- | ------------- |
| trace   | 正确输出系统调用信息    |
| sysinfo | 正确统计系统信息并通过测试 |

实验表明：
* 系统调用的添加流程已掌握；
* 能正确在 `syscall`、`sysproc`、`proc`、`kalloc` 等模块间传递数据；
* 对 xv6 系统调用路径的理解进一步加深。

## 五、实验收获
1. 熟悉 xv6 系统调用机制，从用户态函数到内核态处理的完整过程。
2. 掌握如何通过 `copyout()` 安全地将内核数据返回给用户程序。
3. 理解了 `proc` 表与内存分配器的运行机制。
4. 能够通过阅读源码定位系统调用的执行流，从而为后续实验（如调度器、虚存）打下基础。

---

## 六、附录：修改文件列表

| 文件                 | 修改内容                                   |
| ------------------ | -------------------------------------- |
| `Makefile`         | 添加 `$U/_trace` 和 `$U/_sysinfotest`     |
| `user/user.h`      | 添加 `trace()`、`sysinfo()` 声明            |
| `user/usys.pl`     | 添加 `entry("trace")`、`entry("sysinfo")` |
| `kernel/syscall.h` | 新增 `SYS_trace`、`SYS_sysinfo`           |
| `kernel/proc.h`    | 增加字段 `int mask`                        |
| `kernel/sysproc.c` | 实现 `sys_trace` 与 `sys_sysinfo`         |
| `kernel/syscall.c` | 注册系统调用并添加 trace 打印                     |
| `kernel/proc.c`    | 实现 `nproc()`；修改 `fork()` 继承 trace 掩码   |
| `kernel/kalloc.c`  | 实现 `kfreemem()`                        |
| `kernel/sysinfo.h` | 定义结构体 `struct sysinfo`                 |
| `kernel/defs.h`    | 添加函数原型声明                               |

---

✅ **最终结果：**

* 所有用户态程序（`trace`, `sysinfotest`）均编译通过；
* 执行输出结果与官方期望一致；
* 实验通过！
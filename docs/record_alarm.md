# xv6 Lab: Alarm 实验报告
## 一、实验目的
本实验旨在为 xv6 操作系统添加一个 **周期性用户闹钟机制（sigalarm/sigreturn）**，
使得进程在运行时可以被定时中断并执行一个用户定义的函数。
这是一种 **用户级中断处理（user-level interrupt）** 的实现方式，可扩展到如用户级异常处理等更复杂场景。
## 二、实验原理
### 1. 基本功能
用户通过系统调用：
```c
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```
来注册周期性闹钟。
* `sigalarm(n, handler)`
  表示每当进程消耗 **n 个时钟 tick** 后，内核自动跳转到 `handler` 执行。
* 当 `handler` 执行完后，必须调用 `sigreturn()`，
  以恢复被中断时的上下文并继续执行主程序。
### 2. 关键点
* 每个时钟中断会触发内核 `usertrap()`；
* 如果进程设置了 `alarmticks`，则在计满后由内核将 `epc` 指向用户的 `handler`；
* 为保证中断前的程序能正确恢复，内核需要保存和恢复 `trapframe`；
* 防止重入（在 handler 内再次触发 handler）。
## 三、内核修改
### 1. 修改 `Makefile`
在 `UPROGS` 中添加：
```makefile
$U/_alarmtest\
```
使 `alarmtest.c` 能作为用户程序编译。
### 2. 新增系统调用接口
#### user/user.h
```c
int sigalarm(int, void*);
int sigreturn(void);
```
#### user/usys.pl
```perl
entry("sigalarm");
entry("sigreturn");
```
#### kernel/syscall.h
```c
#define SYS_sigalarm  22
#define SYS_sigreturn 23
```
#### kernel/syscall.c
```c
extern uint64 sys_sigalarm(void);
extern uint64 sys_sigreturn(void);
static uint64 (*syscalls[])(void) = {
  ...
  [SYS_sigalarm]  sys_sigalarm,
  [SYS_sigreturn] sys_sigreturn,
};
```
### 3. 修改 `struct proc`

在 `kernel/proc.h` 中新增：

```c
int alarmticks;                // 闹钟周期
int tickcount;                 // 当前计数
void (*alarmhandler)();        // 用户注册的处理函数
int alarmactive;               // 标记是否正在执行handler
struct trapframe *alarm_tf;    // 保存被打断时的上下文
```
并在 `allocproc()` 中初始化：
```c
p->alarmticks = 0;
p->tickcount = 0;
p->alarmhandler = 0;
p->alarmactive = 0;
p->alarm_tf = 0;
```
### 4. 实现系统调用
#### kernel/sysproc.c
```c
uint64 
sys_sigalarm(void)
{
  int ticks;
  if(argint(0, &ticks) < 0) return -1;
  uint64 handle;
  if(argaddr(1, &handle) < 0) return -1;
  struct proc* p = myproc();
  p->alarmticks = ticks;
  p->alarmhandler = (void*)handle;
  return 0;
}

uint64 
sys_sigreturn(void)
{
  struct proc* p = myproc();
  // 恢复被中断时的trapframe
  p->trapframe = p->alarm_tf;
  p->alarmactive = 0;
  return 0;
}
```
### 5. 修改 `usertrap()`
在 `kernel/trap.c` 的时钟中断处理中（`which_dev==2`）添加逻辑：
```c
} else if((which_dev = devintr()) != 0){
    if(which_dev == 2){  // 时钟中断
      if(p->alarmticks > 0){
        p->tickcount++;
        if(p->tickcount == p->alarmticks && p->alarmactive == 0){
          p->tickcount = 0;
          // 保存当前上下文
          p->alarm_tf = kalloc();
          *(p->alarm_tf) = *(p->trapframe);
          // 修改返回地址为 handler
          p->trapframe->epc = (uint64)(p->alarmhandler);
          p->alarmactive = 1;
        }
      }
    }
```
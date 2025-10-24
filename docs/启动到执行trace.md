# 🧩 一、启动阶段：内核初始化与用户程序启动
当我们在 QEMU 中运行 xv6 时，整个系统的启动流程如下：
```text
_entry → start() → main() → userinit() → exec("/init") → init → exec("sh")
```
## 1. `_entry` → `start()`：设置栈与特权模式
**文件：** `kernel/entry.S`
```S
_entry:
	# set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        la sp, stack0
        li a0, 1024*4
	    csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
	# jump to start() in start.c
        call start
```
* 在机器模式下,`_entry` 是汇编入口，将xv6内核加载到物理地址的内存中0x80000000。它将内核置于0x80000000而不是0x0是因为地址范围0x0:0x80000000包含I/O设备。_entry设置一个堆栈，以便xv6可以运行C代码。Xv6为初始堆栈声明空间.

## 2. `start()`
**文件：** `kernel/start.c`
```c
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}
```
功能start执行一些仅在机器模式下允许的配置，然后切换到管理程序模式。在进入管理模式之前，它将寄存器中的前一个特权模式设置为supervisormstatus，它将返回地址设置为main通过编写main的地址写入寄存器mepc，通过写入禁用管理模式下的虚拟地址转换0进入页表寄存器satp，并将所有中断和异常委托给管理模式。它对时钟芯片进行编程以生成定时器中断。完成此内务处理后，start通过调用mret开始“returns”到管理模式mret。这会导致程序计数器更改为main.

## 3. `main()`：初始化核心子系统
**文件：** `kernel/main.c`
```c
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
```
初始化几个设备和子系统，它通过调用创建第一个进程userinit
## 4. `userinit()`：创建第一个进程 `/init`
**文件：** `kernel/proc.c`
```c
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}
```
这会加载 `user/initcode.S`，其中执行的第一条汇编代码是：
```asm
start:
        la a0, init
        la a1, argv
        li a7, SYS_exec
        ecall
```
意味着系统启动后，**第一个系统调用就是 exec("/init")**，从而运行 `user/init.c`。

## 5. `/init` → `/sh`：启动 Shell
**文件：** `user/init.c`
```c
if(pid == 0){
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }
```

* Shell (`user/sh.c`) 启动后，就能接收用户命令；
* 当我们输入：
  ```
  $ trace 32 grep hello README
  ```
  shell 会 fork 出一个子进程，执行程序 `/trace`。
# 🧠 二、trace 用户程序的运行逻辑
## 1. 用户程序入口
**文件：** `user/trace.c`
```c
int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }
  
  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];
  }
  exec(nargv[0], nargv);// ⭐ 执行目标程序 grep
  exit(0);
}
```
所以用户态执行的第一个动作是：
```c
trace(32);
```
trace 程序在调用 trace(mask) 后，再执行 exec()，让自己“变身”为要追踪的目标程序。
换句话说：trace 程序和 grep 程序是同一个进程（同一个 PID）。只是 trace 在执行 exec 后，它的代码段和数据段被替换成了 grep 的。但进程控制块（struct proc）没变，里面的 mask 字段还在！

# ⚙️ 三、trace 系统调用链的执行流程
从这一行开始分析：
```c
trace(mask);
```
## 1. 用户态 → 系统调用桩 (stub)
**文件：** `user/usys.S`（由 `usys.pl` 自动生成）

```asm
.global trace
trace:
    li a7, SYS_trace   # 把系统调用号放入寄存器 a7
    ecall              # 发出陷入指令，进入内核态
    ret
```
➡️ 这一步：CPU 触发 **RISC-V 的 ecall 指令**，陷入到 **内核态**，硬件会：
* 保存用户上下文（寄存器等）
* 切换页表到内核页表
* 跳转到内核陷入入口：`uservec` → `usertrap`

## 2. 硬件陷入入口
**文件：** `kernel/trampoline.S`

```asm
.globl uservec
uservec:
    // 保存寄存器上下文
    csrr t0, sscratch
    sd sp, 0(t0)
    ...
    # jump to usertrap(), which does not return
    jr t0
```
## 3. 进入内核陷入函数 `usertrap()`
**文件：** `kernel/trap.c`
```c
void usertrap(void) {
  struct proc *p = myproc();

  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if(... // 时钟/中断等
```
到这里，我们从用户态 `ecall` 成功进入了内核的系统调用处理逻辑。

## 4. 系统调用分派
**文件：** `kernel/syscall.c`
```c
void syscall(void)
{
  int num = p->trapframe->a7;  // 从寄存器获取系统调用号
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();  // ⭐ 调用对应内核函数
    // trace输出逻辑
    if(p->mask & (1 << num))
      printf("%d: syscall %s -> %d\n", p->pid, syscall_names[num], p->trapframe->a0);
  } else {
    p->trapframe->a0 = -1;
  }
}
```
此时：
* `num = SYS_trace`（即 22）
* 调用 `sys_trace()` 函数

## 5. 内核系统调用实现

**文件：** `kernel/sysproc.c`
```c
uint64
sys_trace(void)
{
  int mask;
  if(argint(0, &mask) < 0)
    return -1;
  myproc()->mask = mask;   // ⭐ 保存掩码
  return 0;
}
```
> argint()：从用户栈中取系统调用参数（即 trace 的参数 mask）
执行结果：在当前进程的 `proc` 结构中保存：
```c
p->mask = 32;
```

## 6. 退出系统调用

内核调用结束后，返回到 `syscall()`：
* 将返回值 `0` 写回到 `a0`；
* 通过 `usertrapret()` 切换回用户态；
* 用户程序继续执行下一行：
  ```c
  exec(argv[2], &argv[2]);
  ```

# 四、exec() 启动被跟踪的程序
用户态调用：
```c
exec("grep", args);
```
在 `sys_exec()` 内核实现中创建了一个新进程镜像，加载 `grep` 程序。

# 🧠 五、系统调用被追踪时的输出逻辑
当 grep 在执行时（例如 `read`, `close`, `exit`），每次系统调用都会进入 `syscall()`。
```c
void syscall(void) {
  ...
  p->trapframe->a0 = syscalls[num]();  // 执行真实系统调用
  if(p->mask & (1 << num)) {
    printf("%d: syscall %s -> %d\n",
           p->pid, syscall_names[num],
           p->trapframe->a0);
  }
}
```
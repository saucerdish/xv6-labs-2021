# 🧪 实验报告：用户级线程切换机制实现
## 实验背景
在操作系统中，线程是调度的基本单位。现代操作系统支持多任务并发执行，线程切换是操作系统中的核心机制之一。通常，线程切换是由内核完成的，通过保存当前线程的上下文并恢复目标线程的上下文来实现。但在某些情况下，我们需要在用户空间模拟线程切换，这称为 **用户级线程调度**（User-level thread scheduling）。
本实验的目标是设计并实现一个简单的用户级线程切换机制。通过本实验，你将理解如何在用户空间实现线程的创建、切换和调度。
## 🧩 实验目的
* 理解线程切换的原理，特别是在用户空间实现的线程调度。
* 实现 `thread_create()` 和 `thread_switch()` 函数，使得线程能够在用户空间进行切换。
* 实现线程的上下文保存和恢复，模拟多线程的切换。
* 掌握如何使用汇编语言实现上下文切换，了解 **callee-save** 寄存器的作用。
* 使用 `uthread_test` 测试代码，验证实现是否正确。
## 🧠 实验原理
### 1. 线程的创建与上下文
在用户级线程系统中，每个线程都有自己的堆栈和寄存器状态。每当线程切换时，需要保存当前线程的寄存器状态，并恢复目标线程的状态。为了实现这一点，我们需要在每个线程结构体中保存其上下文。
线程的上下文包括：
* **寄存器状态**：保存线程的寄存器值（例如：程序计数器 `PC`、栈指针 `SP` 等）。
* **堆栈**：每个线程都需要一个独立的堆栈来存储局部变量等数据。
### 2. 线程切换机制
线程切换的核心思想是：
1. 在执行 `thread_schedule()` 时，选择一个新的线程并保存当前线程的上下文。
2. 恢复新线程的上下文，使其从上次停止的地方继续执行。
上下文切换是通过 **汇编语言** 来完成的，通常直接操作 **callee-save** 寄存器来保存和恢复上下文。
## ⚙️ 实验步骤
### 🧩 1. 线程创建
在 `thread_create()` 中，我们为每个线程分配堆栈，并设置初始的堆栈指针和返回地址。
```c
void thread_create(void (*func)()) {
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  memset((void *)&t->stack, 0, STACK_SIZE);  // 初始化堆栈
  memset((void *)&t->thread_context, 0, sizeof(struct thread_context));  // 初始化上下文
  t->state = RUNNABLE;
  t->thread_context.sp = (uint64) ((char *)&t->stack + STACK_SIZE);  // 设置初始栈指针
  t->thread_context.ra = (uint64) func;  // 设置线程函数的地址
}
```
* **堆栈指针**：设置为栈顶地址。
* **返回地址**：`ra` 设置为线程函数 `func` 的地址。
### 🧩 2. 线程调度
`thread_schedule()` 实现线程调度，选择一个 **RUNNABLE** 状态的线程并进行切换。
```c
void thread_schedule(void) {
  struct thread *t, *next_thread;

  next_thread = 0;
  t = current_thread + 1;
  for(int i = 0; i < MAX_THREAD; i++) {
    if(t >= all_thread + MAX_THREAD) t = all_thread;
    if(t->state == RUNNABLE) {
      next_thread = t;
      break;
    }
    t = t + 1;
  }

  if (next_thread == 0) {
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    thread_switch((uint64)t, (uint64)current_thread);
  }
  else
    next_thread = 0;
}
```
* **调度策略**：轮询，遍历所有线程，选择一个 **RUNNABLE** 状态的线程。
* **上下文切换**：如果当前线程与下一个线程不同，调用 `thread_switch()` 进行上下文切换。
### 🧩 3. 线程切换
在`uthread.c`中添加寄存器存储相关结构
```c
struct thread_context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  struct thread_context     thread_context;    /* register status */
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
};
```
`thread_switch()` 使用汇编语言来保存和恢复线程的上下文。
```asm
thread_switch:
	/* YOUR CODE HERE */
	sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)
	ret    /* return to ra */
```
* **上下文保存**：保存当前线程的 **callee-saved** 寄存器。
* **上下文恢复**：恢复目标线程的 **callee-saved** 寄存器，并跳转到目标线程的代码。
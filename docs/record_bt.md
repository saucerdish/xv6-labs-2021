# xv6-labs Backtrace 实验报告
## 一、实验目的
在内核调试过程中，**函数调用回溯（Backtrace）** 是非常有用的工具。
它能帮助我们定位内核异常发生的调用路径，即函数嵌套的层级结构。
本实验的目标是在 xv6 内核中实现 `backtrace()` 函数：
* 能打印当前内核函数调用栈中的返回地址；
* 并在 `sys_sleep()` 和 `panic()` 中调用它；
* 运行 `bttest` 后，验证输出是否正确。
## 二、实验原理
### 1. 栈帧（Stack Frame）结构
在 RISC-V 架构下，编译器使用寄存器 `s0`（frame pointer, FP） 来保存当前函数的栈帧基地址。
每个函数调用时：
* 栈帧顶部存放**上一级函数的返回地址**（位于 `fp - 8`）；
* 其次存放**上一级函数的帧指针**（位于 `fp - 16`）。
结构示意：

```
高地址 ↑
+------------------+
|  ... local vars  |
|------------------|
| return address   | <- fp - 8
|------------------|
| previous fp      | <- fp - 16
|------------------|
| caller stack ... |
低地址 ↓
```
因此，可以通过遍历 `s0` 链（frame pointer）来回溯整个调用栈。
### 2. 栈页边界
xv6 为每个内核栈分配一个页（4KB）并对齐到页边界。
我们可以通过：
```c
PGROUNDDOWN(fp)   // 栈底
PGROUNDUP(fp)     // 栈顶
```
判断当前的 `fp` 是否还在栈页范围内，从而控制回溯结束条件。
### 3. 获取当前帧指针
内联汇编用于读取寄存器 `s0`：
```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x));
  return x;
}
```
## 三、实验实现
### 1. 在 `kernel/riscv.h` 中添加
```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x));
  return x;
}
```
### 2. 在 `kernel/defs.h` 中声明函数原型
```c
void backtrace(void);
```
### 3. 在 `kernel/printf.c` 中实现 `backtrace()`
```c
void 
backtrace(void)
{
  printf("backtrace:\n");
  uint64 fp=r_fp();
  uint64 bottom=PGROUNDDOWN(fp);
  uint64 up=PGROUNDDOWN(fp)+PGSIZE;
  // printf("bottom: %p; up: %p\n",bottom,up);
  while(fp<=up && fp>=bottom){
    // printf("fp: %p\n",fp);
    uint64 addr=*(uint64*)(fp - 8);
    uint64 pre_fp=*(uint64*)(fp - 16);
    printf("addr: %p\n",addr);
    fp=pre_fp;
  }
}
```
### 4. 在 `sys_sleep()` 调用 backtrace()

打开 `kernel/sysproc.c`，修改 `sys_sleep()`：

```c
uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);

  // 调用 backtrace()
  backtrace();

  return 0;
}
```
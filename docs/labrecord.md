
## xv6 系统调用加速（ugetpid）
### 实验目的
通过在用户空间与内核空间之间共享一个只读页面，优化 `getpid()` 系统调用的性能。具体实现为：每个进程创建时，在内核中分配并初始化一个共享页（存储进程 PID），将其映射到用户空间的固定虚拟地址 `USYSCALL`，用户态可直接读取该页面获取 PID，无需触发内核态切换，从而加速系统调用。
### 关键修改文件及内容
#### 1. `kernel/proc.h`
在进程结构体 `struct proc` 中添加共享页指针成员，用于跟踪内核分配的 `usyscall` 页面：
```c
struct proc {
  struct usyscall *usyscall_page;  // 内核分配的用户共享页（存放 struct usyscall*
};
```
#### 2. `kernel/proc.c`
##### （1）`allocproc()`：进程创建时分配并初始化共享页
在进程结构体初始化阶段，分配物理内存用于存储 `struct usyscall`，并设置 PID：
```c
static struct proc*
allocproc(void)
{
  // 分配 usyscall 共享页并初始化 PID
  if((p->usyscall_page = (struct usyscall *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  p->usyscall_page->pid = p->pid;  // 将进程 PID 写入共享页

  // 创建进程页表（原有代码）
}
```
##### （2）`freeproc()`：进程销毁时释放共享页
在释放进程资源时，同步释放 `usyscall` 共享页的物理内存：
```c
static void
freeproc(struct proc *p)
{
  // 释放 trapframe 页（原有代码）
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;

  // 释放 usyscall 共享页
  if(p->usyscall_page)
    kfree((void*)p->usyscall_page);
  p->usyscall_page = 0;
}
```
##### （3）`proc_pagetable()`：创建页表时映射共享页
在进程页表中添加 `USYSCALL` 虚拟地址到共享页物理地址的映射，设置只读权限：
```c
pagetable_t
proc_pagetable(struct proc *p)
{
  // 映射 trapframe 页（原有代码）
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
  
  // 映射 usyscall 共享页：USYSCALL（用户VA）-> 共享页物理地址
  // 权限：PTE_R（只读）| PTE_U（用户态可访问）
  if(mappages(pagetable, USYSCALL, PGSIZE,
              (uint64)(p->usyscall_page), PTE_R | PTE_U) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
  return pagetable;
}
```
##### （4）`proc_freepagetable()`：释放页表时解除共享页映射
在释放进程页表前，先解除 `USYSCALL` 地址的映射：
```c
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  // 解除 trampoline、trapframe 映射（原有代码）
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  // 解除 usyscall 共享页映射
  uvmunmap(pagetable, USYSCALL, 1, 0);
  // 释放页表（原有代码）
  uvmfree(pagetable, sz);
}
```
### 实验原理
1. **共享页机制**：内核为每个进程分配独立的物理页，存储进程 PID，通过页表映射将该物理页映射到用户空间固定地址 `USYSCALL`。
2. **权限控制**：映射时设置 `PTE_R | PTE_U` 权限，确保用户态只能读取该页面，无法修改（避免用户伪造 PID）。
3. **性能优化**：传统 `getpid()` 需要触发系统调用（用户态 → 内核态 → 用户态），而 `ugetpid()` 直接读取用户空间的共享页，无需内核切换，大幅提升调用速度。
4. **资源管理**：进程创建时分配共享页，销毁时释放，避免内存泄漏；页表映射与解除同步进行，确保地址空间一致性。
### 测试验证
编译 xv6 并运行 `pgtbltest` 测试程序，若 `ugetpid` 测试用例通过，说明实现正确：
```bash
make qemu
pgtbltest
```
测试通过的核心验证点：
- 每个进程的 `ugetpid()` 返回值与 `getpid()` 一致（PID 正确）。
- 用户态无法修改 `USYSCALL` 页面（权限控制有效）。
- 进程销毁后共享页内存被释放（无内存泄漏）。
### 总结
本实验通过页表映射实现了用户态与内核态的只读数据共享，成功优化了 `getpid()` 系统调用。关键在于理解页表映射机制、权限控制逻辑以及进程资源的生命周期管理。实验不仅加深了对虚拟内存、页表操作的理解，还体会了“共享内存”在系统调用优化中的核心作用——通过减少内核态切换次数，提升系统调用性能。
若需要进一步调试，可通过 `vmprint()` 打印进程页表，验证 `USYSCALL` 地址是否正确映射到 `usyscall_page` 的物理地址。

## xv6 页表打印（vmprint）
### 实验目的
实现 `vmprint()` 函数，以特定格式打印 RISC-V 页表的内容，辅助可视化页表结构。通过在第一个进程（init 进程）完成 `exec()` 后打印其页表，验证页表打印功能的正确性。
### 关键修改文件及内容
#### 1. `kernel/vm.c`
添加 `vmprint()` 函数实现，三层循环打印页表结构：
```c
void
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n",pagetable);
  // there are 2^9 = 512 PTEs in a page table.
  for(int i0 = 0; i0 < 512; i0++){
    pte_t pte0 = pagetable[i0];
    if(pte0 & PTE_V){
      // this PTE points to a lower-level page table.
      uint64 child0 = PTE2PA(pte0);
      printf("..%d: pte %p pa %p\n",i0,pte0,child0);
      
      if ((pte0 & (PTE_R | PTE_W | PTE_X)) != 0) continue;
      pagetable_t pagetable0=(pagetable_t)child0;
      for(int i1 = 0; i1 < 512; i1++){
        pte_t pte1 = pagetable0[i1];
        if(pte1 & PTE_V){
          uint64 child1 = PTE2PA(pte1);
          printf(".. ..%d: pte %p pa %p\n",i1,pte1,child1);

          if ((pte1 & (PTE_R | PTE_W | PTE_X)) != 0) continue;
          pagetable_t pagetable1=(pagetable_t)child1;
          for(int i2 = 0; i2 < 512; i2++){
            pte_t pte2 = pagetable1[i2];
            if(pte2 & PTE_V){
              uint64 child2 = PTE2PA(pte2);
              printf(".. .. ..%d: pte %p pa %p\n",i2,pte2,child2);
            }
          }
        }
      }
    }
  }
}
```
#### 2. `kernel/defs.h`
添加 `vmprint()` 函数原型，使其可在其他文件中调用：
```c
// 在vm相关函数声明中添加
void vmprint(pagetable_t);
```
#### 3. `kernel/exec.c`
在 `exec()` 函数返回前添加调用，打印第一个进程的页表：
```c
// 在return argc;之前插入
if (p->pid == 1) {  // 仅对第一个进程（init进程）打印页表
  vmprint(p->pagetable);
}
```
### 实验原理
1. **页表结构**：RISC-V 采用三级页表结构，每个页表包含 512 个页表项（PTE），通过虚拟地址的不同位段索引各级页表。
2. **递归打印**：`vmprint()` 调用辅助函数 `print_pagetable()`，递归遍历三级页表：
   - 顶级页表（depth=0）：遍历 512 个 PTE，打印有效项（`PTE_V` 置位）。
   - 中间页表（depth=1）：若顶级 PTE 指向中间页表（无 `PTE_R/W/X` 标志），则递归打印。
   - 叶子页表（depth=2）：若中间 PTE 指向叶子页表，继续递归打印，叶子页表的 PTE 指向物理页（含 `PTE_R/W/X` 标志）。
3. **缩进格式**：每层页表打印时增加 `" .."` 缩进，直观体现页表层级。
4. **过滤无效项**：仅打印 `PTE_V` 置位的有效页表项。
### 测试验证
编译 xv6 并启动，系统初始化时会打印 init 进程的页表，格式如下（示例）：
```
page table 0x0000000087f6e000
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
.. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
.. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
.. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
```
若输出格式与预期一致，且 `make grade` 中 `pte printout` 测试通过，则实验正确。
### 总结
本实验通过递归遍历 RISC-V 三级页表，实现了页表内容的格式化打印。关键在于理解页表层级结构、PTE 标志位的含义（如 `PTE_V` 表示有效，`PTE_R/W/X` 表示指向物理页），以及通过递归处理多级页表。实验加深了对虚拟内存管理中页表结构的理解，同时为后续调试提供了实用工具。

## xv6 页访问检测（pgaccess）
### 实验目的
实现 `pgaccess()` 系统调用，通过检测 RISC-V 页表中的访问位（PTE_A），向用户空间报告指定范围内哪些页面被访问过，并清除访问位以便下次检测。
### 关键修改文件及内容
#### 1. `kernel/sysproc.c`
添加 `sys_pgaccess()` 系统调用实现：
```c
int
sys_pgaccess(void)
{
  // 解析第一个参数：起始虚拟地址
  uint64 addr;
  if(argaddr(0, &addr) < 0) return -1;
  
  // 解析第二个参数：要检查的页面数量
  int nums;
  if(argint(1, &nums) < 0) return -1;
  
  // 解析第三个参数：用户空间结果缓冲区地址
  uint64 ua;
  if(argaddr(2, &ua) < 0) return -1;

  // 获取当前进程的页表
  pagetable_t pagetable = myproc()->pagetable;
  uint64 bitmask = 0;  // 用于存储结果的位掩码

  // 遍历每个页面并检查访问位
  for(int i = 0; i < nums; i++){
    uint64 uaddr = addr + i * PGSIZE;  // 计算第i个页面的虚拟地址
    if(uaddr >= MAXVA) return -1;      // 检查地址是否超出用户空间上限

    // 查找页表项（PTE）
    pte_t *pte = walk(pagetable, uaddr, 0);
    // 检查页表项是否有效且被访问过
    if((*pte & PTE_V) && (*pte & PTE_A)){
      bitmask |= (1 << i);   // 设置对应位（第i位对应第i个页面）
      *pte &= ~PTE_A;        // 清除访问位，以便下次检测
    }
  }

  // 将结果从内核空间复制到用户空间缓冲区
  if(copyout(pagetable, ua, (char*)&bitmask, sizeof(bitmask)) < 0)
    return -1;

  return 0;
}
```
#### 2. `kernel/riscv.h`
定义 RISC-V 页表项的访问位（PTE_A）：
```c
// 在现有PTE标志定义后添加
#define PTE_A (1L << 6)  // 访问位：当页面被访问时硬件置位
```
#### 3. `kernel/defs.h`
添加编译时缺少的函数声明：
```c
pte_t*          walk(pagetable_t, uint64, int);
```
### 实验原理
1. **参数解析**：通过 `argaddr()` 和 `argint()` 从用户空间获取起始地址、页面数量和结果缓冲区地址。
2. **页表遍历**：使用 `walk()` 函数遍历进程页表，查找每个页面对应的页表项（PTE）。
3. **访问位检测**：检查 PTE 中的 `PTE_V`（有效位）和 `PTE_A`（访问位），若均置位则标记位掩码对应位。
4. **清除访问位**：检测后通过 `*pte &= ~PTE_A` 清除访问位，确保下次调用能准确检测新的访问。
5. **结果返回**：通过 `copyout()` 将内核中的位掩码复制到用户空间缓冲区。
### 测试验证
编译 xv6 并运行 `pgtbltest` 测试程序，若 `pgaccess` 测试用例通过，则说明实现正确。
### 总结
本实验通过操作 RISC-V 页表的访问位，实现了页面访问跟踪功能。关键在于正确解析用户参数、遍历页表、操作页表项标志位，以及安全地在 kernel/user 空间传递数据。实验加深了对页表结构、虚拟内存管理及系统调用实现的理解。
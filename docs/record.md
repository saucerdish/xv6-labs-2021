# 🧪 XV6 实验报告：Copy-on-Write fork 实现
## 🧩 一、实验目的
在原始 xv6 实现中，`fork()` 会立即将父进程的所有用户页逐页复制到子进程，这会造成两个问题：
1. **性能低下**：大量内存复制浪费 CPU 时间；
2. **空间浪费**：如果子进程紧接着调用 `exec()`，复制的内存立刻会被丢弃。

**Copy-on-Write (COW)** 技术通过「延迟复制」优化了这一行为：父子进程最初共享同一组物理页；两者页表项均被标记为 **只读 (Read-Only)**；当任一方尝试写入该页时，触发 **页错误 (Page Fault)**；
内核在错误处理程序中执行：分配新物理页；复制旧页内容；更新该页表项为可写；从而实现 **“按需复制 (Copy on Write)”**。
## ⚙️ 二、详细实现步骤
### 🧩 1. 物理页引用计数机制（`kernel/kalloc.c`）
#### ✅ (1) 新增引用计数数组与锁
```c
#define MAX_PAGES (PHYSTOP / PGSIZE)
int refcount[MAX_PAGES];        // 每个物理页的引用计数
struct spinlock ref_lock;       // 保护引用计数的自旋锁
```
#### ✅ (2) 工具函数：物理地址 → 数组索引
```c
static inline int pa2idx(uint64 pa) {
  return pa / PGSIZE;
}
```
#### ✅ (3) 初始化锁
```c
void kinit() {
  initlock(&kmem.lock, "kmem");
  initlock(&ref_lock, "ref_lock");
  freerange(end, (void*)PHYSTOP);
}
```
#### ✅ (4) 修改 `kalloc()`
> 新分配页的引用计数初始化为 1。
```c
void *kalloc(void) {
  ...
  if (r) {
    memset((char*)r, 5, PGSIZE);
    acquire(&ref_lock);
    refcount[pa2idx((uint64)r)] = 1;
    release(&ref_lock);
  }
  ...
}
```
#### ✅ (5) 修改 `kfree()`
> 仅当引用计数为 0 时才真正释放物理页。
```c
void kfree(void *pa) {
  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&ref_lock);
  int idx = pa2idx((uint64)pa);
  refcount[idx]--;
  if (refcount[idx] > 0) {
    release(&ref_lock);
    return;
  }
  release(&ref_lock);
  ...
}
```
#### ✅ (6) 提供计数操作函数
```c
void incref(uint64 pa) {
  acquire(&ref_lock);
  refcount[pa2idx(pa)]++;
  release(&ref_lock);
}

void outcref(uint64 pa)
{
  acquire(&ref_lock);
  refcount[pa2idx(pa)]--;
  release(&ref_lock);
}

int getref(uint64 pa) {
  acquire(&ref_lock);
  int rc = refcount[pa2idx(pa)];
  release(&ref_lock);
  return rc;
}
```
#### ✅ (7) 提供计数操作的声明（在 `kernel/defs.h`）
```c
void            incref(uint64);
int             getref(uint64);
void            outcref(uint64);
```
### 🧩 2. 修改 `uvmcopy()`（在 `kernel/vm.c`）
> 父子进程共享物理页，并将页表项设置为只读 + COW。
```c
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");

    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    // 清除写权限，并标记为 COW
    if(flags & PTE_W){
      flags = (flags | PTE_COW) & (~PTE_W);
      *pte = PA2PTE(pa) | flags;
    }

    // 子进程映射相同物理页
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      uvmunmap(new, 0, i / PGSIZE, 1);
      return -1;
    }
    incref(pa);
  }
  return 0;
}
```
在 `kernel/riscv.h`中添加COW的声明
```
#define PTE_COW (1L << 8)
```
### 🧩 3. COW 错误处理（`cowfault()`）（在 `kernel/vm.c`）
> 当进程尝试写入只读页时触发；负责复制物理页。
```c
uint64 cowfault(pagetable_t pagetable, uint64 va)
{
  va = PGROUNDDOWN(va);
  pte_t *pte;

  if((pte = walk(pagetable, va, 0)) == 0)
    return -1;

  uint64 pa = PTE2PA(*pte);
  if (pa == 0) return -1;

  uint flags = PTE_FLAGS(*pte);

  if (flags & PTE_COW) {  
    flags = (flags & ~PTE_COW) | PTE_W;
    char *ka = kalloc();
    if (ka == 0) return -1;
    memmove(ka, (char*)pa, PGSIZE);
    kfree((void*)pa); // 原页引用计数减1
    *pte = PA2PTE((uint64)ka) | flags;
    return 0;
  }
  
  return 0;
}
```
在 `kernel/defs.h`中添加该函数声明
```
uint64          cowfault(pagetable_t , uint64);
```
### 🧩 4. 修改 `usertrap()`（在 `kernel/trap.c`）
> 捕获页错误异常（Store Page Fault）。
```c
else if(r_scause() == 13 || r_scause() == 15){  // Load/Store page fault
  uint64 va = r_stval();  // 获取 fault 地址
  if (va >= MAXVA) {
    p->killed = 1;
  }
  if(cowfault(p->pagetable, va) != 0){
    p->killed = 1;
  }
}
```
### 🧩 5. 修改 `copyout()`（在 `kernel/vm.c`）
> 用户态写数据时也可能触发写时复制。
```c
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0)
    va0 = PGROUNDDOWN(dstva);

    // 如果写入的是 COW 页，触发复制
    if(cowfault(pagetable, va0)!=0)
      return -1;
    ...
}
```

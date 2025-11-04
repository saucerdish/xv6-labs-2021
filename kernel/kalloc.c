// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

#define MAX_PAGES (PHYSTOP / PGSIZE)
int refcount[MAX_PAGES]; 
struct spinlock ref_lock;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

static inline int
pa2idx(uint64 pa)
{
  return pa / PGSIZE;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref_lock, "ref_lock"); 
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  acquire(&ref_lock);
  int idx = pa2idx((uint64)pa);
  if(refcount[idx] > 0)
    refcount[idx]--;
  int rc = refcount[idx];
  release(&ref_lock);

  if(rc > 0)
    return; // 还有别的进程引用，不释放

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire(&ref_lock);
    refcount[pa2idx((uint64)r)] = 1;  // 初始化引用计数为 1
    release(&ref_lock);
  }

  return (void*)r;
}

void
incref(uint64 pa)
{
  acquire(&ref_lock);
  refcount[pa2idx(pa)]++;
  release(&ref_lock);
}

int
getref(uint64 pa)
{
  acquire(&ref_lock);
  int rc = refcount[pa2idx(pa)];
  release(&ref_lock);
  return rc;
}

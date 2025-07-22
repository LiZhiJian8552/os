// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"


// 由物理地址获取物理页id
#define PA2PGREF_ID(p) (((p)-KERNBASE)/PGSIZE)
// 物理页数上限
#define PGREF_MAX_ENTERIES PA2PGREF_ID(PHYSTOP)

// 用于记录每个物理页的引用数，[i]表示第i个物理页的引用数
int pageref[PGREF_MAX_ENTERIES];

// 操作pageref的锁
struct  spinlock pgreflock;

// 获取地址对应的物理页引用数
#define PA2PGREF(p) pageref[PA2PGREF_ID((uint64)(p))]


void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // 初始化锁
  initlock(&pgreflock,"pgref");
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


  acquire(&pgreflock);
  if(--PA2PGREF(pa)<=0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
    
  }
  release(&pgreflock);
  
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
    // 设置新分配的物理页面的引用数为1
    PA2PGREF(r)=1;
  }
  return (void*)r;
}

// 物理页引用数+1
void krefpage(void* pa){
  acquire(&pgreflock);
  PA2PGREF(pa)++;
  release(&pgreflock);
}

// 为写时复制返回一个新的物理地址
// 如果该物理页的引用数>1 ,则将引用数-1，分配一个新的物理页
// 如果该物理页的引用数<=1，则直接返回该物理页
void* kcopy_n_deref(void* pa){
  acquire(&pgreflock);

  // 如果该物理页的引用数<=1，则直接返回该物理页
  if(PA2PGREF(pa)<=1){
    release(&pgreflock);
    return pa;
  }

  // 分配新的物理页
  uint64 newpa=(uint64)kalloc();
  // 内存不足时，返回0
  if(newpa==0){
    release(&pgreflock);
    return 0;
  }
  // 将pa中的内容复制到newpa中
  memmove((void*)newpa,(void*)pa,PGSIZE);

  // 旧页引用数-1
  PA2PGREF(pa)--;
  release(&pgreflock);
  return (void*)newpa;
}
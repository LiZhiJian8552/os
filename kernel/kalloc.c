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

struct run {
  struct run *next;
};

// 为每个CPU分配一个kmem结构体，使得每个CPU都有独立的freelist和对应的锁
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

char* kmem_lock_names[]={
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",
};


void
kinit()
{
  // initlock(&kmem.lock, "kmem");
  /*--------------new add -------------------*/
  for(int i=0;i<NCPU;i++){
    initlock(&kmem[i].lock,kmem_lock_names[i]);
  }
  
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 禁止中断
  push_off();

  // 获取当前的cpu编号
  int cpu=cpuid();



  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  // 打开中断
  pop_off();

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // 关闭中断
  push_off();

  int cpu=cpuid();

  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  // 如果当前cpu还有空闲的页，则分配
  if(r)
    kmem[cpu].freelist = r->next;
  else{ //如果当前cpu没有空闲的页，则尝试从其他cpu上偷空闲页
    /* 一次只偷一页
      int anothercpu;
      // 遍历所有的cpu
      for(anothercpu=0;anothercpu<NCPU;anothercpu++){
        // 如果是当前cpu则跳过
        if(cpu==anothercpu){
          continue;
        }

        // 如果是其他的cpu,获取对方的锁
        acquire(&kmem[anothercpu].lock);

        r=kmem[anothercpu].freelist;
        // 如果可以偷到可用的页表
        if(r){
          kmem[anothercpu].freelist=r->next;
          release(&kmem[anothercpu].lock);
          break;
        }
        // 没有可用的页表
        release(&kmem[anothercpu].lock);
      }
    */

    /*一次偷取多页，减少偷的频率*/
    int steal_left=8;
    for(int i=0;i<NCPU;i++){
      if(i==cpu){
        continue;
      }

      acquire(&kmem[i].lock);
      struct run* rr=kmem[i].freelist;
      while(rr&&steal_left){
        // 从othercpu上将页表拿下
        kmem[i].freelist=rr->next;
        
        // 放到当前cpu上
        rr->next=kmem[cpu].freelist;
        kmem[cpu].freelist=rr;

        rr=kmem[i].freelist;
        steal_left--;
      }
      release(&kmem[i].lock);
      if(steal_left==0){
        break;
      }
    }
    r = kmem[cpu].freelist;
    if(r)
      kmem[cpu].freelist = r->next;
  }
  release(&kmem[cpu].lock);
  pop_off();
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

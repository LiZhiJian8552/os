// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// 哈希表中桶号索引，即哈希表的大小
#define NBUFMAP_BUCKET 13

// 哈希索引
#define BUFMAP_HASH(dev,blockno) ((((dev)<<27)|(blockno))%NBUFMAP_BUCKET)

struct {
  // struct spinlock lock;
  // 所有的缓冲区
  struct buf buf[NBUF];

  // 全局的驱逐锁，用于防止一个区块对应多份缓存的情况
  struct spinlock eviction_lock;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  
  // 哈希表
  struct buf bufmap[NBUFMAP_BUCKET];
  // 桶锁，每个桶都有属于自己的桶锁
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];

  // struct buf head;
} bcache;

void
binit(void)
{
  // struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;

  // // 将所有的buf链接到bache的head上，双向循环链表的头插法
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }

  /*---------------new add-----------------------*/
  // 初始化桶锁
  for(int i=0;i<NBUFMAP_BUCKET;i++){
    initlock(&bcache.bufmap_locks[i],"bcache_bufmap");
    // 将所有桶初始指向的缓冲区都只为NULL（初始都没指向缓冲区）
    bcache.bufmap[i].next=0;
  }

  for(int i=0;i<NBUF;i++){
    // 初始化所有缓存区块
    struct buf* b=&bcache.buf[i];
    initsleeplock(&b->lock,"buffer");
    b->lastuse=0;
    b->refcnt=0;

    // 初始时将所有的缓冲区块都挂到第0个哈希位置
    b->next=bcache.bufmap[0].next;
    bcache.bufmap[0].next=b;
  }


  initlock(&bcache.eviction_lock,"bcache_eviction");

  /*---------------------------------------------*/

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 获取缓冲区
static struct buf*
bget(uint dev, uint blockno)
{
  // struct buf *b;

  // acquire(&bcache.lock);

  // // Is the block already cached?
  // // 遍历链表，查找是否已经存在了指定设备的指定快
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // // Not cached.
  // // Recycle the least recently used (LRU) unused buffer.
  // // 没有找到，需要分配一个未使用的缓冲区
  // // 从链表尾开始向前查找(LRU顺序)，找到引用计数为0的缓冲区(未使用)
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // panic("bget: no buffers");

  /*
    1. 先获取当前的桶锁，扫描对应桶，看是否存在该buf,如果存在则返回该块并释放桶锁
    2. 如果不存在，则在所有桶中寻找一个LRU-buf,寻找时要回去对应的桶锁，找到后释放该锁
    3. 从原桶中将该buf移除，并放入blockno对应的桶中
  */

  /*-------------------new add--------------------------*/
  struct buf* b;

  // 获取桶号
  uint key=BUFMAP_HASH(dev,blockno);

  // 获取桶锁
  acquire(&bcache.bufmap_locks[key]);

  // 判断blockno的缓存区块是否已经在缓冲区中
  for(b=bcache.bufmap[key].next;b!=0;b=b->next){
    if(b->dev==dev&&b->blockno==blockno){
      // 引用计数+1
      b->refcnt++;
      // 释放桶锁
      release(&bcache.bufmap_locks[key]);
      // 获取块锁并返回
      acquiresleep(&b->lock);
      return b;
    }
  }
  // 不存在该缓冲区
  
  // 为了防止死锁，先释放当前桶锁
  release(&bcache.bufmap_locks[key]);
  // 为防止缓冲区重复创建，获取驱逐锁
  acquire(&bcache.eviction_lock);

  // 在其寻找是否已经存在去对应的缓冲区
  for(b=bcache.bufmap[key].next;b!=0;b=b->next){
    if(b->dev==dev&&b->blockno==blockno){
      // 
      acquire(&bcache.bufmap_locks[key]);
      // 引用计数+1
      b->refcnt++;
      // 释放桶锁
      release(&bcache.bufmap_locks[key]);
      release(&bcache.eviction_lock);
      // 获取块锁并返回
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 仍然不存在，则查询所有桶中的LRU-buf
  // 记录当前的LRU-buf块的前一个块
  struct buf* before_least=0;
  // 当前LRU-buf块属于哪个桶
  uint holding_bucket=-1;
  
  // 循环所有的桶
  for(int i=0;i<NBUFMAP_BUCKET;i++){
    acquire(&bcache.bufmap_locks[i]);

    // 是否在当前桶中找到新的LRU-buf
    int newfound=0;

    for(b=&bcache.bufmap[i];b->next!=0;b=b->next){
      // 下一个块未被使用且更久未被使用
      if(b->next->refcnt==0&&(!before_least||b->next->lastuse<before_least->next->lastuse)){
        before_least=b;
        newfound=1;
      }
    }

    // 未在当前桶中找到LRU-buf
    if(!newfound){
      release(&bcache.bufmap_locks[i]);
    }else{
      // 如果当前找到的不是第一个LRU-buf,之前肯定持有holding_bucket对应的桶锁
      if(holding_bucket!=-1){
        release(&bcache.bufmap_locks[holding_bucket]);
      }
      holding_bucket=i;
    }
  }

  // 说明没有空闲的缓存块了
  if(!before_least){
    panic("bget: no buffers");
  }

  // 获取LRU-buf
  b=before_least->next;

  // 如果偷的块不在key桶中，需要将该块从原桶链表上删除
  if(holding_bucket!=key){
    before_least->next=b->next;
    // 删除该块后将对应的桶锁删除
    release(&bcache.bufmap_locks[holding_bucket]);

    // 将该块添加到key桶上
    acquire(&bcache.bufmap_locks[key]);
    b->next=bcache.bufmap[key].next;
    bcache.bufmap[key].next=b;
  }

  // 设置新buf的字段
  b->dev=dev;
  b->blockno=blockno;
  b->refcnt=1;
  b->valid=0;

  // 释放当前持有的锁：桶锁和驱逐锁
  release(&bcache.bufmap_locks[key]);
  release(&bcache.eviction_lock);
  acquiresleep(&b->lock);
  return b;

  /*----------------------------------------------------*/
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  // 获取对应的缓冲区
  b = bget(dev, blockno);
  // 如果缓冲区无效(即还没有数据)
  if(!b->valid) {
    // 从磁盘读取数据到缓冲区,(0表示读,1表示写)
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  // 确保拥有者持有该缓冲区的睡眠锁
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  // 将缓冲区的内容,写入磁盘
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  // 确保拥有者持有该缓冲区的睡眠锁
  if(!holdingsleep(&b->lock))
    panic("brelse");
  // 释放对该缓冲区的持有
  releasesleep(&b->lock);

  uint key=BUFMAP_HASH(b->dev,b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  
  if (b->refcnt == 0) {
    // 记录上次使用的时间
    b->lastuse=ticks;
  }
  
  release(&bcache.bufmap_locks[key]);
}

void
bpin(struct buf *b) {
  uint key=BUFMAP_HASH(b->dev,b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void
bunpin(struct buf *b) {
  uint key=BUFMAP_HASH(b->dev,b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}



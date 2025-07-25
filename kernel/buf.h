struct buf {
  // 表示缓冲区内容是否有效
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  // 设备号
  uint dev;
  // 块号
  uint blockno;
  // 睡眠锁
  struct sleeplock lock;
  // 引用计数
  uint refcnt;
  // 双向链表的指针
  // struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];

  //用于跟踪LRU，即表示时间戳，上次使用的时间
  uint lastuse;
};


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
#define NBUCKET 101
#define hash(x) ((x) % NBUCKET)
#define Max(a, b) ( ((a) >= (b)) ? (a) : (b) )
#define Min(a, b) ( ((a) <= (b)) ? (a) : (b) )

struct {
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  char name[20];

  // Create linked lists of buffers
  for(int t = 0; t < NBUCKET; ++ t){
    snprintf(name, sizeof(name), "bhash%d", t);
    initlock(&bcache.lock[t], name);
    bcache.head[t].prev = &bcache.head[t];
    bcache.head[t].next = &bcache.head[t];
  }
  // Dispatch all the free buf to the first bucket of hashtable.
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "bcache");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint key = hash(blockno);

  acquire(&bcache.lock[key]);

  // Is the block already cached?
  for(b = bcache.head[key].next; b != &bcache.head[key]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.lock[key]);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(int i = 0; i < NBUCKET; ++ i){
    acquire(&bcache.lock[i]);

    for(b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

	if(i != key) {
	  // take out the buf from current list and release the current list's lock.
	  b->next->prev = b->prev;
	  b->prev->next = b->next;
	  release(&bcache.lock[i]);
	  __sync_synchronize();
	  // acquire the lock of the list corresponding to the key.
	  acquire(&bcache.lock[key]);
          b->next = bcache.head[key].next;
	  b->prev = &bcache.head[key];
	  bcache.head[key].next->prev = b;
	  bcache.head[key].next = b;
	  release(&bcache.lock[key]);
	} else {
	  release(&bcache.lock[i]);
	}

        acquiresleep(&b->lock);
        return b;
      }
    }

    release(&bcache.lock[i]);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  uint key = hash(b->blockno);


  releasesleep(&b->lock);

  acquire(&bcache.lock[key]);
  b->refcnt--;
  if (b->refcnt == 0 && bcache.head[key].next != b) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[key].next;
    b->prev = &bcache.head[key];
    bcache.head[key].next->prev = b;
    bcache.head[key].next = b;
  }
  
  release(&bcache.lock[key]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock[hash(b->blockno)]);
  b->refcnt++;
  release(&bcache.lock[hash(b->blockno)]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[hash(b->blockno)]);
  b->refcnt--;
  release(&bcache.lock[hash(b->blockno)]);
}



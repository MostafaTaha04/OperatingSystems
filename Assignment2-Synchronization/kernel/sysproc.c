#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// --- Task 0: Pseudo-Random Number Generator (PRNG) ---

// Static variable to hold the current state of the generator
static uint lcg_state = 1; 

// Spinlock to protect the state variable in a multiprocessor environment
static struct spinlock lcg_lock;
static int lcg_lock_init = 0; // Lazy initialization flag

uint64
sys_lcg_srand(void)
{
  int seed;
  
  // Fetch the integer argument passed from userspace
  argint(0, &seed);

  // Initialize the lock the first time this is called
  if(!lcg_lock_init) {
    initlock(&lcg_lock, "lcg_lock");
    lcg_lock_init = 1;
  }

  acquire(&lcg_lock);
  lcg_state = (uint)seed;
  release(&lcg_lock);

  return 0;
}

uint64
sys_lcg_rand(void)
{
  if(!lcg_lock_init) {
    initlock(&lcg_lock, "lcg_lock");
    lcg_lock_init = 1;
  }

  // Parameters defined in the assignment
  uint a = 1664525;
  uint b = 1013904223;
  uint next_state;

  acquire(&lcg_lock);
  
  // Calculate the next state. 
  // Modulo 2^32 is handled automatically by uint overflow.
  next_state = (a * lcg_state + b);
  lcg_state = next_state;
  
  release(&lcg_lock);

  return next_state;
}

// --- Task 1: Process Groups ---

uint64
sys_setgid(void)
{
  int gid;
  argint(0, &gid);
  myproc()->gid = gid;
  return 0;
}

uint64
sys_getgid(void)
{
  return myproc()->gid;
}

// --- Task 1: Israeli Lock Data Structure & Syscalls ---

#define MAX_ISRAELI_LOCKS 15
#define MAX_QUEUE 16

struct israeli_lock {
  struct spinlock lk;        
  int active;                
  int locked;                
  int favoritism;            
  int owner_pid;             
  int owner_gid;             
  
  int wait_queue_pid[MAX_QUEUE]; 
  int wait_queue_gid[MAX_QUEUE]; 
  int q_count;               
};

struct israeli_lock is_locks[MAX_ISRAELI_LOCKS];

void
israeliinit(void)
{
  for(int i = 0; i < MAX_ISRAELI_LOCKS; i++) {
    initlock(&is_locks[i].lk, "israeli_lock");
    is_locks[i].active = 0;
    is_locks[i].locked = 0;
    is_locks[i].q_count = 0;
  }
}

uint64
sys_israeli_create(void)
{
  int favoritism;
  argint(0, &favoritism);
  
  if(favoritism < 0 || favoritism > 100)
    return -1;

  for(int i = 0; i < MAX_ISRAELI_LOCKS; i++) {
    acquire(&is_locks[i].lk);
    if(is_locks[i].active == 0) {
      is_locks[i].active = 1;
      is_locks[i].locked = 0;
      is_locks[i].favoritism = favoritism;
      is_locks[i].q_count = 0;
      release(&is_locks[i].lk);
      return i;
    }
    release(&is_locks[i].lk);
  }
  return -1;
}

uint64
sys_israeli_destroy(void)
{
  int lock_id;
  argint(0, &lock_id);

  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS)
    return -1;

  acquire(&is_locks[lock_id].lk);
  if(is_locks[lock_id].active == 1) {
    is_locks[lock_id].active = 0;
    release(&is_locks[lock_id].lk);
    return 0;
  }
  release(&is_locks[lock_id].lk);
  return -1;
}

uint64
sys_israeli_acquire(void)
{
  int lock_id;
  argint(0, &lock_id);

  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS) return -1;

  struct proc *p = myproc();
  struct israeli_lock *lk = &is_locks[lock_id];

  acquire(&lk->lk);
  if(lk->active == 0) {
    release(&lk->lk);
    return -1;
  }

  if(lk->q_count >= MAX_QUEUE) {
    release(&lk->lk);
    return -1;
  }
  lk->wait_queue_pid[lk->q_count] = p->pid;
  lk->wait_queue_gid[lk->q_count] = p->gid;
  lk->q_count++;

  while (1) {
     if (lk->locked == 0 && lk->wait_queue_pid[0] == p->pid) {
         lk->locked = 1;
         lk->owner_pid = p->pid;
         lk->owner_gid = p->gid;

         for(int i = 0; i < lk->q_count - 1; i++){
             lk->wait_queue_pid[i] = lk->wait_queue_pid[i+1];
             lk->wait_queue_gid[i] = lk->wait_queue_gid[i+1];
         }
         lk->q_count--;

         release(&lk->lk);
         return 0; 
     }
     
     release(&lk->lk);
     yield(); 
     acquire(&lk->lk);
  }
}

uint64
sys_israeli_release(void)
{
  int lock_id;
  argint(0, &lock_id);

  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS) return -1;
  struct israeli_lock *lk = &is_locks[lock_id];

  acquire(&lk->lk);
  if(lk->active == 0 || lk->locked == 0) {
    release(&lk->lk);
    return -1;
  }

  int G = lk->owner_gid;
  int c = lk->favoritism;

  lk->locked = 0; 

  if (lk->q_count > 0) {
     int chosen_idx = 0; 

     int same_gid_idx = -1;
     for (int i = 0; i < lk->q_count; i++) {
         if (lk->wait_queue_gid[i] == G) {
             same_gid_idx = i;
             break; 
         }
     }

     if (same_gid_idx != -1) {
         uint rand_val = sys_lcg_rand(); 
         if ((rand_val % 100) < c) {
             chosen_idx = same_gid_idx;
         }
     }

     if (chosen_idx != 0) {
         int fav_pid = lk->wait_queue_pid[chosen_idx];
         int fav_gid = lk->wait_queue_gid[chosen_idx];
         
         for (int i = chosen_idx; i > 0; i--) {
             lk->wait_queue_pid[i] = lk->wait_queue_pid[i-1];
             lk->wait_queue_gid[i] = lk->wait_queue_gid[i-1];
         }
         lk->wait_queue_pid[0] = fav_pid;
         lk->wait_queue_gid[0] = fav_gid;
     }
  }

  release(&lk->lk);
  return 0;
}
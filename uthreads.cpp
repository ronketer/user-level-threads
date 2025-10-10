#include "uthreads.h"
#include <queue>
#include <set>
#include <map>
#include <algorithm>
#include <functional>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <iostream>
#include <setjmp.h>
#include <unistd.h>
#include <stdbool.h>

#ifdef __x86_64__
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g"(ret)
            : "0"(addr));
    return ret;
}
#else
typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5
address_t translate_address(address_t addr) {
  address_t ret;
  asm volatile("xor    %%gs:0x18,%0\n"
               "rol    $0x9,%0\n"
  : "=g"(ret)
  : "0"(addr));
  return ret;
}
#endif

#define TIMER_HANDLER_FLAG -1

sigjmp_buf env[MAX_THREAD_NUM];

struct Thread
{
    int id;
    int quantum_count;
    void (*entry_point)(void);
    char *stack;
    bool is_sleeping;
    int sleep_count;
    bool is_blocked;
};

std::queue<int> ready_id_queue;
std::map<int, Thread> thread_map;
std::set<int> taken_threads_ids;
std::set<int> available_tids;
int total_quantum_count = 1;
int current_thread = 0;
struct itimerval timer;

void block_signals()
{
  sigset_t signal_mask;
  if (sigemptyset(&signal_mask) != 0) {
    std::cerr << "system error: sigemptyset failed" << std::endl;
    exit(1);
  }
  if (sigaddset(&signal_mask, SIGVTALRM) != 0) {
    std::cerr << "system error: sigaddset failed" << std::endl;
    exit(1);
  }
  if (sigprocmask(SIG_BLOCK, &signal_mask, NULL) != 0) {
    std::cerr << "system error: sigprocmask failed" << std::endl;
    exit(1);
  }
}

void unblock_signals()
{
  sigset_t signal_mask;
  if (sigemptyset(&signal_mask) != 0) {
    std::cerr << "system error: sigemptyset failed" << std::endl;
    exit(1);
  }
  if (sigaddset(&signal_mask, SIGVTALRM) != 0) {
    std::cerr << "system error: sigaddset failed" << std::endl;
    exit(1);
  }
  if (sigprocmask(SIG_UNBLOCK, &signal_mask, NULL) != 0) {
    std::cerr << "system error: sigprocmask failed" << std::endl;
    exit(1);
  }
}

void remove_tid_from_queue(int tid)
{
  int q_size = ready_id_queue.size();
  while (q_size--)
  {
    int current_val = ready_id_queue.front();
    ready_id_queue.pop();
    if (current_val != tid)
    {
      ready_id_queue.push(current_val);
    }
  }
}

void free_memory()
{
  for (const auto &tid : taken_threads_ids)
  {
    if (tid != 0)
    {
      delete[] thread_map[tid].stack;
    }
  }
}

void setup_thread(int tid, char *stack, void (*entry_point)(void))
{
  address_t sp = (address_t)(stack + STACK_SIZE - sizeof(address_t));
  address_t pc = (address_t)entry_point;

  sigsetjmp(env[tid], 1);
  (env[tid]->__jmpbuf)[JB_SP] = translate_address(sp);
  (env[tid]->__jmpbuf)[JB_PC] = translate_address(pc);
  sigemptyset(&env[tid]->__saved_mask);
}

void change_running_thread()
{
  int old_thread = current_thread;
  current_thread = ready_id_queue.front();
  ready_id_queue.pop();
  total_quantum_count++;
  thread_map[current_thread].quantum_count++;
  if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
  {
    std::cerr << "system error: setitimer error." << std::endl;
    free_memory();
    exit(1);
  }
  int ret_val = sigsetjmp(env[old_thread], 1);
  unblock_signals();
  if (ret_val == 0)
  {
    siglongjmp(env[current_thread], 1);
  }
}

void timer_handler(int sig)
{
  block_signals();
  for (auto& pair : thread_map){
    if(pair.second.is_sleeping){
      pair.second.sleep_count--;
      if(pair.second.sleep_count == 0){
        if(!pair.second.is_blocked){
          pair.second.is_sleeping = false;
          ready_id_queue.push(pair.first);
        }
      }
    }
  }
  if (sig != TIMER_HANDLER_FLAG)
  {
    ready_id_queue.push(current_thread);
  }
  change_running_thread();
}

int uthread_init(int quantum_usecs)
{
  if (quantum_usecs <= 0)
  {
    std::cerr << "thread library error: quantum_usecs must be a positive integer" << std::endl;
    return -1;
  }
  Thread main_thread = {0, 1, nullptr, nullptr, false, 0, false};
  thread_map[0] = main_thread;
  taken_threads_ids.insert(0);
  struct sigaction sa = {0};
  sa.sa_handler = &timer_handler;
  if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
  {
    std::cerr << "system error: sigaction error" << std::endl;
    free_memory();
    exit(1);
  }
  timer.it_value.tv_sec = quantum_usecs / 1000000;
  timer.it_value.tv_usec = quantum_usecs % 1000000;
  timer.it_interval.tv_sec = quantum_usecs / 1000000;
  timer.it_interval.tv_usec = quantum_usecs % 1000000;
  if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
  {
    std::cerr << "system error: setitimer error." << std::endl;
    free_memory();
    exit(1);
  }
  for (int tid = 1; tid < MAX_THREAD_NUM; tid++){
    available_tids.insert(tid);
  }
  return 0;
}

int uthread_spawn(void (*entry_point)(void))
{
  block_signals();
  if (entry_point == nullptr)
  {
    std::cerr << "thread library error: entry_point can not be null" << std::endl;
    unblock_signals();
    return -1;
  }
  if (taken_threads_ids.size() >= MAX_THREAD_NUM)
  {
    std::cerr << "thread library error: you have reached the maximal number of threads" << std::endl;
    unblock_signals();
    return -1;
  }
  int new_tid = *available_tids.begin();
  available_tids.erase(new_tid);
  char *stack = new (std::nothrow) char[STACK_SIZE];
  if (!stack)
  {
    std::cerr << "system error: memory allocation failed" << std::endl;
    unblock_signals();
    return -1;
  }
  taken_threads_ids.insert(new_tid);
  Thread new_thread = {new_tid, 0, entry_point, stack, false, 0, false};
  thread_map.insert(std::make_pair(new_tid, new_thread));
  ready_id_queue.push(new_tid);
  setup_thread(new_tid, stack, entry_point);
  unblock_signals();
  return new_tid;
}

int uthread_terminate(int tid)
{
  block_signals();
  if (taken_threads_ids.find(tid) == taken_threads_ids.end())
  {
    std::cerr << "thread library error: thread with id " << tid << " does not exist" << std::endl;
    unblock_signals();
    return -1;
  }
  if (tid == 0)
  {
    free_memory();
    unblock_signals();
    exit(0);
  }
  taken_threads_ids.erase(tid);
  remove_tid_from_queue(tid);
  available_tids.insert(tid);
  delete[] thread_map[tid].stack;
  thread_map.erase(tid);
  unblock_signals();
  if (current_thread == tid)
  {
    timer_handler(TIMER_HANDLER_FLAG);
  }
  return 0;
}

int uthread_block(int tid)
{

  block_signals();
  if (tid == 0 || taken_threads_ids.find(tid) == taken_threads_ids.end())
  {
    std::cerr << "thread library error: the main thread or non-existing thread can not be blocked " << std::endl;
    unblock_signals();
    return -1;
  }
  if (thread_map[tid].is_blocked)
  {
    unblock_signals();
    return 0;
  }
  thread_map[tid].is_blocked = true;
  remove_tid_from_queue(tid);
  unblock_signals();
  if (current_thread == tid)
  {
    timer_handler(TIMER_HANDLER_FLAG);
  }
  return 0;
}

int uthread_resume(int tid)
{
  block_signals();
  if (taken_threads_ids.find(tid) == taken_threads_ids.end())
  {
    std::cerr << "thread library error: thread with id " << tid << " does not exist" << std::endl;
    unblock_signals();
    return -1;
  }
  if (!thread_map[tid].is_blocked)
  {
    unblock_signals();
    return 0;
  }
  thread_map[tid].is_blocked = false;
  if (!thread_map[tid].is_sleeping){
    ready_id_queue.push(tid);
  }
  unblock_signals();
  return 0;
}

int uthread_sleep(int num_quantums)
{
  block_signals();
  if (current_thread == 0)
  {
    std::cerr << "thread library error: the main thread can't be put to sleep state" << std::endl;
    unblock_signals();
    return -1;
  }
  thread_map[current_thread].is_sleeping = true;
  thread_map[current_thread].sleep_count = num_quantums + 1;
  remove_tid_from_queue(current_thread);
  unblock_signals();
  timer_handler(TIMER_HANDLER_FLAG);
  return 0;
}

int uthread_get_tid()
{
  return current_thread;
}

int uthread_get_total_quantums()
{
  return total_quantum_count;
}

int uthread_get_quantums(int tid)
{
  if (taken_threads_ids.find(tid) == taken_threads_ids.end())
  {
    std::cerr << "thread library error: thread with id " << tid << " does not exist" << std::endl;
    return -1;
  }
  return thread_map[tid].quantum_count;
}

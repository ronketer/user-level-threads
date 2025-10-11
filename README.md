# UThreads — User-Level Threads Library

This project implements a user-level threads library (`uthreads`), developed as part of the Operating Systems course at The Hebrew University. It provides preemptive round-robin scheduling with `SIGVTALRM` and context switching using `sigsetjmp/siglongjmp`.

---

## Overview

- Preemptive round-robin scheduling with configurable quantum
- Thread states: `RUNNING`, `READY`, `BLOCKED`, `SLEEPING`
- Ready-queue scheduling, efficient TID management
- Context switches with `sigsetjmp/siglongjmp`

<p align="center">
  <img src="images/ThreadStateDiagram.png" width="85%" alt="Thread State Diagram"/>
</p>

## Public API

Defined in [uthreads.h](uthreads.h):

- `uthread_init(int quantum_usecs)`
- `uthread_spawn(void (*f)(void))`
- `uthread_terminate(int tid)`
- `uthread_block(int tid)`
- `uthread_resume(int tid)`
- `uthread_sleep(int num_quantums)`
- `uthread_get_tid(void)`
- `uthread_get_total_quantums(void)`
- `uthread_get_quantums(int tid)`

> Names above mirror the header; adjust if they differ.

---

## File structure

- uthreads.cpp — scheduling, states, core logic
- uthreads.h — public API and constants (`MAX_THREAD_NUM`, `STACK_SIZE`)
- Makefile — compiles and archives `libuthreads.a`
- examples/ — minimal usage demos (optional)

---

## Build

```bash
make
```

Outputs a static library `libuthreads.a` and (optionally) example binaries.

## Minimal example

```c
#include "uthreads.h"
#include <stdio.h>

void worker() {
  for (int i = 0; i < 5; ++i) {
    printf("Worker running (q=%d)\n", uthread_get_quantums(uthread_get_tid()));
    uthread_sleep(1); // or uthread_yield() if provided
  }
}

int main() {
  uthread_init(50000); // 50ms quantum
  uthread_spawn(worker);
  // main thread acts as scheduler entry point
  while (uthread_get_total_quantums() < 20) { /* spin or work */ }
  return 0;
}
```

> Replace with a real example from your repo if present.

## Notes

- Signal masking is used to guard critical sections
- Preemption via virtual timer; ensure signal handlers are async-signal-safe
- Stacks and guard pages may vary by platform


> TODO: Add a LICENSE file.

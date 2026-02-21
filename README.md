# UThreads — Preemptive User-Level Threads Library

A high-performance C++ library implementing user-level threading (`uthreads`) with preemptive round-robin scheduling. This project demonstrates low-level systems architecture, manual context switching, and sophisticated signal-based concurrency management.

<p align="center">
  <img src="images/ThreadStateDiagram.png" width="85%" alt="Thread State Diagram"/>
</p>



## 🛠️ Technical Highlights

* **Custom Context Switching:** Implemented manual state saving and restoration using `sigsetjmp` and `siglongjmp` to manage thread execution flows.
* **Low-Level Memory Management:** Architected thread lifecycle operations, including manual stack allocation and pointer arithmetic, ensuring memory safety and preventing stack overflows.
* **Preemptive Scheduling:** Engineered a round-robin scheduler using `SIGVTALRM` virtual timers to enforce configurable time quantums across thread states (`RUNNING`, `READY`, `BLOCKED`, `SLEEPING`).
* **Thread-Safe Critical Sections:** Utilized signal masking to guard internal state transitions and ensure atomicity during scheduling operations.

## 📋 Public API

Defined in `uthreads.h`, the library provides a robust interface for thread management:

| Function | Description |
| :--- | :--- |
| `uthread_init` | Initializes the library and the main execution thread. |
| `uthread_spawn` | Creates a new thread with an isolated stack. |
| `uthread_block/resume` | Manages manual thread state transitions. |
| `uthread_sleep` | Suspends thread execution for a specific number of quantums. |

## 🏗️ Build & Requirements
* **Environment:** Linux/Unix (CLI) 
* **Tools:** G++, Make, Standard Pthreads library (for signal management) 

```bash
make

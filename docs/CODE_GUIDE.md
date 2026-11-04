# Source code guide

## How the files are organized

Each service has one implementation under `src/` and one public header under
`include/`. `main.c` is the entry point and does not need its own header. Public
header comments explain what callers can expect; implementation comments explain
the important steps and decisions. The shared `App` state is passed explicitly
to functions instead of being stored in mutable global variables.

| Read order | Implementation | Header | Responsibility and main functions |
|---|---|---|---|
| 1 | `src/main.c` | none | Parse launch options; initialize; start `shell_run`. |
| 2 | `src/core.c` | `include/core.h` | Shared types and limits; `app_init`, `app_check`, `app_reset`, `app_shutdown`. |
| 3 | `src/shell.c` | `include/shell.h` | Read complete commands; parse integers; route to services; handle errors and exit. |
| 4 | `src/tasks.c` | `include/tasks.h` | `task_create`, `task_find`, `task_block`, `task_wake`, `tasks_print`. |
| 5 | `src/scheduler.c` | `include/scheduler.h` | Circular FIFO, `scheduler_step`, `scheduler_run`, metric display and export. |
| 6 | `src/memory.c` | `include/memory.h` | Allocate, release, clear, and check PID-owned payloads. |
| 7 | `src/notes.c` | `include/notes.h` | Create, append, read, and delete persistent notes. |
| 8 | `src/session.c` | `include/session.h` | Create a study task and emit exactly one completion notice. |
| 9 | `src/logger.c` | `include/logger.h` | Own the log stream; write/flush events; remember I/O failures. |

Open `include/core.h` alongside the implementations. Its field comments explain
the task timestamps, the queue head/count, allocation records, live-byte totals,
file ownership, and the one-time session notification flag.

## Following a task through the program

For `task create Review 3 128`, the shell first checks the complete command.
`task_create` checks the name, burst, and task-record capacity. It asks the memory
module for 128 bytes owned by the candidate PID. Only after allocation and queue
insertion succeed does it publish the task record and advance `next_pid`.
An allocation failure therefore consumes neither a task record nor a PID.

On `step`, the scheduler removes the oldest READY PID, marks that task RUNNING,
and chooses `min(quantum, remaining)` ticks. Other READY tasks accumulate waiting
time; WAITING tasks do not. The clock advances by the slice. Unfinished work
returns to the queue tail; finished work records completion and releases its
payload. The result record remains available for metrics until reset.

## Memory ownership to remember

`Task.payload` aliases the pointer stored in an `AllocationRecord`. The registry
owns that allocation: only the memory module calls `free` on payloads. Completion
releases one PID; reset and exit clear all remaining allocations. A second release
finds no live record, so it cannot free the same pointer again.

The 65,536-byte limit covers task payloads only. The fixed task table, program
buffers, logs, and C library overhead are outside this educational budget.

## Sarah's proposed modules

The proposal assigns Sarah the notes, sessions, logging, and evidence work:

- `notes.c`: follow `note_path`, `write_note`, and the four public operations.
  Comments explain exclusive create, existing-file append, chunked reads,
  newline handling, and detecting buffered write/close failures.
- `session.c`: `session_start` creates a normal task with a session flag;
  `session_complete` checks FINISHED and `notified` before printing or logging.
- `logger.c`: log events contain experiment, virtual tick, event, PID, and detail.
  PID 0 denotes an application/file event. Failures are reported and remembered.
- `tests/collect_evidence.py`: runs the demo files and retains actual output.

These are the proposal's assignments, not a verified record of individual authorship.

## Build and test files

The Makefile explains each target and the compiler options. `make` builds the
application from `src/*.c`. `make test` builds the C assertion program and runs
the Python integration suite. `make sanitize` runs both under ASan/UBSan with
leak detection disabled for restricted environments.

| Test file | Purpose |
|---|---|
| `tests/test_core.c` | Direct checks of exact timing/metrics, blocked waiting, late arrivals, rollback, queue wraparound, and cleanup. |
| `tests/run_tests.py` | T1-T12 CLI/file scenarios; compare saved bytes, observed output, and CSV rows with known answers. |
| `tests/reference.txt` | Proposal reference workload with bursts 4, 3, 2 and quantum 2. |
| `tests/resources.txt` | Block/wake, budget exhaustion, forced allocation failure, recovery, and cleanup. |
| `tests/notes_first.txt` and `tests/notes_restart.txt` | Note creation followed by a separate process reading and deleting it. |
| `tests/five_tasks.txt` | Demonstrate scheduling with more tasks than the three-task reference. |

For metric definitions and detailed design decisions, read `ARCHITECTURE.md`.

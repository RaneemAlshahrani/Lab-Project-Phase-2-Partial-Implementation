# Architecture and design notes

## Request flow

`main.c` opens the logger and trace, then `shell.c` reads and validates commands.
The shell routes task requests to `tasks.c`, dispatches to `scheduler.c`, note
requests to `notes.c`, and study sessions through `session.c`. `core.h` defines
the shared state; `core.c` owns initialization, reset, shutdown and invariant checks.
Memory and logging services are shared across modules.

| Module | Responsibility | Main interface |
|---|---|---|
| main / shell | launch, bounded input, strict numeric parsing, command routing | `shell_run` |
| core | shared model, initialization, reset, consistency checking, shutdown | `app_init`, `app_check`, `app_reset`, `app_shutdown` |
| tasks | transactional creation, PID lookup, task table, legal block/wake | `task_create`, `task_block`, `task_wake` |
| scheduler | bounded circular FIFO, time slices, READY waiting, metrics | `scheduler_step`, `scheduler_run`, `metrics_export` |
| memory | malloc/free wrapper, PID ownership, budget, forced-failure hook | `memory_allocate`, `memory_release`, `memory_clear` |
| notes | create/read/append/delete through standard C file functions | `note_create`, `note_read`, `note_append`, `note_delete` |
| session | session task creation and one-time completion notification | `session_start`, `session_complete` |
| logger | experiment + virtual tick + event + PID + result | `logger_event` |

The proposal's module boundaries are retained. The small additional `core.c/core.h`
centralizes shared structures and lifecycle operations instead of duplicating them.
The ready queue is implemented inside the scheduler module. Tests and evidence
automation are development tools and are separate from the C application.

## Model and ownership

- A static 20-element Task array retains completed records until reset.
- Each unfinished task owns one nonzero heap payload. The memory registry is the
  authority for the allocated pointer, owner PID and size.
- Only payload bytes count toward the 64 KiB budget. Static records, buffers,
  logs, executable size and C library allocations are outside that budget.
- Creation validates inputs and capacity, allocates, then queues and commits the
  record/PID. Allocation failure commits nothing. Queue failure rolls allocation back.
- Completion frees the PID's payload, nulls the task pointer, and retains original
  payload size as historical metadata. Reset and shutdown clear remaining allocations.
- The Task payload pointer aliases the registry entry; no second module owns or frees
  that pointer independently. Double release finds no record and does not call free.
- Between commands, each READY task appears once in the queue; WAITING and FINISHED
  appear zero times. RUNNING is a transient state inside a dispatch.

## State transitions

| Trigger | Before | After | Queue effect |
|---|---|---|---|
| Successful create | no record | READY | append PID |
| Dispatch | READY | RUNNING | pop head |
| Slice expires with work left | RUNNING | READY | append PID |
| Work reaches zero | RUNNING | FINISHED | remain absent; free payload |
| Manual block | READY | WAITING | remove PID |
| Manual wake | WAITING | READY | append PID |
| Reset | any existing task | record removed | clear queue and allocations |

Blocking is explicit and synchronous. It does not represent a real kernel I/O
request. All commands run sequentially; there are no threads or real processes.

## Scheduling and metrics

On a dispatch, `slice = min(quantum, remaining)`. Set first-run time if unset,
mark the task RUNNING, add the slice length to every other READY task's waiting
counter, and advance the clock by the slice. Finish or requeue the running task.
There is no context-switch cost and no automatic idle-clock advancement.

For finished tasks:

- Turnaround = completion - arrival.
- Response = first run - arrival.
- Waiting = accumulated time in READY.

With no blocked interval, waiting = turnaround - burst. With blocked intervals,
that shortcut includes time in WAITING and is wrong for READY-only waiting.
CSV includes finished tasks only; pending tasks are not assigned fake completion
times or included in means. Tasks created later arrive at the current clock.

## File and error behavior

Names use a restricted ASCII alphabet and cannot contain path separators. Each
note name maps to `DATA_DIR/notes/NAME.txt`. Create uses C11 `fopen(..., "wx")`
to avoid replacing existing notes. Append requires a readable existing file.
Each note text command writes one newline-terminated line; line endings follow
the host C text-stream convention. `fputs`, stream errors and `fclose` results are
checked so buffered write errors are detected. Writes are not transactionally
rolled back. A trusted local notes directory is assumed.

Recoverable command errors print a reason, log ERROR and return to the prompt.
The shell checks task/queue/memory invariants after every executed command. A
consistency failure exits with a fatal diagnostic and releases payloads. Event
or trace write/close failures warn on stderr and cause a nonzero exit status.

## Validation evidence

T1-T12 map directly to the proposal's minimum test plan. Direct C assertions add
queue wraparound, removal from a wrapped queue, late arrivals, READY-only waiting,
allocation rollback, harmless repeat release, and reset/exit cleanup checks.
The Python runner verifies actual CSV rows, exact saved note bytes and outputs;
it does not calculate expected values by calling the implementation under test.

Standard GCC and ASan/UBSan executions passed. Full leak-tool checking remains
pending on a host that permits LeakSanitizer's process inspection.

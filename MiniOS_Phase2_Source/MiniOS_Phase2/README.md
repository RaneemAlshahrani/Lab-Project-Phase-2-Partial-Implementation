# MiniOS for a Study Room - Phase 2

A C11 terminal application that makes task states, round-robin CPU scheduling,
task-owned memory, persistent notes, and simulated study sessions observable.

## Build and run

Use Linux, Ubuntu through Windows Subsystem for Linux (WSL), or another system
with GCC and Make. Run these commands **inside the extracted MiniOS_Phase2 folder**:

```sh
make
make run
```

`make run` creates `data/notes` if necessary and opens the command prompt.
For direct compilation without Make:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o minios
mkdir -p data/notes
./minios
```

The application needs only the standard C library. Python 3 is used for automated
tests and evidence collection, not for application functionality. There is no GUI,
database, thread library, or third-party runtime requirement for MiniOS itself.

## Try the proposal's example

```text
session start Study 4 256
task create Review 3 128
task create Practice 2 64
run
metrics
memory
export
check
quit
```

Expected dispatch order: P1, P2, P3, P1, P2. The clock ends at tick 9.
Means: turnaround 7.67, READY waiting 4.67, response 2.00 ticks.
The study session announces completion once at tick 8.

To run the same commands from a file:

```sh
./minios --batch < tests/reference.txt
```

## Commands

| Command | Meaning |
|---|---|
| `task create NAME BURST BYTES` | Create a READY task after successful payload allocation. |
| `session start NAME TICKS BYTES` | Create a study task that announces completion once. |
| `tasks` | Show state, burst, remaining work, arrival, original payload size, ready queue. |
| `block PID` | Remove a READY task from the queue and make it WAITING. |
| `wake PID` | Move a WAITING task to READY at the queue tail. |
| `quantum N` | Set the time quantum for future dispatches. |
| `step` / `run` | Execute one slice / execute until no READY task remains. |
| `metrics` / `export` | Display completed-task metrics / write `data/metrics.csv`. |
| `memory` | Show live payload bytes, free budget, allocation count. |
| `note create NAME TEXT` | Create a new note; existing notes are preserved. |
| `note append NAME TEXT` | Add a newline-terminated text line to an existing note. |
| `note read NAME` / `note delete NAME` | Read or delete a note. |
| `check` | Check queue uniqueness, states, payload ownership and memory counters. |
| `reset` | Free payloads, clear task records and reset the clock; retain notes and quantum. |
| `debug fail-next-allocation` | Force the next otherwise valid task allocation to fail. |
| `help` / `quit` | Show help / release payloads, close files and exit. EOF also exits cleanly. |

Names contain 1-48 ASCII letters, digits, underscores or hyphens. Names cannot
contain spaces or paths; note text can contain spaces. Bursts and quantum are
integers from 1 to 1,000,000. Each payload is 1-65,536 bytes, subject to the total
65,536-byte limit. There are at most 20 task **records**, including finished tasks;
`reset` starts a new experiment when the table fills. A line may contain at most
2,046 bytes; an oversized line is discarded in full.

Time is virtual ticks, not seconds or minutes. Commands execute between slices.
`run` returns when the ready queue is empty, even when WAITING tasks still exist.
Large bursts and quantum 1 produce many trace entries; use small teaching examples.

## Notes, logs and results

By default, the app uses `data/` relative to its working directory:

- `data/notes/NAME.txt`: persists across restarts and reset.
- `data/events.log`: appends START, virtual-timestamped events and SHUTDOWN.
- `data/trace.csv`: replaced at startup; slices include an experiment number.
- `data/metrics.csv`: replaced by `export`; includes finished tasks in the current experiment.

Task records, PID numbering and clock do not persist across process restarts.
The task table's BYTES column shows the original request; `memory` shows current
live allocations. Finished tasks retain their results but their payload is freed.
`reset` retains trace history using a new experiment number, but clears task metrics.

Use a separate writable directory for independent runs:

```sh
mkdir -p my_run/notes
./minios --data-dir my_run
```

Directories must exist before startup. Notes use synchronous C host-file I/O, not a
custom filesystem. The notes directory is trusted local storage, not a secure
sandbox. Failed writes may leave partial note content; errors are reported and
success is never claimed for a detected failed write or close.

## Tests and verification

```sh
make test
make sanitize
```

The first runs T1-T12 plus direct C assertions. The second builds and tests with
AddressSanitizer and UndefinedBehaviorSanitizer, with leak detection disabled for
environments that restrict process inspection. Both completed with 12/12 passing
integration cases and passing direct C assertions on 4 November 2026.

On a local Linux/WSL machine with suitable `/proc` access, also run:

```sh
make leak-check
```

LeakSanitizer could not run in the preparation environment because `/proc/PID/task`
was inaccessible. Its diagnostic is preserved in `evidence/leak_check_unavailable.json`.
No successful leak-tool result is claimed. The registry and direct C assertions
independently verify zero tracked payloads after completion, reset and shutdown.
`/dev/full` is used for a real buffered file-write failure on Linux; the test
records its unavailability on other hosts. Test-generated files use temporary
directories and do not overwrite user notes.

## Folder guide

```text
src/        C application implementations (one main program)
include/    shared types and module interfaces
tests/      C assertions, Python test runner, repeatable demo command files
data/notes/ runtime note directory (initially empty)
evidence/   executed transcripts, CSV/JSON results, logs and console captures
docs/       architecture notes and Phase 2 PDF progress report
Makefile    build, run, test, sanitizer and cleanup commands
```

Read `docs/ARCHITECTURE.md` for ownership, state transitions and metric definitions.
`docs/TEAM_REVIEW.md` provides the proposal's module ownership and final review plan.

## Submission and remaining work

Phase 2 requires a PDF progress report and organized C source as a ZIP or GitHub
repository. The separate progress PDF and this source package satisfy those
deliverable types. Team validation is still needed before submission: compile on
the team's intended machine, review the code and evidence, and confirm the team
details. Do not describe planned ownership as a record of completed individual work.

Before the final project phase: finish member cross-review, rerun leak checking on
a supported host, conduct the team demonstration, and update the final design,
limitations and actual contribution record. No real-time timers or GUI are needed
for the agreed core scope.

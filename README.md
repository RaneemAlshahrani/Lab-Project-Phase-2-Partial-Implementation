# MiniOS for a Study Room

**Operating Systems Project | Phase 2**

A C11 terminal application for simulated study tasks, round-robin scheduling,
task-owned memory, persistent revision notes, and study sessions.

## Team

| Name | KFUPM ID |
|---|---|
| Yasmeen Alshehri | 202271660 |
| Fatimah Alshehab | 202278660 |
| Raneem Alshahrani | 202277080 |
| Sarah Almas | 202261060 |

## Project status

The integrated core prototype is working. Overall project completion is estimated
at 70%; remaining work covers team acceptance checks, final report evidence,
peer review, and the demonstration. This is a planning estimate for the whole
project, rather than a measured percentage of source code.

## Files

| Location | Purpose |
|---|---|
| `src/` | Nine commented C implementation files. |
| `include/` | Eight commented headers containing interfaces and shared types. |
| `Makefile` | Build, run, and executable cleanup commands. |
| `README.md` | Project summary and build/run instructions. |

The progress report is supplied separately as an editable Word document.

## Build and run

Use Linux or Ubuntu through WSL, with GCC and GNU Make. The application uses
only the standard C library.

Extract the ZIP and open a terminal inside the `MiniOS_Phase2` folder:

```sh
make
make run
```

The program displays `minios>`. Enter `help` to see its commands. Run `make`
commands in the host terminal; enter the commands below inside MiniOS.

## Commands

| Command | Purpose |
|---|---|
| `task create NAME BURST BYTES` | Create a READY task. |
| `session start NAME TICKS BYTES` | Create a study-session task. |
| `tasks` | Show task records and ready-queue order. |
| `block PID` / `wake PID` | Move a task between READY and WAITING. |
| `quantum N` | Set the time quantum for future slices. |
| `step` / `run` | Execute one slice / execute until no READY task remains. |
| `metrics` / `export` | Show completed-task metrics / save them as CSV. |
| `memory` | Show current payload bytes and allocation count. |
| `note create NAME TEXT` | Create a new note without replacing an existing one. |
| `note append NAME TEXT` | Append a line to an existing note. |
| `note read NAME` / `note delete NAME` | Read or delete a note. |
| `check` | Check queue and memory consistency. |
| `reset` | Release payloads, clear task records, and reset the virtual clock. |
| `debug fail-next-allocation` | Demonstrate safe handling of an allocation failure. |
| `help` / `quit` | Show commands / clean up and exit. |

Names use 1-48 ASCII letters, digits, underscores, or hyphens. Bursts and quantum
are positive integers up to 1,000,000. Task records are limited to 20, including
finished records until reset. Each payload is 1-65,536 bytes, subject to the total
65,536-byte budget. Time is measured in virtual ticks. Note text can contain spaces.

## Quick example

```text
session start Study 4 256
task create Review 3 128
task create Practice 2 64
run
metrics
memory
quit
```

With quantum 2, dispatch order is P1, P2, P3, P1, P2; execution ends at tick 9.

## Runtime files and cleanup

`make run` creates `data/notes/` automatically. Notes, `data/events.log`, and
`data/trace.csv` are created during use; `export` writes `data/metrics.csv`.
These are application outputs and are not included in this source ZIP.

Notes survive reset and restart; task records and the clock do not survive a
restart. `quit` and EOF free outstanding payloads and close files. `make clean`
removes the executable and preserves saved notes and other runtime outputs.

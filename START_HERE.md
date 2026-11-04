# Start here

MiniOS for a Study Room is a terminal program written in C11. Its application
files are separated by responsibility and include comments explaining the
functions, data structures, state changes, memory ownership, and error paths.

## Run the program

Extract the ZIP, open the `MiniOS_Phase2` folder in a terminal, then enter:

```sh
make
make run
```

`make` is a host-terminal command. After the program displays `minios>`, enter
MiniOS commands such as `help`, `tasks`, or `run`.

## Where to look

| Folder or file | Contents |
|---|---|
| `src/` | The nine C implementation files. Start with `main.c`. |
| `include/` | Public interfaces and the shared structures in `core.h`. |
| `tests/` | Direct C assertions, integration runner, and demo command files. |
| `data/notes/` | Notes created while using the program. |
| `evidence/` | Recorded outputs, logs, CSV data, and test results. |
| `docs/` | Architecture explanation, source guide, team review plan, progress PDF. |
| `Makefile` | Commented build, run, test, and sanitizer commands. |
| `README.md` | Complete installation, command, output, and testing instructions. |

Read `docs/CODE_GUIDE.md` for a suggested source-reading order and function map.
All application behavior is in C. Python files are development tools for tests
and collecting evidence.

## Verify the program

```sh
make test
make sanitize
```

The standard and ASan/UBSan suites pass all 12 defined integration cases and
the direct C assertions. Full LeakSanitizer checking still needs a compatible
local Linux/WSL host; its environment limitation is explained in the README.

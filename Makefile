# MiniOS for a Study Room - C11 application and development checks.
# Run these commands in the extracted project folder, not at the minios> prompt.

# Application build: public headers are under include/, implementations under src/.
CC = gcc
CPPFLAGS = -Iinclude
# Treat warnings as errors so the normal build stays warning-free.
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
SOURCES = $(wildcard src/*.c)
# Unit tests supply their own main and call services without the command loop.
CORE_SOURCES = $(filter-out src/main.c src/shell.c,$(SOURCES))
HEADERS = $(wildcard include/*.h)

.PHONY: all run test sanitize leak-check clean
all: minios

# Build one terminal executable from all application modules.
minios: $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

# Ensure the runtime note directory exists, then start interactive mode.
run: minios
	mkdir -p data/notes
	./minios

# Compile direct assertions against the same core implementation as the program.
tests/test_core: tests/test_core.c $(CORE_SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_core.c $(CORE_SOURCES) -o $@

# The integration runner executes T1-T12 and then the direct C assertion binary.
test: minios tests/test_core
	python3 tests/run_tests.py

# Instrument application/tests for memory-access errors and undefined behavior.
# Leak detection is separate because some hosts restrict /proc inspection.
sanitize:
	$(CC) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/test_core.c $(CORE_SOURCES) -o tests/test_core_asan
	$(CC) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie $(SOURCES) -o minios_asan
	ASAN_OPTIONS=detect_leaks=0 python3 tests/run_tests.py --sanitize

# LeakSanitizer needs /proc access; run this on the team's Linux/WSL machine.
leak-check: sanitize
	ASAN_OPTIONS=detect_leaks=1 python3 tests/run_tests.py --sanitize

# Remove generated executables only; retain notes, source, docs, and evidence.
clean:
	rm -f minios minios_asan tests/test_core tests/test_core_asan

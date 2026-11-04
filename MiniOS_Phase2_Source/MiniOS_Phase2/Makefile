CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
SOURCES = $(wildcard src/*.c)
CORE_SOURCES = $(filter-out src/main.c src/shell.c,$(SOURCES))
HEADERS = $(wildcard include/*.h)

.PHONY: all run test sanitize leak-check clean
all: minios

minios: $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

run: minios
	mkdir -p data/notes
	./minios

tests/test_core: tests/test_core.c $(CORE_SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_core.c $(CORE_SOURCES) -o $@

test: minios tests/test_core
	python3 tests/run_tests.py

sanitize:
	$(CC) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/test_core.c $(CORE_SOURCES) -o tests/test_core_asan
	$(CC) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie $(SOURCES) -o minios_asan
	ASAN_OPTIONS=detect_leaks=0 python3 tests/run_tests.py --sanitize

# LeakSanitizer needs /proc access; run this on the team's Linux/WSL machine.
leak-check: sanitize
	ASAN_OPTIONS=detect_leaks=1 python3 tests/run_tests.py --sanitize

clean:
	rm -f minios minios_asan tests/test_core tests/test_core_asan

# Build the C11 application from its commented source modules.
CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
SOURCES = $(wildcard src/*.c)
HEADERS = $(wildcard include/*.h)

.PHONY: all run clean
all: minios

minios: $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

# Runtime directories are created when the application is launched this way.
run: minios
	mkdir -p data/notes
	./minios

# Remove only the generated executable; preserve saved notes and runtime data.
clean:
	rm -f minios

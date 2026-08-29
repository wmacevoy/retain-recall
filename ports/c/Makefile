# The C port.  Plain make and a C11 compiler; no CMake, no dependencies.
#
#   make test        default backend (thread-local storage class)
#   make test-all    all three storage backends
#   make csort       sort two arrays through the untouched ../../src/sort.c
#
# BACKEND selects the thread-local storage layer -- see retain.h:
#   (empty)                     _Thread_local / __declspec(thread) / __thread
#   -DRETAIN_TLS_KEYS           pthread_key_create / TlsAlloc
#   -DRETAIN_SINGLE_THREADED    a plain static; no TLS at all

CC ?= cc
BACKEND ?=
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS = -I. -I../../include $(BACKEND)

# On Windows the thread shim uses CreateThread, so there is no pthread to
# link.  MSYS2/mingw sets OS=Windows_NT, which is how this tells them apart.
ifeq ($(OS),Windows_NT)
  LDLIBS =
else
  LDLIBS = -lpthread
endif

BIN = tmp

all: $(BIN)/retain_test $(BIN)/csort

$(BIN):
	mkdir -p $(BIN)

$(BIN)/retain_test: retain_test.c thread.c retain.h thread.h | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ retain_test.c thread.c $(LDLIBS)

$(BIN)/csort: csort.c ../../src/sort.c retain.h | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ csort.c ../../src/sort.c

test: $(BIN)/retain_test $(BIN)/csort
	./$(BIN)/retain_test | tail -1
	./$(BIN)/csort

# Each backend needs its own build, so this cleans between them -- and at the
# end, so it never leaves a binary behind that the `transcript` target would
# reuse while believing it was built with the default backend.
test-all:
	$(MAKE) --no-print-directory clean test BACKEND=
	$(MAKE) --no-print-directory clean test BACKEND=-DRETAIN_TLS_KEYS
	$(MAKE) --no-print-directory clean test BACKEND=-DRETAIN_SINGLE_THREADED
	$(MAKE) --no-print-directory clean

# The transcript must match the C++ original byte for byte.  Silent, and it
# builds through a sub-make, so nothing but the transcript reaches stdout and
# the output can be diffed directly.
transcript:
	@$(MAKE) --no-print-directory $(BIN)/retain_test >/dev/null
	@./$(BIN)/retain_test | sed '1d;/^ok: /d;/^$$/d'

clean:
	rm -rf $(BIN)

.PHONY: all test test-all transcript clean

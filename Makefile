# _XOPEN_SOURCE allows the use of drand48
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE
CFLAGS   := -std=c99 -Wall -Wextra
LDLIBS   := -lncurses

.PHONY: build clean

build: episim

clean:
	rm -f episim

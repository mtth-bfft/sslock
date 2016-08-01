# Disable verbose output and implicit rules (harder to debug)
# .SUFFIXES:
# MAKEFLAGS += --no-print-directory
#

DEBUG ?= false
CFLAGS	?= -std=c11 -Wall -Wextra -Werror -pedantic
LDFLAGS ?= -lpam -lpam_misc -lX11

ifeq ($(DEBUG),true)
	CFLAGS += -O0 -g -DDEBUG
endif

all: sslock

sslock: sslock.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

test: sslock
	valgrind --leak-check=full ./sslock

clean:
	rm -f sslock


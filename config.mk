VERSION = 1.0.0

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# includes and libraries
INCS =
LIBS =

# flags
CPPFLAGS =
CFLAGS   = -std=c99 -Wpedantic -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# compiler and linker
CC = gcc

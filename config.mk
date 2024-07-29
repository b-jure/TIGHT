# tight version
VERSION = 1.0.0

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# optimizations 
#OPTS = -O2

# debug flags and definitions
DDEFS = -DTIGHT_ASSERT
ASANFLAGS = -fsanitize=address -fsanitize=undefined
DBGFLAGS = ${ASANFLAGS} -g

# flags
CFLAGS   = -std=c99 -Wpedantic -Wall -Wextra ${DDEFS} ${DBGFLAGS} ${OPTS}
LDFLAGS  = ${LIBS} ${ASANFLAGS}

# compiler and linker
CC = gcc

# archiver
AR = ar
ARARGS = rcs

# tight - loseless file compression program/library
# See 'tight.h' for copyright and licence details.

include config.mk

SRC = src/talloc.c src/tbuffer.c src/tdebug.c src/tdecode.c src/tencode.c \
	  src/tmd5.c src/tstate.c src/ttree.c
OBJ = ${SRC:.c=.o}

# binary
BINSRC = src/tight.c
BINOBJ = src/tight.o
BIN = tight

# shared library
LIBH = tight.h
LIBOBJ = ${SRC:.c=pic.o}
LIB = libtight.so

# archive
ARCHIVE = libtight.a


all: options ${BIN}

library: options ${LIB}

archive: options ${ARCHIVE}

options:
	@echo ${BIN} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

src/%.pic.o: src/%.c
	${CC} -c -fPIC ${CFLAGS} $< -o $@

${LIB}: ${LIBOBJ}
	${CC} -shared ${LDFLAGS} $^ -o $@

${BIN}: ${OBJ} ${BINOBJ}
	${CC} ${LDFLAGS} $^ -o $@

${ARCHIVE}: ${OBJ}
	${AR} ${ARARGS} $@ $^

${OBJ}: config.mk

src/%.o: src/%.c
	${CC} -c ${CFLAGS} $< -o $@

clean:
	rm -f ${BIN} ${LIB} ${ARCHIVE} ${OBJ} ${BINOBJ} ${LIBOBJ} \
	      ${BIN}-${VERSION}.tar.gz

dist: clean
	mkdir -p ${BIN}-${VERSION}
	cp -r COPYING Makefile README.md config.mk \
		${BIN}.1 ${SRC} ${BIN}-${VERSION}
	tar -cf ${BIN}-${VERSION}.tar ${BIN}-${VERSION}
	gzip ${BIN}-${VERSION}.tar
	rm -rf ${BIN}-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${BIN} ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/${BIN}
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < ${BIN}.1 | gzip > ${DESTDIR}${MANPREFIX}/man1/${BIN}.1.gz
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/${BIN}.1.gz

install-library: library
	mkdir -p ${DESTDIR}${PREFIX}/lib
	cp -f ${LIB} ${DESTDIR}${PREFIX}/lib
	cp -f src/${LIBH} ${DESTDIR}${PREFIX}/include
	chmod 755 ${DESTDIR}${PREFIX}/include/${LIBH}
	chmod 755 ${DESTDIR}${PREFIX}/lib/${LIB}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${BIN}\
		${DESTDIR}${MANPREFIX}/man1/${BIN}.1.gz

uninstall-library:
	rm -f ${DESTDIR}${PREFIX}/lib/${LIB}\
	rm -f ${DESTDIR}${PREFIX}/include/${LIBH}

.PHONY: all archive library options clean dist install install-library\
		unistall unistall-library

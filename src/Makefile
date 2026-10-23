# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c dwm.c util.c
OBJ = ${SRC:.c=.o}

all: dwm

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

dwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f dwm ${OBJ} dwm-${VERSION}.tar.gz config.h

dist: clean
	mkdir -p dwm-${VERSION}
	cp -R LICENSE Makefile README config.def.h config.mk\
		dwm.1 drw.h util.h ${SRC} dwm.desktop dwm.png transient.c dwm-${VERSION}
	tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	gzip dwm-${VERSION}.tar
	rm -rf dwm-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm ${DESTDIR}${PREFIX}/bin/dwm
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm
	cp -f ../bar.sh ${DESTDIR}${PREFIX}/bin/dwm-bar
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm-bar
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1
	mkdir -p ${DESTDIR}${APPPREFIX}
	cp -f dwm.desktop ${DESTDIR}${APPPREFIX}
	mkdir -p ${DESTDIR}${ICONPREFIX}/hicolor/scalable/apps
	cp -f ../dwm.svg ${DESTDIR}${ICONPREFIX}/hicolor/scalable/apps/dwm.svg
	mkdir -p ${DESTDIR}${ICONPREFIX}/hicolor/256x256/apps
	cp -f dwm.png ${DESTDIR}${ICONPREFIX}/hicolor/256x256/apps/dwm.png

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm\
		${DESTDIR}${PREFIX}/bin/dwm-bar\
		${DESTDIR}${MANPREFIX}/man1/dwm.1\
		${DESTDIR}${APPPREFIX}/dwm.desktop\
		${DESTDIR}${ICONPREFIX}/hicolor/scalable/apps/dwm.svg\
		${DESTDIR}${ICONPREFIX}/hicolor/256x256/apps/dwm.png

.PHONY: all clean dist install uninstall

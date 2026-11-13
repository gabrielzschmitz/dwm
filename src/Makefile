# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c dwm.c util.c
OBJ = ${SRC:.c=.o}
KEYS_MAN = dwm-keys.1

all: dwm

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

dwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

${KEYS_MAN}: config.h dwm-keys.sh dwm-keys.awk config.mk ${wildcard *.c}
	CC="${CC}" VERSION="${VERSION}" ./dwm-keys.sh config.h > $@
	chmod 644 $@

keys: ${KEYS_MAN}

clean:
	rm -f dwm ${OBJ} ${KEYS_MAN} dwm-${VERSION}.tar.gz config.h

dist: clean
	mkdir -p dwm-${VERSION}
	cp -R LICENSE Makefile README config.def.h config.mk\
		dwm.1 dwm-keys.sh dwm-keys.awk drw.h util.h ${SRC}\
		dwm.desktop dwm.png transient.c dwm-${VERSION}
	tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	gzip dwm-${VERSION}.tar
	rm -rf dwm-${VERSION}

install: all ${KEYS_MAN}
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm ${DESTDIR}${PREFIX}/bin/dwm
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm
	cp -f ../bar.sh ${DESTDIR}${PREFIX}/bin/dwm-bar
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm-bar
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1
	cp -f ${KEYS_MAN} ${DESTDIR}${MANPREFIX}/man1/dwm-keys.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm-keys.1
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
		${DESTDIR}${MANPREFIX}/man1/dwm-keys.1\
		${DESTDIR}${APPPREFIX}/dwm.desktop\
		${DESTDIR}${ICONPREFIX}/hicolor/scalable/apps/dwm.svg\
		${DESTDIR}${ICONPREFIX}/hicolor/256x256/apps/dwm.png

XEPHYR_DISPLAY ?= :1
XEPHYR_SIZE ?= 1280x720

xephyr:
	Xephyr ${XEPHYR_DISPLAY} -screen ${XEPHYR_SIZE} &

run:
	DISPLAY=${XEPHYR_DISPLAY} ./dwm

DEV_DIR = /tmp/dwm-dev
DEV_BAR_PID = ${DEV_DIR}/bar.pid

dev: all
	@mkdir -p ${DEV_DIR}
	@if [ -f ${DEV_BAR_PID} ]; then kill $$(cat ${DEV_BAR_PID}) 2>/dev/null; rm -f ${DEV_BAR_PID}; fi
	@pgrep -x Xephyr >/dev/null || (Xephyr ${XEPHYR_DISPLAY} -screen ${XEPHYR_SIZE} 2>/dev/null &) ; sleep 1
	@DISPLAY=${XEPHYR_DISPLAY} /bin/sh ../bar.sh & echo $$! > ${DEV_BAR_PID}
	DISPLAY=${XEPHYR_DISPLAY} ./dwm; kill $$(cat ${DEV_BAR_PID}) 2>/dev/null; rm -f ${DEV_BAR_PID}

.PHONY: all clean dist install uninstall keys xephyr run dev

#!/bin/sh

[ -z ${CC} ] && CC=gcc

CFLAGS="${CFLAGS:--Wall} $(pkg-config --cflags fuse)"
LDFLAGS="${LDFLAGS} $(pkg-config --libs fuse)"

${CC} ${CFLAGS} ${CPPFLAGS}  ${LDFLAGS} -o passfs *.c "$@"

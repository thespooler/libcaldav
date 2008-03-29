#!/bin/sh

AUTORECONF=$(which autoreconf)

function error {
	echo "Missing tool: $1"
	echo "Cannot proceed until the missing tool is available"
	exit 1
}

if [ ! -z ${AUTORECONF} ]; then
	echo "Using autoreconf to rebuild build system"
	autoreconf --force --install --symlink
	./configure $*
else
	echo "No autoreconf found. Using plain old tools to rebuild build system"
	autoconf --force || error autoconf
	autoheader --force || error autoheader
	aclocal -I m4|| error aclocal
	automake --add-missing --force-missing --gnu || error automake
	libtoolize --automake --force || error libtoolize
	./configure $*
fi

exit 0

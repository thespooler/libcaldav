#!/bin/sh

echo "Rebuilding build system......"

autoreconf --version 2>&1 > /dev/null 2>&1

if [ $? -eq 0 ]; then
	AUTORECONF=autoreconf
else
	AUTORECONF=
fi

error() {
	echo "Missing tool: $1"
	echo "Cannot proceed until the missing tool is available"
	exit 1
}

if [ ! -z ${AUTORECONF} ]; then
	echo "Using autoreconf to rebuild build system"
	autoreconf --force --install --symlink
else
	echo "No autoreconf found. Using plain old tools to rebuild build system"
	libtoolize --automake --force || error libtoolize
	aclocal -I m4|| error aclocal
	autoheader --force || error autoheader
	automake --add-missing --force-missing --gnu || error automake
	autoconf --force || error autoconf
fi
./configure $*

exit 0

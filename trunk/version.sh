#!/bin/sh

if [ "x$1" = "xLIBTOOL" ]; then
    grep -m 1 libcaldav ChangeLog | awk '{print $2}' | \
    cut -c2-6 | sed 's/\./:/g'
else
    grep -m 1 libcaldav ChangeLog | awk '{print $2}' | cut -c2-6
fi

exit 0

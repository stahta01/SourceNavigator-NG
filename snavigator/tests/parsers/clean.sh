#!/bin/sh
## set -x
## Shell script for *nix to clean up output for test cases
##

CLEANOPT="some"

if [ $# -gt 0 ]; then
 CLEANOPT=${1}
fi

rm -f *.out.bak
if [ "${CLEANOPT}" = "all" ]; then
 rm -f *.out
fi


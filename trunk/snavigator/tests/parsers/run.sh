#!/bin/sh
## set -x
## Shell script for *nix to run test case. Requires setting
## installation location
##

## EDIT THIS to point to the prefix root of install
USEPREFIX=/opt/sn

if [ ! -d ${USEPREFIX}/libexec/snavigator ]; then
 echo missing subdirectory libexec/snavigator in ${USEPREFIX}
 exit
fi

PATH=${USEPREFIX}/libexec/snavigator:$PATH

if [ $# -lt 1 ]; then
 echo missing test case parameter
 exit
fi

RUNTESTCASE=${1}

###
### NOTE: Make sure to change the "return" call to an "exit" call
###
if [ ! -f ${RUNTESTCASE}.test ]; then
 echo missing test file ${RUNTESTCASE}.test
 exit
fi
if [ -f ${RUNTESTCASE}.out ]; then
 mv ${RUNTESTCASE}.out ${RUNTESTCASE}.out.bak
fi
echo running tests...
hyper <<-TOHERE > ${RUNTESTCASE}.out
 source ${RUNTESTCASE}.test
 exit
TOHERE
tail -10 ${RUNTESTCASE}.out

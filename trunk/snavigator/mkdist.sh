#!/bin/sh

# This script will create distribution files for a release of
# Source-Navigator. It should be run from the src/ directory
# at the root of a clean CVS checkout.

DIR=/tmp/sourcenav
RELEASE=sourcenav-5.1.0a1
RELEASEDIR=$DIR/$RELEASE

PWD=`pwd`

if test `basename $PWD` != "src" ; then
    echo "Must be run from src/ directory at root of a checkout"
    exit 1
fi

if test -d $DIR ; then
    rm -rf $DIR
fi

mkdir $DIR
mkdir $RELEASEDIR

# Only copy those files that we actually need
ROOTFILES="COPYING ChangeLog Makefile.in config-ml.in config.guess \
config.if config.sub configure configure.in gettext.m4 install-sh \
libtool.m4 ltcf-c.sh ltcf-cxx.sh ltcf-gcj.sh ltconfig ltmain.sh \
missing mkdep mkinstalldirs move-if-change symlink-tree"

for file in $ROOTFILES ; do
    cp -p $file $RELEASEDIR
done

cp -p -R config db itcl libgui snavigator tcl tix tk $RELEASEDIR

cd $RELEASEDIR

find . -name CVS -exec rm -rf {} \; > /dev/null 2>&1
find . -name ".#*" -exec rm -f {} \; > /dev/null 2>&1
find . -name "*~" -exec rm -f {} \; > /dev/null 2>&1

cp snavigator/README.TXT .
cp snavigator/INSTALL.TXT .

cd ..

tar -czf $RELEASE.tar.gz $RELEASE
NODOT=`echo $RELEASE | sed -e 's/\.//g'`
zip -rq8 $NODOT.zip $RELEASE

echo "Done, dist files saved in $DIR"
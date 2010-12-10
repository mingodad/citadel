#!/bin/bash

cd ../lib
CITDIR=`pwd`
OUTDIR=${CITDIR}/../../../coverage/libcitadel

# if we call citserver with ./citserver, we don't need these: 
#cd ${CITDIR}/utillib/; ln -s . utillib; cd ..
#cd ${CITDIR}/modules
#for i in *; do cd $CITDIR/modules/$i; ln -s . modules; ln -s . $i; ln -s ../../user_ops.h .; done

cd ${CITDIR}
ln -s . lib
mkdir -p  ${OUTDIR}
lcov --base-directory ${CITDIR} --directory . --capture --output-file ${OUTDIR}/libcitadel.info $@
#lcov --base-directory ${CITDIR} --directory ../lib/ --capture --output-file ${OUTDIR}/libcitadel.info $@

genhtml --output-directory ${OUTDIR} ${OUTDIR}/libcitadel.info
rm -f lib


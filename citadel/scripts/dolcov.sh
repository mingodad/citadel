#!/bin/bash

CITDIR=`pwd`
OUTDIR=${CITDIR}/../../coverage/citadel
ln -s  parsedate.c y.tab.c

# if we call citserver with ./citserver, we don't need these: 
#cd ${CITDIR}/utillib/; ln -s . utillib; cd ..
#cd ${CITDIR}/modules
#for i in *; do cd $CITDIR/modules/$i; ln -s . modules; ln -s . $i; ln -s ../../user_ops.h .; done

cd ${CITDIR}

mkdir -p  ${OUTDIR}
lcov --base-directory ${CITDIR} --directory . --capture --output-file ${OUTDIR}/citadel.info $@
genhtml --output-directory ${OUTDIR} ${OUTDIR}/citadel.info



#exit
#rm y.tab.c

find -type l -exec rm {} \;
rm -f  .#user_ops.h.gcov
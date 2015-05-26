#!/bin/bash

# This script updates your Easy Install repository from git.

#export BRANCH=master
#export BRANCH=811592051c9c28dfd815d8e36b4006a7b78d6d66
export BRANCH=v8.24

export BASE=`pwd`
rm -vfr citadel 2>/dev/null
/usr/bin/git clone git://git.citadel.org/appl/gitroot/citadel.git
cd $BASE/citadel

/usr/bin/git checkout $BRANCH

for module in libcitadel citadel webcit textclient
do
	echo bootstrap of $module starting
	cd $BASE/citadel/${module}
	./bootstrap
	/usr/bin/git log -1 --pretty=%H . >../${module}-easyinstall.sum
	cd ..
	tar cvhzf ${module}-easyinstall.tar.gz \
		--exclude .git \
		--exclude "tests/testdata" \
		--exclude debian \
		${module}
	mv -vf ${module}-easyinstall.tar.gz .. && mv -vf ${module}-easyinstall.sum ..
done

cd $BASE
rm -vfr citadel

echo Done.

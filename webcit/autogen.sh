#!/bin/bash
# tool to generate the automaticaly provided stuff 
svn log >ChangeLog
#intltoolize --force 2>&1 |grep -v "You should update your 'aclocal.m4' by running aclocal."
autopoint --force
autoreconf -i --force 2>&1|grep -v 'warning: underquoted definition' 

#!/bin/bash
# tool to generate the automaticaly provided stuff 
svn log >ChangeLog
intltoolize --force
autoreconf -i --force


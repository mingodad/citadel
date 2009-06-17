# Copyright (C) 2006 Free Software Foundation, Inc.
# This configure.ac script is free software; the Free Software Foundation
# gives unlimited permission to copy, distribute and modify it.

AC_INIT([ctdlsh], [1.0], [bug-automake@gnu.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])

##########################################################################
# adding the readline library (e.g. with static name 'libreadline.a')
##########################################################################

# adding the lib to the files to link
LIBS="-lreadline $LIBS"

# introduce the optional configure parameter for a non-standard install prefix of readline
AC_ARG_WITH([readline],
    [AS_HELP_STRING([--with-readline=prefix],
        [try this for a non-standard install prefix of the readline library])],
    [readlinePATHSET=1],
    [readlinePATHSET=0])
	
# if optional parameter used, extend path flags for compliler and linker
if test $readlinePATHSET = 1 ; then
    # extend the compiler and linker flags according to the path set
    AM_CXXFLAGS="$AM_CXXFLAGS -I$with_readline/include"
    AM_LDFLAGS="$AM_LDFLAGS -L$with_readline/lib"
fi

##########################################################################


##########################################################################
# check for readline library
##########################################################################

# store current *FLAGS and merge with AM_*FLAGS for compilation and linker check   
OLD_CXXFLAGS=$CXXFLAGS;
OLD_LDFLAGS=$LDFLAGS;
CXXFLAGS="$AM_CXXFLAGS $CXXFLAGS"
LDFLAGS="$AM_LDFLAGS $LDFLAGS"

# ensure the library to check for is covered by the LIBS variable
OLD_LIBS=$LIBS
LIBS="$LIBS -lreadline"

# check for readline library headers   
AC_MSG_CHECKING([for the readline library headers])
# try to compile a file that includes a header of the library readline
AC_COMPILE_IFELSE([[#include <readline/readline.h>]],
    [AC_MSG_RESULT([found])
        # try to link the function 'rl_initialize' out of library readline
        AC_MSG_CHECKING([whether the readline library can be linked])
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM([[#include <readline/readline.h>]],
                [[rl_initialize();]])],
            [AC_MSG_RESULT([yes])
                FOUND_readline=1;],
            [AC_MSG_RESULT([no])
                LIBS=$OLD_LIBS; dnl reset to old value since readline was not found
                FOUND_readline=0;])],
    [AC_MSG_RESULT([not found])
        FOUND_readline=0;])

# reset original *FLAGS
CXXFLAGS=$OLD_CXXFLAGS
LDFLAGS=$OLD_LDFLAGS

# handle check results
if test $FOUND_readline != 1; then
    AC_MSG_NOTICE([])
    AC_MSG_NOTICE([The readline library was not found!])
    if test  $readlinePATHSET = 1 ; then
        AC_MSG_NOTICE([ The install prefix '$with_readline' for the readline library was set.])
        AC_MSG_NOTICE([ --> Maybe wrong ???])
    else
        AC_MSG_NOTICE([ No non-standard install prefix was set.])
        AC_MSG_NOTICE([ --> You might want to use '--with-readline=PREFIX' ?!?])
    fi
    AC_MSG_NOTICE([])
    AC_MSG_ERROR([ readline library is an essential dependency : cannot build and stop here !])
fi

##########################################################################





AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_OUTPUT

dnl Process this file with autoconf to produce a configure script.
dnl $Id$
AC_PREREQ(2.52)
AC_INIT([Citadel/UX], [5.91], [http://uncensored.citadel.org/])
AC_REVISION([$Revision$])
AC_CONFIG_SRCDIR([citserver.c])
AC_PREFIX_DEFAULT(/usr/local/citadel)
if test "$prefix" = NONE; then
	AC_DEFINE_UNQUOTED(BBSDIR, "$ac_default_prefix")
else
	AC_DEFINE_UNQUOTED(BBSDIR, "$prefix")
fi

AC_ARG_ENABLE(autologin, [  --disable-autologin     disable autologin (default is enabled if possible)])
AC_ARG_ENABLE(chkpwd, [  --disable-chkpwd        don't build 'chkpwd'])

AC_ARG_ENABLE(threaded-client, [  --disable-threaded-client
			  disable multithreaded client])

AC_ARG_WITH(pam, [  --with-pam              use PAM if present (see PAM.txt before you try this)])
AC_ARG_WITH(kthread, [  --with-kthread          use kernel threads (on FreeBSD) (not recommended yet)])
AC_ARG_WITH(gdbm, [  --with-gdbm@<:@=DIR@:>@       use GDBM @<:@DIR=/usr/local@:>@])
AC_ARG_WITH(db, [  --with-db@<:@=DIR@:>@         use Sleepycat DB 3.x @<:@DIR=/usr/local/BerkeleyDB.3.@<:@123@:>@@:>@])

if test "x$with_gdbm" != xno -a "x$with_gdbm" != xyes -a "$with_gdbm"; then
	db_dir="$with_gdbm"
	with_gdbm=yes
else
	test -f /usr/local/lib/libgdbm.a -o -f /usr/local/lib/libgdbm.so && db_dir=/usr/local
fi

if test "x$with_db" != xno -a "x$with_db" != xyes -a "$with_db"; then
	db_dir="$with_db"
	with_db=yes
else
	test -d /usr/local/BerkeleyDB.3.1 && db_dir=/usr/local/BerkeleyDB.3.1
	test -d /usr/local/BerkeleyDB.3.2 && db_dir=/usr/local/BerkeleyDB.3.2
	test -d /usr/local/BerkeleyDB.3.3 && db_dir=/usr/local/BerkeleyDB.3.3
fi

if test "x$with_gdbm" = xyes -a "x$with_db" = xyes; then
	AC_MSG_ERROR(can't enable both gdbm and db)
fi

dnl By default, we only build the client (citadel and whobbs) unless we can
dnl figure out how to build with POSIX threads.
TARGETS=client

AC_CANONICAL_HOST
PTHREAD_DEFS=-D_REENTRANT
case "$host" in
	dnl BSDI 3.0 wants relocatable object modules instead of shared libs
	dnl for dlopen(), and has a wrapper script to link with shared libs.
	dnl Also has stupid non-reentrant gethostbyaddr() and friends.
	*-*-bsdi[123]*)
		test -z "$CC" -a -x /usr/bin/shlicc2 && CC=shlicc2
		AC_DEFINE(HAVE_NONREENTRANT_NETDB)
	;;
	*-*-bsdi*)
		AC_DEFINE(HAVE_NONREENTRANT_NETDB)
	;;
	dnl Digital Unix has an odd way to build for pthreads, and we can't
	dnl build pthreads programs with gcc due to header problems.
	alpha*-dec-osf*)
		test -z "$CC" && CC=cc
		PTHREAD_LIBS="-lpthread -lexc"
		check_pthread=no
	;;
	dnl FreeBSD is similar to Digital UNIX with DEC C, which has a -pthread flag:
	*-*-freebsd*)
		if test "$with_kthread" = yes; then
			PTHREAD_LIBS=-kthread
		else
			PTHREAD_LIBS=-pthread
		fi
		check_pthread=no
		PTHREAD_DEFS=-D_THREAD_SAFE
	;;
	*-*-openbsd*)
		PTHREAD_LIBS=-pthread
		check_pthread=no
		PTHREAD_DEFS=-pthread
	;;
	*-*-solaris*)
		PTHREAD_DEFS="-D_REENTRANT -D_PTHREADS"
	;;
esac

dnl Checks for programs.
AC_PROG_CC

dnl Set up system-dependent compiler flags.
if test "$GCC" = yes; then
	case "$host" in
		*-*-solaris*|alpha*-dec-osf*)
			CFLAGS="$CFLAGS -Wall -Wcast-align -Wno-char-subscripts"
		;;
		*)
			CFLAGS="$CFLAGS -Wall -Wcast-align -Wstrict-prototypes"
		;;
	esac
fi
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
AC_PROG_INSTALL
AC_PROG_YACC
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)

dnl Checks for system services.

dnl Checks for libraries.

dnl libdl, libgdbm, and libcrypt are only used in the server, so the
dnl Makefile only passes $(LIBS) to that target. If other programs start
dnl requiring additional libraries, we'll have to use other variables, as is
dnl done with curses.

dnl We want to test for the following in libc before checking for their
dnl respective libraries, because some systems (like Irix) have both, and the
dnl non-libc versions may be broken.
AC_CHECK_FUNCS(crypt dlopen gethostbyname connect)

if test "$ac_cv_func_gethostbyname" = no; then
	AC_CHECK_LIB(nsl, gethostbyname, NETLIBS="-lnsl $NETLIBS")
fi

if test "$ac_cv_func_connect" = no; then
        AC_CHECK_LIB(socket, connect, NETLIBS="-lsocket $NETLIBS",, $NETLIBS)
fi

if test "$ac_cv_func_dlopen" = no; then
        AC_CHECK_LIB(dl, dlopen, [LIBS="-ldl $LIBS"
               test "$with_pam" = yes && chkpwd_LIBS="-ldl $chkpwd_LIBS"])
fi

dnl Check for HP/UX dyanmic loader. This should only be in -ldld.
AC_CHECK_LIB(dld, shl_load, LIBS="-ldld $LIBS")

dnl Check for Solaris realtime support
AC_CHECK_LIB(rt, sched_yield)

dnl Determine the system's authentication capabilities, if autologin is
dnl requested. We currently support PAM, standard getpwnam(), and getspnam()
dnl (Linux shadow passwords)
if test "$enable_autologin" != no; then
	if test "$with_pam" = yes; then
		save_LIBS=$LIBS
		AC_CHECK_LIB(pam, pam_start, [chkpwd_LIBS="-lpam $chkpwd_LIBS"
			LIBS="-lpam $LIBS"])
		AC_CHECK_FUNCS(pam_start)
		test "$enable_chkpwd" != no && LIBS=$save_LIBS
	fi
	if test "$ac_cv_func_pam_start" = no -o "$with_pam" != yes; then
		save_LIBS=$LIBS
		AC_SEARCH_LIBS(getspnam, shadow, [
			if test "$ac_cv_search_getspnam" = -lshadow; then
				chkpwd_LIBS="-lshadow $chkpwd_LIBS"
			fi])
		test "$enable_chkpwd" != no && LIBS=$save_LIBS
		if test "$ac_cv_func_crypt" = no; then
			AC_CHECK_LIB(crypt, crypt, [
				chkpwd_LIBS="-lcrypt $chkpwd_LIBS"
				test "$enable_chkpwd" = no && \
					LIBS="-lcrypt $LIBS"])
		fi
	fi
	if test "$ac_cv_func_crypt" = yes -o "$ac_cv_lib_crypt_crypt" = yes -o "$ac_cv_func_pam_start" = yes; then
		AC_DEFINE(ENABLE_AUTOLOGIN)
		if test "$enable_chkpwd" != no; then
			AC_DEFINE(ENABLE_CHKPWD)
			CHKPWD=chkpwd
		else
			AUTH=auth.lo
		fi
	fi
fi

test "$db_dir" && LDFLAGS="$LDFLAGS -L$db_dir/lib"
if test "x$with_db" != xno -a "x$with_gdbm" != xyes; then
	AC_CHECK_LIB(db3, db_env_create,
		[GDBM="$GDBM -ldb3"
		DATABASE=database_sleepycat.c
		with_gdbm=no], [AC_CHECK_LIB(db, db_env_create,
					[GDBM="$GDBM -ldb"
					DATABASE=database_sleepycat.c
					with_gdbm=no], [], [$NETLIBS])], [$NETLIBS])
fi

if test "x$with_gdbm" != xno -a "x$with_db" != xyes; then
        AC_CHECK_LIB(gdbm, gdbm_open,
                [GDBM="$GDBM -lgdbm"
                DATABASE=database.c
                with_db=no])
fi

test -f /usr/local/lib/libresolv.a && LDFLAGS="$LDFLAGS -L/usr/local/lib"
AC_CHECK_LIB(resolv, res_query, RESOLV="$RESOLV -lresolv")

save_LIBS=$LIBS
LIBS=$CURSES
AC_SEARCH_LIBS(tgetent, [curses termcap])
AC_SEARCH_LIBS(initscr, [curses ncurses])
CURSES=$LIBS
LIBS=$save_LIBS

dnl Check for libpthread(s) if we're not using Digital UNIX or FreeBSD. (On
dnl which the -pthread flag takes care of this.)
if test "$check_pthread" != no; then
	AC_CHECK_LIB(pthread, pthread_create,
		PTHREAD_LIBS="-lpthread $PTHREAD_LIBS")
	AC_CHECK_LIB(pthreads, pthread_create,
		PTHREAD_LIBS="-lpthreads $PTHREAD_LIBS")
fi

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
test "$db_dir" && CPPFLAGS="$CPPFLAGS -I$db_dir/include"
AC_CHECK_HEADERS(curses.h dl.h fcntl.h limits.h termios.h sys/ioctl.h sys/select.h sys/time.h syslog.h unistd.h utmp.h utmpx.h paths.h db3/db.h)

AC_CHECK_HEADER(resolv.h, [SMTP=modules/libsmtp.la; DOMAIN=domain.c])
AC_SUBST(SMTP)
AC_SUBST(DOMAIN)

dnl some systems require -pthread, -D_REENTRANT, etc to be passed to cc if we
dnl include pthread.h:
save_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS $PTHREAD_DEFS"
AC_CHECK_HEADERS(pthread.h)
CPPFLAGS=$save_CPPFLAGS

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Check the size of various builtin types; see typesize.h (error)
AC_CHECK_SIZEOF(char, 0)
AC_CHECK_SIZEOF(short, 0)
AC_CHECK_SIZEOF(int, 0)
AC_CHECK_SIZEOF(long, 0)
dnl AC_CHECK_SIZEOF(long long, 0)

AC_CACHE_CHECK([for ut_type in struct utmp], ac_cv_struct_ut_type,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <utmp.h>], [struct utmp ut; ut.ut_type;],
ac_cv_struct_ut_type=yes, ac_cv_struct_ut_type=no)])
if test $ac_cv_struct_ut_type = yes; then
	AC_DEFINE(HAVE_UT_TYPE)
fi

AC_CACHE_CHECK([for ut_host in struct utmp], ac_cv_struct_ut_host,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <utmp.h>], [struct utmp ut; ut.ut_host;],
ac_cv_struct_ut_host=yes, ac_cv_struct_ut_host=no)])
if test $ac_cv_struct_ut_host = yes; then
	AC_DEFINE(HAVE_UT_HOST)
fi

dnl Checks for library functions.
AC_FUNC_GETPGRP
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(getspnam getutxline mkdir mkfifo mktime rmdir select socket strerror strcasecmp strncasecmp)

dnl Now check for pthreads -- set up variables so that the compiler will be run
dnl with proper flags for pthread programs
save_LIBS=$LIBS
LIBS="$PTHREAD_LIBS $LIBS"

dnl On some platforms, AC_CHECK_FUNC[S] doesn't work for pthreads programs;
dnl we need to include pthread.h
dnl AC_CHECK_FUNCS(pthread_cancel)

AC_CACHE_CHECK([for pthread_cancel], ac_cv_func_pthread_cancel,
[AC_TRY_LINK([#include <pthread.h>],
[	pthread_t thread;

	/* The GNU C library defines this for functions which it implements
	   to always fail with ENOSYS.  Some functions are actually named
	   something starting with __ and the normal name is an alias.  */
#if defined (__stub_pthread_cancel) || defined (__stub___pthread_cancel)
	choke me
#else
	pthread_cancel(thread);
#endif],
ac_cv_func_pthread_cancel=yes, ac_cv_func_pthread_cancel=no)])
if test "$ac_cv_func_pthread_cancel" = yes; then
	AC_DEFINE(HAVE_PTHREAD_CANCEL)
fi

dnl AC_CHECK_FUNC(pthread_create, TARGETS="client server utils serv_modules")

AC_CACHE_CHECK([for pthread_create], ac_cv_func_pthread_create,
[AC_TRY_LINK([#include <pthread.h>],
[	/* The GNU C library defines this for functions which it implements
	   to always fail with ENOSYS.  Some functions are actually named
	   something starting with __ and the normal name is an alias.  */
#if defined (__stub_pthread_cancel) || defined (__stub___pthread_cancel)
	choke me
#else
	pthread_create(NULL, NULL, NULL, NULL);
#endif],
ac_cv_func_pthread_create=yes, ac_cv_func_pthread_create=no)])
if test "$ac_cv_func_pthread_create" = yes; then
	TARGETS="client server utils serv_modules"
	if test "x$enable_threaded_client" != xno; then
		AC_DEFINE(THREADED_CLIENT)
		CLIENT_PTLIBS=$PTHREAD_LIBS
		CL_LIBOBJS='$(LIBOBJS:.o=.ro)'
		CX=.ro
	fi
fi

if test "x$CX" != x.ro; then
	CL_LIBOBJS='$(LIBOBJS)'
	CX=.o
fi

dnl Now restore the old libs so we don't pass the wrong stuff to makefile
LIBS=$save_LIBS

AC_REPLACE_FUNCS(snprintf getutline)

dnl Done! Now write the Makefile and sysdep.h
AC_SUBST(AUTH)
AC_SUBST(CHKPWD)
AC_SUBST(CURSES)
AC_SUBST(GDBM)
AC_SUBST(RESOLV)
AC_SUBST(NETLIBS)
AC_SUBST(chkpwd_LIBS)
AC_SUBST(TARGETS)
AC_SUBST(PTHREAD_DEFS)
AC_SUBST(PTHREAD_LIBS)
AC_SUBST(CLIENT_PTLIBS)
AC_SUBST(CL_LIBOBJS)
AC_SUBST(CX)
AC_SUBST(DATABASE)
AC_CONFIG_HEADER(sysdep.h)
AC_CONFIG_FILES([Makefile weekly])
AC_OUTPUT

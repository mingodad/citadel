dnl Process this file with autoconf to produce a configure script.
dnl $Id$
AC_PREREQ(2.52)
AC_INIT([Citadel/UX], [6.02], [http://uncensored.citadel.org/])
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
AC_ARG_WITH(ssl,
	[  --with-ssl=PATH     Specify path to OpenSSL installation ],
	[
		if test "x$withval" != "xno" ; then
			tryssldir=$withval
		fi
	]
)
AC_ARG_WITH(ncurses, [  --without-ncurses       don't use ncurses])

AC_ARG_WITH(with_zlib, [  --with-zlib             use zlib compression if present])
AC_ARG_WITH(with_libical, [  --with-libical          use libical calendaring library])

if test "x$with_gdbm" != xno -a "x$with_gdbm" != xyes -a "$with_gdbm"; then
	db_dir="$with_gdbm"
	with_gdbm=yes
fi

if test "x$with_db" != xno -a "x$with_db" != xyes -a "$with_db"; then
	db_dir="$with_db"
	with_db=yes
else
	test -f /usr/local/lib/libdb.a -o -f /usr/local/lib/libdb.so \
	  -o -f /usr/local/lib/libdb4.a -o -f /usr/local/lib/libdb4.so \
	  -o -f /usr/local/lib/libdb3.a -o -f /usr/local/lib/libdb3.so && db_dir=/usr/local

	test -d /usr/local/BerkeleyDB.3.1 && db_dir=/usr/local/BerkeleyDB.3.1
	test -d /usr/local/BerkeleyDB.3.2 && db_dir=/usr/local/BerkeleyDB.3.2
	test -d /usr/local/BerkeleyDB.3.3 && db_dir=/usr/local/BerkeleyDB.3.3
	test -d /usr/local/BerkeleyDB.4.0 && db_dir=/usr/local/BerkeleyDB.4.0
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
	dnl Curses support on Mac OS X is kind of screwed at the moment.
	*-*-darwin*)
		AC_DEFINE(DISABLE_CURSES)
	;;
	dnl Digital Unix has an odd way to build for pthreads, and we can't
	dnl build pthreads programs with gcc due to header problems.
	alpha*-dec-osf*)
		test -z "$CC" && CC=cc
		LIBS="-lpthread -lexc $LIBS"
		check_pthread=no
	;;
	dnl FreeBSD is similar to Digital UNIX with DEC C, which has a -pthread flag:
	*-*-freebsd*)
		if test "$with_kthread" = yes; then
			LIBS="-kthread $LIBS"
		else
			LIBS="-pthread $LIBS"
		fi
		check_pthread=no
		PTHREAD_DEFS=-D_THREAD_SAFE
	;;
	*-*-openbsd*)
		LIBS="-pthread $LIBS"
		check_pthread=no
		PTHREAD_DEFS=-pthread
	;;
	*-*-solaris*)
		PTHREAD_DEFS="-D_REENTRANT -D_PTHREADS"
	;;
esac
DEFS="$DEFS $PTHREAD_DEFS"

dnl Checks for programs.
AC_PROG_CC

dnl Set up system-dependent compiler flags.
if test "$GCC" = yes; then
	case "$host" in
		*-*-solaris*|alpha*-dec-osf*)
			CFLAGS="$CFLAGS -Wall -Wcast-qual -Wcast-align -Wno-char-subscripts"
		;;
		*)
			CFLAGS="$CFLAGS -Wall -Wcast-qual -Wcast-align -Wstrict-prototypes"
		;;
	esac
fi
AC_DISABLE_STATIC
AC_PROG_INSTALL
AC_PROG_YACC
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)

dnl Checks for system services.

dnl Check the size of various builtin types; see typesize.h (error)
AC_CHECK_SIZEOF(char, 0)
AC_CHECK_SIZEOF(short, 0)
AC_CHECK_SIZEOF(int, 0)
AC_CHECK_SIZEOF(long, 0)
dnl AC_CHECK_SIZEOF(long long, 0)

dnl Checks for libraries.

dnl We want to test for the following in libc before checking for their
dnl respective libraries, because some systems (like Irix) have both, and the
dnl non-libc versions may be broken.
AC_CHECK_FUNCS(crypt gethostbyname connect)

if test "$ac_cv_func_gethostbyname" = no; then
	AC_CHECK_LIB(nsl, gethostbyname)
fi

if test "$ac_cv_func_connect" = no; then
        AC_CHECK_LIB(socket, connect)
fi

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

test -f /usr/local/lib/libresolv.a && LDFLAGS="$LDFLAGS -L/usr/local/lib"
AC_CHECK_LIB(resolv, res_query, RESOLV="$RESOLV -lresolv")

if test "x$with_ncurses" != "xno"; then
	AC_SEARCH_LIBS(tgetent, [ncurses curses termcap])
	AC_SEARCH_LIBS(scrollok, [ncurses curses])
	AC_SEARCH_LIBS(initscr, [ncurses curses])
else
	AC_SEARCH_LIBS(tgetent, [curses termcap])
	AC_SEARCH_LIBS(initscr, [curses])
fi
AC_CHECK_FUNCS(vw_printw wcolor_set resizeterm wresize)

dnl Check for libpthread(s) if we're not using Digital UNIX or FreeBSD. (On
dnl which the -pthread flag takes care of this.)
if test "$check_pthread" != no; then
	AC_CHECK_LIB(pthread, pthread_create)
	AC_CHECK_LIB(pthreads, pthread_create)
fi

# The big search for OpenSSL
if test "$with_ssl" != "no"; then
	saved_LIBS="$LIBS"
	saved_LDFLAGS="$LDFLAGS"
	saved_CFLAGS="$CFLAGS"
	if test "x$prefix" != "xNONE"; then
		tryssldir="$tryssldir $prefix"
	fi
	AC_CACHE_CHECK([for OpenSSL], ac_cv_openssldir, [
		for ssldir in $tryssldir "" /usr /usr/local/openssl /usr/lib/openssl /usr/local/ssl /usr/lib/ssl /usr/local /usr/pkg /opt /opt/openssl ; do
			CFLAGS="$saved_CFLAGS"
			LDFLAGS="$saved_LDFLAGS"
			LIBS="$saved_LIBS -lssl -lcrypto"
	
			# Skip directories if they don't exist
			if test ! -z "$ssldir" -a ! -d "$ssldir" ; then
				continue;
			fi
			if test ! -z "$ssldir" -a "x$ssldir" != "x/usr"; then
				# Try to use $ssldir/lib if it exists, otherwise
				# $ssldir
				if test -d "$ssldir/lib" ; then
					LDFLAGS="-L$ssldir/lib $saved_LDFLAGS"
					if test ! -z "$need_dash_r" ; then
						LDFLAGS="-R$ssldir/lib $LDFLAGS"
					fi
				else
					LDFLAGS="-L$ssldir $saved_LDFLAGS"
					if test ! -z "$need_dash_r" ; then
						LDFLAGS="-R$ssldir $LDFLAGS"
					fi
				fi
				# Try to use $ssldir/include if it exists, otherwise
				# $ssldir
				if test -d "$ssldir/include" ; then
					CFLAGS="-I$ssldir/include $saved_CFLAGS"
				else
					CFLAGS="-I$ssldir $saved_CFLAGS"
				fi
			fi
	
			# Basic test to check for compatible version and correct linking
			# *does not* test for RSA - that comes later.
			AC_TRY_RUN(
				[
	#include <string.h>
	#include <openssl/rand.h>
	int main(void)
	{
		char a[2048];
		memset(a, 0, sizeof(a));
		RAND_add(a, sizeof(a), sizeof(a));
		return(RAND_status() <= 0);
	}
				],
				[
					found_crypto=1
					break;
				], []
			)
	
			if test ! -z "$found_crypto" ; then
				break;
			fi
		done
	
		if test -z "$ssldir" ; then
			ssldir="(system)"
		fi
	
		if test ! -z "$found_crypto" ; then
			ac_cv_openssldir=$ssldir
		else
			ac_cv_openssldir="no"
		fi
	])
	LIBS="$saved_LIBS"
	LDFLAGS="$saved_LDFLAGS"
	CFLAGS="$saved_CFLAGS"
	
	if test "x$ac_cv_openssldir" != "xno" ; then
		AC_DEFINE(HAVE_OPENSSL)
		LIBS="-lssl -lcrypto $LIBS"
		dnl Need to recover ssldir - test above runs in subshell
		ssldir=$ac_cv_openssldir
		if test ! -z "$ssldir" -a "x$ssldir" != "x/usr" -a "x$ssldir" != "x(system)"; then
			# Try to use $ssldir/lib if it exists, otherwise
			# $ssldir
			if test -d "$ssldir/lib" ; then
				LDFLAGS="-L$ssldir/lib $saved_LDFLAGS"
				if test ! -z "$need_dash_r" ; then
					LDFLAGS="-R$ssldir/lib $LDFLAGS"
				fi
			else
				LDFLAGS="-L$ssldir $saved_LDFLAGS"
				if test ! -z "$need_dash_r" ; then
					LDFLAGS="-R$ssldir $LDFLAGS"
				fi
			fi
			# Try to use $ssldir/include if it exists, otherwise
			# $ssldir
			if test -d "$ssldir/include" ; then
				CFLAGS="-I$ssldir/include $saved_CFLAGS"
			else
				CFLAGS="-I$ssldir $saved_CFLAGS"
			fi
		fi
	fi
fi

if test "x$with_db" != xno -a "x$with_gdbm" != xyes; then
	test "$db_dir" && LDFLAGS="$LDFLAGS -L$db_dir/lib"

	dblib=""

	if test -d "$db_dir/include/db4"; then
		CPPFLAGS="$CPPFLAGS -I$db_dir/include/db4"
		dblib="db4"
	elif test -d "$db_dir/include/db3"; then
		CPPFLAGS="$CPPFLAGS -I$db_dir/include/db3"
		dblib="db3"
	elif test "$db_dir"; then
		CPPFLAGS="$CPPFLAGS -I$db_dir/include"
	elif test -d /usr/include/db4; then
		CPPFLAGS="$CPPFLAGS -I/usr/include/db4"
		dblib="db4"
	elif test -d /usr/include/db3; then
		CPPFLAGS="$CPPFLAGS -I/usr/include/db3"
		dblib="db3"
	fi

	AC_SEARCH_LIBS(db_env_create, [$dblib db db4 db3], [
		DATABASE=database_sleepycat.c
		with_gdbm=no
	])

fi

if test "x$with_gdbm" != xno -a "x$with_db" != xyes; then
	test -f /usr/local/lib/libgdbm.a -o -f /usr/local/lib/libgdbm.so && db_dir=/usr/local
	test "$db_dir" && LDFLAGS="$LDFLAGS -L$db_dir/lib"

	test "$db_dir" && CPPFLAGS="$CPPFLAGS -I$db_dir/include"

        AC_CHECK_LIB(gdbm, gdbm_open,
                [LIBS="-lgdbm $LIBS"
                DATABASE=database.c
                with_db=no])
fi



dnl Checks for the zlib compression library.
if test "x$with_zlib" != xno ; then
	AC_CHECK_HEADERS(zlib.h,
		[AC_CHECK_LIB(z, zlibVersion,
			[ok_zlib=yes],,
	)])
fi

if test "x$ok_zlib" = xyes ; then
	LIBS="-lz $LIBS"
	AC_DEFINE(HAVE_ZLIB)
fi



dnl Checks for the libical calendaring library.
if test "x$with_libical" != xno ; then
	AC_CHECK_HEADERS(ical.h,
		[AC_CHECK_LIB(ical, icalcomponent_new,
			[ok_libical=yes],,
	)])
fi

if test "x$ok_libical" = xyes ; then

	AC_TRY_RUN(
		[
#include <ical.h>
main() {
	float v;
	int i;
	sscanf(ICAL_VERSION, "%f", &v);
	printf("libical version: %0.2f\n", v);
	v = v * 100.0;
	i = (int) v;
	if (i >= 24) {
		printf("This version is ok.\n");
		return(0);
	}
	printf("libical 0.24 or newer required.\n");
	printf("Citadel will be built without calendar support.\n");
	return(1);
}
		], 
		[
			LIBS="-lical $LIBS"
			AC_DEFINE(HAVE_LIBICAL)
		]
	)
fi



dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT

dnl
dnl TODO: for the DB header checks, we should check whether the headers
dnl define db_env_create, somehow
dnl
AC_CHECK_HEADERS(curses.h dl.h fcntl.h limits.h malloc.h termios.h sys/ioctl.h sys/select.h sys/time.h syslog.h unistd.h utmp.h utmpx.h paths.h db.h db3/db.h db4/db.h pthread.h netinet/in.h arpa/nameser.h)

dnl
dnl need to check if this is actually compilable, sometimes there are
dnl conflicts with the system's headers (eg solaris xpg4 style headers)
dnl
if test "x$with_ncurses" != xno; then
	AC_CHECK_HEADERS(ncurses.h,,,[ ])
fi

AC_CHECK_HEADER(resolv.h,,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif])

AC_SUBST(SMTP)
AC_SUBST(DOMAIN)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl defined in acinclude.m4:
CIT_STRUCT_TM

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

dnl Now check for pthreads

dnl On some platforms, AC_CHECK_FUNC[S] doesn't work for pthreads programs;
dnl we need to include pthread.h

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
	test "$DATABASE" && TARGETS="client server utils"
	if test "x$enable_threaded_client" != xno; then
		AC_DEFINE(THREADED_CLIENT)
	fi
fi

AC_REPLACE_FUNCS(snprintf getutline)

# AC_CACHE_CHECK([the weather], ac_cv_weather, [
# 	sleep 1
# 	echo $ECHO_N "opening your window... $ECHO_C" >&6
# 	sleep 2
# 	month=`date | cut -f 2 -d ' '`
# 	case $month in
# 	Dec | Jan | Feb)
# 		ac_cv_weather="it's cold!"
# 		;;
# 	Mar | Apr)
# 		ac_cv_weather="it's wet!"
# 		;;
# 	Jul | Aug)
# 		ac_cv_weather="it's hot!"
# 		;;
# 	Oct | Nov)
# 		ac_cv_weather="it's cool"
# 		;;
# 	May | Jun | Sep | *)
# 		ac_cv_weather="it's fine"
# 		;;
#	esac
#	])

AC_CACHE_CHECK([under the bed], ac_cv_under_the_bed, [
	number=`date | cut -c 19`
	case $number in
	7)
		ac_cv_under_the_bed="lunatics and monsters found"
		;;
	*)
		ac_cv_under_the_bed="dust bunnies found"
		;;
	esac
	])

dnl Done! Now write the Makefile and sysdep.h
AC_SUBST(AUTH)
AC_SUBST(CHKPWD)
AC_SUBST(RESOLV)
AC_SUBST(chkpwd_LIBS)
AC_SUBST(TARGETS)
AC_SUBST(DATABASE)
AC_CONFIG_HEADER(sysdep.h)
AC_CONFIG_FILES([Makefile weekly])
AC_OUTPUT

if test -z "$DATABASE"; then
	AC_MSG_WARN([No database driver was found. Please install Berkeley DB.])
fi

echo ...
echo ...
echo "... On many operating systems, you must use GNU make (gmake) to compile"
echo ... Citadel.
echo ...
echo ...

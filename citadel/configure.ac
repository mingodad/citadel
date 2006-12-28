dnl Process this file with autoconf to produce a configure script.
dnl $Id$
AC_PREREQ(2.52)
AC_INIT([Citadel], [7.02], [http://www.citadel.org/])
AC_REVISION([$Revision$])
AC_CONFIG_SRCDIR([citserver.c])
AC_PREFIX_DEFAULT(/usr/local/citadel)
if test "$prefix" = NONE; then
	AC_DEFINE_UNQUOTED(CTDLDIR, "$ac_default_prefix")
else
	AC_DEFINE_UNQUOTED(CTDLDIR, "$prefix")
fi


dnl Make sure we see all GNU and Solaris extensions.
AC_GNU_SOURCE


dnl Checks for the Datadir
AC_ARG_WITH(datadir, 
		    [  --with-datadir          directory to store the databases under],
			[ if test "x$withval" != "xno" ; then
					      AC_DEFINE(HAVE_DATA_DIR)
						  AC_DEFINE_UNQUOTED(DATA_DIR, "$withval")
						  MAKE_DATA_DIR=$withval
						  AC_SUBST(MAKE_DATA_DIR)
			  fi
			]
)

dnl Checks for the spooldir
AC_ARG_WITH(spooldir, 
			[  --with-spooldir         directory to keep queues under],
			[ if test "x$withval" != "xno" ; then
					    AC_DEFINE(HAVE_SPOOL_DIR)
						AC_DEFINE_UNQUOTED(SPOOL_DIR,"$withval")
						MAKE_SPOOL_DIR=$withval
						AC_SUBST(MAKE_SPOOL_DIR)
			   fi
			]
)


dnl Checks for the Configdir
AC_ARG_WITH(sysconfdir, 
			[  --with-sysconfdir       directory to store the configs under],
			[ if test "x$withval" != "xno" ; then
					    AC_DEFINE(HAVE_ETC_DIR)
						AC_DEFINE_UNQUOTED(ETC_DIR, "$withval")
						MAKE_ETC_DIR=$withval
						AC_SUBST(MAKE_ETC_DIR)
			  fi
			]
)


dnl Checks for the run-dir for the sockets
AC_ARG_WITH(rundir, 
			[  --with-rundir           directory to store the configs under],
			[ if test "x$withval" != "xno" ; then
					    AC_DEFINE(HAVE_RUN_DIR)
						AC_DEFINE_UNQUOTED(RUN_DIR, "$withval")
						MAKE_RUN_DIR=$withval
						AC_SUBST(MAKE_RUN_DIR)
			  fi
			]
)


dnl Checks for the Pseudo Random Generator sockets TODO: this keeps being default.
AC_DEFINE_UNQUOTED(EGD_POOL, "/var/run/egd-pool")
AC_ARG_WITH(egdpool, 
			[  --with-egdpool          the socket from Pseudo Random Generator, defaults to /var/run/egd-pool],
			[ if test "x$withval" != "xno" ; then
						AC_DEFINE_UNQUOTED(EGD_POOL, "$withval")
			  fi
			]
)


AC_ARG_WITH(docdir,
			[  --with-docdir           where to install the documentation. default: /usr/local/citadel/],
			[ if test "x$withval" != "xno" ; then
					MAKE_DOC_DIR=$withval
					AC_SUBST(MAKE_DOC_DIR)
			  fi
			]
)



AC_ARG_ENABLE(chkpwd, [  --disable-chkpwd        don't build 'chkpwd'])

AC_ARG_ENABLE(threaded-client, [  --disable-threaded-client
			  disable multithreaded client])

AC_ARG_ENABLE(pie, [  --enable-pie            build position-independent executables])

AC_ARG_WITH(pam, [  --with-pam              use PAM if present (see PAM.txt before you try this)])
AC_ARG_WITH(kthread, [  --with-kthread          use kernel threads (on FreeBSD) (not recommended yet)])
AC_ARG_WITH(db, [  --with-db@<:@=DIR@:>@         use Sleepycat DB 3.x @<:@DIR=/usr/local/BerkeleyDB.3.@<:@123@:>@@:>@])
AC_ARG_WITH(ssl,
	[  --with-ssl=PATH         Specify path to OpenSSL installation ],
	[
		if test "x$withval" != "xno" ; then
			tryssldir=$withval
		fi
	]
)
AC_ARG_WITH(ncurses, [  --without-ncurses       don't use ncurses])

AC_ARG_WITH(with_zlib, [  --with-zlib             use zlib compression if present])
AC_ARG_WITH(with_ldap, [  --with-ldap             use OpenLDAP client library])
AC_ARG_WITH(with_libical, [  --with-libical          use libical calendaring library])
AC_ARG_WITH(with_libsieve, [  --with-libsieve         use libsieve mail sorting library])

if test "x$with_db" != xno -a "x$with_db" != xyes -a "$with_db"; then
	db_dir="$with_db"
	with_db=yes
else
	test -f /usr/local/lib/libdb.a -o -f /usr/local/lib/libdb.so \
	  -o -f /usr/local/lib/libdb4.a -o -f /usr/local/lib/libdb4.so \
	  && db_dir=/usr/local

	test -d /usr/local/BerkeleyDB.4.1 && db_dir=/usr/local/BerkeleyDB.4.1
	test -d /usr/local/BerkeleyDB.4.2 && db_dir=/usr/local/BerkeleyDB.4.2
	test -d /usr/local/BerkeleyDB.4.3 && db_dir=/usr/local/BerkeleyDB.4.3
fi

dnl By default, we only build the client (citadel and whobbs) unless we can
dnl figure out how to build with POSIX threads.
TARGETS=client

AC_CANONICAL_HOST
PTHREAD_DEFS=-D_REENTRANT
AC_MSG_CHECKING([how to compile with POSIX threads])
case "$host" in
	dnl BSDI 3.0 wants relocatable object modules instead of shared libs
	dnl for dlopen(), and has a wrapper script to link with shared libs.
	dnl Also has stupid non-reentrant gethostbyaddr() and friends.
	*-*-bsdi[123]*)
		test -z "$CC" -a -x /usr/bin/shlicc2 && CC=shlicc2
		AC_DEFINE(HAVE_NONREENTRANT_NETDB)
		AC_MSG_RESULT([Old BSDI])
	;;
	*-*-bsdi*)
		AC_DEFINE(HAVE_NONREENTRANT_NETDB)
		AC_MSG_RESULT([BSD/OS])
	;;
	dnl Curses support on Mac OS X is kind of screwed at the moment.
	*-*-darwin*)
		AC_DEFINE(DISABLE_CURSES)
		AC_MSG_RESULT([Mac OS X])
	;;
	dnl Digital Unix has an odd way to build for pthreads, and we can't
	dnl build pthreads programs with gcc due to header problems.
	alpha*-dec-osf*)
		test -z "$CC" && CC=cc
		LIBS="-lpthread -lexc $LIBS"
		check_pthread=no
		AC_MSG_RESULT([Tru64 or Digital UNIX])
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
		AC_MSG_RESULT([FreeBSD])
	;;
	*-*-openbsd*)
		LIBS="-pthread $LIBS"
		check_pthread=no
		PTHREAD_DEFS=-pthread
		AC_MSG_RESULT([OpenBSD])
	;;
	*-*-linux*)
		PTHREAD_DEFS="-D_REENTRANT -pthread"
		AC_MSG_RESULT([Linux])
	;;
	*-*-solaris*)
		PTHREAD_DEFS="-D_REENTRANT -D_PTHREADS"
		AC_MSG_RESULT([Solaris])
	;;
	*-*-cygwin*)
		SERVER_LDFLAGS="-Wl,-subsystem,windows"
		AC_MSG_RESULT([Cygwin])
	;;
	*)
		AC_MSG_RESULT([default])
	;;
esac
dnl DEFS="$DEFS $PTHREAD_DEFS"


dnl Checks for programs.
AC_PROG_CC

dnl Set up system-dependent compiler flags.
if test "$GCC" = yes; then
	if test "$CC" = icc; then
		CFLAGS="$CFLAGS -w1"
	else
		case "$host" in
			*-*-solaris*|alpha*-dec-osf*)
				CFLAGS="$CFLAGS -Wall -Wcast-qual -Wcast-align -Wno-char-subscripts $PTHREAD_DEFS"
			;;
			*)
			CFLAGS="$CFLAGS -Wall -Wcast-qual -Wcast-align -Wstrict-prototypes $PTHREAD_DEFS"
			;;
		esac
	fi
fi

if test "x$enable_pie" = xyes; then
	save_CFLAGS="$CFLAGS"
	save_LDFLAGS="$LDFLAGS"
	CFLAGS="$CFLAGS -fpie"
	LDFLAGS="$LDFLAGS -pie -fpie"
	AC_CACHE_CHECK([whether compiler accepts -pie -fpie], ac_cv_pie_fpie,
	[AC_TRY_LINK([], [],
	ac_cv_pie_fpie=yes, ac_cv_pie_fpie=no)])
	if test $ac_cv_pie_fpie = no; then
		CFLAGS="$save_CFLAGS"
		LDFLAGS="$save_LDFLAGS"
	fi
fi

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

dnl Determine the system's authentication capabilities.
dnl We currently support PAM, standard getpwnam(), and getspnam()
dnl (Linux shadow passwords)

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
		if test "$enable_chkpwd" != no; then
			AC_DEFINE(ENABLE_CHKPWD)
			CHKPWD=chkpwd
		else
			AUTH=auth.lo
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

test -d /usr/kerberos/include && CPPFLAGS="$CPPFLAGS -I/usr/kerberos/include"

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

if test "x$with_db" != xno; then
	test "$db_dir" && LDFLAGS="$LDFLAGS -L$db_dir/lib"

	dblib=""

	if test -d "$db_dir/include/db4"; then
		CPPFLAGS="$CPPFLAGS -I$db_dir/include/db4"
		dblib="db4"
	elif test "$db_dir"; then
		CPPFLAGS="$CPPFLAGS -I$db_dir/include"
	elif test -d /usr/include/db4; then
		CPPFLAGS="$CPPFLAGS -I/usr/include/db4"
		dblib="db4"
	fi


	AC_CHECK_DB([db db-4.1 db-4 db4],
		    [
		     DATABASE=database_sleepycat.c
		    ],
		    AC_MSG_ERROR([[Can not locate a suitable Berkeley DB
				  library.  Use --with-db=PATH to specify
				  the path]]))

fi

dnl Checks for the zlib compression library.
if test "x$with_zlib" != xno ; then
	AC_CHECK_HEADERS(zlib.h,
		[AC_CHECK_LIB(z, zlibVersion,
			[ok_zlib=yes],,
	)])
fi

if test "x$ok_zlib" = xyes ; then
	SERVER_LIBS="-lz $SERVER_LIBS"
	AC_DEFINE(HAVE_ZLIB)
fi




dnl Checks for the OpenLDAP client library.
if test "x$with_ldap" != xno ; then
	AC_CHECK_HEADERS(ldap.h,
		[AC_CHECK_LIB(ldap, ldap_init,
			[ok_ldap=yes],,
	)])
fi

if test "x$ok_ldap" = xyes ; then
	SERVER_LIBS="-lldap $SERVER_LIBS"
	AC_DEFINE(HAVE_LDAP)
fi





dnl Checks for the libsieve mailbox sorting library.
if test "x$with_libsieve" != xno ; then
	AC_CHECK_HEADERS(sieve2.h,
		[AC_CHECK_LIB(sieve, sieve2_license,
			[ok_libsieve=yes],,
	)])
fi

if test "x$ok_libsieve" = xyes ; then
	SERVER_LIBS="-lsieve $SERVER_LIBS"
	AC_DEFINE(HAVE_LIBSIEVE)
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
        int major, minor, v;
        sscanf(ICAL_VERSION, "%d.%d", &major, &minor);
        v = 100*major + minor;
        printf("libical version: %i\n", v);
        if (v >= 24) {
                printf("This version is ok.\n");
		return(0);
	}
	printf("libical 0.24 or newer required.\n");
	printf("Citadel will be built without calendar support.\n");
	return(1);
}
		], 
		[
			SERVER_LIBS="-lical $SERVER_LIBS"
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
AC_CHECK_HEADERS(curses.h dl.h fcntl.h limits.h malloc.h termios.h sys/ioctl.h sys/select.h sys/time.h sys/prctl.h syslog.h unistd.h utmp.h utmpx.h paths.h db.h db4/db.h pthread.h netinet/in.h arpa/nameser.h arpa/nameser_compat.h)

dnl
dnl need to check if this is actually compilable, sometimes there are
dnl conflicts with the system's headers (eg solaris xpg4 style headers)
dnl
if test "x$with_ncurses" != xno; then
	AC_CHECK_HEADERS(ncurses.h,,,[ ])
fi

AC_CHECK_HEADER(resolv.h,AC_DEFINE(HAVE_RESOLV_H),,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif])


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
AC_SUBST(SERVER_LDFLAGS)
AC_SUBST(SERVER_LIBS)
AC_SUBST(SETUP_LIBS)
AC_CONFIG_HEADER(sysdep.h)
AC_CONFIG_FILES([Makefile])
AC_OUTPUT(database_cleanup.sh)

if test -z "$DATABASE"; then
	AC_MSG_WARN([No database driver was found. Please install Berkeley DB.])
fi

echo ------------------------------------------------------------------------
echo 'zlib compression:                ' $ok_zlib
echo 'Calendar support:                ' $ok_libical
echo 'LDAP support:                    ' $ok_ldap
echo 'Sieve mailbox filtering support: ' $ok_libsieve
echo 
echo 'Note: if you are not using Linux, make sure you are using GNU make'
echo '(gmake) to compile Citadel.'
echo

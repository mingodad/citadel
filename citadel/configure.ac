dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT([Citadel], [8.13], [http://www.citadel.org/])
AC_REVISION([$Revision: 5108 $])
AC_CONFIG_SRCDIR([citserver.c])
AC_CONFIG_HEADER(sysdep.h)
AC_CONFIG_MACRO_DIR([m4])
AC_PREFIX_DEFAULT(/usr/local/citadel)
if test "$prefix" = NONE; then
	AC_DEFINE_UNQUOTED(CTDLDIR, "$ac_default_prefix", [define this to the Citadel home directory])
	ssl_dir="$ac_default_prefix/keys"
	localedir=$ac_default_prefix
else
	AC_DEFINE_UNQUOTED(CTDLDIR, "$prefix", [define this to the Citadel home directory])
	ssl_dir="$prefix/keys"
	localedir=$prefix
fi

dnl Make sure we see all GNU and Solaris extensions.
AC_GNU_SOURCE


dnl Checks for the Datadir
AC_ARG_WITH(datadir, 
		    [  --with-datadir          directory to store the databases under],
			[ if test "x$withval" != "xno" ; then
					      AC_DEFINE(HAVE_DATA_DIR,[],[use alternate database location?])
					      AC_DEFINE_UNQUOTED(DATA_DIR, "$withval",[define, if the user suplied a data-directory to use.])
					      MAKE_DATA_DIR=$withval
					      AC_SUBST(MAKE_DATA_DIR)
			  fi
			]
)


dnl Checks for the helpDatadir
AC_ARG_WITH(helpdir, 
		    [  --with-helpdir          directory to store the helpfiles under],
			[ if test "x$withval" != "xno" ; then
					      AC_DEFINE(HAVE_HELP_DIR,[],[use alternate database location?])
					      AC_DEFINE_UNQUOTED(HELP_DIR, "$withval",[define, if the user suplied a helpfile-directory to use.])
					      MAKE_HELP_DIR=$withval
					      AC_SUBST(MAKE_HELP_DIR)
			  fi
			]
)

dnl Checks for the Static Datadir
AC_ARG_WITH(staticdatadir, 
		    [  --with-staticdatadir    directory to store citadels system messages under],
			[ if test "x$withval" != "xno" ; then
						  AC_DEFINE(HAVE_STATICDATA_DIR, [], [should we activate an alternate static text location?])
						  AC_DEFINE_UNQUOTED(STATICDATA_DIR, "$withval", [where do we put our static text data?])
						  MAKE_STATICDATA_DIR=$withval
						  AC_SUBST(MAKE_STATICDATA_DIR)
			  fi
			]
)


dnl Checks for the SSLdir
dnl this is a bit different than the rest, 
dnl because of the citadel used to have a keys/ subdir.
AC_ARG_WITH(ssldir, 
		    [  --with-ssldir           directory to store the ssl certificates under],
			[ if test "x$withval" != "xno" ; then
					      
						  ssl_dir="$withval"
			  fi
			AC_SUBST(MAKE_SSL_DIR)
			]
)
AC_DEFINE_UNQUOTED(SSL_DIR, "$ssl_dir", [were should we put our keys?])


dnl Checks for the spooldir
AC_ARG_WITH(spooldir, 
			[  --with-spooldir         directory to keep queues under],
			[ if test "x$withval" != "xno" ; then
						AC_DEFINE(HAVE_SPOOL_DIR, [], [enable alternate spool dir?])
						AC_DEFINE_UNQUOTED(SPOOL_DIR,"$withval", [where do we place our spool dirs?])
						MAKE_SPOOL_DIR=$withval
						AC_SUBST(MAKE_SPOOL_DIR)
			   fi
			]
)


dnl Checks for the Configdir
AC_ARG_WITH(sysconfdir, 
			[  --with-sysconfdir       directory to store the configs under],
			[ if test "x$withval" != "xno" ; then
					    AC_DEFINE(HAVE_ETC_DIR, [], [should we search our system config in an alternate place?])
						AC_DEFINE_UNQUOTED(ETC_DIR, "$withval", [where to search our config files])
						MAKE_ETC_DIR=$withval
						AC_SUBST(MAKE_ETC_DIR)
			  fi
			]
)

dnl Checks for the Configdir
AC_ARG_WITH(autosysconfdir, 
			[  --with-autosysconfdir	 directory to store the automaticaly maintained configs under],
			[ if test "x$withval" != "xno" ; then
					    AC_DEFINE(HAVE_AUTO_ETC_DIR, [], [should we search our automatic config in an alternate place?])
						AC_DEFINE_UNQUOTED(AUTO_ETC_DIR, "$withval", [where to search our automatic config files])
						MAKE_AUTO_ETC_DIR=$withval
						AC_SUBST(MAKE_AUTO_ETC_DIR)
			  fi
			]
)

dnl Checks for where to put our utilities
AC_ARG_WITH(utility-bindir, 
			[  --with-utility-bindir   directory where to find helper binaries],
			[ if test "x$withval" != "xno" ; then
					    AC_DEFINE(HAVE_UTILBIN_DIR,[],[should we put our helper binaries to another location?])
						AC_DEFINE_UNQUOTED(UTILBIN_DIR, "$withval", [were to put our helper programs])
						MAKE_UTILBIN_DIR=$withval
						AC_SUBST(MAKE_UTILBIN_DIR)
			  fi
			]
)


dnl Checks for the run-dir for the sockets
AC_ARG_WITH(rundir, 
			[  --with-rundir           directory to place runtime files (UDS) to?],
			[ if test "x$withval" != "xno" ; then
					    AC_DEFINE(HAVE_RUN_DIR, [], [should we put our non volatile files elsewhere?])
						AC_DEFINE_UNQUOTED(RUN_DIR, "$withval", [define, where the config should go in unix style])
						MAKE_RUN_DIR=$withval
						AC_SUBST(MAKE_RUN_DIR)
			  fi
			]
)


dnl Checks for the Pseudo Random Generator sockets TODO: this keeps being default.
AC_DEFINE_UNQUOTED(EGD_POOL, "/var/run/egd-pool", [place to keep our pseudo random generator file])
AC_ARG_WITH(egdpool, 
			[  --with-egdpool          the socket from Pseudo Random Generator, defaults to /var/run/egd-pool],
			[ if test "x$withval" != "xno" ; then
						AC_DEFINE_UNQUOTED(EGD_POOL, "$withval", [the socket from Pseudo Random Generator])
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

dnl where to put the locale files
AC_ARG_WITH(localedir, 
		    [  --with-localedir          directory to put the locale files to],
			[ if test "x$withval" != "xno" ; then
			    localedir=$withval
			  fi
			]
)
AC_DEFINE_UNQUOTED(LOCALEDIR, "$localedir",[where to find our pot files])
LOCALEDIR=$localedir
AC_SUBST(LOCALEDIR)



dnl Checks for the zlib compression library.
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $SERVER_LIBS"
AC_CHECK_HEADER(zlib.h,
	[AC_CHECK_LIB(z, zlibVersion,
		[
			LIBS="-lz $LIBS $SERVER_LIBS"
		],
		[
			AC_MSG_ERROR(zlib was not found or is not usable.  Please install zlib.)
		]
	,
	)],
	[
		AC_MSG_ERROR(zlib.h was not found or is not usable.  Please install zlib.)
	]
)
CFLAGS="$saved_CFLAGS"

dnl Here is the check for a libc integrated iconv
AC_ARG_ENABLE(iconv,
	[  --disable-iconv         do not use iconv charset conversion],
	ok_iconv=no, ok_iconv=yes)

AC_MSG_CHECKING(Checking to see if your system supports iconv)
AC_TRY_RUN([
 	#include <iconv.h>
 	main() {
 		iconv_t ic = (iconv_t)(-1) ;
 		ic = iconv_open("UTF-8", "us-ascii");
 		iconv_close(ic);
 		exit(0);
 	}
 ],
		[
		  ok_iconv=yes
		  AC_MSG_RESULT([yes])
		],
		[ 
		  ok_iconv=no
		  AC_MSG_RESULT([no])
		]
)

dnl Check for iconv in external libiconv
if test "$ok_iconv" = no; then
	AC_MSG_CHECKING(Checking for an external libiconv)
	OLD_LDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS -liconv"
	AC_TRY_RUN([
			#include <iconv.h>
			main() {
				iconv_t ic = (iconv_t)(-1) ;
				ic = iconv_open("UTF-8", "us-ascii");
				iconv_close(ic);
			}
		],
			[
			  ok_iconv=yes
			  AC_MSG_RESULT([yes])
			],
			[ 
			  ok_iconv=no
			  LDFLAGS="$OLD_LDFLAGS"
			  AC_MSG_RESULT([no])
			]
		)
fi	
if test "$ok_iconv" != "no"; then
	AC_MSG_RESULT(Citadel will be built with character set conversion.)
	AC_DEFINE(HAVE_ICONV,[],[whether we have iconv for charset conversion])
else
	AC_MSG_RESULT(Citadel will be built without character set conversion.)
fi

AC_CHECK_LIB(intl, libintl_bindtextdomain, [LDFLAGS="$LDFLAGS -lintl"])

AC_ARG_ENABLE(pie, [  --enable-pie            build position-independent executables])

AC_ARG_WITH(pam, [  --with-pam              use PAM if present (see PAM.txt before you try this)])
AC_ARG_WITH(kthread, [  --with-kthread          use kernel threads (on FreeBSD) (not recommended yet)])
AC_ARG_WITH(db, [  --with-db@<:@=DIR@:>@         use Berkeley DB 3.x @<:@DIR=/usr/local/BerkeleyDB.3.@<:@123@:>@@:>@])
AC_ARG_WITH(ssl,
	[  --with-ssl=PATH         Specify path to OpenSSL installation ],
	[
		if test "x$withval" != "xno" ; then
			tryssldir=$withval
		fi
	]
)
AC_ARG_WITH(with_ldap, [  --with-ldap             use OpenLDAP client library])
dnl AC_ARG_WITH(with_libdspam, [  --with-libdspam         use libdspam mail spam scanning library])
AC_ARG_WITH(with_gc, [  --with-gc             use the Boehm-Demers-Weiser garbage collection library])

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
	test -d /usr/local/BerkeleyDB.4.4 && db_dir=/usr/local/BerkeleyDB.4.4
	test -d /usr/local/BerkeleyDB.4.5 && db_dir=/usr/local/BerkeleyDB.4.5
fi

AC_CANONICAL_HOST
PTHREAD_DEFS=-D_REENTRANT
AC_MSG_CHECKING([how to compile with POSIX threads])
case "$host" in
	dnl BSDI 3.0 wants relocatable object modules instead of shared libs
	dnl for dlopen(), and has a wrapper script to link with shared libs.
	dnl Also has stupid non-reentrant gethostbyaddr() and friends.
	*-*-bsdi[123]*)
		test -z "$CC" -a -x /usr/bin/shlicc2 && CC=shlicc2
		AC_DEFINE(HAVE_NONREENTRANT_NETDB,[], [define this if the OS has broken non-reentrant gethostby{name,addr}() ])
		AC_MSG_RESULT([Old BSDI])
	;;
	*-*-bsdi*)
		AC_DEFINE(HAVE_NONREENTRANT_NETDB, [], [define this if the OS has broken non-reentrant gethostby{name,addr}() ])
		AC_MSG_RESULT([BSD/OS])
	;;
	dnl Curses support on Mac OS X is kind of screwed at the moment.
	dnl TCP buffering isn't ideal under OS X. This define should also be
	dnl checked in other cases of OS X-Linux differences.
	*-*-darwin*)
		AC_DEFINE(HAVE_DARWIN, [], [define if using OS X/Darwin])
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
			CFLAGS="$CFLAGS -Wall -Wcast-qual -Wcast-align -Wstrict-prototypes -Wno-strict-aliasing $PTHREAD_DEFS"
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

AC_MSG_CHECKING([how to create dependancy checks])
		 if test -n "`$CC -V 2>&1 |grep Sun`"; then 
		     	DEPEND_FLAG=-xM;
		 else 
		        DEPEND_FLAG=-M
	         fi
AC_SUBST(DEPEND_FLAG)


AC_PROG_INSTALL
AC_PROG_YACC
AC_PATH_PROG(DIFF,diff)
AC_PATH_PROG(PATCH,patch)
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)

dnl Checks for system services.

dnl Check the size of various builtin types; see typesize.h (error)
AC_CHECK_SIZEOF(char, 0)
AC_CHECK_SIZEOF(short, 0)
AC_CHECK_SIZEOF(int, 0)
AC_CHECK_SIZEOF(long, 0)
AC_CHECK_SIZEOF(size_t, 0)
dnl AC_CHECK_SIZEOF(long long, 0)

dnl Checks for libraries.

dnl We want to test for the following in libc before checking for their
dnl respective libraries, because some systems (like Irix) have both, and the
dnl non-libc versions may be broken.
AC_CHECK_FUNCS(crypt gethostbyname connect flock getpwnam_r getpwuid_r getloadavg)

AC_CHECK_FUNCS(strftime_l uselocale gettext)

if test "$ok_nls" != "no"; then
	AC_CHECK_PROG(ok_xgettext, xgettext, yes, no)
	ok_nls=$ok_xgettext
fi

if test "$ok_nls" != "no"; then
	AC_CHECK_PROG(ok_msgmerge, msgmerge, yes, no)
	ok_nls=$ok_msgmerge
fi

if test "$ok_nls" != "no"; then
	AC_CHECK_PROG(ok_msgfmt, msgfmt, yes, no)
	ok_nls=$ok_msgfmt
fi

if test "$ok_nls" != "no"; then
	AC_MSG_RESULT(citadel will be built with national language support.)
	AC_DEFINE(ENABLE_NLS, [], [whether we have NLS support])
	PROG_SUBDIRS="$PROG_SUBDIRS po/citadel-setup"
else
	AC_MSG_RESULT(citadel will be built without national language support.)
fi

dnl disable backtrace if we don't want it.
AC_ARG_WITH(backtrace, 
		    [  --with-backtrace        enable backtrace dumps in the syslog],
			[ if test "x$withval" != "xno" ; then
			     CFLAGS="$CFLAGS  -rdynamic "
			     LDFLAGS="$LDFLAGS  -rdynamic "
			     SERVER_LDFLAGS="$SERVER_LDFLAGS  -rdynamic "
                             AC_CHECK_FUNCS(backtrace)
			  fi
			]
)

dnl disable backtrace if we don't want it.
AC_ARG_WITH(gprof, 
		    [  --with-gprof            enable profiling],
			[ if test "x$withval" != "xno" ; then
			     CFLAGS="$CFLAGS  -pg "
			     LDFLAGS="$LDFLAGS  -pg "
			     SERVER_LDFLAGS="$SERVER_LDFLAGS  -pg "
			  fi
			]
)

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

save_LIBS=$LIBS
AC_CHECK_LIB(pam, pam_start, [chkpwd_LIBS="-lpam $chkpwd_LIBS"
	LIBS="-lpam $LIBS"])
AC_CHECK_FUNCS(pam_start)
test "$enable_chkpwd" != no && LIBS=$save_LIBS
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

AC_CHECK_FUNCS(vw_printw wcolor_set resizeterm wresize)

dnl Check for libpthread(s) if we're not using Digital UNIX or FreeBSD. (On
dnl which the -pthread flag takes care of this.)
if test "$check_pthread" != no; then
	AC_CHECK_LIB(pthread, pthread_create)
	AC_CHECK_LIB(pthreads, pthread_create)
fi

test -d /usr/kerberos/include && CPPFLAGS="$CPPFLAGS -I/usr/kerberos/include"

dnl Checks for the libical iCalendar library.
AC_CHECK_HEADER(libical/ical.h,
	[AC_CHECK_LIB(ical, icaltimezone_set_tzid_prefix,
		[
			SERVER_LIBS="-lical $SERVER_LIBS"
		],
		[
			AC_MSG_ERROR(libical was not found and is required.  More info: http://www.citadel.org/doku.php/installation:start)
		]
	,
	)],
	[
		AC_MSG_ERROR(libical/ical.h was not found and is required.  More info: http://www.citadel.org/doku.php/installation:start)
	]
)


dnl Checks for the libsieve mailbox sorting library.
AC_CHECK_HEADER(sieve2.h,
	[AC_CHECK_LIB(sieve, sieve2_license,
		[
			SERVER_LIBS="-lsieve $SERVER_LIBS"
		],
		[
			AC_MSG_ERROR(libsieve was not found and is required.  More info: http://www.citadel.org/doku.php/installation:start)
		]
	,
	)],
	[
		AC_MSG_ERROR(sieve2.h was not found and is required.  More info: http://www.citadel.org/doku.php/installation:start)
	]
)

saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $SERVER_LIBS"
dnl Check for libcitadel
AC_CHECK_HEADER(libcitadel.h,
	[AC_CHECK_LIB(citadel, libcitadel_version_string,
		[
			LIBS="-lcitadel $LIBS $SERVER_LIBS"
			chkpwd_LIBS="-lcitadel $chkpwd_LIBS"
		],
		[
			AC_MSG_ERROR(libcitadel was not found or is not usable.  Please install libcitadel.)
		]
	,
	)],
	[
		AC_MSG_ERROR(libcitadel.h was not found or is not usable.  Please install libcitadel.)
	]
)

CFLAGS="$saved_CFLAGS"


AC_CHECK_LIB(cares, ares_parse_mx_reply,
            [
                    C_ARES_LIBS=-lcares
		    AC_DEFINE(HAVE_C_ARES, 1, [Define to use c-ares library])
                    have_good_c_ares=yes
            ],, $SOCKET_LIBS $NSL_LIBS
)



saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $SERVER_LIBS"
dnl Check for c-ares
AC_CHECK_HEADER(ares.h,
	[AC_CHECK_LIB(cares, ares_parse_mx_reply,
		[
			LIBS="-lcares $LIBS $SERVER_LIBS"
		],
		[
			AC_MSG_ERROR(libc-ares was not found or is not usable.  Please install libc-ares.)
		]
	)],
	[
		AC_MSG_ERROR(ares.h was not found or is not usable.  Please install libc-ares.)
	]
)
CFLAGS="$saved_CFLAGS"

saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $SERVER_LIBS"
dnl Check for libev
AC_CHECK_HEADER(ev.h,
	[AC_TRY_COMPILE([#include <math.h>
#include <ev.h>], 
                         [
			 ev_cleanup abort_by_shutdown;
			 struct ev_loop *event_base;
			 ev_cleanup_start(event_base, &abort_by_shutdown);
			],
			[
			LIBS="-lev -lm $LIBS $SERVER_LIBS"
			],
		[
			AC_MSG_ERROR(libev was not found or is not usable.  Please install libev.)
		])
	],
	[
		AC_MSG_ERROR(ev.h was not found or is not usable.  Please install libev.)
	]
)
CFLAGS="$saved_CFLAGS"


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
		AC_DEFINE(HAVE_OPENSSL, [], [Define if you have OpenSSL.])
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
		     DATABASE=database.c
		    ],
		    AC_MSG_ERROR([[Can not locate a suitable Berkeley DB
				  library.  Use --with-db=PATH to specify
				  the path]]))

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
	AC_DEFINE(HAVE_LDAP, [], [define this if you have OpenLDAP client available])
fi






dnl Checks for the libdspam mail spam scanning library.
dnl if test "x$with_libdspam" != xno ; then
dnl 	AC_CHECK_HEADERS(dspam/libdspam.h,
dnl 		[AC_CHECK_LIB(dspam, dspam_init,
dnl 			[ok_libdspam=yes],,
dnl 	)])
dnl fi
dnl 
dnl if test "x$ok_libdspam" = xyes ; then
dnl 	SERVER_LIBS="-ldspam $SERVER_LIBS"
dnl 	AC_DEFINE(HAVE_LIBDSPAM, [], [(unfinished) define this if you have the libdspam mail spam scanning library available])
dnl fi



dnl Checks for the Expat XML parser.
AC_CHECK_HEADER(expat.h,
	[AC_CHECK_LIB(expat, XML_ParserCreateNS,
		[
			SERVER_LIBS="-lexpat $SERVER_LIBS"
		],
		[
			AC_MSG_ERROR(The Expat XML parser was not found and is required.  More info: http://www.citadel.org/doku.php/installation:start)
		]
	,
	)],
	[
		AC_MSG_ERROR(expat.h was not found and is required.  More info: http://www.citadel.org/doku.php/installation:start)
	]
)







dnl Checks for libcurl.
AC_CHECK_HEADER(curl/curl.h,
	[AC_CHECK_LIB(curl, curl_version,
		[
			SERVER_LIBS="-lcurl $SERVER_LIBS"
		],
		[
			AC_MSG_ERROR(libcurl was not found and is required.  More info: http://www.citadel.org/doku.php/installation:start)
		]
	,
	)],
	[
		AC_MSG_ERROR(curl/curl.h was not found and is required.  More info: http://www.citadel.org/doku.php/installation:start)
	]
)










dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT

dnl
dnl TODO: for the DB header checks, we should check whether the headers
dnl define db_env_create, somehow
dnl
AC_CHECK_HEADERS(dl.h fcntl.h limits.h malloc.h termios.h sys/ioctl.h sys/select.h sys/stat.h sys/time.h sys/prctl.h syslog.h unistd.h utmp.h utmpx.h paths.h db.h db4/db.h pthread.h netinet/in.h arpa/nameser.h arpa/nameser_compat.h syscall.h sys/syscall.h)

AC_CHECK_HEADER(resolv.h,AC_DEFINE(HAVE_RESOLV_H, [], [define this if you have the resolv.h header file.]),,
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
	AC_DEFINE(HAVE_UT_TYPE, [], [define this if struct utmp has an ut_type member])
fi

AC_CACHE_CHECK(
	[for call semantics from getpwuid_r], 
	ac_cv_call_getpwuid_r,
	[AC_TRY_COMPILE([#include <sys/types.h>
#include <pwd.h>], 
                         [
				struct passwd pw, *pwp;
				char pwbuf[64];
				uid_t uid;

				getpwuid_r(uid, &pw, pwbuf, sizeof(pwbuf), &pwp);
			],
			ac_cv_call_getpwuid_r=yes, 
			ac_cv_call_getpwuid_r=no)
	])

if test $ac_cv_call_getpwuid_r = no; then
	AC_DEFINE(SOLARIS_GETPWUID,[],[do we need to use solaris call syntax?])
	AC_DEFINE(F_UID_T, "%ld", [whats the matching format string for uid_t?])
	AC_DEFINE(F_PID_T, "%ld",  [whats the matching format string for pid_t?])
	AC_DEFINE(F_XPID_T, "%lx",  [whats the matching format string for xpid_t?])
else
	AC_DEFINE(F_UID_T, "%d",  [whats the matching format string for uid_t?])
	AC_DEFINE(F_PID_T, "%d",  [whats the matching format string for pid_t?])
	AC_DEFINE(F_XPID_T, "%x",  [whats the matching format string for xpid_t?])
fi


dnl Our own happy little check for the resolver library.

case "`uname -a`" in 
 OpenBSD*)
	echo "we don't need to check for resolv on openbsd"  
	;;
 FreeBSD*)
	echo "we don't need to check for resolv on freeBSD"  
	;;
 *)
	test -f /usr/local/lib/libresolv.a && LDFLAGS="$LDFLAGS -L/usr/local/lib"
	AC_CHECK_LIB(resolv, res_query,
		RESOLV="$RESOLV -lresolv",
		[dnl Have to include resolv.h as res_query is sometimes defined as a macro
			AC_MSG_CHECKING([for res_query in -lresolv (with resolv.h if present)])
 			saved_libs="$LIBS"
 			LIBS="-lresolv $LIBS"
 			AC_TRY_LINK([
 				#ifdef HAVE_RESOLV_H
 				#include <resolv.h>
 				#endif],
 				[res_query(0,0,0,0,0)],
 				[AC_MSG_RESULT(yes)
 						have_res_query=yes],
 				[AC_MSG_RESULT(no)
					AC_MSG_ERROR(libresolv was not found.  Citadel requires the resolver library.)
 				])
         ]
	)
	;;
esac



AC_CACHE_CHECK([for ut_host in struct utmp], ac_cv_struct_ut_host,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <utmp.h>], [struct utmp ut; ut.ut_host;],
ac_cv_struct_ut_host=yes, ac_cv_struct_ut_host=no)])
if test $ac_cv_struct_ut_host = yes; then
	AC_DEFINE(HAVE_UT_HOST, [], [define this if struct utmp has an ut_host member])
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
	AC_DEFINE(HAVE_PTHREAD_CANCEL, [], [define this if you have the pthread_cancel() function])
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
	test "$DATABASE" && TARGETS="server utils"
fi

AC_REPLACE_FUNCS(snprintf getutline)

AC_CACHE_CHECK([the weather], ac_cv_weather, [
	sleep 1
	echo $ECHO_N "opening your window... $ECHO_C" >&6
	sleep 2
	month=`date | cut -f 2 -d ' '`
	case $month in
	Dec | Jan | Feb)
		ac_cv_weather="it's cold!"
		;;
	Mar | Apr)
		ac_cv_weather="it's wet!"
		;;
	Jul | Aug)
		ac_cv_weather="it's hot!"
		;;
	Oct | Nov)
		ac_cv_weather="it's cool"
		;;
	May | Jun | Sep | *)
		ac_cv_weather="it's fine"
		;;
	esac
	])




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


STRUCT_UCRED

dnl Done! Now write the Makefile and sysdep.h
AC_SUBST(RESOLV)
AC_SUBST(chkpwd_LIBS)
AC_SUBST(TARGETS)
AC_SUBST(DATABASE)
AC_SUBST(SERVER_LDFLAGS)
AC_SUBST(SERVER_LIBS)
AC_SUBST(SETUP_LIBS)
AC_SUBST(DIFF)
AC_SUBST(PATCH)
AC_CONFIG_FILES([Makefile])
AC_OUTPUT(database_cleanup.sh po/citadel-setup/Makefile)

if test -z "$DATABASE"; then
	AC_MSG_WARN([No database driver was found. Please install Berkeley DB.])
fi

abs_srcdir="`cd $srcdir && pwd`"
abs_builddir="`pwd`"
if test "$abs_srcdir" != "$abs_builddir"; then
   ln -sf $abs_srcdir/include $abs_builddir
   ln -sf $abs_srcdir/Make_sources $abs_builddir
   ln -sf $abs_srcdir/Make_modules $abs_builddir
   for i in $abs_srcdir/*.h ; do
      if test "$abs_srcdir/sysdep.h" != "$i"; then
         ln -sf $i $abs_builddir
      fi
   done
   for d in `/bin/ls $abs_srcdir/modules/`; do 
	(mkdir -p $abs_builddir/modules/$d) 
   done
   if test -d "$abs_srcdir/user_modules/"; then
      for d in `/bin/ls $abs_srcdir/user_modules/`; do 
      	(mkdir -p $abs_builddir/user_modules/$d) 
   	done
   fi
   mkdir -p $abs_builddir/utils
   mkdir -p $abs_builddir/utillib
fi

if test -n "$srcdir"; then 
   export srcdir=.
fi


echo ------------------------------------------------------------------------
echo 'LDAP support:                    ' $ok_ldap
echo 'Character set conversion support:' $ok_iconv
echo 'Boehm-Demers-Weiser support:     ' $ok_gc
dnl echo 'DSpam Scanning support:          ' $ok_libdspam
echo 
echo 'Note: if you are not using Linux, make sure you are using GNU make'
echo '(gmake) to compile Citadel.'
echo

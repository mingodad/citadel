dnl Process this file with autoconf to produce a configure script.
dnl $Id$
AC_INIT([WebCit], [9.01], [http://www.citadel.org/])


AC_SUBST(PROG_SUBDIRS)
AC_DEFINE(PROG_SUBDIRS, [], [Program dirs])
AC_CANONICAL_HOST
AC_PROG_INSTALL
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)

dnl Make sure we see all GNU and Solaris extensions.
AC_GNU_SOURCE

AC_CHECK_PROG(SED, sed, sed, no)
AC_PREFIX_DEFAULT(/usr/local/webcit)
if test "$prefix" = NONE; then
	AC_DEFINE_UNQUOTED(WEBCITDIR, "$ac_default_prefix", [define this to the Citadel home directory])
	ssl_dir="$ac_default_prefix/keys"
else
	AC_DEFINE_UNQUOTED(WEBCITDIR, "$prefix", [define this to the Citadel home directory])
	ssl_dir="$prefix/keys"
fi

AC_ARG_WITH(ssl,
	[  --with-ssl=PATH         Specify path to OpenSSL installation ],
	[
		if test "x$withval" != "xno" ; then
			tryssldir=$withval
		fi
	]
)
dnl Set some system-specific variables which are OK to set before compiler
dnl checks:
PTHREAD_DEFS=-D_REENTRANT
case "$host" in
	dnl Tru64 Unix requires we use -pthread instead of linking the threads
	dnl libraries directly, and we can't build threaded programs with gcc
	dnl due to header file problems.
	alpha*-dec-osf*)
		test -z "$CC" && CC=cc
		LIBS=-pthread
	;;
	dnl FreeBSD is similar:
	*-*-freebsd*)
		LIBS=-pthread
		PTHREAD_DEFS=-D_THREAD_SAFE
	;;
	*-*-solaris*)
		PTHREAD_DEFS='-D_REENTRANT -D_PTHREADS'
	;;
	*-*-darwin*)
		LIBS=-lintl
esac
AC_SUBST(PTHREAD_DEFS)

dnl Checks for programs.
AC_PROG_CC

dnl Configure compiler flags for GCC
if test "$GCC" = yes; then
	case "$host" in
		*-*-solaris*)
dnl			CFLAGS="$CFLAGS -Wall -Wno-char-subscripts --pedantic"
			CFLAGS="$CFLAGS -Wall -Wno-char-subscripts"
		;;
		*)
dnl			CFLAGS="$CFLAGS -Wall --pedantic"
			CFLAGS="$CFLAGS -Wall"
		;;
	esac
fi

# missing_dir=`cd $ac_aux_dir && pwd`
# AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)

dnl Checks for libraries.
if test "$LIBS" != -pthread; then
	AC_CHECK_LIB(pthread, pthread_create)
	AC_CHECK_LIB(pthreads, pthread_create)
fi
AC_SEARCH_LIBS(gethostbyname, nsl)
AC_SEARCH_LIBS(connect, socket)

dnl Checks for header files.
AC_HEADER_STDC
dnl AC_HEADER_SYS_WAIT

AC_CHECK_FUNCS(crypt gethostbyname connect flock getpwnam_r getpwuid_r getloadavg)
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
	AC_DEFINE(SOLARIS_LOCALTIME_R,[], [do we need to use soralis call syntax?])
	AC_DEFINE(F_UID_T, "%ld", [whats the matching format string for uid_t?])
	AC_DEFINE(F_PID_T, "%ld",  [whats the matching format string for pid_t?])
	AC_DEFINE(F_XPID_T, "%lx",  [whats the matching format string for xpid_t?])
else
	AC_DEFINE(F_UID_T, "%d",  [whats the matching format string for uid_t?])
	AC_DEFINE(F_PID_T, "%d",  [whats the matching format string for pid_t?])
	AC_DEFINE(F_XPID_T, "%x",  [whats the matching format string for xpid_t?])
fi



dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
dnl Check the size of various builtin types; see typesize.h (error)
AC_CHECK_SIZEOF(char, 0)
AC_CHECK_SIZEOF(short, 0)
AC_CHECK_SIZEOF(int, 0)
AC_CHECK_SIZEOF(long, 0)
AC_CHECK_SIZEOF(long unsigned int, 0)
AC_CHECK_SIZEOF(size_t, 0)
dnl AC_HEADER_TIME

dnl Checks for library functions.
AC_TYPE_SIGNAL
dnl AC_FUNC_VPRINTF
AC_REPLACE_FUNCS(snprintf)
AC_CHECK_HEADER(CUnit/CUnit.h, [AC_DEFINE(ENABLE_TESTS, [], [whether we should compile the test-suite])])

AC_CHECK_HEADERS(fcntl.h limits.h unistd.h iconv.h xlocale.h)

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
	AC_MSG_RESULT(webcit will be built with character set conversion.)
	AC_DEFINE(HAVE_ICONV,[],[whether we have iconv for charset conversion])
else
	AC_MSG_RESULT(webcit will be built without character set conversion.)
fi


AC_CHECK_LIB(intl, libintl_bindtextdomain, [LDFLAGS="$LDFLAGS -lintl"])


dnl Checks for the libical iCalendar library.
AC_CHECK_HEADER(libical/ical.h,
	[AC_CHECK_LIB(ical, icaltimezone_set_tzid_prefix,
		[
			LIBS="-lical $LIBS"
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

dnl Checks for the discount markdown library.
AC_CHECK_LIB(markdown,
	 markdown,
	 [
	 LIBS="$LIBS -lmarkdown"
	 AC_DEFINE(HAVE_MARKDOWN,[],[whether we have markdown message rendering])
])

dnl Check for libcitadel
AC_CHECK_HEADER(libcitadel.h,
	[AC_CHECK_LIB(citadel, libcitadel_version_string,
		[
			LIBS="-lcitadel $LIBS"
			SETUP_LIBS="-lcitadel $SETUP_LIBS"
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

AC_MSG_CHECKING([whether your system likes memcpy + HKEY]) 
AC_TRY_COMPILE([
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/types.h>

#include "lib/libcitadel.h"
],
[
	char foo[22];
	memcpy(foo, HKEY("foo"));
	
],
		[

		  AC_MSG_RESULT([yes])
		],
		[ 
		  AC_DEFINE(UNDEF_MEMCPY, [], [whether we need to undefine memcpy])
		  AC_MSG_RESULT([no])
		]
)

dnl Checks for the Expat XML parser.
AC_CHECK_HEADER(expat.h,
	[AC_CHECK_LIB(expat, XML_ParserCreateNS,
		[
			LIBS="-lexpat $LIBS"
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



found_ssl=no
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
		AC_DEFINE(HAVE_OPENSSL,[],[whethe we have openssl])
		found_ssl=yes
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
dnl Checks for the SSLdir
dnl this is a bit different than the rest, 
dnl because of the citadel used to have a keys/ subdir.
AC_ARG_WITH(ssldir, 
		    [  --with-ssldir          directory to store the ssl certificates under],
			[ if test "x$withval" != "xno" ; then
					      
						  ssl_dir="$withval"
						  if test "$found_ssl" = "no"; then
						     echo "Your setup was incomplete; ssldir doesn't make sense without openssl"
						     exit
						  fi
			  fi
			AC_SUBST(MAKE_SSL_DIR)
			]
)
AC_DEFINE_UNQUOTED(SSL_DIR, "$ssl_dir", [were should we put our keys?])




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
	AC_MSG_RESULT(WebCit will be built with national language support.)
	AC_DEFINE(ENABLE_NLS, [], [whether we have NLS support])
	PROG_SUBDIRS="$PROG_SUBDIRS po/webcit/"
else
	AC_MSG_RESULT(WebCit will be built without national language support.)
fi

AC_SUBST(SETUP_LIBS)

dnl disable backtrace if we don't want it.
AC_ARG_WITH(gprof, 
		    [  --with-gprof          enable profiling],
			[ if test "x$withval" != "xno" ; then
			     CFLAGS="$CFLAGS  -pg "
			     LDFLAGS="$LDFLAGS  -pg "
			  fi
			]
)

dnl disable backtrace if we don't want it.
AC_ARG_WITH(backtrace, 
		    [  --with-backtrace          enable backtrace dumps in the syslog],
			[ if test "x$withval" != "xno" ; then
			     CFLAGS="$CFLAGS  -rdynamic "
			     LDFLAGS="$LDFLAGS  -rdynamic "
                             AC_CHECK_FUNCS(backtrace)
			  fi
			]
)

if test "$prefix" = NONE; then
   	datadir=$ac_default_prefix
	localedir=$ac_default_prefix
	wwwdir=$ac_default_prefix
	rundir=$ac_default_prefix
	editordir=$ac_default_prefix/tiny_mce
	markdowneditordir=$ac_default_prefix/epic
	etcdir=$ac_default_prefix
else
	localedir=$prefix
	wwwdir=$prefix
	datadir=$prefix
	rundir=$prefix
	editordir=$prefix/tiny_mce
	markdowneditordir=$prefix/epic
	etcdir=$prefix
fi

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

dnl Checks where to put our templates
AC_ARG_WITH(wwwdir, 
		    [  --with-wwwdir             directory to put our templates],
			[ if test "x$withval" != "xno" ; then
				wwwdir=$withval
			  fi
			]
)
AC_DEFINE_UNQUOTED(WWWDIR, "$wwwdir", [where to find our templates and pics])
WWWDIR=$wwwdir
AC_SUBST(WWWDIR)


dnl Checks for the run-dir for the sockets
AC_ARG_WITH(rundir, 
			[  --with-rundir           directory to place runtime files (UDS) to?],
			[ if test "x$withval" != "xno" ; then
					    AC_DEFINE(HAVE_RUN_DIR, [], [should we put our non volatile files elsewhere?])
						
						rundir=$withval
						AC_SUBST(MAKE_RUN_DIR)
			  fi
			]
)
AC_DEFINE_UNQUOTED(RUNDIR, "$rundir", [define, where the config should go in unix style])

dnl Checks for the Datadir
AC_ARG_WITH(datadir, 
		    [  --with-datadir          directory to store the databases under],
			[ if test "x$withval" != "xno" ; then
					      datadir=$withval
			  fi
			]
)
AC_DEFINE_UNQUOTED(DATADIR, "$datadir",[define, if the user suplied a data-directory to use.])

dnl Checks where to put our editor
AC_ARG_WITH(editordir, 
		    [  --with-editordir             directory to put our editor],
			[ if test "x$withval" != "xno" ; then
				editordir=$withval
			  fi
			]
)
AC_DEFINE_UNQUOTED(EDITORDIR, "$editordir", [where to find our mail editor])

AC_ARG_WITH(markdowneditordir, 
		    [  --with-markdowneditordir             directory to put our markdown editor],
			[ if test "x$withval" != "xno" ; then
				markdowneditordir=$withval
			  fi
			]
)
AC_DEFINE_UNQUOTED(MARKDOWNEDITORDIR, "$markdowneditordir", [where to find our markdown editor])

dnl Checks where to find our configs
AC_ARG_WITH(etcdir, 
		    [  --with-etcdir             directory to read our configs],
			[ if test "x$withval" != "xno" ; then
				etcdir=$withval
			  fi
			]
)
AC_DEFINE_UNQUOTED(ETCDIR, "$etcdir", [where to find our configs])
ETCDIR=$etcdir
AC_SUBST(ETCDIR)

abs_srcdir="`cd $srcdir && pwd`"
abs_builddir="`pwd`"
if test "$abs_srcdir" != "$abs_builddir"; then
   CFLAGS="$CFLAGS -I $abs_builddir"
fi
AC_CONFIG_HEADER(sysdep.h)
AC_OUTPUT(Makefile po/webcit/Makefile tests/Makefile)

if test "$abs_srcdir" != "$abs_builddir"; then
   ln -s $abs_srcdir/static $abs_builddir
   ln -s $abs_srcdir/tiny_mce $abs_builddir
   ln -s $abs_srcdir/epic $abs_builddir
   ln -s $abs_srcdir/*.h $abs_builddir
   make mkdir-init
   
else
dnl just run it on devel systems and if non vdir build.
   if test -d .svn; then
      ./mk_module_init.sh
   fi
fi

if test -n "$srcdir"; then 
   export srcdir=.
fi


echo ------------------------------------------------------------------------
echo 'Character set conversion support:' $ok_iconv
echo 'National language support:       ' $ok_nls
echo 

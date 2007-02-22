dnl Process this file with autoconf to produce a configure script.
dnl $Id: configure.in 4798 2006-12-28 16:34:20Z ajc $
AC_INIT(webserver.c)


PACKAGE=webcit
VERSION=7.05
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

AC_SUBST(PROG_SUBDIRS)
AC_CANONICAL_HOST
AC_PROG_INSTALL
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AC_CHECK_PROG(SED, sed, sed, no)
AC_PREFIX_DEFAULT(/usr/local/webcit)

AC_ARG_WITH(with_libical, [  --with-libical          use libical calendaring library])
AC_ARG_WITH(with_zlib,    [  --with-zlib             use zlib compression if present])
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
esac
AC_SUBST(PTHREAD_DEFS)

dnl Checks for programs.
AC_PROG_CC

dnl Configure compiler flags for GCC
if test "$GCC" = yes; then
	case "$host" in
		*-*-solaris*)
			CFLAGS="$CFLAGS -Wall -Wno-char-subscripts"
		;;
		*)
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
AC_CHECK_HEADERS(fcntl.h limits.h sys/time.h unistd.h)




dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
dnl AC_HEADER_TIME

dnl Checks for library functions.
AC_TYPE_SIGNAL
dnl AC_FUNC_VPRINTF
dnl AC_CHECK_FUNCS(strerror)
AC_REPLACE_FUNCS(snprintf)

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
			LIBS="-lical $LIBS"
			AC_DEFINE(HAVE_LIBICAL)
		]
	)
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

dnl Here is the check for a usable iconv

AC_ARG_ENABLE(iconv,
	[  --disable-iconv         do not use iconv charset conversion],
	ok_iconv=no, ok_iconv=yes)

if test "$ok_nls" != "no"; then
	AC_MSG_RESULT(Checking to see if your system supports iconv...)
	AC_TRY_RUN([
		#include <iconv.h>
		main() {
			iconv_t ic = (iconv_t)(-1) ;
			ic = iconv_open("UTF-8", "us-ascii");
			iconv_close(ic);
			exit(0);
		}
	],
		ok_iconv=yes,
		ok_iconv=no
	)
fi

if test "$ok_iconv" != "no"; then
	AC_MSG_RESULT(WebCit will be built with character set conversion.)
	AC_DEFINE(HAVE_ICONV)
else
	AC_MSG_RESULT(WebCit will be built without character set conversion.)
fi



dnl Here is the check for libintl etc.

AC_ARG_ENABLE(nls,
	[  --disable-nls           do not use Native Language Support],
	ok_nls=no, ok_nls=yes)

if test "$ok_nls" != "no"; then
	AC_MSG_RESULT(Checking for per-thread NLS support...)
	AC_TRY_RUN([
                #define _GNU_SOURCE
                #include <libintl.h>
                #include <locale.h>
                #include <time.h>
                main() {
                        char *foo = NULL;
                        char baz[32];
			struct tm *tm;
                        uselocale(LC_GLOBAL_LOCALE);
                        foo = gettext("bar");
			if (0) {
                        	strftime_l(baz, sizeof baz, "%c", tm, LC_GLOBAL_LOCALE);
			}
                        exit(0);
		}
	],
		ok_uselocale=yes,
		ok_uselocale=no
	)
	ok_nls=$ok_uselocale
fi

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
	AC_DEFINE(ENABLE_NLS)
	PROG_SUBDIRS="$PROG_SUBDIRS po"
else
	AC_MSG_RESULT(WebCit will be built without national language support.)
fi

AC_SUBST(SETUP_LIBS)


if test "$prefix" = NONE; then
	LOCALEDIR=$ac_default_prefix
	WWWDIR=$ac_default_prefix
	EDITORDIR=$ac_default_prefix/tiny_mce
else
	LOCALEDIR=$prefix
	WWWDIR=$prefix
	EDITORDIR=$prefix/tiny_mce
fi

dnl where to put the locale files
AC_ARG_WITH(localedir, 
		    [  --with-localedir          directory to put the locale files to],
			[ if test "x$withval" != "xno" ; then
						  LOCALEDIR=$withval
			  fi
			]
)
AC_SUBST(LOCALEDIR)

dnl Checks where to put our templates
AC_ARG_WITH(wwwdir, 
		    [  --with-wwwdir             directory to put our templates],
			[ if test "x$withval" != "xno" ; then
						  WWWDIR=$withval
			  fi
			]
)
AC_SUBST(WWWDIR)

dnl Checks where to put our editor
AC_ARG_WITH(editordir, 
		    [  --with-editordir             directory to put our editor],
			[ if test "x$withval" != "xno" ; then
						  EDITORDIR=$withval
			  fi
			]
)
AC_SUBST(EDITORDIR)


AC_OUTPUT(Makefile po/Makefile )

echo ------------------------------------------------------------------------
echo 'zlib compression:                ' $ok_zlib
echo 'Calendar support:                ' $ok_libical
echo 'Character set conversion support:' $ok_iconv
echo 'National language support:       ' $ok_nls
echo 

# CIT_STRUCT_TM
# ------------------
# Figure out how to get the current GMT offset.  If `struct tm' has a
# `tm_gmtoff' member, define `HAVE_STRUCT_TM_TM_GMTOFF'.  Otherwise, if the
# external variable `timezone' is found, define `HAVE_TIMEZONE'.
AC_DEFUN([CIT_STRUCT_TM],
[AC_REQUIRE([AC_STRUCT_TM])dnl
AC_CHECK_MEMBERS([struct tm.tm_gmtoff],,,[#include <sys/types.h>
#include <$ac_cv_struct_tm>
])
if test "$ac_cv_member_struct_tm_tm_gmtoff" != yes; then
  AC_CACHE_CHECK(for timezone, ac_cv_var_timezone,
[AC_TRY_LINK(
[#include <time.h>],
[printf("%ld", (long)timezone);], ac_cv_var_timezone=yes, ac_cv_var_timezone=no)])
  if test $ac_cv_var_timezone = yes; then
    AC_DEFINE(HAVE_TIMEZONE, 1,
              [Define if you don't have `tm_gmtoff' but do have the external
               variable `timezone'.])
  fi
fi
])# CIT_STRUCT_TM

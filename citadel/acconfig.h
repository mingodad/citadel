/* define this to the bbs home directory */
#undef BBSDIR

/* define this to enable the autologin feature */
#undef ENABLE_AUTOLOGIN

/* define this to enable use of the chkpwd program (for shadow passwords) */
#undef ENABLE_CHKPWD

/* define this if struct utmp has an ut_type member */
#undef HAVE_UT_TYPE

/* define this if struct utmp has an ut_host member */
#undef HAVE_UT_HOST

/* define this if the OS has broken non-reentrant gethostby{name,addr}() */
#undef HAVE_NONREENTRANT_NETDB

/* define this if you have the pthread_cancel() function */
#undef HAVE_PTHREAD_CANCEL

/* define this if you want to enable the multithreaded client */
#undef THREADED_CLIENT

/* Define if you have OpenSSL. */
#undef HAVE_OPENSSL

/* Define if curses implementation has vw_printw */
#undef VW_PRINTW_IN_CURSES

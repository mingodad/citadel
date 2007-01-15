/* define this to the Citadel home directory */
#undef CTDLDIR

/* define this to disable use of curses */
#undef DISABLE_CURSES

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

/* define this if you have zlib compression available */
#undef HAVE_ZLIB

/* define this if you have OpenLDAP client available */
#undef HAVE_LDAP

/* define this if you have the libical calendaring library available */
#undef HAVE_LIBICAL

/* define this if you have the libsieve mailbox filtering library available */
#undef HAVE_LIBSIEVE

/* define if using OS X/Darwin */
#undef HAVE_DARWIN

/* define this if you have the newt window library available */
#undef HAVE_NEWT

/* define this if you have the resolv.h header file. */
#undef HAVE_RESOLV_H

/* define, if the user suplied a data-directory to use. */
#undef HAVE_DATA_DIR
#undef DATA_DIR

/* define, if the user suplied a spool-directory to use. */
#undef HAVE_SPOOL_DIR
#undef SPOOL_DIR

/* define, where the config should go in unix style */
#undef HAVE_ETC_DIR
#undef ETC_DIR

/* define, where the config should go in unix style */
#undef HAVE_RUN_DIR
#undef RUN_DIR


#undef EGD_POOL 

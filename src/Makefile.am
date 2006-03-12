# $Id: Makefile.in 4225 2006-01-20 21:39:28Z ajc $
PACKAGE = @PACKAGE@
VERSION = @VERSION@
top_srcdir = @top_srcdir@


datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

AM_CFLAGS = -Wall -DLOCALEDIR=\"$(localedir)\"
#  -Werror 
#




LIBS = @LTLIBINTL@
#INCLUDES = 






sbin_PROGRAMS = webserver setup

webserver_LDADD = @LIBINTL@

#SUBDIRS = $(LIB_SUBDIRS) $(PROG_SUBDIRS) 
setup_SOURCES = setup.c tools.c

webserver_SOURCES = \
	 context_loop.c \
	tools.c \
	ical_dezonify.c \
	cookie_conversion.c \
	locate_host.c \
	floors.c \
	summary.c \
	webcit.c \
	auth.c \
	tcp_sockets.c \
	mainmenu.c \
	serv_func.c \
	who.c \
	roomops.c \
	messages.c \
	userlist.c \
	paging.c \
	sysmsgs.c \
	useredit.c \
	vcard.c \
	vcard_edit.c \
	preferences.c \
	html2html.c \
	listsub.c \
	mime_parser.c \
	graphics.c \
	netconf.c \
	siteconfig.c \
	subst.c \
	rss.c \
	calendar.c \
	calendar_tools.c \
	calendar_view.c \
	event.c \
	availability.c \
	iconbar.c \
	crypto.c \
	inetconf.c \
	notes.c \
	wiki.c \
	groupdav_main.c \
	groupdav_get.c \
	groupdav_propfind.c \
	fmt_date.c \
	groupdav_options.c \
	autocompletion.c \
	gettext.c \
	tabs.c \
	groupdav_delete.c \
	groupdav_put.c \
	http_datestring.c \
	setup_wizard.c \
	webserver.c


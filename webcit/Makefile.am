PACKAGE = @PACKAGE@
VERSION = @VERSION@

SUBDIRS= po src 

EXTRA_DIST= autoconf/config.rpath autoconf/mkinstalldirs  m4/ChangeLog tiny_mce_DATA static_DATA
ACLOCAL_AMFLAGS = -I m4

DISTFILES=ABOUT_NLS
# usual unix way would be here: $(datadir)/$(PACKAGE)
#STATIC_DIR=@STATIC_DIR@

DIRS=tiny_mce static




install-data-local:
		@$(NORMAL_INSTALL); \
		if test -n "@STATIC_DIR@" ; then \
			export STATIC_DIR=@STATIC_DIR@; \
		else \
			export STATIC_DIR=$(prefix); \
		fi; \
		for dir in $(DIRS); do \
			$(mkinstalldirs) $(DESTDIR)/$$STATIC_DIR/$$dir; \
			for subdir in `find $$dir -type d |grep -v .svn`; do \
				$(mkinstalldirs) $(DESTDIR)/$$STATIC_DIR/$$subdir; \
					for file in $(srcdir)/$$subdir/*; do \
						if test -f "$$file" ; then \
							$(INSTALL_DATA) $$file $(DESTDIR)$$STATIC_DIR/$$subdir; \
						fi \
					done \
			done \
		done

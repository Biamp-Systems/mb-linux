#
# Copyright (c) 2005 Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = *~

EXTRA_DIST = README access.conf

securelibdir = $(SECUREDIR)
secureconfdir = $(SCONFIGDIR)

AM_CFLAGS = -I$(top_srcdir)/libpam/include -I$(top_srcdir)/libpamc/include \
	-DPAM_ACCESS_CONFIG=\"$(SCONFIGDIR)/access.conf\"
AM_LDFLAGS =  -no-undefined -avoid-version -module \
	-L$(top_builddir)/libpam -lpam @LIBNSL@
if HAVE_VERSIONING
  AM_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

securelib_LTLIBRARIES = pam_access.la

secureconf_DATA = access.conf

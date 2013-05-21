SUBDIRS = data po src

ACLOCAL_AMFLAGS = -I build

INTLTOOL_FILES = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

EXTRA_DIST = \
	$(INTLTOOL_FILES) \
	po/$(GETTEXT_PACKAGE).pot \
	win32/$(GETTEXT_PACKAGE).ico \
	win32/$(GETTEXT_PACKAGE)-icon.rc

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache

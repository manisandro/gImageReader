SUBDIRS = data po src

ACLOCAL_AMFLAGS = -I build

INTLTOOL_FILES = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

EXTRA_DIST = \
	autogen.sh \
	$(INTLTOOL_FILES) \
	po/$(GETTEXT_PACKAGE).pot \
	packaging

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache

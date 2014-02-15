EXTRA_DIST =
CLEANFILES =

# UI files
uidir = $(pkgdatadir)
ui_DATA = gimagereader.ui crashhandler.ui
EXTRA_DIST += $(ui_DATA)

# Icons
iconsdir = $(pkgdatadir)/icons
icons_DATA = \
	icons/angle.png icons/angle.svg \
	icons/autolayout.png icons/autolayout.svg \
	icons/brightness.png icons/brightness.svg \
	icons/contrast.png icons/contrast.svg \
	icons/controls.png icons/controls.svg \
	icons/ins_append.png icons/ins_append.svg \
	icons/ins_cursor.png icons/ins_cursor.svg \
	icons/ins_replace.png icons/ins_replace.svg \
	icons/page.png icons/page.svg \
	icons/resolution.png icons/resolution.svg \
	icons/stripcrlf.png icons/stripcrlf.svg
EXTRA_DIST += $(icons_DATA)

# Application icons
app_icons = \
	icons/gimagereader_48.png \
	icons/gimagereader_256.png

install-icons:
	for size in 48 256; do \
		mkdir -p $(DESTDIR)$(datadir)/icons/hicolor/$${size}x$${size}/apps; \
		$(INSTALL_DATA) $(srcdir)/icons/gimagereader_$${size}.png $(DESTDIR)$(datadir)/icons/hicolor/$${size}x$${size}/apps/gimagereader.png; \
	done

uninstall-icons:
	for size in 48 256; do \
		rm -f $(DESTDIR)$(datadir)/icons/hicolor/$${size}x$${size}/apps/gimagereader.png; \
	done

install-data-local: install-icons
uninstall-local: uninstall-icons

EXTRA_DIST += $(app_icons)

# Desktop file
desktopdir=$(datadir)/applications
desktop_DATA = gimagereader.desktop
EXTRA_DIST += $(desktop_DATA)

# GSettings schema
gsettings_SCHEMAS = org.gnome.gimagereader.gschema.xml
@GSETTINGS_RULES@
EXTRA_DIST += $(gsettings_SCHEMAS)

# Manual
manualdir = $(pkgdatadir)
manual_DATA = manual.html
EXTRA_DIST += $(manual_DATA)

# Appdata file
appdatadir=$(datadir)/appdata
appdata_DATA = gimagereader.appdata.xml
EXTRA_DIST += $(appdata_DATA)

# Hooks
install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || gtk-update-icon-cache -f -t "$(datadir)/icons/hicolor"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || gtk-update-icon-cache -f -t "$(datadir)/icons/hicolor"

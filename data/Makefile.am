EXTRA_DIST =
CLEANFILES =

# UI files
uidir = $(pkgdatadir)
ui_DATA = gimagereader.ui
EXTRA_DIST += $(ui_DATA)

# Icons
iconsdir = $(pkgdatadir)/icons
icons_DATA = \
	icons/angle.png \
	icons/autolayout.png \
	icons/brightness.png \
	icons/contrast.png \
	icons/controls.png \
	icons/ins_append.png \
	icons/ins_cursor.png \
	icons/ins_replace.png \
	icons/page.png \
	icons/resolution.png \
	icons/stripcrlf.png
EXTRA_DIST += $(icons_DATA)

# Logo
logo48dir = $(datadir)/icons/hicolor/48x48/apps
logoSVGdir = $(datadir)/icons/hicolor/scalable/apps
logo48_DATA = icons/gimagereader.png
logoSVG_DATA = icons/gimagereader.svg
EXTRA_DIST += $(logo48_DATA) $(logoSVG_DATA)

# Desktop file
desktopdir=$(datadir)/applications
desktop_DATA = gimagereader.desktop
EXTRA_DIST += $(desktop_DATA)

# GSettings schema
gsettings_SCHEMAS = apps.gimagereader.gschema.xml
@GSETTINGS_RULES@
EXTRA_DIST += $(gsettings_SCHEMAS)

# Manual
manualdir = $(pkgdatadir)
manual_DATA = manual.html
EXTRA_DIST += $(manual_DATA)

# Hooks
install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || gtk-update-icon-cache -f -t "$(datadir)/icons/hicolor"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || gtk-update-icon-cache -f -t "$(datadir)/icons/hicolor"

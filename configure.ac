dnl Process this file with autoconf to produce a configure script.

AC_INIT(gImageReader, 2.92, http://sourceforge.net/projects/gimagereader/,
		gimagereader)

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR(src/main.cc)
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([-Wall])

AM_SILENT_RULES([yes])

dnl ******************************** C++ App **********************************
AC_PROG_CXX
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_11([noext])

dnl ******************************** GSettings ********************************
GLIB_GSETTINGS

dnl *************************** Internationalization **************************
GETTEXT_PACKAGE=AC_PACKAGE_NAME
IT_PROG_INTLTOOL([0.35.0])
AC_SUBST([GETTEXT_PACKAGE], [$PACKAGE_TARNAME])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[The name of the gettext domain])

dnl ************************** Libtool initialization *************************
LT_INIT

dnl ********************************* Modules *********************************
PKG_CHECK_MODULES(GIMAGEREADER, [gtkmm-3.0 >= 3.6 gtkspellmm-3.0 >= 3.0.2
								 cairomm-1.0 poppler-glib])

PKG_CHECK_MODULES(TESSERACT, [tesseract >= 3.02.02], [], [
    AC_MSG_NOTICE([tesseract not found via pkg-config, using hardcoded CFLAGS and LIBS])
    TESSERACT_CFLAGS+=-I/usr/include/tesseract
    TESSERACT_LIBS+=-ltesseract
])

GIMAGEREADER_CFLAGS+=$TESSERACT_CFLAGS
GIMAGEREADER_LIBS+=$TESSERACT_LIBS

dnl *************************** Enable version check **************************
AC_ARG_ENABLE([versioncheck],
	 AS_HELP_STRING([--enable-versioncheck], [enable version check (default is yes)]),
	  [case "${enableval}" in
		yes) versioncheck=true ;;
		no)  versioncheck=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-versioncheck]) ;;
	  esac],[versioncheck=true])
AM_CONDITIONAL([ENABLE_VERSIONCHECK], [test x$versioncheck = xtrue])

if test x$versioncheck = xtrue; then
	AC_DEFINE(ENABLE_VERSIONCHECK, 1, [Whether version-check is enabled.])
fi

dnl ************************** Look for SANE on linux *************************
case "$host_os" in
  linux*)
		PKG_CHECK_MODULES(SANE, [sane-backends])
		;;
	*)
		;;
esac

dnl ****************************** Detect Windows *****************************
AC_MSG_CHECKING([for some Win32 platform])
case "$host" in
  *-*-mingw*|*-*-cygwin*)
	platform_win32=yes
	;;
  *)
	platform_win32=no
	;;
esac
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL([PLATFORM_WIN32], [test "$platform_win32" = "yes"])
if test "$with_platform" = "win32"; then
  AC_CHECK_TOOL([WINDRES],[windres])
fi

AC_SUBST(PACKAGE_RFC_DATE, `date -R`)
AC_SUBST(PACKAGE_DATE, `LANG=en date  +'%a %b %d %Y'`)
AC_DEFINE_UNQUOTED(PACKAGE_REVISION, "`git rev-parse HEAD 2>/dev/null || echo "?"`", [Package git revision])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/manual.html
src/Makefile
po/Makefile.in
packaging/gimagereader.spec
packaging/debian/changelog
])

AC_OUTPUT

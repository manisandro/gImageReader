CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)
LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
INCLUDE(GSettings)
SET(ENV{LC_ALL} "C")

# Project properties
PROJECT(gimagereader)
SET(PACKAGE_NAME gImageReader)
SET(PACKAGE_VERSION 2.94)
SET(PACKAGE_LOCALE_DIR "${CMAKE_INSTALL_PREFIX}/share/locale")

# Variables
SET(INTERFACE_TYPE "Qt4" CACHE STRING "Select interface type: Qt4/Qt5/gtk")
MESSAGE(STATUS "${INTERFACE_TYPE} interface will be built")
SET(ENABLE_VERSIONCHECK 1 CACHE BOOL "Enable version check")
EXECUTE_PROCESS(COMMAND date +%a\ %b\ %d\ %Y OUTPUT_VARIABLE PACKAGE_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
EXECUTE_PROCESS(COMMAND date -R OUTPUT_VARIABLE PACKAGE_RFC_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
EXECUTE_PROCESS(COMMAND git rev-parse HEAD OUTPUT_VARIABLE PACKAGE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE)


# Configure files
CONFIGURE_FILE(data/manual.html.in data/manual.html @ONLY)
CONFIGURE_FILE(packaging/gimagereader.spec.in packaging/gimagereader.spec @ONLY)
CONFIGURE_FILE(packaging/debian/changelog.in packaging/debian/changelog @ONLY)


# Definitions
ADD_DEFINITIONS(-DPACKAGE_NAME=\"${PACKAGE_NAME}\")
ADD_DEFINITIONS(-DPACKAGE_VERSION=\"${PACKAGE_VERSION}\")
ADD_DEFINITIONS(-DPACKAGE_REVISION=\"${PACKAGE_REVISION}\")
ADD_DEFINITIONS(-DGETTEXT_PACKAGE=\"${CMAKE_PROJECT_NAME}\")
ADD_DEFINITIONS(-DENABLE_VERSIONCHECK=${ENABLE_VERSIONCHECK})
ADD_DEFINITIONS(-std=c++11)


# Dependencies
FIND_PACKAGE(Gettext REQUIRED)
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(TESSERACT REQUIRED tesseract)
IF(UNIX)
	PKG_CHECK_MODULES(SANE sane-backends)
ENDIF(UNIX)

INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR} ${TESSERACT_INCLUDE_DIRS} ${SANE_INCLUDE_DIRS})

IF("${INTERFACE_TYPE}" STREQUAL "gtk")
	PKG_CHECK_MODULES(GTKMM REQUIRED gtkmm-3.0)
	PKG_CHECK_MODULES(GTKSPELLMM REQUIRED gtkspellmm-3.0)
	PKG_CHECK_MODULES(CAIROMM REQUIRED cairomm-1.0)
	PKG_CHECK_MODULES(POPPLER REQUIRED poppler-glib)
	INCLUDE_DIRECTORIES(
		${GTKMM_INCLUDE_DIRS}
		${GTKSPELLMM_INCLUDE_DIRS}
		${CAIROMM_INCLUDE_DIRS}
		${POPPLER_INCLUDE_DIRS}
	)
	SET(gimagereader_LIBS
		${GTKMM_LDFLAGS}
		${GTKSPELLMM_LDFLAGS}
		${CAIROMM_LDFLAGS}
		${POPPLER_LDFLAGS}
	)
	SET(srcdir "gtk")
ELSEIF("${INTERFACE_TYPE}" STREQUAL "Qt4")
	SET(CMAKE_AUTOMOC ON)
	FIND_PACKAGE(Qt4 REQUIRED)
	PKG_CHECK_MODULES(QTSPELL REQUIRED QtSpell-qt4)
	PKG_CHECK_MODULES(POPPLER REQUIRED poppler-qt4)
	INCLUDE_DIRECTORIES(${QTSPELL_INCLUDE_DIRS} ${POPPLER_INCLUDE_DIRS})
	SET(gimagereader_LIBS ${QTSPELL_LDFLAGS} ${POPPLER_LDFLAGS})
	SET(srcdir "qt")
ELSEIF("${INTERFACE_TYPE}" STREQUAL "Qt5")
	SET(CMAKE_AUTOMOC ON)
	FIND_PACKAGE(Qt5Widgets REQUIRED)
	PKG_CHECK_MODULES(QTSPELL REQUIRED QtSpell-qt5)
	PKG_CHECK_MODULES(POPPLER REQUIRED poppler-qt5)
	INCLUDE_DIRECTORIES(${QTSPELL_INCLUDE_DIRS} ${POPPLER_INCLUDE_DIRS})
	SET(gimagereader_LIBS ${QTSPELL_LDFLAGS} ${POPPLER_LDFLAGS})
	SET(srcdir "qt")
ELSE()
	MESSAGE(FATAL_ERROR "Invalid interface type ${INTERFACE_TYPE}")
ENDIF()


# Files
FILE(GLOB gimagereader_SRCS ${srcdir}/src/*.cc)
FILE(GLOB gimagereader_HDRS ${srcdir}/src/*.hh)
FILE(GLOB gimagereader_FORMS ${srcdir}/data/*.ui)
FILE(GLOB gimagereader_ICONS data/icons/*.png)
FILE(GLOB gimagereader_PO po/*.po)

IF("${srcdir}" STREQUAL "qt")
	FILE(GLOB gimagereader_RESOURCES qt/data/*.qrc)
	
	# Gettextize ui files
	ADD_CUSTOM_TARGET(
		gettextizeui
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gettextizeui.cmake ${gimagereader_FORMS_HEADERS}
		DEPENDS ${gimagereader_FORMS_HEADERS}
	)
ENDIF()

IF("${srcdir}" STREQUAL "gtk")
	ADD_SCHEMA("gtk/data/org.gnome.gimagereader.gschema.xml")
	ADD_CUSTOM_COMMAND(
		OUTPUT gimagereader.gresource.c
		COMMAND glib-compile-resources gimagereader.gresource.xml --target=${CMAKE_CURRENT_BINARY_DIR}/gimagereader.gresource.c --generate-source
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/gtk/data/
		DEPENDS ${CMAKE_SOURCE_DIR}/gtk/data/gimagereader.gresource.xml
				${CMAKE_SOURCE_DIR}/gtk/data/gimagereader.ui
				${CMAKE_SOURCE_DIR}/gtk/data/crashhandler.ui
				${CMAKE_SOURCE_DIR}/gtk/data/appmenu.ui)
	LIST(APPEND gimagereader_SRCS gimagereader.gresource.c)
ELSEIF("${INTERFACE_TYPE}" STREQUAL "Qt4")
	QT4_WRAP_UI(gimagereader_FORMS_HEADERS ${gimagereader_FORMS})
	QT4_ADD_RESOURCES(gimagereader_RESOURCES_RCC ${gimagereader_RESOURCES})
ELSEIF("${INTERFACE_TYPE}" STREQUAL "Qt5")
	QT5_WRAP_UI(gimagereader_FORMS_HEADERS ${gimagereader_FORMS})
	QT5_ADD_RESOURCES(gimagereader_RESOURCES_RCC ${gimagereader_RESOURCES})
ENDIF()

FILE(READ po/LINGUAS langs_string)
STRING(REGEX MATCHALL "[a-zA-Z_]+" langs "${langs_string}")
FILE(COPY ${gimagereader_PO} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
GETTEXT_PROCESS_POT_FILE(po/gimagereader.pot ALL INSTALL_DESTINATION ${PACKAGE_LOCALE_DIR} LANGUAGES ${langs})

IF(MINGW)
  ENABLE_LANGUAGE(RC)
  SET(CMAKE_RC_COMPILER_INIT ${CMAKE_GENERATOR_RC})
  SET(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -i <SOURCE> -o <OBJECT>")
  SET(gimagereader_SRCS ${gimagereader_SRCS} packaging/win32/gimagereader-icon.rc)
ENDIF(MINGW)


# Build and install
ADD_EXECUTABLE(gimagereader
	${gimagereader_HDRS}
	${gimagereader_SRCS}
	${gimagereader_FORMS_HEADERS}
	${gimagereader_RESOURCES_RCC}
)
TARGET_LINK_LIBRARIES(gimagereader
	${TESSERACT_LDFLAGS}
	${gimagereader_LIBS}
	${SANE_LDFLAGS}
	-ldl
)
IF("${INTERFACE_TYPE}" STREQUAL "Qt4")
	QT4_USE_MODULES(gimagereader Core Gui Network DBus)
	ADD_DEPENDENCIES(gimagereader gettextizeui)
ELSEIF("${INTERFACE_TYPE}" STREQUAL "Qt5")
	QT5_USE_MODULES(gimagereader Widgets Network DBus Concurrent)    
	ADD_DEPENDENCIES(gimagereader gettextizeui)
ENDIF()

INSTALL(TARGETS gimagereader DESTINATION bin)
INSTALL(FILES data/icons/48x48/gimagereader.png DESTINATION share/icons/hicolor/48x48/apps/)
INSTALL(FILES data/icons/128x128/gimagereader.png DESTINATION share/icons/hicolor/128x128/apps/)
INSTALL(FILES data/icons/256x256/gimagereader.png DESTINATION share/icons/hicolor/256x256/apps/)
INSTALL(FILES data/gimagereader.desktop DESTINATION share/applications)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/data/manual.html DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/gimagereader")
INSTALL(FILES data/gimagereader.appdata.xml DESTINATION share/appdata)


# Dist
add_custom_target(dist
    COMMAND git archive --format=tar --prefix=${CMAKE_PROJECT_NAME}-${PACKAGE_VERSION}/ HEAD | xz > ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-${PACKAGE_VERSION}.tar.xz
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

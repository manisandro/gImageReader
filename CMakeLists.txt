CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
IF(${CMAKE_VERSION} VERSION_GREATER "3.17.0")
CMAKE_POLICY(SET CMP0100 NEW)
ENDIF()
LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
INCLUDE(GSettings)
SET(ENV{LC_ALL} "C")


# Project properties
PROJECT(gimagereader C CXX)
SET(PACKAGE_NAME gImageReader)
SET(PACKAGE_VERSION 3.4.2)
SET(PACKAGE_LOCALE_DIR "${CMAKE_INSTALL_PREFIX}/share/locale")


# Variables
SET(INTERFACE_TYPE "qt6" CACHE STRING "Select interface type: qt5/qt6/gtk")
MESSAGE(STATUS "${INTERFACE_TYPE} interface will be built")
SET(MANUAL_DIR "share/doc/gimagereader" CACHE PATH "Path where manual will be installed")
SET(ENABLE_VERSIONCHECK 1 CACHE BOOL "Enable version check")
string(TIMESTAMP PACKAGE_DATE "%a\ %b\ %d\ %Y" UTC)
string(TIMESTAMP PACKAGE_ISO_DATE "%Y-%m-%d" UTC)
EXECUTE_PROCESS(COMMAND date -R OUTPUT_VARIABLE PACKAGE_RFC_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
EXECUTE_PROCESS(COMMAND git rev-parse HEAD OUTPUT_VARIABLE PACKAGE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE)

IF(NOT IS_ABSOLUTE "${MANUAL_DIR}")
    SET(MANUAL_DIR "${CMAKE_INSTALL_PREFIX}/${MANUAL_DIR}")
ENDIF()

# Configure files
CONFIGURE_FILE(data/gimagereader.appdata.xml.in data/gimagereader-${INTERFACE_TYPE}.appdata.xml @ONLY)
CONFIGURE_FILE(packaging/gimagereader.spec.in packaging/gimagereader.spec @ONLY)
CONFIGURE_FILE(packaging/debian/changelog.in packaging/debian/changelog @ONLY)

FILE(GLOB gimagereader_HTML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} data/manual*.html.in)
SET(manualFiles)
FOREACH(inFile ${gimagereader_HTML})
    STRING(REGEX REPLACE ".in\$" "" outFile "${inFile}")
    CONFIGURE_FILE(${inFile} ${outFile} @ONLY)
    SET(manualFiles ${manualFiles} "${CMAKE_CURRENT_BINARY_DIR}/${outFile}")
ENDFOREACH()


# Definitions
ADD_DEFINITIONS(-DPACKAGE_NAME=\"${PACKAGE_NAME}\")
ADD_DEFINITIONS(-DPACKAGE_VERSION=\"${PACKAGE_VERSION}\")
ADD_DEFINITIONS(-DPACKAGE_REVISION=\"${PACKAGE_REVISION}\")
ADD_DEFINITIONS(-DGETTEXT_PACKAGE=\"${CMAKE_PROJECT_NAME}\")
ADD_DEFINITIONS(-DENABLE_VERSIONCHECK=${ENABLE_VERSIONCHECK})
ADD_DEFINITIONS(-DMANUAL_DIR=\"${MANUAL_DIR}\")
SET(CMAKE_CXX_STANDARD 17)
SET(CXX_STANDARD_REQUIRED ON)


# Dependencies
FIND_PACKAGE(Gettext REQUIRED)
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(TESSERACT tesseract)
PKG_CHECK_MODULES(ddjvuapi REQUIRED ddjvuapi)
PKG_CHECK_MODULES(PODOFO libpodofo)
IF(NOT TESSERACT_FOUND)
    MESSAGE(WARNING "Using hardcoded cflags and ldflags for tesseract")
    SET(TESSERACT_INCLUDE_DIRS /usr/include/tesseract)
    SET(TESSERACT_LDFLAGS -ltesseract)
ENDIF(NOT TESSERACT_FOUND)
IF(NOT PODOFO_FOUND)
  MESSAGE(WARNING "Using hardcoded cflags and ldflags for podofo")
  SET(PODOFO_INCLUDE_DIRS /usr/include/)
  SET(PODOFO_LDFLAGS -lpodofo)
ENDIF(NOT PODOFO_FOUND)
IF(UNIX)
    PKG_CHECK_MODULES(SANE sane-backends)
ENDIF(UNIX)
FIND_PROGRAM(INTLTOOL_MERGE_EXECUTABLE intltool-merge)
IF(NOT INTLTOOL_MERGE_EXECUTABLE)
    MESSAGE(FATAL_ERROR "Couldn't find intltool-merge, please install intltool")
ENDIF(NOT INTLTOOL_MERGE_EXECUTABLE)

PKG_CHECK_MODULES(ENCHANT enchant-2)
IF(ENCHANT_FOUND)
    ADD_DEFINITIONS(-DHAVE_ENCHANT2)
ELSE(ENCHANT_FOUND)
    PKG_CHECK_MODULES(ENCHANT REQUIRED enchant)
    IF(${ENCHANT_VERSION} VERSION_GREATER_EQUAL 2)
        ADD_DEFINITIONS(-DHAVE_ENCHANT2)
    ENDIF()
ENDIF(ENCHANT_FOUND)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_BINARY_DIR}
  ${TESSERACT_INCLUDE_DIRS}
  ${SANE_INCLUDE_DIRS}
  ${ENCHANT_INCLUDE_DIRS}
  ${PODOFO_INCLUDE_DIRS}
)

IF("${INTERFACE_TYPE}" STREQUAL "gtk")
    PKG_CHECK_MODULES(GTKMM REQUIRED gtkmm-3.0)
    PKG_CHECK_MODULES(GTKSOURCEVIEWMM gtksourceviewmm-4.0)
    IF(NOT GTKSOURCEVIEWMM_FOUND)
      PKG_CHECK_MODULES(GTKSOURCEVIEWMM REQUIRED gtksourceviewmm-3.0)
    ENDIF(NOT GTKSOURCEVIEWMM_FOUND)
    PKG_CHECK_MODULES(GTKSPELLMM REQUIRED gtkspellmm-3.0>=3.0.5)
    PKG_CHECK_MODULES(CAIROMM REQUIRED cairomm-1.0)
    PKG_CHECK_MODULES(PANGOMM REQUIRED pangomm-1.4)
    PKG_CHECK_MODULES(POPPLER REQUIRED poppler-glib)
    PKG_CHECK_MODULES(JSONGLIB REQUIRED json-glib-1.0)
    PKG_CHECK_MODULES(LIBXMLPP libxml++-3.0)
    IF(LIBXMLPP_FOUND)
      ADD_DEFINITIONS(-DHAVE_LIBXMLPP_3)
    ELSE()
      PKG_CHECK_MODULES(LIBXMLPP2 REQUIRED libxml++-2.6)
      ADD_DEFINITIONS(-DHAVE_LIBXMLPP_2)
      SET(LIBXMLPP_INCLUDE_DIRS ${LIBXMLPP2_INCLUDE_DIRS})
      SET(LIBXMLPP_LDFLAGS ${LIBXMLPP2_LDFLAGS})
    ENDIF()
    PKG_CHECK_MODULES(LIBZIP REQUIRED libzip)
    FIND_PACKAGE(JPEG)
    IF(JPEG_FOUND)
        SET(LIBJPEG_INCLUDE_DIRS ${JPEG_INCLUDE_DIRS})
        SET(LIBJPEG_LDFLAGS ${JPEG_LIBRARIES})
    ELSE()
        PKG_CHECK_MODULES(LIBJPEG libjpeg)
        IF(NOT LIBJPEG_FOUND)
            MESSAGE(WARNING "Using hardcoded cflags and ldflags for libjpeg")
            SET(LIBJPEG_LDFLAGS "-ljpeg")
        ENDIF()
    ENDIF()
    PKG_CHECK_MODULES(FONTCONFIG REQUIRED fontconfig)
    INCLUDE_DIRECTORIES(
        ${GTKMM_INCLUDE_DIRS}
        ${GTKSOURCEVIEWMM_INCLUDE_DIRS}
        ${GTKSPELLMM_INCLUDE_DIRS}
        ${CAIROMM_INCLUDE_DIRS}
        ${PANGOMM_INCLUDE_DIRS}
        ${POPPLER_INCLUDE_DIRS}
        ${JSONGLIB_INCLUDE_DIRS}
        ${LIBXMLPP_INCLUDE_DIRS}
        ${LIBJPEG_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${LIBZIP_INCLUDE_DIRS}
    )
    SET(gimagereader_LIBS
        ${GTKMM_LDFLAGS}
        ${GTKSOURCEVIEWMM_LDFLAGS}
        ${GTKSPELLMM_LDFLAGS}
        ${CAIROMM_LDFLAGS}
        ${PANGOMM_LDFLAGS}
        ${POPPLER_LDFLAGS}
        ${JSONGLIB_LDFLAGS}
        ${LIBXMLPP_LDFLAGS}
        ${LIBJPEG_LDFLAGS}
        ${FONTCONFIG_LDFLAGS}
        ${LIBZIP_LDFLAGS}
    )
    SET(srcdir "gtk")
    # Fix symbol collision
    IF(MINGW)
        ADD_DEFINITIONS(-DNODRAWTEXT)
    ENDIF(MINGW)
ELSEIF("${INTERFACE_TYPE}" STREQUAL "qt5" OR "${INTERFACE_TYPE}" STREQUAL "qt6")
    STRING(REGEX REPLACE "^qt" "" QT_VER "${INTERFACE_TYPE}")

    SET(CMAKE_AUTOMOC ON)
    FIND_PACKAGE(Qt${QT_VER}Concurrent REQUIRED)
    IF(${QT_VER} STREQUAL "5")
        IF(Qt5Concurrent_VERSION VERSION_LESS 5.15)
	    MESSAGE(FATAL_ERROR "Qt5 version must be at least 5.15 (found ${Qt5Concurrent_VERSION})")
	ENDIF()
    ENDIF()

    FIND_PACKAGE(Qt${QT_VER}DBus REQUIRED)
    FIND_PACKAGE(Qt${QT_VER}Network REQUIRED)
    FIND_PACKAGE(Qt${QT_VER}Widgets REQUIRED)
    FIND_PACKAGE(Qt${QT_VER}Xml REQUIRED)
    FIND_PACKAGE(Qt${QT_VER}PrintSupport REQUIRED)
    FIND_PACKAGE(QuaZip-Qt${QT_VER} QUIET)
    IF(QuaZip-Qt${QT_VER}_FOUND)
      SET(QUAZIP_LIBRARIES QuaZip::QuaZip)
    ELSE()
        FIND_PACKAGE(QuaZip5 QUIET)
        IF(NOT QUAZIP_FOUND)
            FIND_PATH(QUAZIP_INCLUDE_DIR NAME quazipfile.h PATH_SUFFIXES quazip${QT_VER} QuaZip-Qt${QT_VER}-1.0/quazip QuaZip-Qt${QT_VER}-1.1/quazip QuaZip-Qt${QT_VER}-1.2/quazip)
            FIND_LIBRARY(QUAZIP_LIBRARIES NAMES quazip${QT_VER} quazip1-qt${QT_VER})
            IF(NOT QUAZIP_INCLUDE_DIR OR NOT QUAZIP_LIBRARIES)
                MESSAGE(FATAL_ERROR "QuaZip${QT_VER} is required but was not found")
            ENDIF()
        ENDIF()
        INCLUDE_DIRECTORIES(${QUAZIP_INCLUDE_DIR})
    ENDIF()
    PKG_CHECK_MODULES(QTSPELL REQUIRED QtSpell-qt${QT_VER}>=0.8.0)
    PKG_CHECK_MODULES(POPPLER REQUIRED poppler-qt${QT_VER})
    INCLUDE_DIRECTORIES(${QTSPELL_INCLUDE_DIRS} ${POPPLER_INCLUDE_DIRS})
    SET(gimagereader_LIBS ${QTSPELL_LDFLAGS} ${POPPLER_LDFLAGS} ${QUAZIP_LIBRARIES})
    SET(srcdir "qt")
ELSE()
    MESSAGE(FATAL_ERROR "Invalid interface type ${INTERFACE_TYPE}")
ENDIF()

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/${srcdir}/src/
  ${CMAKE_SOURCE_DIR}/${srcdir}/src/hocr/
)

# Files
# sort lists to avoid dependency on readdir() order and make build reproducible
FILE(GLOB gimagereader_SRCS
  ${srcdir}/src/*.cc
  ${srcdir}/src/hocr/*.cc
  common/*.cc
)
LIST(SORT gimagereader_SRCS)

FILE(GLOB gimagereader_HDRS
  ${srcdir}/src/*.hh
  ${srcdir}/src/hocr/*.hh
  common/*.hh)
LIST(SORT gimagereader_HDRS)

FILE(GLOB gimagereader_FORMS ${srcdir}/data/*.ui)
LIST(SORT gimagereader_FORMS)

FILE(GLOB gimagereader_ICONS data/icons/*.png)
LIST(SORT gimagereader_ICONS)

IF("${srcdir}" STREQUAL "qt")
    FILE(GLOB gimagereader_RESOURCES qt/data/*.qrc)
    LIST(SORT gimagereader_RESOURCES)
ENDIF()
IF(UNIX)
    LIST(APPEND gimagereader_SRCS ${srcdir}/src/scanner/ScannerSane.cc)
    LIST(APPEND gimagereader_HDRS ${srcdir}/src/scanner/ScannerSane.hh)
ELSE(UNIX)
    LIST(APPEND gimagereader_SRCS ${srcdir}/src/scanner/ScannerTwain.cc)
    LIST(APPEND gimagereader_HDRS ${srcdir}/src/scanner/ScannerTwain.hh)
ENDIF(UNIX)


# Generate resources
IF("${srcdir}" STREQUAL "gtk")
    ADD_SCHEMA("gtk/data/org.gnome.gimagereader.gschema.xml")
    ADD_CUSTOM_COMMAND(
        OUTPUT  ${CMAKE_BINARY_DIR}/gimagereader.gresource.c
        COMMAND glib-compile-resources gimagereader.gresource.xml ARGS --target=${CMAKE_BINARY_DIR}/gimagereader.gresource.c --generate-source
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/gtk/data/
        DEPENDS ${gimagereader_FORMS})
    LIST(APPEND gimagereader_SRCS ${CMAKE_BINARY_DIR}/gimagereader.gresource.c)

    # Just to allow running the application from the build tree (via XDG_DATA_DIRS=$PWD:$XDG_DATA_DIRS ./gimagereader-gtk)
    ADD_CUSTOM_COMMAND(
        OUTPUT  ${CMAKE_BINARY_DIR}/glib-2.0/schemas/gschemas.compiled
        COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_SOURCE_DIR}/gtk/data/org.gnome.gimagereader.gschema.xml ${CMAKE_BINARY_DIR}/glib-2.0/schemas/org.gnome.gimagereader.gschema.xml
        COMMAND glib-compile-schemas ARGS ${CMAKE_BINARY_DIR}/glib-2.0/schemas/
        DEPENDS ${CMAKE_SOURCE_DIR}/gtk/data/org.gnome.gimagereader.gschema.xml
    )
    SET(gimagereader_RESOURCES_RCC ${CMAKE_BINARY_DIR}/glib-2.0/schemas/gschemas.compiled)

    FOREACH(form ${gimagereader_FORMS})
        GET_FILENAME_COMPONENT(outfile ${form} NAME_WE)
        SET(outfile ${CMAKE_BINARY_DIR}/ui_${outfile}.hh)
        ADD_CUSTOM_COMMAND(
            OUTPUT ${outfile}
            COMMAND python3 ARGS ${CMAKE_SOURCE_DIR}/gtk/data/uigen.py ${form}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS ${form}
        )
        LIST(APPEND gimagereader_FORMS_HEADERS ${outfile})
    ENDFOREACH()

ELSEIF("${srcdir}" STREQUAL "qt")
    QT_ADD_RESOURCES(gimagereader_RESOURCES_RCC ${gimagereader_RESOURCES})
    SET(UIC_EXECUTABLE Qt${QT_VER}::uic)

    # Custom WRAP_UI which also gettext-izes the result
    MESSAGE(${CMAKE_COMMAND})
    FOREACH(form ${gimagereader_FORMS})
        GET_FILENAME_COMPONENT(outfile ${form} NAME_WE)
        SET(outfile ${CMAKE_BINARY_DIR}/ui_${outfile}.h)
        ADD_CUSTOM_COMMAND(
            OUTPUT ${outfile}
            COMMAND ${UIC_EXECUTABLE} ARGS -o ${outfile} ${form}
            COMMAND ${CMAKE_COMMAND} ARGS -P ${CMAKE_SOURCE_DIR}/cmake/gettextizeui.cmake ${outfile}
            DEPENDS ${form}
        )
        LIST(APPEND gimagereader_FORMS_HEADERS ${outfile})
    ENDFOREACH()

    SET_PROPERTY(SOURCE ${gimagereader_FORMS_HEADERS} PROPERTY SKIP_AUTOMOC ON)
    SET_PROPERTY(SOURCE ${gimagereader_RESOURCES_RCC} PROPERTY SKIP_AUTOMOC ON)
ENDIF()


# Process translations
FILE(READ po/LINGUAS langs_string)
STRING(REGEX MATCHALL "[a-zA-Z_]+" langs "${langs_string}")
FOREACH(lang ${langs})
    CONFIGURE_FILE(po/${lang}.po ${lang}.po COPYONLY)
ENDFOREACH()
GETTEXT_PROCESS_POT_FILE(po/gimagereader.pot ALL INSTALL_DESTINATION ${PACKAGE_LOCALE_DIR} LANGUAGES ${langs})

# Process desktop file
CONFIGURE_FILE(data/gimagereader.desktop.in data/gimagereader-${INTERFACE_TYPE}.desktop.in @ONLY)
# add translations to desktop file
ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/data/gimagereader-${INTERFACE_TYPE}.desktop
    COMMAND ${INTLTOOL_MERGE_EXECUTABLE} --desktop-style ${CMAKE_SOURCE_DIR}/po ${CMAKE_CURRENT_BINARY_DIR}/data/gimagereader-${INTERFACE_TYPE}.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/data/gimagereader-${INTERFACE_TYPE}.desktop
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/data/gimagereader-${INTERFACE_TYPE}.desktop.in)
ADD_CUSTOM_TARGET(desktop_file ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/data/gimagereader-${INTERFACE_TYPE}.desktop)

IF(MINGW)
  ENABLE_LANGUAGE(RC)
  SET(CMAKE_RC_COMPILER_INIT ${CMAKE_GENERATOR_RC})
  SET(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -i <SOURCE> -o <OBJECT>")
  SET(gimagereader_SRCS ${gimagereader_SRCS} packaging/win32/gimagereader-icon.rc)
  LIST(APPEND gimagereader_LIBS -lintl -mwindows)
ENDIF(MINGW)


# Build and install
IF("${srcdir}" STREQUAL "qt")
    STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_TOLOWER)
    IF(NOT CMAKE_BUILD_TYPE_TOLOWER MATCHES "debug")
        ADD_DEFINITIONS(-DQT_NO_DEBUG_OUTPUT)
    ENDIF()
ENDIF()

ADD_EXECUTABLE(gimagereader
    ${gimagereader_HDRS}
    ${gimagereader_SRCS}
    ${gimagereader_FORMS}
    ${gimagereader_FORMS_HEADERS}
    ${gimagereader_RESOURCES_RCC}
)
TARGET_LINK_LIBRARIES(gimagereader
    ${TESSERACT_LDFLAGS}
    ${gimagereader_LIBS}
    ${SANE_LDFLAGS}
    ${ddjvuapi_LDFLAGS}
    ${ENCHANT_LDFLAGS}
    ${PODOFO_LDFLAGS}
    -ldl
)

FIND_PACKAGE(OpenMP REQUIRED)
SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

SET_TARGET_PROPERTIES(gimagereader PROPERTIES OUTPUT_NAME gimagereader-${INTERFACE_TYPE})
IF("${srcdir}" STREQUAL "qt")
    TARGET_LINK_LIBRARIES(gimagereader Qt${QT_VER}::Widgets Qt${QT_VER}::Network Qt${QT_VER}::DBus Qt${QT_VER}::Xml Qt${QT_VER}::PrintSupport Qt${QT_VER}::Concurrent)
ENDIF()

INSTALL(TARGETS gimagereader DESTINATION bin)
INSTALL(FILES data/icons/48x48/gimagereader.png DESTINATION share/icons/hicolor/48x48/apps/)
INSTALL(FILES data/icons/128x128/gimagereader.png DESTINATION share/icons/hicolor/128x128/apps/)
INSTALL(FILES data/icons/256x256/gimagereader.png DESTINATION share/icons/hicolor/256x256/apps/)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/data/gimagereader-${INTERFACE_TYPE}.appdata.xml DESTINATION share/metainfo)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/data/gimagereader-${INTERFACE_TYPE}.desktop DESTINATION share/applications)
INSTALL(FILES ${manualFiles} DESTINATION "${MANUAL_DIR}")


# Dist
ADD_CUSTOM_TARGET(dist
    COMMAND git archive --format=tar --prefix=${CMAKE_PROJECT_NAME}-${PACKAGE_VERSION}/ HEAD | xz > ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-${PACKAGE_VERSION}.tar.xz
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

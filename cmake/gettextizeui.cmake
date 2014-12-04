# Function to replace QApplication::translate with gettext() in ui headers
# 3: argv0 = cmake, argv1 = -P, argv2 = gettextizeui.cmake
FOREACH(i RANGE 3 ${CMAKE_ARGC})
	SET(form_header "${CMAKE_ARGV${i}}")
	IF(NOT "${form_header}" STREQUAL "")
		MESSAGE(STATUS "Gettextizing ${form_header}")
		FILE(READ ${form_header} _text)
		STRING(REGEX REPLACE "QApplication::translate\\(\"[^\"]+\", \"((\\\\.|[^\\\"])*)\", [^\\)]+\\)" "gettext(\"\\1\")" _text_out "${_text}")
		FILE(WRITE ${form_header} "${_text_out}")
	ENDIF()
ENDFOREACH()


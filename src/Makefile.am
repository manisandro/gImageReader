AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\" \
	$(GIMAGEREADER_CFLAGS)

AM_CFLAGS = -Wall -g

bin_PROGRAMS = gimagereader

gimagereader_SOURCES = \
	Acquirer.cc \
	Config.cc \
	ConfigSettings.cc \
	Displayer.cc \
	DisplayRenderer.cc \
	DisplayManipulator.cc \
	DisplaySelection.cc \
	main.cc \
	MainWindow.cc \
	Notifier.cc \
	OutputManager.cc \
	Recognizer.cc \
	SaneScanner.cc \
	SourceManager.cc \
	UndoableBuffer.cc \
	Utils.cc

gimagereader_LDFLAGS =

gimagereader_LDADD = $(GIMAGEREADER_LIBS)

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\" \
	$(GIMAGEREADER_CFLAGS)

AM_CFLAGS = -Wall -g

bin_PROGRAMS = gimagereader

gimagereader_SOURCES = \
	main.cc \
	common.hh \
	Acquirer.cc \
	Acquirer.hh \
	AsyncQueue.hh \
	Config.cc \
	Config.hh \
	ConfigSettings.cc \
	ConfigSettings.hh \
	Displayer.cc \
	Displayer.hh \
	DisplayRenderer.cc \
	DisplayRenderer.hh \
	DisplayManipulator.cc \
	DisplayManipulator.hh \
	DisplaySelection.cc \
	DisplaySelection.hh \
	Geometry.hh \
	MainWindow.cc \
	MainWindow.hh \
	Notifier.cc \
	Notifier.hh \
	OutputManager.cc \
	OutputManager.hh \
	Recognizer.cc \
	Recognizer.hh \
	Scanner.cc \
	Scanner.hh \
	ScannerSane.hh \
	ScannerTwain.hh \
	SourceManager.cc \
	SourceManager.hh \
	UndoableBuffer.cc \
	UndoableBuffer.hh \
	Utils.cc \
	Utils.hh \
	twain.h

gimagereader_LDFLAGS =

gimagereader_LDADD = $(GIMAGEREADER_LIBS) -ldl

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\" \
	$(GIMAGEREADER_CFLAGS) $(SANE_CFLAGS)

AM_CFLAGS = -Wall -g

bin_PROGRAMS = gimagereader

gimagereader_SOURCES = \
	main.cc \
	common.hh \
	Acquirer.cc \
	Acquirer.hh \
	Application.hh \
	AsyncQueue.hh \
	Config.cc \
	Config.hh \
	ConfigSettings.cc \
	ConfigSettings.hh \
	CrashHandler.cc \
	CrashHandler.hh \
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
	Utils.hh

gimagereader_LDFLAGS =

gimagereader_LDADD = $(GIMAGEREADER_LIBS) $(SANE_LIBS) -ldl


if PLATFORM_WIN32
gimagereader_LDFLAGS += -mwindows
gimagereader_LDADD += gimagereader-icon.o

gimagereader-icon.o: $(top_srcdir)/packaging/win32/gimagereader-icon.rc $(top_srcdir)/packaging/win32/gimagereader.ico
	$(AM_V_GEN)$(WINDRES) $(top_srcdir)/packaging/win32/gimagereader-icon.rc -I $(top_srcdir)/packaging/win32 gimagereader-icon.o

endif

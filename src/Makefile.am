## Process this file with automake to produce Makefile.in
#####################################
##Standard GCC Makefile.am:
#####################################
SUBDIRS = net

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = gnaural

gnaural_SOURCES = \
	main.c \
	main.h \
	main_ArrayConstants.h \
	BinauralBeat.c \
	BinauralBeat.h \
	ScheduleGUI.c \
	ScheduleGUI.h \
	ScheduleXML.c \
	ScheduleXML.h \
	callbacks.c \
	callbacks.h \
	exportAudio.c \
	exportAudio.h \
	voiceGUI.c \
	voiceGUI.h \
	playAudio.c \
	playAudio.h \
	gnauralnet.h \
	gnauralnet_main.c \
	gnauralnet_lists.c \
	gnauralnet_clocksync.c \
	gnauralnet_socket.c \
	gnauralnet_running.c \
	gnauralnet_gui.c \
	gettext.h \
	gnauralXML.c \
	gnauralXML.h \
	gnauralRecentMenu.c \
	gnauralRecentMenu.h \
	gnauralVU.c \
	gnauralVU.h \
	gnaural.glade

gnaural_LDADD = @PACKAGE_LIBS@ $(INTLLIBS) 
gnaural_LDFLAGS = @LDFLAGS@ 
#gnaural_LDFLAGS = @LDFLAGS@ -Wl,--as-needed -Wl,--no-undefined 
#####################################

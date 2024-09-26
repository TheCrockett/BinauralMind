## Process this file with automake to produce Makefile.in
#####################################
##Standard GCC Makefile.am:
#####################################
INCLUDES = \
	-DGN_STANDALONE \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	@PACKAGE_CFLAGS@

noinst_PROGRAMS =  gnauralnet_client_example \
 gnauralnet_flasher \
 gnauralscape 

gnauralnet_client_example_SOURCES = gnauralnet_client_example.c \
	../gnauralnet_clocksync.c \
	../gnauralnet_lists.c \
	../gnauralnet_main.c \
	../gnauralnet_socket.c 

gnauralnet_client_example_LDADD = @PACKAGE_LIBS@ $(INTLLIBS) 
gnauralnet_client_example_LDFLAGS =`pkg-config --libs glib-2.0`

gnauralnet_flasher_SOURCES = gnauralnet_flasher.c \
	gnauralnet_flasher.h \
	../gnauralnet_clocksync.c \
	../gnauralnet_lists.c \
	../gnauralnet_main.c \
	../gnauralnet_socket.c 

gnauralnet_flasher_LDADD = @PACKAGE_LIBS@ $(INTLLIBS) 
gnauralnet_flasher_LDFLAGS =`pkg-config --libs glib-2.0` @LDFLAGS@ 

gnauralscape_SOURCES = gnauralscape.c \
	MakeLand.c \
	MakeLand.h \
	LandCaster.c \
	LandCaster.h 

gnauralscape_LDADD = @PACKAGE_LIBS@ $(INTLLIBS) 
gnauralscape_LDFLAGS =`pkg-config --libs glib-2.0` @LDFLAGS@ 
#####################################

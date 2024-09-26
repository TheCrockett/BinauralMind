## Process this file with automake to produce Makefile.in

SUBDIRS = src po

EXTRA_DIST = config.rpath m4/ChangeLog  \
	autogen.sh

#gnomemenudir = $(DESTDIR)$(datadir)$(packagemenudir)
#gnomemenudir = $(DESTDIR)$(datadir)/gnome/apps/Multimedia
#gnomemenu_DATA = gnaural.desktop

#gnaural_glade_filedir = $(DESTDIR)$(pkgdatadir)
#gnaural_glade_file_DATA = gnaural.glade

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(pkgdatadir)/pixmaps; \
	    fi \
	  done \
	fi; \
	$(mkinstalldirs) $(DESTDIR)$(datadir)/gnome/apps/Multimedia; \
	$(INSTALL_DATA) gnaural.desktop $(DESTDIR)$(datadir)/gnome/apps/Multimedia; \
	$(INSTALL_DATA) src/gnaural.glade $(DESTDIR)$(pkgdatadir);

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi

ACLOCAL_AMFLAGS = -I m4

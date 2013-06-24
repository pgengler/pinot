localedir = $(datadir)/locale
INCLUDES = -DLOCALEDIR=\"$(localedir)\" -DSYSCONFDIR=\"$(sysconfdir)\"

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = 	pinot
pinot_SOURCES =	browser.cpp \
		chars.cpp \
		color.cpp \
		cut.cpp \
		files.cpp \
		global.cpp \
		help.cpp \
		macros.h \
		move.cpp \
		pinot.cpp \
		pinot.h \
		prompt.cpp \
		proto.h \
		rcfile.cpp \
		search.cpp \
		syntax.cpp \
		text.cpp \
		utils.cpp \
		winio.cpp

pinot_LDADD = @GLIB_LIBS@ @LIBINTL@

install-exec-hook:
	cd $(DESTDIR)$(bindir) && rm -f rpinot && $(LN_S) pinot rpinot
uninstall-hook:
	cd $(DESTDIR)$(bindir) && rm -f rpinot

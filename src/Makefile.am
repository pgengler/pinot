localedir = $(datadir)/locale
AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" -DSYSCONFDIR=\"$(sysconfdir)\" -I$(top_builddir)/libtermkey-0.17

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = 	pinot
pinot_SOURCES =	\
	EventQueue.cpp \
	browser.cpp \
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

pinot_LDADD = @GLIB_LIBS@ @LIBINTL@ $(top_builddir)/libtermkey-0.17/libtermkey.la

install-exec-hook:
	cd $(DESTDIR)$(bindir) && rm -f rpinot && $(LN_S) pinot rpinot
uninstall-hook:
	cd $(DESTDIR)$(bindir) && rm -f rpinot

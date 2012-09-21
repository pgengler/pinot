localedir = $(datadir)/locale
INCLUDES = -DLOCALEDIR=\"$(localedir)\" -DSYSCONFDIR=\"$(sysconfdir)\"

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = 	pinot
pinot_SOURCES =	browser.c \
		chars.c \
		color.c \
		cut.c \
		files.c \
		global.c \
		help.c \
		move.c \
		pinot.c \
		pinot.h \
		prompt.c \
		proto.h \
		rcfile.c \
		search.c \
		text.c \
		utils.c \
		winio.c 

pinot_LDADD = @GLIB_LIBS@ @LIBINTL@

install-exec-hook:
	cd $(DESTDIR)$(bindir) && rm -f rpinot && $(LN_S) pinot rpinot
uninstall-hook:
	cd $(DESTDIR)$(bindir) && rm -f rpinot

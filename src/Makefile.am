AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" -DSYSCONFDIR=\"$(sysconfdir)\" -I$(top_builddir)/libtermkey-0.17 -Wall -Wextra -Wno-sign-compare

bin_PROGRAMS = 	pinot
pinot_SOURCES =	\
	Keyboard.cpp \
	OpenFile.cpp \
	browser.cpp \
	chars.cpp \
	color.cpp \
	cpputil.cpp \
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

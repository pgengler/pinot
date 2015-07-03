# Configuration for pinot - a small and user-friendly text editor
#
# Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
# Free Software Foundation, Inc.
#
# pinot is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3, or (at your option) any
# later version.
#
# pinot is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with pinot; if not, write to the Free Software Foundation, Inc.,
## 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
#
AC_INIT([pinot], [0.1.0], [phil@pgengler.net], [pinot])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/pinot.cpp])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config.h])

AC_PREREQ([2.68])

dnl Checks for programs.

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_LN_S
AC_SEARCH_LIBS([strerror],[cposix])
AC_SYS_LARGEFILE

AX_CXX_COMPILE_STDCXX_11

dnl Internationalization macros.

AM_GNU_GETTEXT_VERSION(0.11.5)
AM_GNU_GETTEXT([external], [need-ngettext])
AM_CONDITIONAL(USE_NLS, test x$USE_NLS = xyes)

dnl Data location.

pkgdatadir=${datadir}/${PACKAGE}
AC_DEFINE_DIR([PKGDATADIR], [pkgdatadir], [Where data are placed to.])

dnl Checks for header files.

AC_CHECK_HEADERS(getopt.h libintl.h limits.h pcreposix.h sys/param.h wchar.h wctype.h stdarg.h magic.h)

dnl Checks for options.

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable debugging (disabled by default)]), [
  if test "x$enableval" = xyes; then
    AC_DEFINE(DEBUG, 1, [Define this to enable debug messages and assert warnings.])
  else
    AC_DEFINE(NDEBUG, 1, [Shut up assert warnings :-)])
  fi
])

AC_ARG_ENABLE(speller, AS_HELP_STRING([--enable-speller], [Enable spell checker functions]), [
  if test "x$enableval" = xyes; then
    AC_DEFINE(ENABLE_SPELLER, 1, [Define this to enable the spell checker functions.])
  fi
])

AC_ARG_ENABLE(wrapping-as-root, AS_HELP_STRING([--disable-wrapping-as-root], [Disable wrapping of text as root by default]), [
  if test "x$enableval" != xyes; then
    AC_DEFINE(DISABLE_ROOTWRAPPING, 1, [Define this to disable text wrapping as root by default.])
  fi
])

AC_ARG_ENABLE(libmagic, AS_HELP_STRING([--disable-libmagic], [Disable detection of file types via libmagic]))

AC_MSG_CHECKING([whether to use slang])
CURSES_LIB_NAME=""
AC_ARG_WITH(slang, AS_HELP_STRING([--with-slang[=DIR]], [Use the slang library instead of curses]), [
  case "$with_slang" in
    no)
      AC_MSG_RESULT(no)
      ;;
    *)
      AC_MSG_RESULT(yes)

	if test x$with_slang != xyes; then
	    # Add additional search path
	    LDFLAGS="-L$with_slang/lib $LDFLAGS"
	    CPPFLAGS="-I$with_slang/include $CPPFLAGS"
	fi

  AC_CHECK_HEADER(slcurses.h,
		AC_MSG_CHECKING([for SLutf8_enable in -lslang])
		_libs=$LIBS
		LIBS="$LIBS -lslang"
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <slcurses.h>
int main(void)
{
    SLutf8_enable(1);
    return 0;
}]])],[AC_MSG_RESULT(yes)
		    AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
		    CURSES_LIB_WIDE=yes
		    if test x$with_slang != xyes; then
			CURSES_LIB="-L${with_slang}/lib -lslang"
		    else
			CURSES_LIB="-lslang"
		    fi
		    CURSES_LIB_NAME=slang],[AC_MSG_RESULT(no)

		    # We might need the term library
		    for termlib in ncurses curses termcap terminfo termlib; do
			AC_CHECK_LIB([${termlib}], tputs, [tcap="-l$termlib"])
			test -n "$tcap" && break
		    done

		    AC_MSG_CHECKING([for SLutf8_enable in -lslang $tcap])
		    LIBS="$LIBS $tcap"
		    AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLutf8_enable(1);
    return 0;
}],
			[AC_MSG_RESULT(yes)
			AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
			CURSES_LIB_WIDE=yes
			if test x$with_slang != xyes; then
			    CURSES_LIB="-L${with_slang}/lib -lslang $tcap"
			else
			    CURSES_LIB="-lslang $tcap"
			fi
			CURSES_LIB_NAME=slang],
			[AC_MSG_RESULT(no)

			# We might need the math library
			AC_MSG_CHECKING([for SLutf8_enable in -lslang $tcap -lm])
			LIBS="$LIBS -lm"
			AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLutf8_enable(1);
    return 0;
}],
			    [AC_MSG_RESULT(yes)
			    AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
			    CURSES_LIB_WIDE=yes
			    if test x$with_slang != xyes; then
				CURSES_LIB="-L${with_slang}/lib -lslang $tcap -lm"
			    else
				CURSES_LIB="-lslang $tcap -lm"
			    fi
			    CURSES_LIB_NAME=slang],
			    [AC_MSG_RESULT(no)],
    AC_MSG_WARN([*** Can not use slang when cross-compiling]))],
    AC_MSG_WARN([*** Can not use slang when cross-compiling]))],[AC_MSG_WARN(*** Can not use slang when cross-compiling)]),
	    AC_MSG_ERROR([
*** The header file slcurses.h was not found.  If you wish to use
*** slang support this header file is required.  Please either
*** install a version of slang that includes the slcurses.h file or
*** do not call the configure script with --with-slang.]))

	if eval "test x$CURSES_LIB_NAME = x"; then
	    # Reset libs if the above slang tests failed
	    AC_CHECK_HEADER(slcurses.h,
		AC_MSG_CHECKING([for SLtt_initialize in -lslang])
		_libs=$LIBS
		LIBS="$LIBS -lslang"
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <slcurses.h>
int main(void)
{
    SLtt_initialize(NULL);
    return 0;
}]])],[AC_MSG_RESULT(yes)
		    AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
		    if test x$with_slang != xyes; then
			CURSES_LIB="-L${with_slang}/lib -lslang"
		    else
			CURSES_LIB="-lslang"
		    fi
		    CURSES_LIB_NAME=slang],[AC_MSG_RESULT(no)

		    # We might need the term library
		    for termlib in ncurses curses termcap terminfo termlib; do
			AC_CHECK_LIB([${termlib}], tputs, [tcap="-l$termlib"])
			test -n "$tcap" && break
		    done

		    AC_MSG_CHECKING([for SLtt_initialize in -lslang $tcap])
		    LIBS="$LIBS $tcap"
		    AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLtt_initialize(NULL);
    return 0;
}],
			[AC_MSG_RESULT(yes)
			AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
			if test x$with_slang != xyes; then
			    CURSES_LIB="-L${with_slang}/lib -lslang $tcap"
			else
			    CURSES_LIB="-lslang $tcap"
			fi
			CURSES_LIB_NAME=slang],
			[AC_MSG_RESULT(no)

			# We might need the math library
			AC_MSG_CHECKING([for SLtt_initialize in -lslang $tcap -lm])
			LIBS="$LIBS -lm"
			AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLtt_initialize(NULL);
    return 0;
}],
			    [AC_MSG_RESULT(yes)
			    AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
			    if test "x$with_slang" != xyes; then
				CURSES_LIB="-L${with_slang}/lib -lslang $tcap -lm"
			    else
				CURSES_LIB="-lslang $tcap -lm"
			    fi
			    CURSES_LIB_NAME=slang],
			    [AC_MSG_RESULT(no)],
    AC_MSG_WARN([*** Can not use slang when cross-compiling]))],
    AC_MSG_WARN([*** Can not use slang when cross-compiling]))],
    AC_MSG_WARN([*** Can not use slang when cross-compiling])),
	    AC_MSG_ERROR([
*** The header file slcurses.h was not found.  If you wish to use
*** slang support this header file is required.  Please either
*** install a version of slang that includes the slcurses.h file or
*** do not call the configure script with --with-slang.]))
	fi

	if test "${_libs+set}" = "set"; then
	    LIBS=$_libs
	fi

	if test x$with_slang != xyes; then
	    LDFLAGS=${_ldflags}
	fi
	;;
    esac], [AC_MSG_RESULT(no)])

if test x$CURSES_LIB_NAME = xslang; then
    AC_DEFINE(DISABLE_BROWSER, 1, [Define this to disable the built-in file browser.])
    AC_DEFINE(DISABLE_MOUSE, 1, [Define this to disable the mouse support.])
    AC_DEFINE(DISABLE_OPERATINGDIR, 1, [Define this to disable the setting of the operating directory (chroot of sorts).])
    AC_DEFINE(DISABLE_WRAPPING, 1, [Define this to disable all text wrapping.])
fi

AC_ARG_ENABLE(altrcname, AS_HELP_STRING([--enable-altrcname], [Specify alternate rcfile name (default: .pinotrc)]), [
  if test "x$enableval" != no; then
    AC_DEFINE_UNQUOTED(RCFILE_NAME, "$enableval", [Specify alternate rcfile name (default: .pinotrc)]) rcfilename=$enableval
  fi
])

dnl Checks for functions.

AC_CHECK_FUNCS(getdelim getline isblank strcasecmp strcasestr strncasecmp strnlen snprintf vsnprintf)
AC_CHECK_FUNCS(iswalnum iswblank iswpunct iswspace nl_langinfo mblen mbstowcs mbtowc wctomb wcwidth)

if test x$ac_cv_func_snprintf = xno; then
    AM_PATH_GLIB_2_0(2.0.0,,
	AC_MSG_ERROR([
*** snprintf() not found.  GLIB 2.x not found either.  You should
*** install the GLIB 2.x library which can be found at ftp://ftp.gtk.org/.]),
	glib)
fi

if test x$ac_cv_func_vsnprintf = xno; then
    AM_PATH_GLIB_2_0(2.0.0,,
	AC_MSG_ERROR([
*** vsnprintf() not found.  GLIB 2.x not found either.  You should
*** install the GLIB 2.x library which can be found at ftp://ftp.gtk.org/.]),
	glib)
fi

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(getopt_long)
dnl Checks for libraries.

if eval "test x$CURSES_LIB_NAME = x"; then
    AC_CHECK_HEADERS(ncurses.h)

	OLDLIBS="$LIBS"
	AC_CHECK_TOOL(NCURSESW_CONFIG, ncursesw5-config, no)
	if test "x$NCURSESW_CONFIG" != xno; then
	    CURSES_LIB=`$NCURSESW_CONFIG --libs`
	    LIBS="$CURSES_LIB $LIBS"
	    CPPFLAGS="`$NCURSESW_CONFIG --cflags` $CPPFLAGS"
	    AC_CHECK_LIB(ncursesw, get_wch, [CURSES_LIB_NAME=ncursesw CURSES_LIB_WIDE=yes])
	else
	    AC_CHECK_LIB(ncursesw, get_wch, [CURSES_LIB="-lncursesw" CURSES_LIB_NAME=ncursesw CURSES_LIB_WIDE=yes])
	fi
	LIBS="$OLDLIBS"

    if eval "test x$CURSES_LIB_NAME = x"; then
	AC_CHECK_LIB(ncurses, initscr, [CURSES_LIB="-lncurses" CURSES_LIB_NAME=ncurses])
    fi
fi

if eval "test x$CURSES_LIB_NAME = x"; then
    AC_CHECK_HEADERS(curses.h)

	AC_CHECK_LIB(curses, get_wch, [CURSES_LIB="-lcurses" CURSES_LIB_NAME=curses CURSES_LIB_WIDE=yes])

    if eval "test x$CURSES_LIB_NAME = x"; then
	 AC_CHECK_LIB(curses, initscr, [CURSES_LIB="-lcurses" CURSES_LIB_NAME=curses])
    fi
fi

if eval "test x$CURSES_LIB_NAME = x"; then
    AC_MSG_WARN([
*** No curses lib available.  Consider getting the official ncurses
*** distribution from ftp://ftp.gnu.org/pub/gnu/ncurses if you get
*** errors compiling pinot.])
else
    AC_MSG_RESULT([Using $CURSES_LIB_NAME as the curses library])
fi

AC_CHECK_LIB([$CURSES_LIB_NAME], use_default_colors, AC_DEFINE(HAVE_USE_DEFAULT_COLORS, 1, [Define this if your curses library has the use_default_colors() command.]))

dnl Parse any configure options.

LIBS="$LIBS $CURSES_LIB"

AC_SUBST(CURSES_LIB)

if test "x$GLIB_CFLAGS" != "x"; then
    CFLAGS="$CFLAGS $GLIB_CFLAGS"
fi
if test "x$GLIB_LIBS" != "x"; then
    LDFLAGS="$LDFLAGS $GLIB_LIBS"
fi

AC_MSG_CHECKING([whether LINES and COLS can be redefined])
AC_TRY_RUN([
#include <curses.h>
int main(void)
{
    LINES = 80;
    COLS = 25;
    return 0;
}],
AC_DEFINE(REDEFINING_MACROS_OK, 1, [Define this if you know your curses library allows LINES and COLS to be redefined to deal with a resizing bug])
AC_MSG_RESULT(yes),
AC_MSG_RESULT(no),
AC_MSG_WARN([*** Can't check for macro redefinability when cross-compiling]))

AC_CHECK_HEADERS(pcreposix.h)
AC_CHECK_LIB(pcreposix, regcomp)
if test x$ac_cv_header_pcreposix_h = xno; then
    AC_MSG_ERROR([*** The header file pcreposix.h was not found.])
fi

AS_IF([test "x$enable_libmagic" != "xno"], [
	AC_CHECK_HEADERS_ONCE([magic.h])
	AC_CHECK_LIB(magic, magic_open)
])

# Check for groff html support
AC_MSG_CHECKING([for HTML support in groff])
groff -t -mandoc -Thtml </dev/null >/dev/null
if test $? -ne 0 ; then
    AC_MSG_RESULT([no])
    AC_MSG_WARN([*** Will not generate HTML version of man pages  ***
*** Consider installing a newer version of groff with HTML support ***])
    groff_html_support=no
else
    AC_MSG_RESULT([yes])
    groff_html_support=yes
fi
AM_CONDITIONAL(GROFF_HTML, test x$groff_html_support = xyes)

# Check for the availability of makeinfo.
AC_CHECK_PROG(haveit, makeinfo, yes, no)
AM_CONDITIONAL(HAVE_MAKEINFO, test x$haveit = xyes)

AC_CONFIG_FILES([
Makefile
doc/Makefile
doc/pinotrc.sample
doc/man/Makefile
doc/syntax/Makefile
doc/texinfo/Makefile
libtermkey-0.17/Makefile
m4/Makefile
po/Makefile.in
src/Makefile
pinot.spec
])

LT_INIT
AC_OUTPUT

info_TEXINFOS = nano.texi

MAKEINFO = makeinfo --no-split

BUILT_SOURCES = pinot.html

pinot.html: pinot.texi
	makeinfo --no-split --html < $< > $@

EXTRA_DIST = $(info_TEXINFOS) $(BUILT_SOURCES)

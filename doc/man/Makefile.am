BUILT_SOURCES =
dist_man_MANS = pinot.1 pinotrc.5
html_pages = pinot.1.html pinotrc.5.html

pinot.1.html: pinot.1
	groff -t -mandoc -Thtml < $? > $@
pinotrc.5.html: pinotrc.5
	groff -t -mandoc -Thtml < $? > $@

if GROFF_HTML
BUILT_SOURCES += $(html_pages)
dist_html_DATA = $(html_pages)
endif

EXTRA_DIST = $(BUILT_SOURCES)

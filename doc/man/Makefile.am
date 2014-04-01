BUILT_SOURCES =
dist_man_MANS = pinot.1 pinotrc.5
if GROFF_HTML
BUILT_SOURCES += pinot.1.html pinotrc.5.html
endif

if GROFF_HTML
dist_html_DATA = pinot.1.html pinotrc.5.html

pinot.1.html: pinot.1
	groff -t -mandoc -Thtml < $? > $@
pinotrc.5.html: pinotrc.5
	groff -t -mandoc -Thtml < $? > $@
endif

EXTRA_DIST = $(BUILT_SOURCES)

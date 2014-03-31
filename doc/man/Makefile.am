if USE_NLS
SUBDIRS = 
else
SUBDIRS =
endif

man_MANS = pinot.1 pinotrc.5 rpinot.1
if GROFF_HTML
BUILT_SOURCES = pinot.1.html pinotrc.5.html rpinot.1.html
endif

pinot_man_mans = pinot.1 pinotrc.5 rpinot.1
if GROFF_HTML
dist_html_DATA = pinot.1.html pinotrc.5.html rpinot.1.html
pinot_built_sources = pinot.1.html pinotrc.5.html rpinot.1.html

pinot.1.html: pinot.1
	groff -t -mandoc -Thtml < $? > $@
pinotrc.5.html: pinotrc.5
	groff -t -mandoc -Thtml < $? > $@
rpinot.1.html: rpinot.1
	groff -t -mandoc -Thtml < $? > $@

EXTRA_DIST = $(pinot_man_mans) $(pinot_built_sources)
else 
EXTRA_DIST = $(pinot_man_mans)
endif

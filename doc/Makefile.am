dist_noinst_DATA = GettingStarted.md Doxyfile
EXTRA_DIST = Doxyfile.in
CLEANFILES = Doxyfile doxygen

edit = $(SED) -e 's|@PACKAGE[@]|@PACKAGE@|g' \
              -e 's|@VERSION[@]|@VERSION@|g' \
              -e 's|@RELEASE[@]|@RELEASE@|g' \
              -e 's|@PACKAGE_VERSION[@]|@PACKAGE_VERSION@|g' \
              -e 's|@top_srcdir[@]|@top_srcdir@|g'

Doxyfile: $(srcdir)/Doxyfile.in
	$(edit) $< > $@

EXTRA_DIST = compat control.in copyright rules.in \
             veoffload.install.in veoffload-dev.install.in

dist_noinst_DATA = control rules veoffload.install veoffload-dev.install

CLEANFILES = $(dist_noinst_DATA)

edit = $(SED) -e 's|@PACKAGE[@]|@PACKAGE@|g' \
              -e 's|@VERSION[@]|@VERSION@|g' \
              -e 's|@RELEASE[@]|@RELEASE@|g' \
              -e 's|@prefix[@]|@prefix@|g' \
              -e 's|@includedir[@]|@includedir@|g' \
              -e 's|@libdir[@]|@libdir@|g' \
              -e 's|@localstatedir[@]|@localstatedir@|g' \
              -e 's|@VEORUN_BIN[@]|@VEORUN_BIN@|g' \
              -e 's|@VEOS_SOCKET[@]|@VEOS_SOCKET@|g'

control: $(srcdir)/control.in Makefile
	$(edit) $< > $@

rules: $(srcdir)/rules.in Makefile
	$(edit) $< > $@
	chmod a+x $@

veoffload.install: $(srcdir)/veoffload.install.in Makefile
	$(edit) $< > $@

veoffload-dev.install: $(srcdir)/veoffload-dev.install.in Makefile
	$(edit) $< > $@

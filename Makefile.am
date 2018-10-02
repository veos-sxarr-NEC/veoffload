ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src include debian doc
EXTRA_DIST = veo.spec.in examples
dist_noinst_DATA = veo.spec
CLEANFILES = veo.spec

edit = $(SED) -e 's|@PACKAGE[@]|@PACKAGE@|g' \
              -e 's|@VERSION[@]|@VERSION@|g' \
              -e 's|@RELEASE[@]|@RELEASE@|g' \
              -e 's|@prefix[@]|@prefix@|g' \
              -e 's|@libdir[@]|@libdir@|g' \
              -e 's|@includedir[@]|@includedir@|g' \
              -e 's|@localstatedir[@]|@localstatedir@|g' \
              -e 's|@VEORUN_BIN[@]|@VEORUN_BIN@|g' \
              -e 's|@VEOS_SOCKET[@]|@VEOS_SOCKET@|g'

veo.spec: $(srcdir)/veo.spec.in
	$(edit) $< > $@

lib_LTLIBRARIES = libveo.la
common_headers = $(top_srcdir)/include/ve_offload.h \
                 $(top_srcdir)/include/veo_private_defs.h
dist_noinst_DATA = libveo.map

libveo_la_SOURCES = $(common_headers) \
                    log.hpp log.cpp loader_veo.c \
                    api.cpp VEOException.hpp \
                    Command.hpp Command.cpp \
                    CallArgs.hpp CallArgs.cpp \
                    ProcHandle.cpp ProcHandle.hpp \
                    ThreadContext.cpp ThreadContext.hpp

libveo_la_CPPFLAGS = -DVEOS_SOCKET=\"$(VEOS_SOCKET)\" \
                     -DVE_DEV=\"@VE_DEV@\" -DVEORUN_BIN=\"@VEORUN_BIN@\" \
                     -D_GNU_SOURCE \
                     -I$(top_srcdir)/include \
                     -I$(ve_includedir) \
                     -I$(VEOS_SRC)/lib/libvepseudo/psm \
                     -I$(VEOS_SRC)/lib/libvepseudo/common \
                     -I$(VEOS_SRC)/lib/libvepseudo/amm \
                     -I$(VEOS_SRC)/lib/libvepseudo/loader \
                     -I$(VEOS_SRC)/include
# to avoid comilation error on ve_list.h
libveo_la_CXXFLAGS = -fpermissive

libveo_la_LDFLAGS = -lvepseudo -Wl,--version-script=$(srcdir)/libveo.map

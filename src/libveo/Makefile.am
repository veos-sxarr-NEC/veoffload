lib_LTLIBRARIES = libveo.la
common_headers = $(top_srcdir)/include/ve_offload.h \
                 $(top_srcdir)/include/veo_private_defs.h
dist_noinst_DATA = libveo.map

libveo_la_SOURCES = $(common_headers) \
                    log.hpp log.cpp loader_veo.c vars.c \
                    api.cpp VEOException.hpp \
                    CallArgs.hpp CallArgs.cpp \
                    Command.hpp Command.cpp \
                    ProcHandle.cpp ProcHandle.hpp \
                    CommandImpl.hpp \
                    ThreadContext.cpp ThreadContext.hpp \
                    AsyncTransfer.cpp

libveo_la_CPPFLAGS = -DVEOS_SOCKET=\"$(VEOS_SOCKET)\" \
                     -DVE_DEV=\"@VE_DEV@\" -DVEORUN_BIN=\"@VEORUN_BIN@\" \
                     -D_GNU_SOURCE \
                     -I$(top_srcdir)/include \
                     -I$(ve_includedir) \
                     -I$(includedir)/libvepseudo \
                     -I$(includedir)/libvepseudo/amm \
                     -I$(includedir)/libvepseudo/common \
                     -I$(includedir)/libvepseudo/loader \
                     -I$(includedir)/libvepseudo/psm
# to avoid comilation error on ve_list.h
libveo_la_CXXFLAGS = -fpermissive

libveo_la_LDFLAGS = -lvepseudo -Wl,--version-script=$(srcdir)/libveo.map

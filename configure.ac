AC_PREREQ([2.69])
AC_INIT([veoffload], [1.2.2])
AM_INIT_AUTOMAKE([foreign -Wall])
AC_CONFIG_SRCDIR([src/libveo/api.cpp])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_WITH([release-id], [AS_HELP_STRING([--with-release-id],
                                          [Specify the release ID [1]])],
            [RELEASE=${with_release_id}],
            [RELEASE=1])
AC_SUBST(RELEASE)

AC_ARG_WITH([veos-socket], [AS_HELP_STRING([--with-veos-socket],
    [VEOS socket name format [localstatedir/veos%d.sock]])],
    [VEOS_SOCKET=${with_veos_socket}],
    [VEOS_SOCKET=${localstatedir}/veos%d.sock])
AC_SUBST(VEOS_SOCKET)

AC_ARG_WITH([veprefix], [AS_HELP_STRING([--with-veprefix],
    [Prefix to VE programs (veorun) [/opt/nec/ve]])],
    [], [with_veprefix=/opt/nec/ve])

AC_ARG_WITH([velibexecdir], [AS_HELP_STRING([--with-velibexecdir],
    [libexecdir of VE packages [VEPREFIX/libexec]])],
    [], [with_velibexecdir="${with_veprefix}/libexec"])

AC_ARG_WITH([veorun_includedir], [AS_HELP_STRING([--with-veorun-includedir],
    [includedir of VE packages [VEPREFIX/include]])],
    [], [with_veorun_includedir="${with_veprefix}/include/veorun"])
veorun_includedir="${with_veorun_includedir}"
AC_SUBST(veorun_includedir)

AC_ARG_WITH([veorun], [AS_HELP_STRING([--with-veorun],
    [Path to veorun [/opt/nec/ve/libexec/veorun]])],
    [], [with_veorun=${with_velibexecdir}/veorun])
VEORUN_BIN="${with_veorun}"
AC_SUBST(VEORUN_BIN)

CPPFLAGS="$CPPFLAGS -I${includedir}"
LDFLAGS="$LDFLAGS -L${libdir}"
# make configurable if necessary.
VE_DEV="/dev/veslot%d"
AC_SUBST(VE_DEV)

# Checks for programs.
AC_PROG_CXX
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT

AX_CXX_COMPILE_STDCXX_11([], [mandatory])

# Checks for libraries.

# Checks for header files.
# check consistency between headers for veorun and those in this package.
orig_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="${CPPFLAGS} -I${veorun_includedir} -I${srcdir}/include"
AC_MSG_CHECKING([consistency between headers installed and in source.])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdint.h>
#include <veorun.h>
#include <ve_offload.h>
#if VEO_SYMNAME_LEN_MAX != VEORUN_SYMNAME_LEN_MAX
#error "VEO_SYMNAME_LEN_MAX != VEORUN_SYMNAME_LEN_MAX"
#endif
]], [[]])], [],
[AC_MSG_ERROR([inconsistent definition of VEO_SYMNAME_LEN_MAX])])
CPPFLAGS="$orig_CPPFLAGS"

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT64_T

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 src/Makefile
                 src/libveo/Makefile])
AC_OUTPUT

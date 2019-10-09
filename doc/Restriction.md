# Restriction

VEO does not support:
 - to use quadruple precision real number a variable length character string 
   as a return value and an argument of Fortran subroutines and functions,
 - to use multiple VEs by a VH process,
 - to re-create a VE process after the destruction of the VE process, and
 - to call API of VE DMA or VH-VE SHM on the VE side if VEO API is called from child thread on the VH side.

The current veo_context_close() implementation remains threads on the VE side glibc when calling veo_context_close().

By default, an OpenMP program statically linked can cause load imbalance of threads.
If you set environmental veriable of OMP_NUM_THREADS to number of ve cores minus one, loaded threads are balanced in case of using statically linked program.
By default, an OpenMP program for dynamic loading can load threads in balance.

Ported commands such as /opt/nec/ve/bin/ps and /opt/nec/ve/bin/top do not show VE processes when they are created by a child thread on VH side using VEO API.

Ported commands such as /opt/nec/ve/bin/free show memory used by these VE processes.

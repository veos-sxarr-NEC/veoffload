# Restriction

VEO does not support:
 - to call veo_proc_create() or veo_proc_create_static() in child thread;
 - to use VE program written in Fortran.
 - to use multiple VEs by a VH process.
 - to re-create a VE process after the destruction of the VE process.

The current veo_context_close() implementation remains threads on the VE side glibc 
when calling veo_context_close().

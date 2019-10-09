# Getting Started with VEO
VE Offloading framework (VEO) is a framework to provide accelerator-style
programming on Vector Engine (VE).

## Introduction
Recently the accelerator programming model has become popular due to
the wide usage of GPGPU and the increasing use of Xeon Phi processors.
The accelerator programming model executes parallelized and/or vectorized
numeric code such as matrix operations on accelerators and a main code
controlling accelerators and performing I/O on a host.
This programming model explicitly coded into parallel frameworks such as
OpenMP 4 (device construct) and OpenACC, and into lower level APIs
such as OpenCL and CUDA.

SX-Aurora TSUBASA provides VE Offloading framework (VEO) for
the accelerator programming model and enables to develop a program
using both VEs and VH computing resources.

## Hello World
First, let's try a "Hello, World!" program on VE.

### Install required packages
To run programs using VEO, please install veoffload-veorun, veoffload
and runtime packages of the compiler.

To develop programs using VEO, veoffload-veorun-devel, veoffload-devel
and development packages of compiler are also required.

To install the packages to run VEO programs by yum, execute
the following command as root:

~~~
# yum install veoffload
~~~

veoffload-veorun is automatically installed by dependency.

To install the packages to develop VEO programs by yum, execute
the following command as root:

~~~
# yum install veoffload-veorun-devel veoffload-devel
~~~

The following runtime package of the compiler are required to run VEO programs.
 * nec-nc++-shared-2.3.0-2.3.0-1 or later
 * nec-nc++-shared-inst-2-2.3.0-1.noarch or later
 * nec-nfort-shared-2.3.0-2.3.0-1.x86_64 or later
 * nec-nfort-shared-inst-2-2.3.0-1.noarch or later
 * nec-nfort-runtime-2.0.0-1.x86_64 or later
 * binutils-ve-2.26-2.2.x86_64.rpm or later

The following development packages of the compiler are also required to create a shared object for Fortran or C/C++ programs.
 * nec-nc++-2.3.0-2.3.0-1.x86_64 or later
 * nec-nc++-inst-2.3.0-1.noarch or later
 * nec-nc++-shared-devel-2.3.0-2.3.0-1.x86_64 or later
 * nec-nfort-2.3.0-2.3.0-1.x86_64 or later
 * nec-nfort-inst-2.3.0-1.noarch or later
 * nec-nfort-shared-devel-2.3.0-2.3.0-1.x86_64 or later

### VE Code
Code to run on VE is shown below. Standard C functions are available,
hence, you can use printf(3).

~~~c
#include <stdio.h>
#include <stdint.h>
uint64_t hello()
{
  printf("Hello, world\n");
  return 0;
}
~~~

Save the above code as libvehello.c.

A function on VE called via VEO needs to return a 64-bit unsigned integer.
A function on VE called via VEO can have arguments as mentioned later.

### Compile VE Code
VEO supports a function in an executable or in a shared library.

To execute a function on VE using VEO, compile and link a source file
into a binary for VE.

To build an executable with the functions statically linked, execute as follows:
~~~
$ /opt/nec/ve/bin/ncc -c -o libvehello.o libvehello.c
$ /opt/nec/ve/bin/mk_veorun_static -o vehello libvehello.o
~~~
To build a shared library with the functions for dynamic loading, execute as follows:
~~~
$ /opt/nec/ve/bin/ncc -fpic -shared -o libvehello.so libvehello.c
~~~

### VH Main Program
Main routine on VH side to run VE program is shown here.

A program using VEO needs to include "ve_offload.h".
In the header, the prototypes of VEO functions and constants for
VEO API are defined.

The example VH program to call a VE function in a statically linked executable:
~~~c
#include <ve_offload.h>
int main()
{
  /* Load "vehello" on VE node 0 */
  struct veo_proc_handle *proc = veo_proc_create_static(0, "./vehello");
  uint64_t handle = NULL;/* find a function in the executable */

  struct veo_thr_ctxt *ctx = veo_context_open(proc);

  struct veo_args *argp = veo_args_alloc();
  uint64_t id = veo_call_async_by_name(ctx, handle, "hello", argp);
  uint64_t retval;
  veo_call_wait_result(ctx, id, &retval);
  veo_args_free(argp);
  veo_context_close(ctx);
  return 0;
}
~~~
Save the above code as hello.c.

To call a VE function in a statically linked executable:
1. Create a process on a VE node by veo_proc_create_static().
 Specify VE node number and an executable to run on the VE.
 A VEO process handle is returned.
2. Create a VEO context, a thread in a VE process specified by a VEO process
 handle to execute a VE function, by veo_context_open().
3. Create a VEO arguments object by veo_args_alloc() and set arguments.
 See the next chapter "Various Arguments for a VE function" in detail.
4. Call a VE function by veo_call_async_by_name() with a symbol of a function or a variale
 and a VEO arguments object. A request ID is returned.
5. Wait for the completion and get the return value by veo_call_wait_result().

The example VH program to call a VE function in a dynamic library with VEO:
~~~c
#include <ve_offload.h>
int main()
{
  /* Load "vehello" on VE node 0 */
  struct veo_proc_handle *proc = veo_proc_create(0);
  uint64_t handle = veo_load_library(proc, "./libvehello.so");
  struct veo_thr_ctxt *ctx = veo_context_open(proc);

  struct veo_args *argp = veo_args_alloc();
  uint64_t id = veo_call_async_by_name(ctx, handle, "hello", argp);
  uint64_t retval;
  veo_call_wait_result(ctx, id, &retval);
  veo_args_free(argp);
  veo_context_close(ctx);
  return 0;
}
~~~
To call a VE function in a dynamic library with VEO:
1. Create a process on a VE node by veo_proc_create().
 Specify VE node number to create a VE process.
 A VEO process handle is returned.
2. Load a VE library and find an address of a function to call.
 veo_load_library() loads a VE shared library on the VE process.
3. Create a VEO context, a thread in a VE process specified by a VEO process
 handle to execute a VE function, by veo_context_open().
4. Create a VEO arguments object by veo_args_alloc() and set arguments.
 See the next chapter "Various Arguments for a VE function" in detail.
5. Call a VE function by veo_call_async_by_name() with the name of a function
 and a VEO arguments object. 
 A request ID is returned.
6. Wait for the completion and get the return value by veo_call_wait_result().

### Compile VH Main Program
Compile source code on VH side as shown below.

~~~
$ gcc -o hello hello.c -I/opt/nec/ve/veos/include -L/opt/nec/ve/veos/lib64 \
   -Wl,-rpath=/opt/nec/ve/veos/lib64 -lveo
~~~

The headers for VEO are installed in /opt/nec/ve/veos/include;
libveo, the shared library of VEO, is in /opt/nec/ve/veos/lib64.

### Run a program with VEO
Execute the compiled VEO program.

~~~
$ ./hello
Hello, world
~~~

VE code is executed on VE node 0, specified by `veo_proc_create_static()` or `veo_proc_create()`.

## Various Arguments for a VE function
You can pass one or more arguments to a function on VE.
To specify arguments, VEO arguments object is used.
A VEO argument object is created by veo_args_alloc().
When a VEO argument object is created, the VEO argument object is empty,
without any arguments passed.
Even if a VE function has no arguments, a VEO arguments object is still
necessary.

VEO provides functions to set an argument in various types.

### Basic Types
To pass an integer value, the following functions are used.

~~~c
int veo_args_set_i64(struct veo_args *args, int argnum, int64_t val);
int veo_args_set_u64(struct veo_args *args, int argnum, uint64_t val);
int veo_args_set_i32(struct veo_args *args, int argnum, int32_t val);
int veo_args_set_u32(struct veo_args *args, int argnum, uint32_t val);
int veo_args_set_i16(struct veo_args *args, int argnum, int16_t val);
int veo_args_set_u16(struct veo_args *args, int argnum, uint16_t val);
int veo_args_set_i8(struct veo_args *args, int argnum, int8_t val);
int veo_args_set_u8(struct veo_args *args, int argnum, uint8_t val);

~~~

You can pass also a floating point number argument.

~~~c
int veo_args_set_double(struct veo_args *args, int argnum, double val);
int veo_args_set_float(struct veo_args *args, int argnum, float val);
~~~

For instance: suppose that proc is a VEO process handle and
func(int, double) is defined in a VE library whose handle is handle.

~~~c
struct veo_args *args = veo_args_alloc();
veo_args_set_i32(args, 0, 1);
veo_args_set_double(args, 1, 2.0);
uint64_t id = veo_call_async_by_name(ctx, handle, "func", args);
~~~

In this case, func(1, 2.0) is called on VE.

### Stack Arguments
Non basic typed arguments and arguments by reference are put on a stack.
VEO supports an argument on a stack.

To set a stack argument to a VEO arguments object, call veo_args_set_stack().
~~~c
int veo_args_set_stack(struct veo_args *args, int argnum, veo_args_intent inout,
                       int argnum, char *buff, size_t len);
~~~

The third argument specifies the argument is for input and/or output.
 - VEO_INTENT_IN: the argument is for input; data is copied into a VE stack
  on call.
 - VEO_INTENT_OUT: the argument is for output; a VE stack area is allocated
  without copy-in and data is copied out to VH memory on completion.
 - VEO_INTENT_INOUT: the argument is for both input and output;
  data is copied into and out from a VE stack area.

##How to call the function written by Fortran

###VE Code (Fortran)
Code written by Fortran to run on VE is shown below.

~~~c
SUBROUTINE SUB1(x, ret)
  implicit none
  INTEGER, INTENT(IN) :: x
  INTEGER, INTENT(OUT) :: ret
  ret = x + 1
END SUBROUTINE SUB1

INTEGER FUNCTION FUNC1(x, y)
  implicit none
  INTEGER, VALUE :: x, y
  FUNC1 = x + y
END FUNCTION FUNC1
~~~
Save the above code as libvefortran.f90.

###Compile VE Code (Fortran)

To build an executable with the functions statically linked, execute as follows:
~~~
$ /opt/nec/ve/bin/nfort -c -o libvefortran.o libvefortran.f90
$ /opt/nec/ve/bin/mk_veorun_static -o vefortran libvefortran.o
~~~
To build a shared library with the functions for dynamic loading, execute as follows:
~~~
$ /opt/nec/ve/bin/nfort -shared -fpic -o libvefortran.so libvefortran.f90
~~~

### VH Main Program (Fortran)
Main routine on VH side to run VE program written by Fortran is shown here.

The example VH program to call a VE Fortran function in a statically linked executable:
~~~c
#include <ve_offload.h>
int main()
{
  /* Load "vefortran" on VE node 0 */
  struct veo_proc_handle *proc = veo_proc_create_static(0, "./vefortran");
  uint64_t handle = NULL;/* find a function in the executable */
  struct veo_thr_ctxt *ctx = veo_context_open(proc);
  struct veo_args *argp = veo_args_alloc();
  long x = 42;
  long y;
  veo_args_set_stack(argp, VEO_INTENT_IN, 0, &x, sizeof(x));
  veo_args_set_stack(argp, VEO_INTENT_OUT, 1, &y, sizeof(y));
  uint64_t id = veo_call_async_by_name(ctx, handle, "sub1_", argp);
  uint64_t retval;
  veo_call_wait_result(ctx, id, &retval);
  printf("SUB1 return %lu\n", retval);
  veo_args_clear(argp);
  veo_args_set_i64(argp, 0, 1);
  veo_args_set_i64(argp, 1, 2);
  id = veo_call_async_by_name(ctx, handle, "func1_", argp);
  veo_call_wait_result(ctx, id, &retval);
  printf("FUNC1 return %lu\n", retval);
  veo_args_free(argp);
  veo_context_close(ctx);
  return 0;
}
~~~
Save the above code as fortran.c.

If you want to pass arguments to VE Fortran function, please use veo_args_set_stack() to pass arguments as stack arguments.
However if you want to pass arguments to arguments with VALUE attribute in Fortran function, please pass arguments by value in the same way as VE C function.

When you want to call VE Fortran function by veo_call_async_by_name() with the name of a Fortran function, 
please change the name of the Fortran function to lowercase, and add "_" at the end of the function name.

Taking libvefortran.f90 and fortran.c as an example, pass "sub1_" as a argument to veo_call_async_by_name() in fortran.c when calling the Fortran function named "SUB1" in libvefortran.f90.

The method of compiling and running VH main program are same as C program.

### Compile VH Main Program (Fortran)
Compile source code on VH side as shown below.
This is the same as the compilation method described above.

~~~
$ gcc -o fortran fortran.c -I/opt/nec/ve/veos/include -L/opt/nec/ve/veos/lib64 \
   -Wl,-rpath=/opt/nec/ve/veos/lib64 -lveo
~~~
### Run a program with VEO
Execute the compiled VEO program.
This is also the same as the execution method described above.

~~~
$ ./fortran
SUB1 return 43
FUNC1 return 3
~~~

##How to get log file
Create a file named .log4crc in your home directory.
Copy the following lines to .log4crc.
~~~c
<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE log4c SYSTEM "">

<log4c>
        <config>
                <bufsize>1024</bufsize>
                <debug level="0"/>
                <nocleanup>0</nocleanup>
        </config>

        <category name="veos.veo" priority="DEBUG" appender="veo_appender" />
        <appender name="veo_appender" layout="ve" type="rollingfile" rollingpolicy="veo_rp" logdir="." prefix="veo.log"/>
        <rollingpolicy name="veo_rp" type="sizewin" maxsize="4194304" maxnum="10" />
        <layout name="ve" type="ve_layout"/>
</log4c>
~~~
When VEO program is executed, a log file named veo.log.* is created in the current directory.
veo.log.* contains VEO and pseudo log.
If you want to separate VEO and pseudo log, edit .log4crc in your home directory as follows.

~~~c
        <category name="veos.veo.veo" priority="DEBUG" appender="veo_appender" />
        <appender name="veo_appender" layout="ve" type="rollingfile" rollingpolicy="veo_rp" logdir="." prefix="veo.log"/>
        <rollingpolicy name="veo_rp" type="sizewin" maxsize="4194304" maxnum="10" />

        <category name="veos.veo.pseudo" priority="DEBUG" appender="veo_pseudo_appender" />
        <appender name="veo_pseudo_appender" layout="ve" type="rollingfile" rollingpolicy="veo_pseudo_rp" logdir="." prefix="veo.pseudo.log"/>
        <rollingpolicy name="veo_pseudo_rp" type="sizewin" maxsize="4194304" maxnum="10" />
~~~

To separate VEO and pseudo log file:
1. Change `category name` `"veos.veo"` to `"veos.veo.veo"` and set `priority`.
 Because VEO supports `"TRACE"`, `"DEBUG"` and `"ERROR"` priorities,
 you can set `priority` to either `"TRACE"`, `"DEBUG"` or `"ERROR"`.
2. Add the line `<category name="veos.veo.pseudo" ... />` or below.
 These lines set the pseudo log parameters.

After .log4crc is edited, a log file named veo.log.* and 
veo.pseudo.log.* are created in the current directory 
when VEO program is executed.

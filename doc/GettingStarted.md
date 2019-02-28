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

### Install VEO Packages
To run programs using VEO, please install veoffload-veorun and veoffload
packages.
To develop programs using VEO, veoffload-devel package is also necessary.

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
To execute a function on VE using VEO, compile and link the source file
into an executable for VEO.

~~~
$ /opt/nec/ve/bin/ncc -c -o libvehello.o libvehello.c
$ /opt/nec/ve/libexec/mk_veorun_static vehello libvehello.o -pthread -ldl
~~~

### VH Main Program
Main routine to run VE program is shown below.

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

A program using VEO needs to include "ve_offload.h".
In the header, the prototypes of VEO functions and constants for
VEO API are defined.

To execute a VE function with VEO:
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

VE code is executed on VE node 0, specified by `veo_proc_create_static()`.

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

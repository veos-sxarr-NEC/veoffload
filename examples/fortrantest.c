/**
 * gcc -o fortrantest fortrantest.c -I/opt/nec/ve/veos/include -L/opt/nec/ve/veos/lib64 -Wl,-rpath=/opt/nec/ve/veos/lib64 -lveo
 */
#include <stdio.h>
#include <stdlib.h>
#include <ve_offload.h>

int
main()
{
  struct veo_proc_handle *proc = veo_proc_create(0);
  if (proc == NULL) {
    perror("veo_proc_create");
    exit(1);
  }

  printf("proc %p is created\n", proc);
  uint64_t hdl = veo_load_library(proc, "./libfortrantest.so");
  printf("library handle %#lx\n", hdl);
  struct veo_thr_ctxt *ctx = veo_context_open(proc);
  if (ctx == NULL) {
    perror("veo_context_open");
    exit(1);
  }
  printf("context %p is opened\n", ctx);
  struct veo_args *arg = veo_args_alloc();
  long x = 42;
  long y;
  veo_args_set_stack(arg, VEO_INTENT_IN, 0, &x, sizeof(x));
  veo_args_set_stack(arg, VEO_INTENT_OUT, 1, &y, sizeof(y));
  uint64_t id0 = veo_call_async_by_name(ctx, hdl, "sub1_", arg);

  uint64_t rv;
  int status;
  status = veo_call_wait_result(ctx, id0, &rv);
  printf("id #%d: %d (return value = %lu)\n", id0, status, rv);
  printf("y = %d\n", y);
  veo_args_clear(arg);

  veo_args_set_i64(arg, 0, 1);
  veo_args_set_i64(arg, 1, 2);
  uint64_t id1 = veo_call_async_by_name(ctx, hdl, "func1_", arg);
  status = veo_call_wait_result(ctx, id1, &rv);
  printf("id #%d: %d (return value = %lu)\n", id1, status, rv);

  veo_args_free(arg);
  veo_context_close(ctx);
  return 0;
}

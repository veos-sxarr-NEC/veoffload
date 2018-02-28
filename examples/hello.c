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
  uint64_t handle = veo_load_library(proc, "./libvehello.so");
  printf("handle = %p\n", (void *)handle);
  uint64_t sym = veo_get_sym(proc, handle, "hello");
  printf("symbol address = %p\n", (void *)sym);

  struct veo_thr_ctxt *ctx = veo_context_open(proc);
  printf("VEO context = %p\n", ctx);
  struct veo_args *arg = veo_args_alloc();
  veo_args_set_i64(arg, 0, 42);
  uint64_t id = veo_call_async(ctx, sym, arg);
  printf("VEO request ID = 0x%lx\n", id);
  uint64_t buffer = 0;
  uint64_t bufptr = veo_get_sym(proc, handle, "buffer");
  int ret;
  ret = veo_read_mem(proc, &buffer, bufptr, sizeof(buffer));
  printf("veo_read_mem() returned %d\n", ret);
  printf("%016lx\n", buffer);
  buffer = 0xc0ffee;
  ret = veo_write_mem(proc, bufptr, &buffer, sizeof(buffer));
  printf("veo_write_mem() returned %d\n", ret);
  uint64_t sym2 = veo_get_sym(proc, handle, "print_buffer");
  uint64_t id2 = veo_call_async(ctx, sym2, arg);
  uint64_t retval;
  veo_args_free(arg);
  ret = veo_call_wait_result(ctx, id, &retval);
  printf("0x%lx: %d, %lu\n", id, ret, retval);
  ret = veo_call_wait_result(ctx, id2, &retval);
  printf("0x%lx: %d, %lu\n", id2, ret, retval);
  int close_status = veo_context_close(ctx);
  printf("close status = %d\n", close_status);
  return 0;
}

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <ve_offload.h>

int
main(int argc, const char *argv[])
{
  int count = 10;
  switch (argc) {
  case 2:
    count = atoi(argv[1]);
  case 1:
    break;
  default:
    fprintf(stderr, "usage: %s count\n", argv[0]);
    exit(1);
  }
  struct veo_proc_handle *proc = veo_proc_create(0);
  if (proc == NULL) {
    perror("veo_proc_create");
    exit(1);
  }
  uint64_t libhandle = veo_load_library(proc, "./libvesimplefunc.so");
  if (libhandle == 0) {
    fputs("veo_load_library failed.\n", stderr);
    exit(1);
  }
  uint64_t sym = veo_get_sym(proc, libhandle, "simplefunc");
  struct veo_thr_ctxt *ctx = veo_context_open(proc);
  struct veo_args *argp = veo_args_alloc();

  struct timespec t0, t1;
  uint64_t reqid[count];
  
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < count; ++i) {
    reqid[i] = veo_call_async(ctx, sym, argp);
  }
  for (int j = 0; j < count; ++j) {
    uint64_t retval;
    veo_call_wait_result(ctx, reqid[j], &retval);
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double t = (t1.tv_sec - t0.tv_sec)
           + (t1.tv_nsec - t0.tv_nsec) / 1000000000.0;
  printf("%f\n", t);
  veo_args_free(argp);
  veo_context_close(ctx);
  return 0;
}

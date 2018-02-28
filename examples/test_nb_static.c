//
// gcc -std=gnu99 -o test_nb_static test_nb_static.c -I/opt/nec/ve/veos/include -pthread -L/opt/nec/ve/veos/lib64 -Wl,-rpath=/opt/nec/ve/veos/lib64 -lveo
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ve_offload.h>

int main()
{
	int ret;
	struct veo_proc_handle *proc = veo_proc_create(0);
	if (proc == NULL) {
		printf("veo_proc_create() failed!\n");
		exit(1);
	}
	uint64_t sym = veo_get_sym(proc, NULL, "do_sleep");
	printf("symbol address = %p\n", (void *)sym);
	
	struct veo_thr_ctxt *ctx1 = veo_context_open(proc);
	printf("VEO context1 = %p\n", ctx1);

        uint64_t reqs[2];
	struct veo_args *arg = veo_args_alloc();
	veo_args_set_i64(arg, 0, 5);
	reqs[0] = veo_call_async(ctx1, sym, arg);
	printf("VEO request ID1 = 0x%lx\n", reqs[0]);

	uint64_t retval;
        while (ret = veo_call_peek_result(ctx1, reqs[0], &retval),
	       ret == VEO_COMMAND_UNFINISHED) {
		printf("sleep 1...\n");
		sleep(1);
	}
	veo_args_free(arg);
	
	int close_status = veo_context_close(ctx1);
	printf("close status 1 = %d\n", close_status);
	return 0;
}


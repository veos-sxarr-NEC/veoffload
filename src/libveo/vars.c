/**
 * @file vars.c
 * @brief global variables not in libvepseudo
 */
#include <pthread.h>
#include <signal.h>
#include "handle.h"
#include "comm_request.h"
#include "sys_process_mgmt.h"
#include "vemva_mgmt.h"
#include "loader.h"
__thread sigset_t ve_proc_sigmask;
__thread struct _ve_page_info ve_page_info;
pthread_mutex_t tid_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
struct tid_info global_tid_info[VEOS_MAX_VE_THREADS];
struct ve_address_space_info ve_info;
uint64_t default_page_size;
struct ve_load_data load_elf;
struct vemva_header vemva_header;

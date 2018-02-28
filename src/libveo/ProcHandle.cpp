/**
 * @file ProcHandle.cpp
 * @brief implementation of ProcHandle
 */
#include "ProcHandle.hpp"
#include "ThreadContext.hpp"
#include "VEOException.hpp"
#include "log.hpp"

#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/shm.h>

#include <libved.h>
/* VE OS internal headers */
extern "C" {
#define new new__ // avoid keyword
#include "comm_request.h"
#include "handle.h"
#include "mm_type.h"
#include "process_mgmt_comm.h"
#include "sys_process_mgmt.h"
#include "vemva_mgmt.h"
#include "loader.h"
#include "pseudo_ptrace.h"
#undef new

/* symbols required but undefined in libvepseudo */
__thread veos_handle *g_handle;
extern struct tid_info global_tid_info[VEOS_MAX_VE_THREADS];

int init_stack_veo(veos_handle*, int, char**, char**, struct ve_start_ve_req_cmd *);

// copied from pseudo_process.c
/**
 * @brief Create shared memory region used for system call arguments.
 *
 * @param handle VE OS handle
 * @return shared memory region ID upon success; -1 upon failure.
 */
int init_lhm_shm_area(veos_handle *handle)
{
  using veo::VEO_LOG_ERROR;
  using veo::VEO_LOG_DEBUG;
  using veo::VEO_LOG_TRACE;
  int retval = 0;
  uint64_t shm_lhm_area = 0;
  veo::ThreadContext *ctx = nullptr;
  VEO_TRACE(ctx, "Entering %s", __func__);

  /* Allocate shared memory segment */
  retval = shmget(getpid(), PAGE_SIZE_4KB, IPC_CREAT|S_IRWXU);
  if (-1 == retval) {
    VEO_DEBUG(ctx, "Failed to get shared memory (errno=%d)", errno);
    goto out_error1;
  }

  /* Attach to the above allocated shared memory segment */
  shm_lhm_area = (uint64_t)shmat(retval, NULL, 0);
  if ((void *)-1 == (void *)(shm_lhm_area)) {
    VEO_DEBUG(ctx, "Failed to attach shared memory (errno=%d)", errno);
    retval = -1;
    goto out_error;
  }

  VEO_DEBUG(ctx, "%lx", shm_lhm_area);

  /* stay on memory until the process exits */
  if (-1 == mlock((void *)shm_lhm_area, PAGE_SIZE_4KB)) {
    VEO_ERROR(ctx, "Failed to lock memory (errno=%d)", errno);
    retval = -1;
    goto out_error1;
  }
  memset((void *)shm_lhm_area, 0, PAGE_SIZE_4KB);

  vedl_set_shm_lhm_addr(handle->ve_handle, (void *)shm_lhm_area);
out_error:
  /* Mark shared memory segment as destroyed */
  if (-1 == shmctl(retval, IPC_RMID, NULL)) {
    VEO_DEBUG(ctx, "Failed to destroy shared memory (errno=%d)", errno);
    retval = -1;
  }
out_error1:
  VEO_TRACE(ctx, "Exiting %s", __func__);
  return retval;
}

/**
 * @brief abort pseudo process
 *
 * Functions in libvepseudo call pseudo_abort() on fatal error.
 */
void pseudo_abort()
{
  abort();
}
/* end of symbols required */
} // extern "C"

#ifndef PAGE_SIZE_4KB
#define PAGE_SIZE_4KB (4 * 1024)
#endif

namespace veo {
/* necessary to allocate PATH_MAX because VE OS requests to
 * transfer PATH_MAX. */
char helper_name[PATH_MAX] = VEORUN_BIN;

/**
 * @brief create a VE process and initialize a thread context
 *
 * @param[in,out] ctx the thread context of the main thread
 * @param oshandle VE OS handle for the VE process (main thread)
 */
int spawn_helper(ThreadContext *ctx, veos_handle *oshandle)
{
  int rv = -1;
  // libvepseudo touches PTRACE_PRIVATE_DATA area.
  void *ptrace_private = mmap((void *)PTRACE_PRIVATE_DATA, 4096,
                              PROT_READ|PROT_WRITE,
                              MAP_ANON|MAP_PRIVATE|MAP_FIXED, -1, 0);
  int saved_errno = errno;
  if (MAP_FAILED == ptrace_private) {
    PSEUDO_DEBUG("Fail to alloc chunk for ptrace private: %s",
      strerror(errno));
    throw VEOException("Failled to allocate ptrace related data", saved_errno);
  }

  /* Check if the request address is obtained or not */
  if (ptrace_private != (void *)PTRACE_PRIVATE_DATA) {
    PSEUDO_DEBUG("Request: %lx but got: %p for ptrace data.",
      PTRACE_PRIVATE_DATA, ptrace_private);
    munmap(ptrace_private, 4096);
    throw VEOException("Failled to allocate ptrace related data", saved_errno);
  }

  memset(ptrace_private, 0, 4096);

  const char* env_p = std::getenv("VEORUN_BIN");
  if (env_p != nullptr) {
    if (strlen(env_p) < PATH_MAX - 1) {
      memcpy((void *)helper_name, (const void *)env_p, strlen(env_p) + 1);
      PSEUDO_DEBUG("Replaced 'veorun' by '%s'\n", helper_name);
    }
  }
  // Set global TID array for main thread.
  global_tid_info[0].vefd = oshandle->ve_handle->vefd;
  // Initialize the syscall argument area.
  rv = init_lhm_shm_area(oshandle);
  if (rv < 0) {
    throw VEOException("failed to create shared memory region.", 0);
  }
  // Request VE OS to create a new VE process
  new_ve_proc ve_proc = {0};
  // TODO: set resource limit.
  memset(&ve_proc.lim, -1, sizeof(ve_proc.lim));
  ve_proc.gid = getgid();
  ve_proc.uid = getuid();
  ve_proc.shm_lhm_addr = (uint64_t)vedl_get_shm_lhm_addr(oshandle->ve_handle);
  ve_proc.shmid = rv;
  ve_proc.core_id = -1;
  ve_proc.traced_proc = 0;
  ve_proc.tracer_pid = getppid();
  ve_proc.exec_path = (uint64_t)helper_name;
  auto exe_name_buf = strdup(helper_name);
  auto exe_base_name = basename(exe_name_buf);
  memset(ve_proc.exe_name, '\0', ACCT_COMM + 1);
  strncpy(ve_proc.exe_name, exe_base_name, ACCT_COMM);
  free(exe_name_buf);

  int retval = pseudo_psm_send_new_ve_process(oshandle->veos_sock_fd, ve_proc);
  if (0 > retval) {
    VEO_ERROR(ctx, "Failed to send NEW VE PROC request (%d)", retval);
    return retval;
  }
  int core_id, node_id;
  retval = pseudo_psm_recv_load_binary_req(oshandle->veos_sock_fd,
                                           &core_id, &node_id);
  if (0 > retval) {
    VEO_ERROR(ctx, "VEOS acknowledgement error (%d)", retval);
    return retval;
  }
  VEO_DEBUG(ctx, "CORE ID: %d\t NODE ID: %d", core_id, node_id);
  vedl_set_syscall_area_offset(oshandle->ve_handle, 0);

  // initialize VEMVA space
  INIT_LIST_HEAD(&vemva_header.vemva_list);
  retval = init_vemva_header();
  if (retval) {
    VEO_ERROR(ctx, "failed to initialize (%d)", retval);
    return retval;
  }

  // Load an executable
  struct ve_start_ve_req_cmd start_ve_req = {{0}};
  retval = pse_load_binary(helper_name, oshandle, &start_ve_req);
  if (retval) {
    VEO_ERROR(ctx, "failed to load ve binary (%d)", retval);
    process_thread_cleanup(oshandle, -1);
    return retval;
  }

  // initialize the stack
  char *ve_argv[] = { helper_name, nullptr};
  retval = init_stack_veo(oshandle, 1, ve_argv, environ, &start_ve_req);
  if (retval) {
    VEO_ERROR(ctx, "failed to make stack region (%d)", retval);
    process_thread_cleanup(oshandle, -1);
    return retval;
  }
  memcpy(&start_ve_req.ve_info, &ve_info,
    sizeof(struct ve_address_space_info_cmd));

  // start VE process
  retval = pseudo_psm_send_start_ve_proc_req(&start_ve_req,
             oshandle->veos_sock_fd);
  if (0 > retval) {
    VEO_ERROR(ctx, "failed to send start VE process request (%d)", retval);
    return retval;
  }
  retval = pseudo_psm_recv_start_ve_proc(oshandle->veos_sock_fd);
  if (0 > retval) {
    VEO_ERROR(ctx, "Failed to receive START VE PROC ack (%d)", retval);
    return retval;
  }
  VEO_TRACE(ctx, "%s: Succeed to create a VE process.", __func__);
  return 0;
}

/**
 * @brief constructor
 *
 * @param ossock path to VE OS socket
 * @param vedev path to VE device file
 */
ProcHandle::ProcHandle(const char *ossock, const char *vedev)
{
  int retval;
  // open VE OS handle
  veos_handle *os_handle = veos_handle_create(const_cast<char *>(vedev),
                             const_cast<char *>(ossock), nullptr, -1);
  if (os_handle == NULL) {
    throw VEOException("veos_handle_create failed.");
  }
  g_handle = os_handle;
  // initialize the main thread context
  this->main_thread.reset(new ThreadContext(this, os_handle, true));
  if (spawn_helper(this->main_thread.get(), os_handle) != 0) {
    veos_handle_free(os_handle);
    throw VEOException("The creation of a VE process failed.", 0);
  }
  // VE process is ready here. The state is changed to RUNNING.
  this->main_thread->state = VEO_STATE_RUNNING;

  // handle some system calls from main thread for initialization of VE libc.
  this->waitForBlock();
  // VE process is to stop at the first block here.
  // sysve(VEO_BLOCK, &veo__helper_functions);
  uint64_t funcs_addr = this->main_thread->_collectReturnValue();
  VEO_DEBUG(this->main_thread.get(), "helper functions set: %p\n",
            (void *)funcs_addr);
  int rv = ve_recv_data(os_handle, funcs_addr, sizeof(this->funcs),
                        &this->funcs);
  if (rv != 0) {
    throw VEOException("Failed to receive data from VE");
  }
}

/**
 * @brief Load a VE library in VE process space
 *
 * @param libname a library name
 * @return handle of the library loaded upon success; zero upon failure.
 */
uint64_t ProcHandle::loadLibrary(const char *libname)
{
  size_t len = strlen(libname);
  if (len > VEO_SYMNAME_LEN_MAX) {
    throw VEOException("Too long name", ENAMETOOLONG);
  }
  std::lock_guard<std::mutex> lock(this->main_mutex);
  auto rv = ve_send_data(this->osHandle(), this->funcs.name_buffer,
                         len + 1, (char *)libname);
  if (rv != 0) {
    throw VEOException("Failed to send a library name to VE");
  }
  CallArgs args;// no argument.
  this->main_thread->_doCall(this->funcs.load_library, args);
  this->waitForBlock();
  return this->main_thread->_collectReturnValue();
}

/**
 * @brief Find a symbol in VE program
 *
 * @param libhdl handle of library
 * @param symname a symbol name to find
 * @return VEMVA of the symbol upon success; zero upon failure.
 */
uint64_t ProcHandle::getSym(const uint64_t libhdl, const char *symname)
{
  size_t len = strlen(symname);
  if (len > VEO_SYMNAME_LEN_MAX) {
    throw VEOException("Too long name", ENAMETOOLONG);
  }
  std::lock_guard<std::mutex> lock(this->main_mutex);
  auto rv = ve_send_data(this->osHandle(), this->funcs.name_buffer,
                         len + 1, (void *)symname);
  if (rv != 0) {
    throw VEOException("Failed to send a symbol name to VE");
  }
  CallArgs args;
  args.set(0, libhdl);
  this->main_thread->_doCall(this->funcs.find_sym, args);
  this->waitForBlock();
  return this->main_thread->_collectReturnValue();
}

/**
 * @brief Allocate a buffer on VE
 *
 * @param size of buffer
 * @return VEMVA of the buffer upon success; zero upon failure.
 */
uint64_t ProcHandle::allocBuff(const size_t size)
{
  std::lock_guard<std::mutex> lock(this->main_mutex);
  CallArgs args;
  args.set(0, size);
  this->main_thread->_doCall(this->funcs.alloc_buff, args);
  this->waitForBlock();
  return this->main_thread->_collectReturnValue();
}

/**
 * @brief Free a buffer on VE
 *
 * @param buff VEMVA of the buffer
 * @return nothing
 */
void ProcHandle::freeBuff(const uint64_t buff)
{
  std::lock_guard<std::mutex> lock(this->main_mutex);
  CallArgs args;
  args.set(0, buff);
  this->main_thread->_doCall(this->funcs.free_buff, args);
  this->waitForBlock();
  return;
}

/**
 * @brief Exit veorun on VE side
 *
 */
void ProcHandle::exitProc()
{
  std::lock_guard<std::mutex> lock(this->main_mutex);
  VEO_TRACE(this->main_thread.get(), "%s()", __func__);
  process_thread_cleanup(this->osHandle(), -1);
  this->main_thread.get()->state = VEO_STATE_EXIT;
  veos_handle_free(this->osHandle());
  return;
}

/**
 * @brief open a new context (VE thread)
 *
 * @return a new thread context created
 */
ThreadContext *ProcHandle::openContext()
{
  CallArgs args;
  std::lock_guard<std::mutex> lock(this->main_mutex);
  this->main_thread->_doCall(this->funcs.create_thread, args);
  uint64_t exc;
  // hook clone() on VE
  auto req = this->main_thread->exceptionHandler(exc,
               &ThreadContext::hookCloneFilter);
  if (!_is_clone_request(req)) {
    throw VEOException("VE process requests block unexpectedly.", 0);
  }
  // create a new ThreadContext for a child thread
  std::unique_ptr<ThreadContext> newctx(new ThreadContext(this,
                                   this->osHandle()));
  // handle clone() request.
  auto tid = newctx->handleCloneRequest();
  // restart execution; execute until the next block request.
  this->main_thread->_unBlock(tid);
  this->waitForBlock();

  uint64_t r = newctx->callAsync(this->funcs.get_sp, args);
  newctx->callWaitResult(r, &newctx->ve_sp);
  VEO_TRACE(newctx.get(), "sp = %p", (void *)newctx->ve_sp);

  auto rv = newctx.release();
  return rv;
}

/**
 * @brief read data from VE memory
 * @param[out] dst buffer to store the data
 * @param src VEMVA to read
 * @param size size to transfer in byte
 * @return zero upon success; negative upon failure
 */
int ProcHandle::readMem(void *dst, uint64_t src, size_t size)
{
  std::lock_guard<std::mutex> lock(this->main_mutex);
  auto osh = this->osHandle();
  return ve_recv_data(osh, src, size, dst);
}

/**
 * @brief write data to VE memory
 * @param dst VEMVA to write the data
 * @param src buffer holding data to write
 * @param size size to transfer in byte
 * @return zero upon success; negative upon failure
 */
int ProcHandle::writeMem(uint64_t dst, void *src, size_t size)
{
  std::lock_guard<std::mutex> lock(this->main_mutex);
  auto osh = this->osHandle();
  return ve_send_data(osh, dst, size, src);
}
} // namespace veo

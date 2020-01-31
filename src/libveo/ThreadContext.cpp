/**
 * @file ThreadContext.cpp
 * @brief implementation of ThreadContext
 */
#include <set>

#include <pthread.h>
#include <cerrno>
#include <semaphore.h>
#include <signal.h>

#include <libved.h>
#include <veosinfo/veosinfo.h>

/* VE OS internal headers */
extern "C" {
#include "handle.h"
#include "sys_common.h"
#include "process_mgmt_comm.h"
#include "sys_process_mgmt.h"
#define VE_SYSCALL(num, name, type, handler) NR_##name = (num),
enum ve_syscall_number {
#include "ve_syscall_no.h"
};

extern __thread sigset_t ve_proc_sigmask;
} // extern "C"

#include "CallArgs.hpp"
#include "veo_private_defs.h"
#include "ThreadContext.hpp"
#include "ProcHandle.hpp"
#include "CommandImpl.hpp"
#include "VEOException.hpp"
#include "log.hpp"

namespace veo {
namespace internal {
/**
 * system calls filtered by default filter
 */
std::set<int> default_filtered_syscalls {
  NR_ve_rt_sigaction,
  NR_ve_rt_sigreturn,
  NR_ve_fork,
  NR_ve_vfork,
  NR_ve_execve,
  NR_ve_exit,
  NR_ve_wait4,
  NR_ve_rt_sigpending,
  NR_ve_rt_sigtimedwait,
  NR_ve_rt_sigsuspend,
  NR_ve_sigaltstack,
  NR_ve_exit_group,
  NR_ve_signalfd,
  NR_ve_signalfd4,
};

/**
 * @brief determinant of BLOCK
 *
 * Determine BLOCK is requested or not.
 *
 * @param vehdl VEDL handle
 * @param sysnum system call number
 * @return true when BLOCK is requested.
 */
bool is_veo_block(vedl_handle *vehdl, int sysnum)
{
  if (sysnum != NR_ve_sysve)
    return false;

  uint64_t args[2];
  vedl_get_syscall_args(vehdl, args, 2);
  return args[0] == VE_SYSVE_VEO_BLOCK;
}

/**
 * @brief argument of child thread
 *
 * Data to be passed to function on thread creation
 */
struct child_thread_arg {
  ThreadContext *context;//!< VEO thread context of the thread created
  sem_t *semaphore;//!< semaphore to notify of the completion of initialization
};

/**
 * @brief Event loop for a VEO pseudo thread.
 */
void start_child_thread(veos_handle *os_handle, void *arg)
{
  child_thread_arg *ap = reinterpret_cast<child_thread_arg *>(arg);
  ap->context->startEventLoop(os_handle, ap->semaphore);
}
} // namespace internal

ThreadContext::ThreadContext(ProcHandle *p, veos_handle *osh, bool is_main):
  proc(p), os_handle(osh), state(VEO_STATE_UNKNOWN),
  pseudo_thread(pthread_self()), is_main_thread(is_main), seq_no(0) {}

/**
 * @brief handle a single exception from VE process
 *
 * Handle an exception. If a system call is requested, call a filter function
 * and return the flag value set by the filter upon filtered.
 * If the system call is not to be filtered, this function return zero.
 *
 * @param[out] exs EXS value on exception
 * @param filter System call filter
 *
 * @return 0 upon system call; -1 upon failure; positive upon filtered.
 *
 */
int ThreadContext::handleSingleException(uint64_t &exs, SyscallFilter filter)
{
  int ret;
  int break_flag = 0;
  sigset_t signal_mask;

  sigfillset(&signal_mask);

  VEO_TRACE(this, "%s()", __func__);
  constexpr uint64_t VEO_EXCEPTION_MASK = ~0xffUL;
  for (;;) {
    // restore the signal mask before entering VE driver.
    pthread_sigmask(SIG_SETMASK, &ve_proc_sigmask, NULL);
    ret = vedl_wait_exception(this->os_handle->ve_handle, &exs);
    // Block all signals
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    if (ret != 0) {
      if ( ret == -1 && errno == EINTR )
        continue;
      throw VEOException("vedl_wait_exception failed", errno);
    }
    if (!(exs & VEO_EXCEPTION_MASK)) { // no exceptions
      VEO_DEBUG(this, "No exception; exs = %lx", exs);
      // vedl_wait_exception() can return when no exceptions are raised.
      // Retry in such case.
      continue;
    } else
      break;
  }
  VEO_TRACE(this, "exs = 0x%016lx", exs);
  if (exs & EXS_MONC) {
    int sysnum = vedl_get_syscall_num(this->os_handle->ve_handle);
    VEO_TRACE(this, "Syscall #%d", sysnum);
    bool filtered = false;
    if (filter != nullptr) {
      VEO_TRACE(this, "syscall number %d -> filter %p is applied", sysnum,
                filter);
      filtered = (this->*filter)(sysnum, &break_flag);
    }
    if (filtered) {
      VEO_DEBUG(this, "syscall %d is filtered.", sysnum);
    } else {
      VEO_DEBUG(this, "syscall %d (to be handled)", sysnum);
      this->state = VEO_STATE_SYSCALL;
      ve_syscall_handler(this->os_handle, sysnum);
      this->state = VEO_STATE_RUNNING;
    }
  }
  if ((exs & EXS_MONT) || (exs & (UNCORRECTABLE_ERROR)) ||
      ((exs & (CORRECTABLE_ERROR)) && !(exs & (EXS_MONC|EXS_RDBG)))) {
    VEO_ERROR(this, "caused error (EXS=0x%016lx)", exs);
    // get the current position
    block_syscall_req_ve_os(this->os_handle);
    pid_t tid = syscall(SYS_gettid);
    int regid[] = {IC, ICE};
    uint64_t regvals[2];
    int ret = ve_get_regvals(this->proc->veNumber(), tid, 2, regid, regvals);
    if (ret != 0) {
      VEO_ERROR(this, "failed to get register values... (%d)", ret);
    } else {
      VEO_ERROR(this, "IC = %#lx, ICE = %#lx", regvals[0], regvals[1]);
    }
    return VEO_HANDLER_STATUS_EXCEPTION;
  }
  if (break_flag) {
    return break_flag;
  }
  return 0;
}

/**
 * @brief The default system call filter.
 *
 * @param sysnum System call number
 * @param[out] break_flag VEO_HANDLER_STATUS_BLOCK_REQUESTED is set
 *                        when the system call is a block request; and
 *                        zero is set, otherwise.
 *
 * @retval true upon filtered.
 * @retval false upon system call not filtered.
 */
bool ThreadContext::defaultFilter(int sysnum, int *break_flag)
{
  VEO_TRACE(this, "%s(%d)", __func__, sysnum);
  *break_flag = 0; // the default filter never breaks event loop
  auto search = internal::default_filtered_syscalls.find(sysnum);
  if (search != internal::default_filtered_syscalls.end()) {
    VEO_ERROR(this, "system call %d is not allowed in VEO program", sysnum);
    // filtered system calls are "blocking" system calls.
    un_block_and_retval_req(this->os_handle, sysnum, -ENOSYS, 1);
    return true;
  }
  if (internal::is_veo_block(this->os_handle->ve_handle, sysnum)) {
    block_syscall_req_ve_os(this->os_handle);// notify VEOS of BLOCKED state.
    *break_flag = VEO_HANDLER_STATUS_BLOCK_REQUESTED;
    this->state = VEO_STATE_BLOCKED;
    return true;
  }
  return false;
}

/**
 * @brief A system call filter to catch clone() call.
 *
 * A system call used to catch a request to create VE thread
 * from _veo_create_thread_helper.
 *
 * @param sysnum System call number
 * @param[out] break_flag VEO_HANDLER_STATUS_BLOCK_REQUESTED is set
 *                        when the system call is a block request;
 *                        NR_veo_clone on clone() system call; and
 *                        zero is set, otherwise.
 * @retval true upon filtered.
 * @retval false upon system call not filtered.
 */
bool ThreadContext::hookCloneFilter(int sysnum, int *break_flag)
{
  VEO_TRACE(this, "%s(%d)", __func__, sysnum);
  *break_flag = 0;
  if (sysnum == NR_ve_clone) {
    VEO_TRACE(this, "clone() is requested (thread %d).", this->pseudo_thread);
    *break_flag = NR_ve_clone;
    return true;
  }
  return this->defaultFilter(sysnum, break_flag);
}

/**
 * @brief start a function on VE thread
 *
 * @param addr VEMVA of function called
 * @param args arguments of the function
 */
void ThreadContext::_doCall(uint64_t addr, CallArgs &args)
{
  VEO_TRACE(this, "%s(%#lx, ...)", __func__, addr);
  VEO_DEBUG(this, "VE function = %p", (void *)addr);
  ve_set_user_reg(this->os_handle, SR12, addr, ~0UL);
  // ve_sp is updated in CallArgs::setup()
  VEO_DEBUG(this, "current stack pointer = %p", (void *)this->ve_sp);
  args.setup(this->ve_sp);
  auto regs = args.getRegVal(this->ve_sp);
  VEO_ASSERT(regs.size() <= NUM_ARGS_ON_REGISTER);
  for (auto i = 0; i < regs.size(); ++i) {
    // set register arguments
    uint64_t regval = regs[i];
    VEO_DEBUG(this, "arg#%d: %#lx", i, regval);
    ve_set_user_reg(this->os_handle, SR00 + i, regval, ~0UL);
  }
  auto writemem = std::bind(&ThreadContext::_writeMem, this,
                            std::placeholders::_1, std::placeholders::_2,
                            std::placeholders::_3);
  args.copyin(writemem);
  // shift the stack pointer as the stack is extended.
  VEO_DEBUG(this, "set stack pointer -> %p", (void *)this->ve_sp);
  ve_set_user_reg(this->os_handle, SR11, this->ve_sp, ~0UL);
  VEO_TRACE(this, "unblock (start at %p)", (void *)addr);
  this->_unBlock(regs.size() > 0 ? regs[0] : 0);
}

/**
 * @brief unblock VE thread
 * @param sr0 value set to SR0 on unblock, VE thread starting
 */
void ThreadContext::_unBlock(uint64_t sr0)
{
  VEO_TRACE(this, "%s(%#lx)", __func__, sr0);
  VEO_DEBUG(this, "state = %d", this->state);
  un_block_and_retval_req(this->os_handle, NR_ve_sysve, sr0, 1);
  this->state = VEO_STATE_RUNNING;
  VEO_TRACE(this, "%s() done. state = %d", __func__, this->state);
}

/**
 * @brief read a return value from VE function
 *
 * Collect a return value from VE function at BLOCK.
 */
uint64_t ThreadContext::_collectReturnValue()
{
  VEO_TRACE(this, "%s()", __func__);
  // VE process is to stop at sysve(VE_SYSVE_VEO_BLOCK, retval, _, _, _, sp);
  uint64_t args[6];
  vedl_get_syscall_args(this->os_handle->ve_handle, args, 6);
  VEO_ASSERT(args[0] == VE_SYSVE_VEO_BLOCK);
  // update the current sp
  this->ve_sp = args[5];
  VEO_DEBUG(this, "return = %#lx, sp = %#012lx", args[1], this->ve_sp);
  return args[1];
}

/**
 * @brief handler of clone() on VE
 *
 * On creation of a new VEO context, VEO executes pthread_create()
 * on VE. This function handles the clone() call from VE thread.
 */
long ThreadContext::handleCloneRequest()
{
  sem_t child_thread_sem;
  if (sem_init(&child_thread_sem, 0, 0) != 0) {
    throw VEOException("sem_init failed.");
  }
  internal::child_thread_arg arg = {
    .context = this,
    .semaphore = &child_thread_sem,
  };
  char name[] = "__clone_veo";// the 2nd argument is not const.
  auto rv = ve__do_clone(NR_ve_clone, name, this->os_handle,
              &internal::start_child_thread, &arg);
  if ( rv < 0 ) {
    VEO_ERROR(this, "ve__do_clone() fail. (errno = %d)", -rv);
    return rv;
  }
  while (sem_wait(&child_thread_sem) != 0) {
    VEO_ASSERT(errno == EINTR);
  }
  sem_destroy(&child_thread_sem);
  return rv;
}

/**
 * @brief start an event loop on a child thread
 *
 * @param newhdl a new VEOS handle created in clone() handler.
 * @param sem semaphore to notify the parent completing initialization.
 */
void ThreadContext::startEventLoop(veos_handle *newhdl, sem_t *sem)
{
  VEO_TRACE(this, "%s()", __func__);
  // VEO child threads never handle signals.
  sigset_t sigmask;
  /*
   * It is OK to fill since NPTL ignores attempts to block signals
   * used internally; pthread_cancel() still works.
   */
  sigfillset(&sigmask);
  sigdelset(&sigmask, SIGCONT);// SIGCONT shall never be masked.
  ve_proc_sigmask = sigmask;
  pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
  this->os_handle = newhdl;// replace to a new handle for the child thread.
  this->pseudo_thread = pthread_self();
  this->state = VEO_STATE_RUNNING;
  VEO_ASSERT(sem_post(sem) == 0);

  // execute until the first block request
  uint64_t exs;
  auto status_1stblock = this->defaultExceptionHandler(exs);
  switch (status_1stblock) {
  case VEO_HANDLER_STATUS_BLOCK_REQUESTED:
    VEO_TRACE(this, "OK. the child context (%p) is ready.", this);
    this->_collectReturnValue();// collect ve_sp.
    break;
  case VEO_HANDLER_STATUS_EXCEPTION:
    VEO_ERROR(this, "unexpected error (exs = 0x%016lx)", exs);
    this->state = VEO_STATE_EXIT;
    return;
  default:
    VEO_ERROR(this, "unexpected status (%d)", status_1stblock);
    this->state = VEO_STATE_EXIT;
    return;
  }

  this->eventLoop();
}

/**
 * @brief execute a command on VE
 */
bool ThreadContext::_executeVE(int &status, uint64_t &exs)
{
  status = this->defaultExceptionHandler(exs);
  VEO_DEBUG(this, "status = %d, exs = 0x%016lx\n", status, exs);
  return VEO_HANDLER_STATUS_BLOCK_REQUESTED == status;
}

/**
 * @brief event loop
 *
 * Pseudo thread processes commands from request queue while BLOCKED.
 */
void ThreadContext::eventLoop()
{
  sigset_t signal_mask;
  sigfillset(&signal_mask);

  while (this->state == VEO_STATE_BLOCKED) {
    // Restore the signal mask before popping queue.
    pthread_sigmask(SIG_SETMASK, &ve_proc_sigmask, NULL);
    auto command = std::move(this->comq.popRequest());
    // Block all signals
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    auto rv = (*command)();
    if (rv != 0) {
      this->state = VEO_STATE_EXIT;
      this->comq.setRequestStatus(VEO_QUEUE_CLOSED);
      this->comq.pushCompletion(std::move(command));
      this->comq.setCompletion();
      VEO_ERROR(this, "Internal error on executing a command(%d)", rv);
      return;
    }
    this->comq.pushCompletion(std::move(command));
  }
}

/**
 * @brief function to be set to close request (command)
 */
int64_t ThreadContext::_closeCommandHandler(uint64_t id)
{
  VEO_TRACE(this, "%s()", __func__);
  process_thread_cleanup(this->os_handle, -1);
  this->state = VEO_STATE_EXIT;
  /*
   * pthread_exit() can invoke destructors for objects on the stack,
   * which can cause double-free.
   */
  auto dummy = [](Command *)->int64_t{return 0;};
  auto newc = new internal::CommandImpl(id, dummy);
  /* push the reply here because this function never returns. */
  newc->setResult(0, 0);
  this->comq.pushCompletion(std::unique_ptr<Command>(newc));
  pthread_exit(0);
  return 0;
}

/**
 * @brief close this thread context
 *
 * @return zero upon success; negative upon failure.
 *
 * Close this VEO thread context; terminate the pseudo thread.
 * The current implementation always returns zero.
 */
int ThreadContext::close()
{
  if ( this->state == VEO_STATE_EXIT )
    return 0;

  auto id = this->issueRequestID();
  auto f = std::bind(&ThreadContext::_closeCommandHandler, this, id);
  std::unique_ptr<Command> req(new internal::CommandImpl(id, f));
  if(this->comq.pushRequest(std::move(req)))
    id = VEO_REQUEST_ID_INVALID;
  auto c = this->comq.waitCompletion(id);
  return c->getRetval();
}

/**
 * @brief call a VE function asynchronously
 *
 * @param addr VEMVA of VE function to call
 * @param args arguments of the function
 * @return request ID
 */
uint64_t ThreadContext::callAsync(uint64_t addr, CallArgs &args)
{
  if ( addr == 0 || this->state == VEO_STATE_EXIT)
    return VEO_REQUEST_ID_INVALID;
  
  auto id = this->issueRequestID();
  auto f = [&args, this, addr, id] (Command *cmd) {
    VEO_TRACE(this, "[request #%d] start...", id);
    this->_doCall(addr, args);
    VEO_TRACE(this, "[request #%d] VE execution", id);
    int status;
    uint64_t exs;
    auto successful = this->_executeVE(status, exs);
    VEO_TRACE(this, "[request #%d] executed.", id);
    if (!successful) {
      VEO_ERROR(this, "_executeVE() failed (%d, exs=0x%016lx)", status, exs);
      if (status == VEO_HANDLER_STATUS_EXCEPTION) {
        cmd->setResult(exs, VEO_COMMAND_EXCEPTION);
      } else {
        cmd->setResult(status, VEO_COMMAND_ERROR);
      }
      return 1;
    }
    auto rv = this->_collectReturnValue();
    cmd->setResult(rv, VEO_COMMAND_OK);
    // post
    VEO_TRACE(this, "[request #%d] post process", id);
    auto readmem = std::bind(&ThreadContext::_readMem, this,
                             std::placeholders::_1, std::placeholders::_2,
                             std::placeholders::_3);
    args.copyout(readmem);
    VEO_TRACE(this, "[request #%d] done", id);
    return 0;
  };

  std::unique_ptr<Command> req(new internal::CommandImpl(id, f));
  if(this->comq.pushRequest(std::move(req)))
    return VEO_REQUEST_ID_INVALID;
  return id;
}

/**
 * @brief call a VE function specified by symbol name asynchronously
 *
 * @param libhdl handle of library
 * @param symname a symbol name to find
 * @param args arguments of the function
 * @return request ID
 */
uint64_t ThreadContext::callAsyncByName(uint64_t libhdl, const char *symname, CallArgs &args)
{
  uint64_t addr = this->proc->getSym(libhdl, symname);
  return this->callAsync(addr, args);
}

/**
 * @brief call a VH function asynchronously
 *
 * @param func address of VH function to call
 * @param arg pointer to opaque arguments structure for the function
 * @return request ID
 */
uint64_t ThreadContext::callVHAsync(uint64_t (*func)(void *), void *arg)
{
  if ( func == nullptr || this->state == VEO_STATE_EXIT)
    return VEO_REQUEST_ID_INVALID;

  auto id = this->issueRequestID();
  auto f = [this, func, arg, id] (Command *cmd) {
    VEO_TRACE(this, "[request #%lu] start...", id);
    auto rv = (*func)(arg);
    VEO_TRACE(this, "[request #%lu] executed. (return %ld)", id, rv);
    cmd->setResult(rv, VEO_COMMAND_OK);
    VEO_TRACE(this, "[request #%lu] done", id);
    return 0;
  };
  std::unique_ptr<Command> req(new internal::CommandImpl(id, f));
  this->comq.pushRequest(std::move(req));
  return id;
}

uint64_t ThreadContext::_callOpenContext(ProcHandle *proc,
                                         uint64_t addr, CallArgs &args)
{
  if ( this->state == VEO_STATE_EXIT )
    return VEO_REQUEST_ID_INVALID;

  auto id = this->issueRequestID();
  auto f = [&args, this, proc, addr, id] (Command *cmd) {
    VEO_TRACE(this, "[request #%d] start...", id);
    this->_doCall(addr, args);
    VEO_TRACE(this, "[request #%d] VE execution", id);

    uint64_t exc;
    // hook clone() on VE
    auto req = this->exceptionHandler(exc,
                 &ThreadContext::hookCloneFilter);
    if (!_is_clone_request(req)) {
      VEO_ERROR(this, "VE open context blocked unexpectedly. %p", exc);
      cmd->setResult(exc, VEO_COMMAND_EXCEPTION);
    }
    // create a new ThreadContext for a child thread
    std::unique_ptr<ThreadContext> newctx(new ThreadContext(proc,
                                     this->os_handle));
    // handle clone() request.
    auto tid = newctx->handleCloneRequest();
    VEO_DEBUG(this, "new context has TID %ld", tid);
    // restart execution; execute until the next block request.
    this->_unBlock(tid);
    if (this->defaultExceptionHandler(exc)
        != VEO_HANDLER_STATUS_BLOCK_REQUESTED) {
      throw VEOException("Unexpected exception occured");
    }
    if(tid < 0){
      VEO_ERROR(this, "newctx->handleCloneRequest() fail. (errno = %d)", -tid);
      cmd->setResult(tid, VEO_COMMAND_OK);
    }
    else{
      VEO_TRACE(newctx.get(), "sp = %p", (void *)newctx->ve_sp);
      auto rv = newctx.release();
      cmd->setResult(rv, VEO_COMMAND_OK);
    }
    VEO_TRACE(this, "[request #%d] done", id);
    return 0;
  };

  std::unique_ptr<Command> req(new internal::CommandImpl(id, f));
  if(this->comq.pushRequest(std::move(req)))
    return VEO_REQUEST_ID_INVALID;
  return id;
}

/**
 * @brief check if the result of a request (command) is available
 *
 * @param reqid request ID to wait
 * @param retp pointer to buffer to store the return value.
 * @retval VEO_COMMAND_OK the execution of the function succeeded.
 * @retval VEO_COMMAND_EXCEPTION exception occured on the execution.
 * @retval VEO_COMMAND_ERROR error occured on handling the command.
 * @retval VEO_COMMAND_UNFINISHED the command is not finished.
 */
int ThreadContext::callPeekResult(uint64_t reqid, uint64_t *retp)
{
  std::lock_guard<std::mutex> lock(this->req_mtx);
  auto itr = rem_reqid.find(reqid);
  if( itr == rem_reqid.end() ) {
    return VEO_COMMAND_ERROR;
  }
  auto c = this->comq.peekCompletion(reqid);
  if (c != nullptr) {
    if (!rem_reqid.erase(reqid))
      return VEO_COMMAND_ERROR;
    *retp = c->getRetval();
    return c->getStatus();
  }
  return VEO_COMMAND_UNFINISHED;
}

/**
 * @brief wait for the result of request (command)
 *
 * @param reqid request ID to wait
 * @param retp pointer to buffer to store the return value.
 * @retval VEO_COMMAND_OK the execution of the function succeeded.
 * @retval VEO_COMMAND_EXCEPTION exception occured on the execution.
 * @retval VEO_COMMAND_ERROR error occured on handling the command.
 * @retval VEO_COMMAND_UNFINISHED the command is not finished.
 */
int ThreadContext::callWaitResult(uint64_t reqid, uint64_t *retp)
{
  req_mtx.lock();
  auto itr = rem_reqid.find(reqid);
  if( itr == rem_reqid.end() ) {
    req_mtx.unlock();
    return VEO_COMMAND_ERROR;
  }
  if (!rem_reqid.erase(reqid)) {
    req_mtx.unlock();
    return VEO_COMMAND_ERROR;
  }
  req_mtx.unlock();
  auto c = this->comq.waitCompletion(reqid);
  *retp = c->getRetval();
  return c->getStatus();
}

/**
 * @brief read data from VE memory
 * @param[out] dst buffer to store the data
 * @param src VEMVA to read
 * @param size size to transfer in byte
 * @return zero upon success; negative upon failure
 */
int ThreadContext::_readMem(void *dst, uint64_t src, size_t size)
{
  return ve_recv_data(this->os_handle, src, size, dst);
}

/**
 * @brief write data to VE memory
 * @param dst VEMVA to write the data
 * @param src buffer holding data to write
 * @param size size to transfer in byte
 * @return zero upon success; negative upon failure
 */
int ThreadContext::_writeMem(uint64_t dst, const void *src, size_t size)
{
  return ve_send_data(this->os_handle, dst, size, const_cast<void *>(src));
}


/**
 * @brief determinant of clone() system call
 */
bool _is_clone_request(int rv_handler)
{
  return rv_handler == NR_ve_clone;
}
} // namespace veo

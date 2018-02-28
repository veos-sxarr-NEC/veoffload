/**
 * @file ThreadContext.cpp
 * @brief implementation of ThreadContext
 */
#include <set>

#include <pthread.h>
#include <cerrno>
#include <semaphore.h>
#include <signal.h>
#include <algorithm>

#include <libved.h>

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
} // extern "C"

#include "veo_private_defs.h"
#include "ThreadContext.hpp"
#include "ProcHandle.hpp"
#include "VEOException.hpp"
#include "log.hpp"

namespace veo {
namespace internal {
/**
 * system calls filtered by default filter
 */
std::set<int> default_filtered_syscalls {
  NR_ve_rt_sigaction,
  NR_ve_rt_sigprocmask,
  NR_ve_rt_sigreturn,
  NR_ve_clone,
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
 * @brief template to define a command handled by pseudo thread
 */
template <typename T> class CommandImpl: public Command {
  T handler;
public:
  CommandImpl(uint64_t id, T h): handler(h), Command(id) {}
  int64_t operator()() {
    return handler();
  }
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
  VEO_TRACE(this, "%s()", __func__);
  ret = vedl_wait_exception(this->os_handle->ve_handle, &exs);
  if (ret != 0) {
    throw VEOException("vedl_wait_exception failed", errno);
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
void ThreadContext::_doCall(uint64_t addr, const CallArgs &args)
{
  VEO_TRACE(this, "%s(%p, ...)", __func__, (void *)addr);
  VEO_DEBUG(this, "VE function = %p", (void *)addr);
  ve_set_user_reg(this->os_handle, SR12, addr, ~0UL);

  auto nargs = args.numArgs();
  uint64_t locals_start = this->ve_sp - 8 * ((args.locals.size() + 7) / 8);
  // maximum 8 arguments are passed in registers
  for (int i = 0; i < std::min(nargs, 8); i++)
    ve_set_user_reg(this->os_handle, SR00 + i, args.get(locals_start, i), ~0UL);
  // when more than 8 args or local variables: pass on stack
  if (nargs > 8 || args.locals.size() > 0) {
    // stack length = (2 + RSA + args + (locals/8)) * 8 bytes
    int64_t stack[22 + nargs + ((args.locals.size() + 7) / 8)];
    // locals are local variables passed on stack and referenced by arguments
    if (args.locals.size() > 0) {
      VEO_DEBUG(this, "locals_start = %p", locals_start);
      std::memcpy((void *)&stack[22 + nargs], args.locals.data(), args.locals.size());
    }
    for (int i = 0; i < nargs; i++)
      stack[22 + i] = args.get(locals_start, i);
    VEO_DEBUG(this, "stack transfer to %p", this->ve_sp - sizeof(stack));
    if (this->proc->writeMem(this->ve_sp - sizeof(stack),
                             (void *)stack, sizeof(stack)))
      throw VEOException("stack transfer failed.");
    // and now set the stack pointer
    ve_set_user_reg(this->os_handle, SR11, this->ve_sp - sizeof(stack), ~0UL);
  }
  this->_unBlock(nargs > 0 ? args.get(locals_start, 0) : 0UL);
}

/**
 * @brief unblock VE thread
 * @param sr0 value set to SR0 on unblock, VE thread starting
 */
void ThreadContext::_unBlock(uint64_t sr0)
{
  VEO_TRACE(this, "%s()", __func__);
  VEO_DEBUG(this, "state = %d", this->state);
  un_block_and_retval_req(this->os_handle, NR_ve_sysve, sr0, 1);
  this->state = VEO_STATE_RUNNING;
}

/**
 * @brief read a return value from VE function
 *
 * Collect a return value from VE function at BLOCK.
 */
uint64_t ThreadContext::_collectReturnValue()
{
  // VE process is to stop at sysve(VE_SYSVE_VEO_BLOCK, retval);
  uint64_t args[2];
  vedl_get_syscall_args(this->os_handle->ve_handle, args, 2);
  VEO_ASSERT(args[0] == VE_SYSVE_VEO_BLOCK);
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
   * It is OK to fill singce NPTL ignores attempts to block signals
   * used internally; pthread_cancel() still works.
   */
  sigfillset(&sigmask);
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
 * @brief handle a command
 */
int ThreadContext::handleCommand(Command *cmd)
{
  int error = 0;
  auto retval = (*cmd)();
  if (retval != 0) {
    VEO_ERROR(this, "Error on posting a command (%d)", retval);
    cmd->setResult(retval, VEO_COMMAND_ERROR);
    return -1;
  }
  uint64_t exs;
  auto status = this->defaultExceptionHandler(exs);
  switch (status) {
  case VEO_HANDLER_STATUS_BLOCK_REQUESTED:
    cmd->setResult(this->_collectReturnValue(), VEO_COMMAND_OK);
    break;
  case VEO_HANDLER_STATUS_EXCEPTION:
    VEO_ERROR(this, "unexpected error (exs = 0x%016lx)", exs);
    cmd->setResult(exs, VEO_COMMAND_EXCEPTION);
    error = 1;
    break;
  default:
    VEO_ERROR(this, "unexpected status (%d)", status);
    cmd->setResult(status, VEO_COMMAND_ERROR);
    error = 1;
    break;
  }
  return error;
}

/**
 * @brief event loop
 *
 * Pseudo thread processes commands from request queue while BLOCKED.
 */
void ThreadContext::eventLoop()
{
  while (this->state == VEO_STATE_BLOCKED) {
    auto command = std::move(this->comq.popRequest());
    auto e = this->handleCommand(command.get());
    this->comq.pushCompletion(std::move(command));
    if (e) {
      this->state = VEO_STATE_EXIT;
      return;
    }
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
  auto dummy = []()->int64_t{return 0;};
  auto newc = new internal::CommandImpl<decltype(dummy)>(id, dummy);
  /* push the reply here because this function never returns. */
  newc->setResult(0, 0);
  this->comq.pushCompletion(std::unique_ptr<Command>(newc));
  pthread_exit(0);
  return 0;
}

/**
 * @brief function to be set to call_async request (command)
 */
int64_t ThreadContext::_callAsyncHandler(uint64_t addr, const CallArgs &args)
{
  VEO_TRACE(this, "%s()", __func__);
  this->_doCall(addr, args);
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
  auto id = this->issueRequestID();
  auto f = std::bind(&ThreadContext::_closeCommandHandler, this, id);
  std::unique_ptr<Command> req(new internal::CommandImpl<decltype(f)>(id, f));
  this->comq.pushRequest(std::move(req));
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
uint64_t ThreadContext::callAsync(uint64_t addr, const CallArgs &args)
{
  auto id = this->issueRequestID();
  auto f = std::bind(&ThreadContext::_callAsyncHandler, this, addr, args);
  std::unique_ptr<Command> req(new internal::CommandImpl<decltype(f)>(id, f));
  this->comq.pushRequest(std::move(req));
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
 * @retval VEO_COMMAND_UNFINISHED the command is not finished, yet.
 */
int ThreadContext::callPeekResult(uint64_t reqid, uint64_t *retp)
{
  auto c = this->comq.peekCompletion(reqid);
  if (c != nullptr) {
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
 */
int ThreadContext::callWaitResult(uint64_t reqid, uint64_t *retp)
{
  auto c = this->comq.waitCompletion(reqid);
  *retp = c->getRetval();
  return c->getStatus();
}

/**
 * @brief determinant of clone() system call
 */
bool _is_clone_request(int rv_handler)
{
  return rv_handler == NR_ve_clone;
}
} // namespace veo

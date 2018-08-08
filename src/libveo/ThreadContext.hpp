/**
 * @file ThreadContext.hpp
 * @brief VEO thread context
 */
#ifndef _VEO_THREAD_CONTEXT_HPP_
#define _VEO_THREAD_CONTEXT_HPP_

#include "Command.hpp"

#include <pthread.h>
#include <semaphore.h>

#include <ve_offload.h>

extern "C" {
#include <libvepseudo.h>
}

namespace veo {
/**
 * @brief status returned from exception handler
 */
enum ExceptionHandlerStatus {
  VEO_HANDLER_STATUS_EXCEPTION = -1,//!< hardware exception
  VEO_HANDLER_STATUS_TERMINATED = 0,//!< no longer RUNNING
  VEO_HANDLER_STATUS_BLOCK_REQUESTED,//!< VE thread requests BLOCK.
};

class ProcHandle;
class RequestHandle;
class CallArgs;
namespace internal {
class CommandExecuteVE;
}

/**
 * @brief VEO thread context
 */
class ThreadContext {
  friend class ProcHandle;// ProcHandle controls the main thread directly.
  friend class internal::CommandExecuteVE;
  typedef bool (ThreadContext::*SyscallFilter)(int, int *);
private:
  pthread_t pseudo_thread;
  veos_handle *os_handle;
  ProcHandle *proc;
  CommQueue comq;
  veo_context_state state;
  bool is_main_thread;
  uint64_t seq_no;
  uint64_t ve_sp;

  bool defaultFilter(int, int *);
  bool hookCloneFilter(int, int *);
  int handleSingleException(uint64_t &, SyscallFilter);
  /**
   * @brief exception handler while RUNNING
   * @param[out] exc EXS register value at return
   * @param filter system call filter function
   * @return VEO_HANDLER_STATUS_TERMINATED (=0) the VEO context state is
   *     no longer RUNNING; positive upon filtered; -1 upon exceptions.
   *
   * This function repeats handling exceptions while this VEO context
   * (VE thread) is running normally.
   */
  int exceptionHandler(uint64_t &exc, SyscallFilter filter) {
    while (this->state == VEO_STATE_RUNNING) {
      auto rv = this->handleSingleException(exc, filter);
      if (rv != 0) {
        return rv;
      }
    }
    return VEO_HANDLER_STATUS_TERMINATED;
  }
  void _unBlock(uint64_t);
  int handleCommand(Command *);
  void eventLoop();
  /**
   * @brief Issue a new request ID
   * @return a request ID, 64 bit integer, to identify a command
   */
  uint64_t issueRequestID() {
    uint64_t ret = VEO_REQUEST_ID_INVALID;
    while (ret == VEO_REQUEST_ID_INVALID) {
      ret = __atomic_fetch_add(&this->seq_no, 1, __ATOMIC_SEQ_CST);
    }
    return ret;
  }
  // handlers for commands
  int64_t _closeCommandHandler(uint64_t);
  int64_t _callAsyncHandler(uint64_t, CallArgs &);
  int _executeVE(Command *);
  int _readMem(void *, uint64_t, size_t);
  int _writeMem(uint64_t, const void *, size_t);
public:
  ThreadContext(ProcHandle *, veos_handle *, bool is_main = false);
  ~ThreadContext() {};
  ThreadContext(const ThreadContext &) = delete;//non-copyable
  veo_context_state getState() { return this->state; }
  uint64_t callAsync(uint64_t, CallArgs &);
  int callWaitResult(uint64_t, uint64_t *);
  int callPeekResult(uint64_t, uint64_t *);
  uint64_t asyncReadMem(void *, uint64_t, size_t);
  uint64_t asyncWriteMem(uint64_t, const void *, size_t);

  /**
   * @brief default exception handler
   * @param[out] exc EXS register value at return
   * @return VEO_HANDLER_STATUS_TERMINATED (=0) the VEO context state is
   *     no longer RUNNING; positive upon filtered; -1 upon exceptions.
   */
  int defaultExceptionHandler(uint64_t &exc) {
    return this->exceptionHandler(exc, &ThreadContext::defaultFilter);
  }
  void _doCall(uint64_t addr, CallArgs &args);
  uint64_t _collectReturnValue();
  long handleCloneRequest();
  void startEventLoop(veos_handle *, sem_t *);

  veo_thr_ctxt *toCHandle() {
    return reinterpret_cast<veo_thr_ctxt *>(this);
  }
  bool isMainThread() { return this->is_main_thread;}
  int close();

};

bool _is_clone_request(int);
} // namespace veo
#endif

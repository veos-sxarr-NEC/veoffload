/**
 * @file ProcHandle.hpp
 * @brief VEO process handle
 */
#ifndef _VEO_PROC_HANDLE_HPP_
#define _VEO_PROC_HANDLE_HPP_
#include <memory>
#include <mutex>
#include <iostream>

#include <ve_offload.h>
#include <veorun.h>
#include "ThreadContext.hpp"
#include "VEOException.hpp"

namespace veo {

/**
 * @brief VEO process handle
 */
class ProcHandle {
private:
  std::mutex main_mutex;//!< acquire while using main_thread
  std::unique_ptr<ThreadContext> main_thread;
  std::unique_ptr<ThreadContext> worker;
  struct veo__helper_functions funcs;

  /**
   * @brief run VE main thread until BLOCK call.
   */
  void waitForBlock() {
    uint64_t exception;
    if (this->main_thread->defaultExceptionHandler(exception)
        != VEO_HANDLER_STATUS_BLOCK_REQUESTED) {
      throw VEOException("Unexpected exception occured");
    }
  }
  veos_handle *osHandle() { return this->main_thread->os_handle; }
public:
  ProcHandle(const char *, const char *, const char *);
  ~ProcHandle();

  uint64_t loadLibrary(const char *);
  uint64_t getSym(const uint64_t, const char *);

  uint64_t allocBuff(const size_t);
  void freeBuff(const uint64_t);

  int readMem(void *, uint64_t, size_t);
  int writeMem(uint64_t, const void *, size_t);

  void exitProc(void);

  ThreadContext *openContext();
  
  veo_proc_handle *toCHandle() {
    return reinterpret_cast<veo_proc_handle *>(this);
  }
};
} // namespace veo
#endif

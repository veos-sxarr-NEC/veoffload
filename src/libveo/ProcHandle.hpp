/**
 * @file ProcHandle.hpp
 * @brief VEO process handle
 */
#ifndef _VEO_PROC_HANDLE_HPP_
#define _VEO_PROC_HANDLE_HPP_
#include <unordered_map>
#include <utility>
#include <memory>
#include <mutex>
#include <iostream>

#include <ve_offload.h>
#include <veorun.h>
#include "ThreadContext.hpp"
#include "VEOException.hpp"

namespace std {
    template <>
    class hash<std::pair<uint64_t, std::string>> {
    public:
        size_t operator()(const std::pair<uint64_t, std::string>& x) const{
            return hash<uint64_t>()(x.first) ^ hash<std::string>()(x.second);
        }
    };
}

namespace veo {

/**
 * @brief VEO process handle
 */
class ProcHandle {
private:
  std::unordered_map<std::pair<uint64_t, std::string>, uint64_t> sym_name;
  std::mutex sym_mtx;
  std::mutex main_mutex;//!< acquire while using main_thread
  std::unique_ptr<ThreadContext> main_thread;
  std::unique_ptr<ThreadContext> worker;
  struct veo__helper_functions funcs;
  int num_child_threads;
  int ve_number;

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
  int setnumChildThreads(int );
  int getnumChildThreads(){ return num_child_threads; };
  void exitProc(void);

  ThreadContext *openContext();
  
  veo_proc_handle *toCHandle() {
    return reinterpret_cast<veo_proc_handle *>(this);
  }

  int veNumber() { return this->ve_number; }
};
} // namespace veo
#endif

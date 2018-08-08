/**
 * @file AsyncTransfer.cpp
 * @brief implementation of asynchronous memory transfer
 */
#include "ProcHandle.hpp"
#include "ThreadContext.hpp"
#include "CommandImpl.hpp"

namespace veo {
/**
 * @brief asynchronously read data from VE memory
 *
 * @param[out] dst buffer to store data
 * @param src VEMVA to read
 * @param size size to transfer in byte
 * @return request ID
 */
uint64_t ThreadContext::asyncReadMem(void *dst, uint64_t src, size_t size)
{
  auto id = this->issueRequestID();
  auto f = std::bind(&ProcHandle::readMem, this->proc, dst, src, size);
  std::unique_ptr<Command> req(new internal::CommandImpl(id, f));
  this->comq.pushRequest(std::move(req));
  return id;
}

uint64_t ThreadContext::asyncWriteMem(uint64_t dst, const void *src,
                                      size_t size)
{
  auto id = this->issueRequestID();
  auto f = std::bind(&ProcHandle::writeMem, this->proc, dst, src, size);
  std::unique_ptr<Command> req(new internal::CommandImpl(id, f));
  this->comq.pushRequest(std::move(req));
  return id;
}
} // namespace veo

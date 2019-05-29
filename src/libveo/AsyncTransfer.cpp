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
  if( this->state == VEO_STATE_EXIT )
    return VEO_REQUEST_ID_INVALID;

  auto id = this->issueRequestID();
  auto f = [this, dst, src, size] (Command *cmd) {
    auto rv = this->_readMem(dst, src, size);
    cmd->setResult(rv, rv == 0 ? VEO_COMMAND_OK : VEO_COMMAND_ERROR);
    return rv;
  };
  std::unique_ptr<Command> req(new internal::CommandImpl(id, f));

//  return this->comq.pushRequest(std::move(req), this);
  this->comq.pushRequest(std::move(req));
  return id;
}

uint64_t ThreadContext::asyncWriteMem(uint64_t dst, const void *src,
                                      size_t size)
{
  if( this->state == VEO_STATE_EXIT )
    return VEO_REQUEST_ID_INVALID;

  auto id = this->issueRequestID();
  auto f = [this, dst, src, size] (Command *cmd) {
    auto rv = this->_writeMem(dst, src, size);
    cmd->setResult(rv, rv == 0 ? VEO_COMMAND_OK : VEO_COMMAND_ERROR);
    return rv;
  };
  std::unique_ptr<Command> req(new internal::CommandImpl(id, f));

//  return this->comq.pushRequest(std::move(req), this);
  this->comq.pushRequest(std::move(req));
  return id;
}
} // namespace veo

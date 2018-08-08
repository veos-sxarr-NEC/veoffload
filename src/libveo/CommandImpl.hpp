/**
 * @file CommandImpl.hpp
 * @brief VEO command implementation
 */
#include <functional>
#include "Command.hpp"

namespace veo {
namespace internal {
/**
 * @brief a command handled by pseudo thread
 */
class CommandImpl: public Command {
public:
  using proctype = std::function<int64_t()>;
private:
  proctype handler;
public:
  CommandImpl(uint64_t id, proctype h): handler(h), Command(id) {}
  int64_t operator()() {
    return this->handler();
  }
  CommandImpl() = delete;
  CommandImpl(const CommandImpl &) = delete;
};

class CommandExecuteVE: public CommandImpl {
public:
  using postproctype = std::function<void()>;
private:
  ThreadContext *context;
  postproctype posthandler;
public:
  CommandExecuteVE(ThreadContext *tc, uint64_t id,
                   CommandImpl::proctype p, postproctype ppost):
    CommandImpl(id, p), context(tc), posthandler(ppost) {}
  int64_t operator()() {
    auto rv = this->CommandImpl::operator()();
    if (rv != 0) {
      this->setResult(rv, VEO_COMMAND_ERROR);
      return -1;
    }
    rv = context->_executeVE(this);
    this->posthandler();
    return rv;
  }
  CommandExecuteVE() = delete;
  CommandExecuteVE(const CommandExecuteVE &) = delete;
};
} // namespace internal
} // namespace veo

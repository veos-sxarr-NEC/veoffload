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
  using proctype = std::function<int64_t(Command *)>;
private:
  proctype handler;
public:
  CommandImpl(uint64_t id, proctype h): handler(h), Command(id) {}
  int operator()() {
    return this->handler(this);
  }
  CommandImpl() = delete;
  CommandImpl(const CommandImpl &) = delete;
};

} // namespace internal
} // namespace veo

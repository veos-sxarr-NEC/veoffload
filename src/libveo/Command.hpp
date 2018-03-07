/**
 * @file Command.hpp
 * @brief classes for communication between main and pseudo thread
 *
 * @internal
 * @author VEO
 */
#ifndef _VEO_COMMAND_HPP_
#define _VEO_COMMAND_HPP_
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "ve_offload.h"
namespace veo {
class ThreadContext;

typedef enum veo_command_state CommandStatus;

/**
 * @brief base class of command handled by pseudo thread
 *
 * a command is to be implemented as a function object inheriting Command
 * (see CommandImpl in ThreadContext.cpp).
 */
class Command {
private:
  uint64_t msgid;/*! message ID */
  uint64_t retval;/*! returned value from the function on VE */
  int status;
public:
  explicit Command(uint64_t id): msgid(id) {}
  Command() = delete;
  Command(const Command &) = delete;
  virtual int64_t operator()() = 0;
  void setResult(uint64_t r, int s) { this->retval = r; this->status = s; }
  uint64_t getID() { return this->msgid; }
  int getStatus() { return this->status; }
  uint64_t getRetval() { return this->retval; }
};

/**
 * @brief blocking queue used in CommQueue
 */
class BlockingQueue {
private:
  std::mutex mtx;
  std::condition_variable cond;
  std::deque<std::unique_ptr<Command> > queue;
  std::unique_ptr<Command> tryFindNoLock(uint64_t);
public:
  void push(std::unique_ptr<Command>);
  std::unique_ptr<Command> pop();
  std::unique_ptr<Command> tryFind(uint64_t);
  std::unique_ptr<Command> wait(uint64_t);
};

/**
 * @brief communication queue pair between main thread and pseudo thread
 */
class CommQueue {
private:
  BlockingQueue request;/*! request queue: main -> pseudo */
  BlockingQueue completion;/*! completion queue: pseudo -> main */
public:
  CommQueue() {};

  void pushRequest(std::unique_ptr<Command>);
  std::unique_ptr<Command> popRequest();
  void pushCompletion(std::unique_ptr<Command>);
  std::unique_ptr<Command> waitCompletion(uint64_t msgid);
  std::unique_ptr<Command> peekCompletion(uint64_t msgid);
};
} // namespace veo
#endif

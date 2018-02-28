#include "CallArgs.hpp"
#include "log.hpp"
#include <stdio.h>

namespace veo {

  /**
   * @brief set argument for VEO function to value
   * @param argnum argument number
   * @param val argument value
   *
   * TODO: convert this to template member function
   */
  void CallArgs::set(const int argnum, const uint64_t value) {
    this->arguments[argnum] = OneArg(value);
  }
  void CallArgs::set(const int argnum, const uint32_t value) {
    this->arguments[argnum] = OneArg((uint64_t)value);
  }
  void CallArgs::set(const int argnum, const uint16_t value) {
    this->arguments[argnum] = OneArg((uint64_t)value);
  }
  void CallArgs::set(const int argnum, const uint8_t value) {
    this->arguments[argnum] = OneArg((uint64_t)value);
  }
  void CallArgs::set(const int argnum, const int64_t value) {
    this->arguments[argnum] = OneArg(value);
  }
  void CallArgs::set(const int argnum, const int32_t value) {
    this->arguments[argnum] = OneArg((int64_t)value);
  }
  void CallArgs::set(const int argnum, const int16_t value) {
    this->arguments[argnum] = OneArg((int64_t)value);
  }
  void CallArgs::set(const int argnum, const int8_t value) {
    this->arguments[argnum] = OneArg((int64_t)value);
  }
  void CallArgs::set(const int argnum, const float value) {
    this->arguments[argnum] = OneArg(value);
  }
  void CallArgs::set(const int argnum, const double value) {
    this->arguments[argnum] = OneArg(value);
  }
  
  /**
   * @brief pass buffer on the stack and point argument argnum to it
   * @param inout argument intent
   * @param argnum argument number which will reference the buffer
   * @param buff pointer to memory buffer on VH
   * @param len length of memory buffer on VH
   */
  void CallArgs::set_on_stack(const enum veo_args_intent inout,
                              const int argnum, const char *buff,
                              const size_t len) {
    if (inout != VEO_INTENT_IN)
      throw VEOException("Only arguments with intent VEO_INTENT_IN are"
                         "currently supported!");
    // pad vector such that new elements are 64 bit aligned
    if (this->locals.size() % 8 > 0) {
      std::vector<char> fill (8 - (this->locals.size() % 8), 0);
      this->locals.insert(this->locals.end(), fill.begin(), fill.end());
    }

    // don't exceed 63MB, the initial stack page is 64MB
    if (this->locals.size() + len > MAX_LOCALS_SIZE)
      throw VEOException("locals on stack too large!");

    // remember the offset into the stack
    uint64_t old_size = this->locals.size();
    this->arguments[argnum] = OneArg(ARGTYPE_STACK_OFFS, old_size);

    // add buff content at the end of locals
    std::vector<char> add (buff, buff + len);
    this->locals.insert(this->locals.end(), add.begin(), add.end());
  }
  
  /**
   * @brief return number of arguments for VEO function
   */
  int CallArgs::numArgs() {
    int i;
    for (i = 0; i < VEO_MAX_NUM_ARGS; i++)
      if (this->arguments.find(i) == this->arguments.end())
        break;
    return i;
  }

  uint64_t CallArgs::get(const uint64_t sp, const int argnum) {
    auto k = this->arguments.find(argnum);
    if (k == this->arguments.end())
      throw VEOException("argument not found", argnum);
    auto arg = k->second;
    uint64_t res;
    if (arg.type == ARGTYPE_VALUE)
      res = arg.val.u64;
    else if (arg.type == ARGTYPE_STACK_OFFS)
      res = sp + arg.val.u64;
    else
      throw VEOException("unknown argument type", arg.type);
    VEO_DEBUG(nullptr, "setting VEO arg %d to 0x%lx", argnum, res);
    return res;
  }

} //namespace veo

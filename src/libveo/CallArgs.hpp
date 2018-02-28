/**
 * @file CallArgs.hpp
 * @brief VEO function call arguments handling
 */
#ifndef _VEO_CALL_ARGS_HPP_
#define _VEO_CALL_ARGS_HPP_


#include "VEOException.hpp"
#include <ve_offload.h>

#include <map>
#include <vector>
#include <cstring>
#include <stdlib.h>

#define MAX_LOCALS_SIZE (63 * 1024 * 1024)

namespace veo {

  enum ArgType {
    ARGTYPE_VALUE,      //< val contains a register value
    ARGTYPE_STACK_OFFS, //< val.u64 is a stack offset
    ARGTYPE_REFERENCE   //< val.u64 contains an absolute reference
  };

  struct OneArg {
    ArgType type;
    union {
      uint64_t u64;
      int64_t i64;
      float f32[2];
      double d64;
    } val;

    OneArg() : type(ARGTYPE_VALUE) {}
    OneArg(const uint64_t val) : type(ARGTYPE_VALUE) {
      this->val.u64 = val;
    }
    OneArg(const int64_t val) : type(ARGTYPE_VALUE) {
      this->val.i64 = val;
    }
    OneArg(const float val) : type(ARGTYPE_VALUE) {
      this->val.f32[0] = 0;
      this->val.f32[1] = val;
    }
    OneArg(const double val) : type(ARGTYPE_VALUE) {
      this->val.d64 = val;
    }
    OneArg(const ArgType t, const uint64_t val) : type(t) {
      this->val.u64 = val;
    }
  };
  
  struct CallArgs {
    std::map<int, OneArg> arguments;
    std::vector<char> locals;

    CallArgs() = default;

    ~CallArgs() = default;

    void clear() {
      this->arguments.clear();
      if (this->locals.size())
        this->locals.clear();
    }

    /**
     * @brief set arument for VEO function to value
     * @param argnum argument number
     * @param val argument value
     *
     * TODO: convert this to template member function
     */
    void set(const int argnum, const uint64_t value);
    void set(const int argnum, const uint32_t value);
    void set(const int argnum, const uint16_t value);
    void set(const int argnum, const uint8_t value);
    void set(const int argnum, const int64_t value);
    void set(const int argnum, const int32_t value);
    void set(const int argnum, const int16_t value);
    void set(const int argnum, const int8_t value);
    void set(const int argnum, const float value);
    void set(const int argnum, const double value);

    /**
     * @brief pass buffer on the stack and point argument argnum to it
     * @param argnum argument number which will reference the buffer
     * @param buff pointer to memory buffer on VH
     * @param len length of memory buffer on VH
     */
    void set_on_stack(const enum veo_args_intent inout, const int argnum,
                      const char *buff, const size_t len);

    /**
     * @brief return number of arguments for VEO function
     */
    int numArgs();
    
    uint64_t get(const uint64_t sp, const int argnum);
    veo_args *toCHandle() {
      return reinterpret_cast<veo_args *>(this);
    }

  };

} //namespace veo

#endif // _VEO_CALL_ARGS_HPP_

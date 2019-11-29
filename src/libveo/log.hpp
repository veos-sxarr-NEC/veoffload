#ifndef _VEO_LOG_HPP_
#define _VEO_LOG_HPP_
#include <log4c.h>
#include <stdarg.h>
namespace veo {
class ThreadContext;

enum VEOLogLevel{
  VEO_LOG_ERROR = LOG4C_PRIORITY_ERROR,
  VEO_LOG_DEBUG = LOG4C_PRIORITY_DEBUG,
  VEO_LOG_TRACE = LOG4C_PRIORITY_TRACE,
};

void veo__vlog(const ThreadContext *, const log4c_location_info_t *,
               int, const char *, va_list);
void veo__log(const ThreadContext *, const log4c_location_info_t *,
              int, const char *, ...);
} // namespace veo
#define VEO_LOG(ctx, prio, fmt, ...) do { \
  const log4c_location_info_t location__ = \
    LOG4C_LOCATION_INFO_INITIALIZER(NULL); \
  veo__log(ctx, &location__, prio, fmt, __VA_ARGS__); \
} while (0)


#define VEO_ERROR(ctx, fmt, ...) VEO_LOG(ctx, veo::VEO_LOG_ERROR, fmt, \
                                         __VA_ARGS__)
#define VEO_DEBUG(ctx, fmt, ...) VEO_LOG(ctx, veo::VEO_LOG_DEBUG, fmt, \
                                         __VA_ARGS__)
#define VEO_TRACE(ctx, fmt, ...) VEO_LOG(ctx, veo::VEO_LOG_TRACE, fmt, \
                                         __VA_ARGS__)


#define VEO_ASSERT(_cond) do { \
  if (!(_cond)) { \
    fprintf(stderr, "%s %d: Assertion failure: %s\n", \
              __FILE__, __LINE__, #_cond); \
    VEO_ERROR(nullptr, "%s %d: Assertion failure: %s", \
              __FILE__, __LINE__, #_cond); \
    abort(); \
  } \
} while (0)

#endif

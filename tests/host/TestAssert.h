#pragma once

#include <cstdio>
#include <cstdlib>

// Standard assert() disappears when NDEBUG is defined, which can also remove
// function calls placed inside the expression. Host checks must run in every
// build configuration, including Release.
#ifdef assert
#undef assert
#endif

#define assert(expression)                                                   \
  do {                                                                       \
    if (!(expression)) {                                                     \
      std::fprintf(stderr, "Test check failed: %s (%s:%d)\n", #expression, \
                   __FILE__, __LINE__);                                      \
      std::abort();                                                          \
    }                                                                        \
  } while (false)

#ifndef SIMDUTF_PORTABILITY_H
#define SIMDUTF_PORTABILITY_H

#include "simdutf/compiler_check.h"

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifndef _WIN32
  // strcasecmp, strncasecmp
  #include <strings.h>
#endif

#if defined(__apple_build_version__)
  #if __apple_build_version__ < 14000000
    #define SIMDUTF_SPAN_DISABLED                                              \
      1 // apple-clang/13 doesn't support std::convertible_to
  #endif
#endif

#if SIMDUTF_CPLUSPLUS20
  #include <version>
  #if __cpp_concepts >= 201907L && __cpp_lib_span >= 202002L &&                \
      !defined(SIMDUTF_SPAN_DISABLED)
    #define SIMDUTF_SPAN 1
  #endif // __cpp_concepts >= 201907L && __cpp_lib_span >= 202002L
  #if __cpp_lib_atomic_ref >= 201806L
    #define SIMDUTF_ATOMIC_REF 1
  #endif // __cpp_lib_atomic_ref
  #if __has_cpp_attribute(maybe_unused) >= 201603L
    #define SIMDUTF_MAYBE_UNUSED_AVAILABLE 1
  #endif // __has_cpp_attribute(maybe_unused) >= 201603L
#endif

/**
 * We want to check that it is actually a little endian system at
 * compile-time.
 */

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
  #define SIMDUTF_IS_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined(_WIN32)
  #define SIMDUTF_IS_BIG_ENDIAN 0
#else
  #if defined(__APPLE__) ||                                                    \
      defined(__FreeBSD__) // defined __BYTE_ORDER__ && defined
                           // __ORDER_BIG_ENDIAN__
    #include <machine/endian.h>
  #elif defined(sun) ||                                                        \
      defined(__sun) // defined(__APPLE__) || defined(__FreeBSD__)
    #include <sys/byteorder.h>
  #else // defined(__APPLE__) || defined(__FreeBSD__)

    #ifdef __has_include
      #if __has_include(<endian.h>)
        #include <endian.h>
      #endif //__has_include(<endian.h>)
    #endif   //__has_include

  #endif // defined(__APPLE__) || defined(__FreeBSD__)

  #ifndef !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__)
    #define SIMDUTF_IS_BIG_ENDIAN 0
  #endif

  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SIMDUTF_IS_BIG_ENDIAN 0
  #else // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SIMDUTF_IS_BIG_ENDIAN 1
  #endif // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#endif // defined __BYTE_ORDER__ && defined __ORDER_BIG_ENDIAN__

/**
 * At this point in time, SIMDUTF_IS_BIG_ENDIAN is defined.
 */

#ifdef _MSC_VER
  #define SIMDUTF_VISUAL_STUDIO 1
  /**
   * We want to differentiate carefully between
   * clang under visual studio and regular visual
   * studio.
   *
   * Under clang for Windows, we enable:
   *  * target pragmas so that part and only part of the
   *     code gets compiled for advanced instructions.
   *
   */
  #ifdef __clang__
    // clang under visual studio
    #define SIMDUTF_CLANG_VISUAL_STUDIO 1
  #else
    // just regular visual studio (best guess)
    #define SIMDUTF_REGULAR_VISUAL_STUDIO 1
  #endif // __clang__
#endif   // _MSC_VER

#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  // https://en.wikipedia.org/wiki/C_alternative_tokens
  // This header should have no effect, except maybe
  // under Visual Studio.
  #include <iso646.h>
#endif

#if (defined(__x86_64__) || defined(_M_AMD64)) && !defined(_M_ARM64EC)
  #define SIMDUTF_IS_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
  #define SIMDUTF_IS_ARM64 1
#else
  #error "simdutf supports only 64-bit x86 and ARM targets"
#endif

// this is almost standard?
#define SIMDUTF_STRINGIFY_IMPLEMENTATION_(a) #a
#define SIMDUTF_STRINGIFY(a) SIMDUTF_STRINGIFY_IMPLEMENTATION_(a)

//
// Enable valid runtime implementations, and select
// SIMDUTF_BUILTIN_IMPLEMENTATION
//

// We are going to use runtime dispatch.
#if defined(SIMDUTF_IS_X86_64)
  #ifdef __clang__
    // clang does not have GCC push pop
    // warning: clang attribute push can't be used within a namespace in clang
    // up til 8.0 so SIMDUTF_TARGET_REGION and SIMDUTF_UNTARGET_REGION must be
    // *outside* of a namespace.
    #define SIMDUTF_TARGET_REGION(T)                                           \
      _Pragma(SIMDUTF_STRINGIFY(clang attribute push(                          \
          __attribute__((target(T))), apply_to = function)))
    #define SIMDUTF_UNTARGET_REGION _Pragma("clang attribute pop")
  #elif defined(__GNUC__)
    // GCC is easier
    #define SIMDUTF_TARGET_REGION(T)                                           \
      _Pragma("GCC push_options") _Pragma(SIMDUTF_STRINGIFY(GCC target(T)))
    #define SIMDUTF_UNTARGET_REGION _Pragma("GCC pop_options")
  #endif // clang then gcc

#endif // defined(SIMDUTF_IS_X86_64)

// Default target region macros don't do anything.
#ifndef SIMDUTF_TARGET_REGION
  #define SIMDUTF_TARGET_REGION(T)
  #define SIMDUTF_UNTARGET_REGION
#endif

// Is threading enabled?
#if defined(_REENTRANT) || defined(_MT)
  #ifndef SIMDUTF_THREADS_ENABLED
    #define SIMDUTF_THREADS_ENABLED
  #endif
#endif

// workaround for large stack sizes under -O0.
// https://github.com/simdutf/simdutf/issues/691
#ifdef __APPLE__
  #ifndef __OPTIMIZE__
    // Apple systems have small stack sizes in secondary threads.
    // Lack of compiler optimization may generate high stack usage.
    // Users may want to disable threads for safety, but only when
    // in debug mode which we detect by the fact that the __OPTIMIZE__
    // macro is not defined.
    #undef SIMDUTF_THREADS_ENABLED
  #endif
#endif

#ifdef SIMDUTF_VISUAL_STUDIO
  // This is one case where we do not distinguish between
  // regular visual studio and clang under visual studio.
  // clang under Windows has _stricmp (like visual studio) but not strcasecmp
  // (as clang normally has)
  #define simdutf_strcasecmp _stricmp
  #define simdutf_strncasecmp _strnicmp
#else
  // The strcasecmp, strncasecmp, and strcasestr functions do not work with
  // multibyte strings (e.g. UTF-8). So they are only useful for ASCII in our
  // context.
  // https://www.gnu.org/software/libunistring/manual/libunistring.html#char-_002a-strings
  #define simdutf_strcasecmp strcasecmp
  #define simdutf_strncasecmp strncasecmp
#endif

#if defined(__GNUC__) && !defined(__clang__)
  #if __GNUC__ >= 11
    #define SIMDUTF_GCC11ORMORE 1
  #endif //  __GNUC__ >= 11
  #if __GNUC__ == 10
    #define SIMDUTF_GCC10 1
  #endif //  __GNUC__ == 10
  #if __GNUC__ < 10
    #define SIMDUTF_GCC9OROLDER 1
  #endif //  __GNUC__ == 10
#endif   // defined(__GNUC__) && !defined(__clang__)

#endif // SIMDUTF_PORTABILITY_H

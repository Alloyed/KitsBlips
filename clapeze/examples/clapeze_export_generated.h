
#ifndef CLAPEZE_EXPORT_H
#define CLAPEZE_EXPORT_H

#ifdef CLAPEZE_STATIC_DEFINE
#  define CLAPEZE_EXPORT
#  define CLAPEZE_NO_EXPORT
#else
#  ifndef CLAPEZE_EXPORT
#    ifdef clapeze_examples_EXPORTS
        /* We are building this library */
#      define CLAPEZE_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define CLAPEZE_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef CLAPEZE_NO_EXPORT
#    define CLAPEZE_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef CLAPEZE_DEPRECATED
#  define CLAPEZE_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef CLAPEZE_DEPRECATED_EXPORT
#  define CLAPEZE_DEPRECATED_EXPORT CLAPEZE_EXPORT CLAPEZE_DEPRECATED
#endif

#ifndef CLAPEZE_DEPRECATED_NO_EXPORT
#  define CLAPEZE_DEPRECATED_NO_EXPORT CLAPEZE_NO_EXPORT CLAPEZE_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef CLAPEZE_NO_DEPRECATED
#    define CLAPEZE_NO_DEPRECATED
#  endif
#endif

#endif /* CLAPEZE_EXPORT_H */

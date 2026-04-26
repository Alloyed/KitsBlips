#if defined(__linux__) && defined(__GLIBC__)
// from https://gcc.gnu.org/onlinedocs/gcc-3.3.6/g77/Floating_002dpoint-Exception-Handling.html
#define _GNU_SOURCE 1
#include <fenv.h>
static void __attribute__((constructor)) trapfpe() {
    /* Enable some exceptions.  At startup all exceptions are masked.  */

    feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
}
#endif
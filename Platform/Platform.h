#if defined(__linux__) || defined(__GLIBC__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
  defined(__OpenBSD__)
/* TODO: actually test other *nixes */
#include "X11.c"
#include "Platform/LibCMath.c"
#else
#error "unknown platform"
#endif

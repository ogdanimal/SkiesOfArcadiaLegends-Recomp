// Linux releases build on Ubuntu 22.04 (glibc 2.35) so they run on it and anything newer, but Dawn's prebuilt
// package was compiled against glibc 2.38+, whose headers turn strtol and friends into the C23 entry points
// (__isoc23_*: the same functions, plus "0b" prefixes). On an older glibc they forward to the classic ones.
#if defined(__linux__) && !defined(__ANDROID__)
#include <features.h>
#if defined(__GLIBC__) && !__GLIBC_PREREQ(2, 38)
#include <stdlib.h>

long __isoc23_strtol(const char* nptr, char** endptr, int base)
{
  return strtol(nptr, endptr, base);
}
long long __isoc23_strtoll(const char* nptr, char** endptr, int base)
{
  return strtoll(nptr, endptr, base);
}
unsigned long __isoc23_strtoul(const char* nptr, char** endptr, int base)
{
  return strtoul(nptr, endptr, base);
}
unsigned long long __isoc23_strtoull(const char* nptr, char** endptr, int base)
{
  return strtoull(nptr, endptr, base);
}
#endif
#endif

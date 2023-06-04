#include <stdio.h>
#include "micro_printf.h"

#if defined(STD)
#define SNPRINTF snprintf
#else
#define SNPRINTF m_snprintf
#endif

int main(void) {
  char buf[100];
  return SNPRINTF(buf, sizeof(buf), "%-08x %.*s %g", 123, 3, "abcd", 1.234);
}

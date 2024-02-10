// Copyright (c) 2023 Cesanta Software Limited
// All rights reserved

#include <assert.h>
#include <float.h>   // DBL_EPSILON and HUGE_VAL
#include <math.h>    // NAN
#include <stdio.h>   // printf/snprintf etc
#include <string.h>  // strcmp

#include "str.h"

static int sf(const char *expected, const char *fmt, ...) {
  char buf[100];
  va_list ap;
  int result;
  va_start(ap, fmt);
  xvsnprintf(buf, sizeof(buf), fmt, &ap);
  va_end(ap);
  result = strcmp(expected, buf) == 0;
  if (!result) printf("[%s] != [%s]\n", expected, buf);
  return result;
}

// This function compares micro_printf() with the libc's printf()
static int sn(const char *fmt, ...) {
  char buf[100], tmp[1] = {0}, buf2[sizeof(buf)];
  size_t n, n2, n1;
  va_list ap;
  int result;
  va_start(ap, fmt);
  n = (size_t) vsnprintf(buf2, sizeof(buf2), fmt, ap);
  va_end(ap);
  va_start(ap, fmt);
  n1 = xvsnprintf(buf, sizeof(buf), fmt, &ap);
  va_end(ap);
  va_start(ap, fmt);
  n2 = xvsnprintf(tmp, 0, fmt, &ap);
  va_end(ap);
  result = n1 == n2 && n1 == n && strcmp(buf, buf2) == 0;
  if (!result)
    printf("[%s] -> [%s] != [%s] %ld %ld %ld\n", fmt, buf, buf2, (long) n1,
           (long) n2, (long) n);
  return result;
}

static void test_std(void) {
  assert(sn("%d", 0));
  assert(sn("%d", 1));
  assert(sn("%d", -1));
  assert(sn("%.*s", 0, "ab"));
  assert(sn("%.*s", 1, "ab"));
  assert(sn("%.1s", "ab"));
  assert(sn("%.99s", "a"));
  assert(sn("%11s", "a"));
  assert(sn("%s", "a\0b"));
  assert(sn("%2s", "a"));
  assert(sn("%.*s", 3, "a\0b"));
  assert(sn("%d", 7));
  assert(sn("%d", 123));
#if !defined(_MSC_VER)
  assert(sn("%lld", (uint64_t) 0xffffffffff));
  assert(sn("%lld", (uint64_t) -1));
  assert(sn("%llu", (uint64_t) -1));
  assert(sn("%llx", (uint64_t) 0xffffffffff));
  assert(sn("%p", (void *) (size_t) 7));
#endif
  assert(sn("%lx", (unsigned long) 0x6204d754));
  assert(sn("ab"));
  assert(sn("%dx", 1));
  assert(sn("%sx", "a"));
  assert(sn("%cx", 32));
  assert(sn("%x", 15));
  assert(sn("%2x", 15));
  assert(sn("%02x", 15));
  assert(sn("%hx:%hhx", (short) 1, (char) 2));
  assert(sn("%hx:%hhx", (short) 1, (char) 2));
  assert(sn("%%"));
  assert(sn("%x", 15));
  assert(sn("%#x", 15));
  assert(sn("%#6x", 15));
  assert(sn("%#06x", 15));
  assert(sn("%#02x", 11));
  assert(sn("%#-6x", 15));
  assert(sn("%-2s!", "a"));
  assert(sn("%s %s", "a", "b"));
  assert(sn("%s %s", "a", "b"));
  assert(sn("ab%dc", 123));
  assert(sn("%s ", "a"));
  assert(sn("%s %s", "a", "b"));
  assert(sn("%2s %s", "a", "b"));

  assert(sf("foo %v", "foo %v", 123));  // Uknown specifier left intact
}

static void test_float(void) {
  char tmp[40];
#define DBLWIDTH(a, b) a, b
#define TEST_FLOAT(fmt_, num_, res_)                                          \
  do {                                                                        \
    const char *N = #num_;                                                    \
    size_t n = xsnprintf(tmp, sizeof(tmp), fmt_, num_);                       \
    if (0) printf("[%s] [%s] -> [%s] [%.*s]\n", fmt_, N, res_, (int) n, tmp); \
    assert(n == strlen(res_));                                                \
    assert(strcmp(tmp, res_) == 0);                                           \
  } while (0)

  TEST_FLOAT("%g", 0.0, "0");
  TEST_FLOAT("%g", 0.123, "0.123");
  TEST_FLOAT("%g", 0.00123, "0.00123");
  TEST_FLOAT("%g", 0.123456333, "0.123456");
  TEST_FLOAT("%g", 123.0, "123");
  TEST_FLOAT("%g", 11.5454, "11.5454");
  TEST_FLOAT("%g", 11.0001, "11.0001");
  TEST_FLOAT("%g", 0.999, "0.999");
  TEST_FLOAT("%g", 0.999999, "0.999999");
  TEST_FLOAT("%g", 0.9999999, "1");
  TEST_FLOAT("%g", 10.9, "10.9");
  TEST_FLOAT("%g", 10.01, "10.01");
  TEST_FLOAT("%g", 1.0, "1");
  TEST_FLOAT("%g", 10.0, "10");
  TEST_FLOAT("%g", 100.0, "100");
  TEST_FLOAT("%g", 1000.0, "1000");
  TEST_FLOAT("%g", 10000.0, "10000");
  TEST_FLOAT("%g", 100000.0, "100000");
  TEST_FLOAT("%g", 1000000.0, "1e+06");
  TEST_FLOAT("%g", 10000000.0, "1e+07");
  TEST_FLOAT("%g", 100000001.0, "1e+08");
  TEST_FLOAT("%g", 10.5454, "10.5454");
  TEST_FLOAT("%g", 999999.0, "999999");
  TEST_FLOAT("%g", 9999999.0, "1e+07");
  TEST_FLOAT("%g", 44556677.0, "4.45567e+07");
  TEST_FLOAT("%g", 1234567.2, "1.23457e+06");
  TEST_FLOAT("%g", -987.65432, "-987.654");
  TEST_FLOAT("%g", 0.0000000001, "1e-10");
  TEST_FLOAT("%g", 2.34567e-57, "2.34567e-57");
  TEST_FLOAT("%.*g", DBLWIDTH(7, 9999999.0), "9999999");
  TEST_FLOAT("%.*g", DBLWIDTH(10, 0.123456333), "0.123456333");
  TEST_FLOAT("%g", 123.456222, "123.456");
  TEST_FLOAT("%.*g", DBLWIDTH(10, 123.456222), "123.456222");
  TEST_FLOAT("%g", 600.1234, "600.123");
  TEST_FLOAT("%g", -600.1234, "-600.123");
  TEST_FLOAT("%g", 599.1234, "599.123");
  TEST_FLOAT("%g", -599.1234, "-599.123");

#ifndef _WIN32
  TEST_FLOAT("%g", (double) INFINITY, "inf");
  TEST_FLOAT("%g", (double) -INFINITY, "-inf");
  TEST_FLOAT("%g", (double) NAN, "nan");
#else
  TEST_FLOAT("%g", HUGE_VAL, "inf");
  TEST_FLOAT("%g", -HUGE_VAL, "-inf");
#endif

  {
    char buf[20];
    double d;
    json_get_num("1.23", 4, "$", &d);      // Parse floating point number
    xsnprintf(buf, sizeof(buf), "%g", d);  // 1.23 (print parsed number)
  }
}

static void out(char ch, void *arg) {
  putchar(ch);
  (void) arg;
}

static void test_m(void) {
  uint8_t mac[6] = {1, 2, 3, 4, 5, 6};     // MAC address
  uint8_t ip6[16] = {1, 100, 33};          // IPv6 address
  uint32_t ip4 = 0x0100007f;               // IPv4 address
  const char *esc = "_a\\nb_123";          // Expected escaped string
  const char *quo = "_\"127.0.0.1\"_123";  // Expected quoted string
  assert(sf("_127.0.0.1_123", "_%M_%d", fmt_ip4, &ip4, 123));
  assert(sf("_[164:2100:0:0:0:0:0:0]_123", "_%M_%d", fmt_ip6, ip6, 123));
  assert(sf("_01:02:03:04:05:06_123", "_%M_%d", fmt_mac, mac, 123));
  assert(sf(esc, "_%M_%d", XESC("a\nb"), 123));
  assert(sf("_eHl6_123", "_%M_%d", fmt_b64, 3, "xyz", 123));
  assert(sf(quo, "_%m_%d", fmt_ip4, &ip4, 123));
  assert(sf(quo, "_%m_%d", XESC("127.0.0.1"), 123));

  xprintf(out, NULL, "%s: %g\n", "dbl", 1.234);  // dbl: 1.234
  xprintf(out, NULL, "%.*s\n", 3, "foobar");     // foo
  xprintf(out, NULL, "%#04x\n", 11);             // 0x0b
  xprintf(out, NULL, "%d %5s\n", 7, "pad");      // 7   pad
  // JSON: {"value": 1.234}
  xprintf(out, NULL, "JSON: {%m: %g}\n", XESC("value"), 1.234);
}

static void test_json(void) {
  char buf[100];
  const char *s = "{\"a\": -42, \"b\": [\"hi\\t\\u0020\", true, { }, -1.7e-2]}";
  const char *s2 = "\"foobar\"";
  int ofs, n, b = 0, len = (int) strlen(s);
  double d = 0.0;
  assert(json_get_long(s, len, "$.a", 0) == -42);
  assert(json_get_str(s, len, "$.b[0]", buf, sizeof(buf)) == 4);
  assert(strcmp(buf, "hi\t ") == 0);
  assert(json_get_str(s, len, "$.b[0]", buf, 4) < 0);
  assert(json_get_bool(s, len, "$.b[1]", &b) == 1 && b == 1);
  assert(json_get(s, len, "$.c", &n) < 0);
  assert(json_get(s, len, "$.b[4]", &n) < 0);
  assert((ofs = json_get(s, len, "$.b[2]", &n)) > 0);
  assert(n == 3 && s[ofs] == '{' && s[ofs + 2] == '}');
  assert(json_get_num(s, len, "$.b[3]", &d) == 1 && d == -0.017);
  assert((ofs = json_get(s2, (int) strlen(s2), "$", &n)) == 0);
  assert(n == 8);
}

static void test_base64(void) {
  char a[100], b[100];
  memset(a, ' ', sizeof(a));
  memset(b, ' ', sizeof(b));
  xsnprintf(a, sizeof(a), "%m", fmt_b64, 2, "hi");
  assert(strcmp(a, "\"aGk=\"") == 0);
  assert(json_get_b64(a, (int) strlen(a), "$", b, sizeof(b)) == 2);
  assert(strcmp(b, "hi") == 0);
}

int main(void) {
  test_std();
  test_float();
  test_m();
  test_json();
  test_base64();
  printf("SUCCESS\n");
  return 0;
}

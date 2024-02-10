// Copyright (c) 2023 Cesanta Software Limited
// SPDX-License-Identifier: AGPL-3.0 or commercial

#ifndef STR_H_
#define STR_H_

#include <stdarg.h>
#include <stddef.h>

#if defined(_MSC_VER) && _MSC_VER < 1700
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef unsigned char uint8_t;
typedef char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#define inline __inline
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define XESC(str) fmt_esc, 0, (str)

// Low level basic functions
size_t xvprintf(void (*)(char, void *), void *, const char *, va_list *);
size_t xprintf(void (*)(char, void *), void *, const char *, ...);

// Convenience wrappers around xprintf
size_t xvsnprintf(char *buf, size_t len, const char *fmt, va_list *ap);
size_t xsnprintf(char *, size_t, const char *fmt, ...);

// Pre-defined %M/%m formatting functions
size_t fmt_ip4(void (*fn)(char, void *), void *arg, va_list *ap);
size_t fmt_ip6(void (*fn)(char, void *), void *arg, va_list *ap);
size_t fmt_mac(void (*fn)(char, void *), void *arg, va_list *ap);
size_t fmt_b64(void (*fn)(char, void *), void *arg, va_list *ap);
size_t fmt_esc(void (*fn)(char, void *), void *arg, va_list *ap);

// Utility functions
void xhexdump(void (*fn)(char, void *), void *arg, const void *buf, size_t len);
size_t xb64_decode(const char *src, size_t slen, char *dst, size_t dlen);

// JSON parsing API
int json_get(const char *buf, int len, const char *path, int *size);
int json_get_num(const char *buf, int len, const char *path, double *val);
int json_get_bool(const char *buf, int len, const char *path, int *val);
long json_get_long(const char *buf, int len, const char *path, long dflt);
int json_get_str(const char *buf, int len, const char *path, char *dst,
                 size_t dlen);
int json_get_b64(const char *buf, int len, const char *path, char *dst,
                 size_t dlen);

#if !defined(STR_API_ONLY)
typedef void (*xout_t)(char, void *);                 // Output function
typedef size_t (*xfmt_t)(xout_t, void *, va_list *);  // %M format function

struct xbuf {
  char *buf;
  size_t size, len;
};

static void xout_buf(char ch, void *param) {
  struct xbuf *mb = (struct xbuf *) param;
  if (mb->len < mb->size) mb->buf[mb->len] = ch;
  mb->len++;
}

size_t xvsnprintf(char *buf, size_t len, const char *fmt, va_list *ap) {
  struct xbuf mb = {buf, len, 0};
  size_t n = xvprintf(xout_buf, &mb, fmt, ap);
  if (len > 0) buf[n < len ? n : len - 1] = '\0';  // NUL terminate
  return n;
}

size_t xsnprintf(char *buf, size_t len, const char *fmt, ...) {
  va_list ap;
  size_t n;
  va_start(ap, fmt);
  n = xvsnprintf(buf, len, fmt, &ap);
  va_end(ap);
  return n;
}

size_t fmt_ip4(void (*fn)(char, void *), void *arg, va_list *ap) {
  uint8_t *p = va_arg(*ap, uint8_t *);
  return xprintf(fn, arg, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
}

#define U16(p) ((((uint16_t) (p)[0]) << 8) | (p)[1])
size_t fmt_ip6(void (*fn)(char, void *), void *arg, va_list *ap) {
  uint8_t *p = va_arg(*ap, uint8_t *);
  return xprintf(fn, arg, "[%x:%x:%x:%x:%x:%x:%x:%x]", U16(&p[0]), U16(&p[2]),
                 U16(&p[4]), U16(&p[6]), U16(&p[8]), U16(&p[10]), U16(&p[12]),
                 U16(&p[14]));
}

size_t fmt_mac(void (*fn)(char, void *), void *arg, va_list *ap) {
  uint8_t *p = va_arg(*ap, uint8_t *);
  return xprintf(fn, arg, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2],
                 p[3], p[4], p[5]);
}

static int xisdigit(int c) {
  return c >= '0' && c <= '9';
}

static size_t xstrlen(const char *s) {
  size_t n = 0;
  while (s[n] != '\0') n++;
  return n;
}

static int addexp(char *buf, int e, int sign) {
  int n = 0;
  buf[n++] = 'e';
  buf[n++] = (char) sign;
  if (e > 400) return 0;
  if (e < 10) buf[n++] = '0';
  if (e >= 100) buf[n++] = (char) (e / 100 + '0'), e -= 100 * (e / 100);
  if (e >= 10) buf[n++] = (char) (e / 10 + '0'), e -= 10 * (e / 10);
  buf[n++] = (char) (e + '0');
  return n;
}

static int xisinf(double x) {
  union {
    double f;
    uint64_t u;
  } ieee754 = {x};
  return ((unsigned) (ieee754.u >> 32) & 0x7fffffff) == 0x7ff00000 &&
         ((unsigned) ieee754.u == 0);
}

static int xisnan(double x) {
  union {
    double f;
    uint64_t u;
  } ieee754 = {x};
  return ((unsigned) (ieee754.u >> 32) & 0x7fffffff) +
             ((unsigned) ieee754.u != 0) >
         0x7ff00000;
}

static size_t xdtoa(char *dst, size_t dstlen, double d, int width, int tz) {
  char buf[40];
  int i, s = 0, n = 0, e = 0;
  double t, mul, saved;
  if (d == 0.0) return xsnprintf(dst, dstlen, "%s", "0");
  if (xisinf(d)) return xsnprintf(dst, dstlen, "%s", d > 0 ? "inf" : "-inf");
  if (xisnan(d)) return xsnprintf(dst, dstlen, "%s", "nan");
  if (d < 0.0) d = -d, buf[s++] = '-';

  // Round
  saved = d;
  mul = 1.0;
  while (d >= 10.0 && d / mul >= 10.0) mul *= 10.0;
  while (d <= 1.0 && d / mul <= 1.0) mul /= 10.0;
  for (i = 0, t = mul * 5; i < width; i++) t /= 10.0;
  d += t;
  // Calculate exponent, and 'mul' for scientific representation
  mul = 1.0;
  while (d >= 10.0 && d / mul >= 10.0) mul *= 10.0, e++;
  while (d < 1.0 && d / mul < 1.0) mul /= 10.0, e--;
  // printf(" --> %g %d %g %g\n", saved, e, t, mul);

  if (e >= width && width > 1) {
    n = (int) xdtoa(buf, sizeof(buf), saved / mul, width, tz);
    // printf(" --> %.*g %d [%.*s]\n", 10, d / t, e, n, buf);
    n += addexp(buf + s + n, e, '+');
    return xsnprintf(dst, dstlen, "%.*s", n, buf);
  } else if (e <= -width && width > 1) {
    n = (int) xdtoa(buf, sizeof(buf), saved / mul, width, tz);
    // printf(" --> %.*g %d [%.*s]\n", 10, d / mul, e, n, buf);
    n += addexp(buf + s + n, -e, '-');
    return xsnprintf(dst, dstlen, "%.*s", n, buf);
  } else {
    for (i = 0, t = mul; t >= 1.0 && s + n < (int) sizeof(buf); i++) {
      int ch = (int) (d / t);
      if (n > 0 || ch > 0) buf[s + n++] = (char) (ch + '0');
      d -= ch * t;
      t /= 10.0;
    }
    // printf(" --> [%g] -> %g %g (%d) [%.*s]\n", saved, d, t, n, s + n, buf);
    if (n == 0) buf[s++] = '0';
    while (t >= 1.0 && n + s < (int) sizeof(buf)) buf[n++] = '0', t /= 10.0;
    if (s + n < (int) sizeof(buf)) buf[n + s++] = '.';
    // printf(" 1--> [%g] -> [%.*s]\n", saved, s + n, buf);
    for (i = 0, t = 0.1; s + n < (int) sizeof(buf) && n < width; i++) {
      int ch = (int) (d / t);
      buf[s + n++] = (char) (ch + '0');
      d -= ch * t;
      t /= 10.0;
    }
  }
  while (tz && n > 0 && buf[s + n - 1] == '0') n--;  // Trim trailing zeroes
  if (n > 0 && buf[s + n - 1] == '.') n--;           // Trim trailing dot
  n += s;
  if (n >= (int) sizeof(buf)) n = (int) sizeof(buf) - 1;
  buf[n] = '\0';
  return xsnprintf(dst, dstlen, "%s", buf);
}

static double xatod(const char *p, int len, int *numlen) {
  double d = 0.0;
  int i = 0, sign = 1;

  // Sign
  if (i < len && *p == '-') {
    sign = -1, i++;
  } else if (i < len && *p == '+') {
    i++;
  }

  // Decimal
  for (; i < len && p[i] >= '0' && p[i] <= '9'; i++) {
    d *= 10.0;
    d += p[i] - '0';
  }
  d *= sign;

  // Fractional
  if (i < len && p[i] == '.') {
    double frac = 0.0, base = 0.1;
    i++;
    for (; i < len && p[i] >= '0' && p[i] <= '9'; i++) {
      frac += base * (p[i] - '0');
      base /= 10.0;
    }
    d += frac * sign;
  }

  // Exponential
  if (i < len && (p[i] == 'e' || p[i] == 'E')) {
    int j, exp = 0, minus = 0;
    i++;
    if (i < len && p[i] == '-') minus = 1, i++;
    if (i < len && p[i] == '+') i++;
    while (i < len && p[i] >= '0' && p[i] <= '9' && exp < 308)
      exp = exp * 10 + (p[i++] - '0');
    if (minus) exp = -exp;
    for (j = 0; j < exp; j++) d *= 10.0;
    for (j = 0; j < -exp; j++) d /= 10.0;
  }

  if (numlen != NULL) *numlen = i;
  return d;
}

static size_t xlld(char *buf, int64_t val, int is_signed, int is_hex) {
  const char *letters = "0123456789abcdef";
  uint64_t v = (uint64_t) val;
  size_t s = 0, n, i;
  if (is_signed && val < 0) buf[s++] = '-', v = (uint64_t) (-val);
  // This loop prints a number in reverse order. I guess this is because we
  // write numbers from right to left: least significant digit comes last.
  // Maybe because we use Arabic numbers, and Arabs write RTL?
  if (is_hex) {
    for (n = 0; v; v >>= 4) buf[s + n++] = letters[v & 15];
  } else {
    for (n = 0; v; v /= 10) buf[s + n++] = letters[v % 10];
  }
  // Reverse a string
  for (i = 0; i < n / 2; i++) {
    char t = buf[s + i];
    buf[s + i] = buf[s + n - i - 1], buf[s + n - i - 1] = t;
  }
  if (val == 0) buf[n++] = '0';  // Handle special case
  return n + s;
}

static size_t scpy(void (*o)(char, void *), void *ptr, char *buf, size_t len) {
  size_t i = 0;
  while (i < len && buf[i] != '\0') o(buf[i++], ptr);
  return i;
}

static char xesc(int c, int esc) {
  const char *p, *esc1 = "\b\f\n\r\t\\\"", *esc2 = "bfnrt\\\"";
  for (p = esc ? esc1 : esc2; *p != '\0'; p++) {
    if (*p == c) return esc ? esc2[p - esc1] : esc1[p - esc2];
  }
  return 0;
}

size_t fmt_esc(void (*fn)(char, void *), void *param, va_list *ap) {
  unsigned len = va_arg(*ap, unsigned);
  const char *s = va_arg(*ap, const char *);
  size_t i, n = 0;
  if (len == 0) len = s == NULL ? 0 : (unsigned) xstrlen(s);
  for (i = 0; i < len && s[i] != '\0'; i++) {
    char c = xesc(s[i], 1);
    if (c) {
      fn('\\', param), fn(c, param), n += 2;
    } else {
      fn(s[i], param);
      n++;
    }
  }
  return n;
}

size_t fmt_b64(void (*fn)(char, void *), void *param, va_list *ap) {
  unsigned len = va_arg(*ap, unsigned);
  uint8_t *buf = va_arg(*ap, uint8_t *);
  size_t i, n = 0;
  const char *t =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (i = 0; i < len; i += 3) {
    uint8_t c1 = buf[i], c2 = i + 1 < len ? buf[i + 1] : 0,
            c3 = i + 2 < len ? buf[i + 2] : 0;
    char tmp[4] = {t[c1 >> 2], t[(c1 & 3) << 4 | (c2 >> 4)], '=', '='};
    if (i + 1 < len) tmp[2] = t[(c2 & 15) << 2 | (c3 >> 6)];
    if (i + 2 < len) tmp[3] = t[c3 & 63];
    n += scpy(fn, param, tmp, sizeof(tmp));
  }
  return n;
}

size_t xprintf(void (*fn)(char, void *), void *ptr, const char *fmt, ...) {
  size_t len = 0;
  va_list ap;
  va_start(ap, fmt);
  len = xvprintf(fn, ptr, fmt, &ap);
  va_end(ap);
  return len;
}

size_t xvprintf(xout_t fn, void *param, const char *fmt, va_list *ap) {
  size_t i = 0, n = 0;
  while (fmt[i] != '\0') {
    if (fmt[i] == '%') {
      size_t j, k, x = 0, is_long = 0, w = 0 /* width */, pr = ~0U /* prec */;
      char pad = ' ', minus = 0, c = fmt[++i];
      if (c == '#') x++, c = fmt[++i];
      if (c == '-') minus++, c = fmt[++i];
      if (c == '0') pad = '0', c = fmt[++i];
      while (xisdigit(c)) w *= 10, w += (size_t) (c - '0'), c = fmt[++i];
      if (c == '.') {
        c = fmt[++i];
        if (c == '*') {
          pr = (size_t) va_arg(*ap, int);
          c = fmt[++i];
        } else {
          pr = 0;
          while (xisdigit(c)) pr *= 10, pr += (size_t) (c - '0'), c = fmt[++i];
        }
      }
      while (c == 'h') c = fmt[++i];  // Treat h and hh as int
      if (c == 'l') {
        is_long++, c = fmt[++i];
        if (c == 'l') is_long++, c = fmt[++i];
      }
      if (c == 'p') x = 1, is_long = 1;
      if (c == 'd' || c == 'u' || c == 'x' || c == 'X' || c == 'p' ||
          c == 'g' || c == 'f') {
        int s = (c == 'd'), h = (c == 'x' || c == 'X' || c == 'p');
        char tmp[40];
        size_t xl = x ? 2 : 0;
#if !defined(NO_FLOAT)
        if (c == 'g' || c == 'f') {
          double v = va_arg(*ap, double);
          if (pr == ~0U) pr = 6;
          k = xdtoa(tmp, sizeof(tmp), v, (int) pr, c == 'g');
        } else
#endif
            if (is_long == 2) {
          int64_t v = va_arg(*ap, int64_t);
          k = xlld(tmp, v, s, h);
        } else if (is_long == 1) {
          long v = va_arg(*ap, long);
          k = xlld(tmp, s ? (int64_t) v : (int64_t) (unsigned long) v, s, h);
        } else {
          int v = va_arg(*ap, int);
          k = xlld(tmp, s ? (int64_t) v : (int64_t) (unsigned) v, s, h);
        }
        for (j = 0; j < xl && w > 0; j++) w--;
        for (j = 0; pad == ' ' && !minus && k < w && j + k < w; j++)
          n += scpy(fn, param, &pad, 1);
        n += scpy(fn, param, (char *) "0x", xl);
        for (j = 0; pad == '0' && k < w && j + k < w; j++)
          n += scpy(fn, param, &pad, 1);
        n += scpy(fn, param, tmp, k);
        for (j = 0; pad == ' ' && minus && k < w && j + k < w; j++)
          n += scpy(fn, param, &pad, 1);
      } else if (c == 'm' || c == 'M') {
        xfmt_t f = va_arg(*ap, xfmt_t);
        if (c == 'm') fn('"', param);
        n += f(fn, param, ap);
        if (c == 'm') n += 2, fn('"', param);
      } else if (c == 'c') {
        int ch = va_arg(*ap, int);
        fn((char) ch, param);
        n++;
      } else if (c == 's') {
        char *p = va_arg(*ap, char *);
        if (pr == ~0U) pr = p == NULL ? 0 : strlen(p);
        for (j = 0; !minus && pr < w && j + pr < w; j++)
          n += scpy(fn, param, &pad, 1);
        n += scpy(fn, param, p, pr);
        for (j = 0; minus && pr < w && j + pr < w; j++)
          n += scpy(fn, param, &pad, 1);
      } else if (c == '%') {
        fn('%', param);
        n++;
      } else {
        fn('%', param);
        fn(c, param);
        n += 2;
      }
      i++;
    } else {
      fn(fmt[i], param), n++, i++;
    }
  }
  return n;
}

static char json_esc(int c, int esc) {
  const char *p, *e[] = {"\b\f\n\r\t\\\"", "bfnrt\\\""};
  const char *esc1 = esc ? e[0] : e[1], *esc2 = esc ? e[1] : e[0];
  for (p = esc1; *p != '\0'; p++) {
    if (*p == c) return esc2[p - esc1];
  }
  return 0;
}

static int json_pass_string(const char *s, int len) {
  int i;
  for (i = 0; i < len; i++) {
    if (s[i] == '\\' && i + 1 < len && json_esc(s[i + 1], 1)) {
      i++;
    } else if (s[i] == '\0') {
      return -1;
    } else if (s[i] == '"') {
      return i;
    }
  }
  return -1;
}

int json_get(const char *s, int len, const char *path, int *toklen) {
  enum { S_VALUE, S_KEY, S_COLON, S_COMMA_OR_EOO } expecting = S_VALUE;
  unsigned char nesting[20];
  int i = 0;             // Current offset in `s`
  int j = 0;             // Offset in `s` we're looking for (return value)
  int depth = 0;         // Current depth (nesting level)
  int ed = 0;            // Expected depth
  int pos = 1;           // Current position in `path`
  int ci = -1, ei = -1;  // Current and expected index in array

  if (toklen) *toklen = 0;
  if (path[0] != '$') return -1;

#define MG_CHECKRET()                                   \
  do {                                                  \
    if (depth == ed && path[pos] == '\0' && ci == ei) { \
      if (toklen) *toklen = i - j + 1;                  \
      return j;                                         \
    }                                                   \
  } while (0)

// In the ascii table, the distance between `[` and `]` is 2.
// Ditto for `{` and `}`. Hence +2 in the code below.
#define MG_EOO()                                \
  do {                                          \
    if (depth == ed && ci != ei) return -2;     \
    if (c != nesting[depth - 1] + 2) return -1; \
    depth--;                                    \
    MG_CHECKRET();                              \
  } while (0)

  for (i = 0; i < len; i++) {
    unsigned char c = ((unsigned char *) s)[i];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
    switch (expecting) {
      case S_VALUE:
        // p("V %s [%.*s] %d %d %d %d\n", path, pos, path, depth, ed, ci, ei);
        if (depth == ed) j = i;
        if (c == '{') {
          if (depth >= (int) sizeof(nesting)) return -3;
          if (depth == ed && path[pos] == '.' && ci == ei) {
            // If we start the object, reset array indices
            ed++, pos++, ci = ei = -1;
          }
          nesting[depth++] = c;
          expecting = S_KEY;
          break;
        } else if (c == '[') {
          if (depth >= (int) sizeof(nesting)) return -3;
          if (depth == ed && path[pos] == '[' && ei == ci) {
            ed++, pos++, ci = 0;
            for (ei = 0; path[pos] != ']' && path[pos] != '\0'; pos++) {
              ei *= 10;
              ei += path[pos] - '0';
            }
            if (path[pos] != 0) pos++;
          }
          nesting[depth++] = c;
          break;
        } else if (c == ']' && depth > 0) {  // Empty array
          MG_EOO();
        } else if (c == 't' && i + 3 < len && memcmp(&s[i], "true", 4) == 0) {
          i += 3;
        } else if (c == 'n' && i + 3 < len && memcmp(&s[i], "null", 4) == 0) {
          i += 3;
        } else if (c == 'f' && i + 4 < len && memcmp(&s[i], "false", 5) == 0) {
          i += 4;
        } else if (c == '-' || ((c >= '0' && c <= '9'))) {
          int numlen = 0;
          xatod(&s[i], len - i, &numlen);
          i += numlen - 1;
        } else if (c == '"') {
          int n = json_pass_string(&s[i + 1], len - i - 1);
          if (n < 0) return n;
          i += n + 1;
        } else {
          return -1;
        }
        MG_CHECKRET();
        if (depth == ed && ei >= 0) ci++;
        expecting = S_COMMA_OR_EOO;
        break;

      case S_KEY:
        if (c == '"') {
          int n = json_pass_string(&s[i + 1], len - i - 1);
          if (n < 0) return n;
          if (i + 1 + n >= len) return -2;
          if (depth < ed) return -2;
          if (depth == ed && path[pos - 1] != '.') return -2;
          // printf("K %s [%.*s] [%.*s] %d %d %d\n", path, pos, path, n,
          //  &s[i + 1], n, depth, ed);
          // NOTE(cpq): in the check sequence below is important.
          // strncmp() must go first: it fails fast if the remaining length of
          // the path is smaller than `n`.
          if (depth == ed && path[pos - 1] == '.' &&
              strncmp(&s[i + 1], &path[pos], (size_t) n) == 0 &&
              (path[pos + n] == '\0' || path[pos + n] == '.' ||
               path[pos + n] == '[')) {
            pos += n;
          }
          i += n + 1;
          expecting = S_COLON;
        } else if (c == '}') {  // Empty object
          MG_EOO();
          expecting = S_COMMA_OR_EOO;
          if (depth == ed && ei >= 0) ci++;
        } else {
          return -1;
        }
        break;

      case S_COLON:
        if (c == ':') {
          expecting = S_VALUE;
        } else {
          return -1;
        }
        break;

      case S_COMMA_OR_EOO:
        if (depth <= 0) {
          return -1;
        } else if (c == ',') {
          expecting = (nesting[depth - 1] == '{') ? S_KEY : S_VALUE;
        } else if (c == ']' || c == '}') {
          MG_EOO();
          if (depth == ed && ei >= 0) ci++;
        } else {
          return -1;
        }
        break;
    }
  }
  return -2;
}

static unsigned char xnimble(unsigned char c) {
  return (c >= '0' && c <= '9')   ? (unsigned char) (c - '0')
         : (c >= 'A' && c <= 'F') ? (unsigned char) (c - '7')
                                  : (unsigned char) (c - 'W');
}

static unsigned long xunhexn(const char *s, size_t len) {
  unsigned long i = 0, v = 0;
  for (i = 0; i < len; i++) v <<= 4, v |= xnimble(((uint8_t *) s)[i]);
  return v;
}

static int json_unescape(const char *buf, size_t len, char *to, size_t n) {
  size_t i, j;
  for (i = 0, j = 0; i < len && j < n; i++, j++) {
    if (buf[i] == '\\' && i + 5 < len && buf[i + 1] == 'u') {
      //  \uXXXX escape. We could process a simple one-byte chars
      // \u00xx from the ASCII range. More complex chars would require
      // dragging in a UTF8 library, which is too much for us
      if (buf[i + 2] != '0' || buf[i + 3] != '0') return -1;  // Give up
      ((unsigned char *) to)[j] = (unsigned char) xunhexn(buf + i + 4, 2);
      i += 5;
    } else if (buf[i] == '\\' && i + 1 < len) {
      char c = json_esc(buf[i + 1], 0);
      if (c == 0) return -1;
      to[j] = c;
      i++;
    } else {
      to[j] = buf[i];
    }
  }
  if (j >= n) return -1;
  if (n > 0) to[j] = '\0';
  return (int) j;
}

int xb64_decode_single(int c);
int xb64_decode_single(int c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  } else if (c >= 'a' && c <= 'z') {
    return c + 26 - 'a';
  } else if (c >= '0' && c <= '9') {
    return c + 52 - '0';
  } else if (c == '+') {
    return 62;
  } else if (c == '/') {
    return 63;
  } else if (c == '=') {
    return 64;
  } else {
    return -1;
  }
}

size_t xb64_decode(const char *src, size_t slen, char *dst, size_t dlen) {
  const char *end = src == NULL ? NULL : src + slen;  // Cannot add to NULL
  size_t len = 0;
  if (dlen < slen / 4 * 3 + 1) goto fail;
  while (src != NULL && src + 3 < end) {
    int a = xb64_decode_single(src[0]), b = xb64_decode_single(src[1]),
        c = xb64_decode_single(src[2]), d = xb64_decode_single(src[3]);
    if (a == 64 || a < 0 || b == 64 || b < 0 || c < 0 || d < 0) goto fail;
    dst[len++] = (char) ((a << 2) | (b >> 4));
    if (src[2] != '=') {
      dst[len++] = (char) ((b << 4) | (c >> 2));
      if (src[3] != '=') dst[len++] = (char) ((c << 6) | d);
    }
    src += 4;
  }
  dst[len] = '\0';
  return len;
fail:
  if (dlen > 0) dst[0] = '\0';
  return 0;
}

int json_get_num(const char *buf, int len, const char *path, double *v) {
  int found = 0, n = 0, off = json_get(buf, len, path, &n);
  if (off >= 0 && (buf[off] == '-' || (buf[off] >= '0' && buf[off] <= '9'))) {
    if (v != NULL) *v = xatod(buf + off, n, NULL);
    found = 1;
  }
  return found;
}

int json_get_bool(const char *buf, int len, const char *path, int *v) {
  int found = 0, off = json_get(buf, len, path, NULL);
  if (off >= 0 && (buf[off] == 't' || buf[off] == 'f')) {
    if (v != NULL) *v = buf[off] == 't';
    found = 1;
  }
  return found;
}

int json_get_str(const char *buf, int len, const char *path, char *dst,
                 size_t dlen) {
  int result = -1, n = 0, off = json_get(buf, len, path, &n);
  if (off >= 0 && n > 1 && buf[off] == '"') {
    result = json_unescape(buf + off + 1, (size_t) (n - 2), dst, dlen);
  }
  return result;
}

int json_get_b64(const char *buf, int len, const char *path, char *dst,
                 size_t dlen) {
  int result = -1, n = 0, off = json_get(buf, len, path, &n);
  if (off >= 0 && n > 1 && buf[off] == '"') {
    result = (int) xb64_decode(buf + off + 1, (size_t) (n - 2), dst, dlen);
  }
  return result;
}

long json_get_long(const char *buf, int len, const char *path, long dflt) {
  double v;
  if (json_get_num(buf, len, path, &v)) dflt = (long) v;
  return dflt;
}

static char xnibble(char c) {
  return c < 10 ? c + '0' : c + 'W';
}

void xhexdump(void (*fn)(char, void *), void *a, const void *buf, size_t len) {
  const unsigned char *p = (const unsigned char *) buf;
  char ascii[16];
  size_t i, j, n = 0;
  for (i = 0; i < len; i++) {
    if ((i & 15) == 0) {
      // Print buffered ascii chars
      if (i > 0) {
        fn(' ', a), fn(' ', a);
        for (j = 0; j < sizeof(ascii); j++) fn(ascii[j], a);
        fn('\n', a), n = 0;
      }
      // Print hex address, then \t
      fn(xnibble((i >> 12) & 15), a), fn(xnibble((i >> 8) & 15), a);
      fn(xnibble((i >> 4) & 15), a), fn('0', a);
      fn(' ', a), fn(' ', a), fn(' ', a);
    }
    fn(xnibble((p[i] >> 4) & 15), a), fn(xnibble(p[i] & 15), a);
    fn(' ', a);  // Space after hex number
    if (p[i] >= ' ' && p[i] <= '~') {
      ascii[n++] = (char) p[i];  // Printable
    } else {
      ascii[n++] = '.';  // Non-printable
    }
  }
  if (n > 0) {
    while (n < 16) fn(' ', a), fn(' ', a), fn(' ', a), ascii[n++] = ' ';
    fn(' ', a), fn(' ', a);
    for (j = 0; j < sizeof(ascii); j++) fn(ascii[j], a);
  }
  fn('\n', a);
}

#endif  // STR_API_ONLY

#ifdef __cplusplus
}
#endif
#endif  // STR_H_

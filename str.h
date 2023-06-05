// Copyright (c) 2023 Cesanta Software Limited
// SPDX-License-Identifier: AGPL-3.0 or commercial

#pragma once

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

#define XAPI static inline
#define ESC(str) fmt_esc, 0, (str)

// Low level basic functions
XAPI size_t xvprintf(void (*)(char, void *), void *, const char *, va_list *);
XAPI size_t xprintf(void (*)(char, void *), void *, const char *, ...);

// Convenience wrappers around xprintf
XAPI size_t xvsnprintf(char *buf, size_t len, const char *fmt, va_list *ap);
XAPI size_t xsnprintf(char *, size_t, const char *fmt, ...);

// Pre-defined %M/%m formatting functions
XAPI size_t fmt_ip4(void (*out)(char, void *), void *arg, va_list *ap);
XAPI size_t fmt_ip6(void (*out)(char, void *), void *arg, va_list *ap);
XAPI size_t fmt_mac(void (*out)(char, void *), void *arg, va_list *ap);
XAPI size_t fmt_b64(void (*out)(char, void *), void *arg, va_list *ap);
XAPI size_t fmt_esc(void (*out)(char, void *), void *arg, va_list *ap);

XAPI int json_parse(const char *buf, size_t len, int *tokens);

#if !defined(STR_API_ONLY)
typedef void (*xout_t)(char, void *);                 // Output function
typedef size_t (*xfmt_t)(xout_t, void *, va_list *);  // %M format function

#if defined(ENABLE_PRINTF)
extern void xputchar(char, void *);
int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  size_t len = xvprintf(xputchar, NULL, fmt, &ap);
  va_end(ap);
  return (int) len;
}
#endif

struct xbuf {
  char *buf;
  size_t size, len;
};

XAPI void xout_buf(char ch, void *param) {
  struct xbuf *mb = (struct xbuf *) param;
  if (mb->len < mb->size) mb->buf[mb->len] = ch;
  mb->len++;
}

XAPI size_t xvsnprintf(char *buf, size_t len, const char *fmt, va_list *ap) {
  struct xbuf mb = {buf, len, 0};
  size_t n = xvprintf(xout_buf, &mb, fmt, ap);
  if (len > 0) buf[n < len ? n : len - 1] = '\0';  // NUL terminate
  return n;
}

XAPI size_t xsnprintf(char *buf, size_t len, const char *fmt, ...) {
  va_list ap;
  size_t n;
  va_start(ap, fmt);
  n = xvsnprintf(buf, len, fmt, &ap);
  va_end(ap);
  return n;
}

XAPI size_t fmt_ip4(void (*out)(char, void *), void *arg, va_list *ap) {
  uint8_t *p = va_arg(*ap, uint8_t *);
  return xprintf(out, arg, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
}

#define U16(p) ((((uint16_t) (p)[0]) << 8) | (p)[1])
XAPI size_t fmt_ip6(void (*out)(char, void *), void *arg, va_list *ap) {
  uint8_t *p = va_arg(*ap, uint8_t *);
  return xprintf(out, arg, "[%x:%x:%x:%x:%x:%x:%x:%x]", U16(&p[0]), U16(&p[2]),
                 U16(&p[4]), U16(&p[6]), U16(&p[8]), U16(&p[10]), U16(&p[12]),
                 U16(&p[14]));
}

XAPI size_t fmt_mac(void (*out)(char, void *), void *arg, va_list *ap) {
  uint8_t *p = va_arg(*ap, uint8_t *);
  return xprintf(out, arg, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2],
                 p[3], p[4], p[5]);
}

XAPI int xisdigit(int c) { return c >= '0' && c <= '9'; }

XAPI size_t xstrlen(const char *s) {
  size_t n = 0;
  while (s[n] != '\0') n++;
  return n;
}

XAPI int addexp(char *buf, int e, int sign) {
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

XAPI int xisinf(double x) {
  union {
    double f;
    uint64_t u;
  } ieee754 = {x};
  return ((unsigned) (ieee754.u >> 32) & 0x7fffffff) == 0x7ff00000 &&
         ((unsigned) ieee754.u == 0);
}

XAPI int xisnan(double x) {
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

XAPI double xatod(const char *p, int len, int *numlen) {
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

XAPI size_t xlld(char *buf, int64_t val, int is_signed, int is_hex) {
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

XAPI size_t scpy(void (*o)(char, void *), void *ptr, char *buf, size_t len) {
  size_t i = 0;
  while (i < len && buf[i] != '\0') o(buf[i++], ptr);
  return i;
}

XAPI char xesc(int c, int esc) {
  const char *p, *esc1 = "\b\f\n\r\t\\\"", *esc2 = "bfnrt\\\"";
  for (p = esc ? esc1 : esc2; *p != '\0'; p++) {
    if (*p == c) return esc ? esc2[p - esc1] : esc1[p - esc2];
  }
  return 0;
}

XAPI size_t fmt_esc(void (*out)(char, void *), void *param, va_list *ap) {
  unsigned len = va_arg(*ap, unsigned);
  const char *s = va_arg(*ap, const char *);
  size_t i, n = 0;
  if (len == 0) len = s == NULL ? 0 : (unsigned) xstrlen(s);
  for (i = 0; i < len && s[i] != '\0'; i++) {
    char c = xesc(s[i], 1);
    if (c) {
      out('\\', param), out(c, param), n += 2;
    } else {
      out(s[i], param);
      n++;
    }
  }
  return n;
}

XAPI size_t fmt_b64(void (*out)(char, void *), void *param, va_list *ap) {
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
    n += scpy(out, param, tmp, sizeof(tmp));
  }
  return n;
}

XAPI size_t xprintf(void (*out)(char, void *), void *ptr, const char *fmt,
                    ...) {
  size_t len = 0;
  va_list ap;
  va_start(ap, fmt);
  len = xvprintf(out, ptr, fmt, &ap);
  va_end(ap);
  return len;
}

XAPI size_t xvprintf(xout_t out, void *param, const char *fmt, va_list *ap) {
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
          n += scpy(out, param, &pad, 1);
        n += scpy(out, param, (char *) "0x", xl);
        for (j = 0; pad == '0' && k < w && j + k < w; j++)
          n += scpy(out, param, &pad, 1);
        n += scpy(out, param, tmp, k);
        for (j = 0; pad == ' ' && minus && k < w && j + k < w; j++)
          n += scpy(out, param, &pad, 1);
      } else if (c == 'm' || c == 'M') {
        xfmt_t f = va_arg(*ap, xfmt_t);
        if (c == 'm') out('"', param);
        n += f(out, param, ap);
        if (c == 'm') n += 2, out('"', param);
      } else if (c == 'c') {
        int ch = va_arg(*ap, int);
        out((char) ch, param);
        n++;
      } else if (c == 's') {
        char *p = va_arg(*ap, char *);
        if (pr == ~0U) pr = p == NULL ? 0 : strlen(p);
        for (j = 0; !minus && pr < w && j + pr < w; j++)
          n += scpy(out, param, &pad, 1);
        n += scpy(out, param, p, pr);
        for (j = 0; minus && pr < w && j + pr < w; j++)
          n += scpy(out, param, &pad, 1);
      } else if (c == '%') {
        out('%', param);
        n++;
      } else {
        out('%', param);
        out(c, param);
        n += 2;
      }
      i++;
    } else {
      out(fmt[i], param), n++, i++;
    }
  }
  return n;
}

#endif  // STR_API_ONLY

#ifdef __cplusplus
}
#endif

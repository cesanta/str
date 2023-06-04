# Single header string library for microcontrollers

[![License: AGPLv3/Commercial](https://img.shields.io/badge/License-AGPLv3%20or%20Commercial-green.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Build Status](https://github.com/cesanta/str/workflows/test/badge.svg)](https://github.com/cesanta/str/actions)
[![Code Coverage](https://codecov.io/gh/cesanta/str/branch/main/graph/badge.svg?token=B5LH9CRPQT)](https://codecov.io/gh/cesanta/str)

Single header string library for microcontrollers implements the
following routines:

- `*printf()` - implements `printf()` family of functions that can print
  standard specifiers (including floating point `%f` and `%g`) as well as
  non-standard `%m` and `%M` specifiers that allow custom formatting like JSON,
  hex, base64
- `xdtoa()` - convert `double` to string

## Usage example

```c
// Print into a buffer
char buf[100];
xsnprintf(buf, sizeof(buf), "%s: %g\n", "dbl", 1.234);  // dbl: 1.234
xsnprintf(buf, sizeof(buf), "%.*s\n", 3, "foobar");     // foo
xsnprintf(buf, sizeof(buf), "%#04x\n", 11);             // 0x0b
xsnprintf(buf, sizeof(buf), "%d %5s\n", 7, "pad");      // 7   pad

// Define printing function for printf()
void xputchar(char ch, void *param) {
  HAL_UART_Transmit(param, &ch, 1, 100);
}

// To enable printf, add "#define ENABLE_PRINTF" line before #include "str.h"
printf("JSON: {%m: %g}\n", ESC("value"), 1.234);   // JSON: {"value": 1.234}
```

## API reference

### xprintf(), vxprintf()
```c
size_t vxprintf(void (*out)(char, void *), void *arg, const char *fmt, va_list *);
size_t xprintf(void (*out)(char, void *), void *arg, const char *fmt, ...);
```

Print formatted string using an output function `fn()`. The output function
outputs a single byte: `void fn(char ch, void *param) { ... }`. By using
different output functions, it is possible to print data to anywhere.
Parameters:
- `out` - an output function
- `arg` - an parameter for the `out()` output function
- `fmt` - printf-like format string which supports the following specifiers:
  - `%hhd`, `%hd`, `%d`, `%ld`, `%lld` - for `char`, `short`, `int`, `long`, `int64_t`
  - `%hhu`, `%hu`, `%u`, `%lu`, `%llu` - same but for unsigned variants
  - `%hhx`, `%hx`, `%x`, `%lx`, `%llx` - same, for unsigned hex output
  - `%g`, `%f` - for `double`
  - `%c` - for `char`
  - `%s` - for `char *`
  - `%%` - prints `%` character itself
  - `%p` - for any pointer, prints `0x.....` hex value
  - `%M` - (NON-STANDARD EXTENSION): prints using a custom format function
  - `%m` - (NON-STANDARD EXTENSION): same as `%M` but double-quotes the result
  - `%X.Y` - optional width and precision modifiers
  - `%.*` - optional precision modifier specified as `int` argument

Return value: Number of bytes printed

The `%M` specifier expects a cusom format function that can grab any number of
positional arguments. That format function should return the number of bytes
it has printed. Here its signature:

```c
size_t (*ff)(void (*out)(char, void *), void *arg, va_list *ap);
```
Parameters:
- `out` - an output function
- `arg` - an parameter for the `out()` output function
- `ap` - a pointer for fetching positional arguments

This library ships with several pre-defined format functions described below.
For example, a `fmt_b64()` format function grabs two positional arguments:
`int` and `void *`, and base64-encodes that memory location:

```c
char buf[100];                                        // Base64-encode "abc"
xsnprintf(buf, sizeof(buf), "%M", fmt_b64, 3, "abc"); // buf contains: YWJj
```

### xsnprintf(), xvsnprintf()
```c
size_t xvsnprintf(char *buf, size_t len, const char *fmt, va_list *ap);
size_t xsnprintf(char *buf, size_t len, const char *fmt, ...);
```

Print formatted string into a fixed-size buffer. Parameters:
- `buf` - a buffer to print to. Can be NULL, in this case `len` must be 0
- `len` - a size of the `buf`
- `fmt` - a format string. Supports all specifiers mentioned above

Return value: number of bytes printed. The result is guaranteed to be NUL
terminated.

### Pre-defined `%M` format functions

```c
size_t fmt_*(void (*out)(char, void *), void *arg, va_list *ap);
```

Pre-defined helper functions for `%M` specifier:
- `fmt_ip4` - print IPv4 address. Expect a pointer to 4-byte IPv4 address
- `fmt_ip6` - print IPv6 address. Expect a pointer to 16-byte IPv6 address
- `fmt_mac` - print MAC address. Expect a pointer to 6-byte MAC address
- `fmt_b64` - print base64 encoded data. Expect `int`, `void *`
- `fmt_esc` - print a string, escaping `\n`, `\t`, `\r`, `"`. Espects `char *`

Examples:

```
uint32_t ip4 = 0x0100007f;                              // Print IPv4 address:
xsnprintf(buf, sizeof(buf), "%m", fmt_ip4, &ip4);    // 127.0.0.1

uint8_t ip6[16] = {1, 100, 33};                         // Print IPv6 address:
xsnprintf(buf, sizeof(buf), "%m", fmt_ip4, &ip6);    // [164:2100:0:0:0:0:0:0]

uint8_t mac[6] = {1, 2, 3, 4, 5, 6};                    // Print MAC address:
xsnprintf(buf, sizeof(buf), "%m", fmt_mac, &mac);    // 01:02:03:04:05:06

const char str[] = {'a', \n, '"', 0};                   // Print escaped string:
xsnprintf(buf, sizeof(buf), "%m", fmt_esc, str);     // a\n\"

const char *data = "xyz";                               // Print base64 data:
xsnprintf(buf, sizeof(buf), "%m", fmt_b64, 3, data); // eHl6
```

### Custom `%M` format functions

It is easy to create your own format functions to format data that is
specific to your application. For example, if you want to print your
data structure as JSON string, just create your custom formatting function:

```c
struct foo { int a; double b; const char *c; };

size_t fmt_foo(void (*out)(char, void *), void *arg, va_list *ap) {
  struct foo *foo = va_arg(*ap, struct foo *);
  return xxprintf(out, arg, "{\"a\":%d, \"b\":%g, \"c\":%m}",
                   foo->a, foo->b, fmt_esc, c);
}
```

And now, you can use that function:

```
struct foo foo = {1, 2.34, "hi"};
xsnprintf(buf, sizeof(buf), "%M", fmt_foo, &foo);
```

## Printing to a dynamic memory

The `x*printf()` functions always return the total number of bytes that the
result string takes. Therefore it is possible to print to a `malloc()-ed`
buffer in two passes: in the first pass we calculate the length, and in the
second pass we print:

```c
size_t len = xsnprintf(NULL, 0, "Hello, %s", "world");
char *buf = malloc(len + 1);  // +1 is for the NUL terminator
xsnprintf(buf, len + 1, "Hello, %s", "world");
...
free(buf);
```

## Footprint

The following table contains footprint measurements for the ARM Cortex-M0 and
ARM Cortex-M7 builds of `xsnprintf()` compared to the standard `snprintf()`.
The compilation is done with `-Os` flag using ARM GCC 10.3.1.  See
[test/footprint.c](test/footprint.c) and the corresponding
[Makefile snippet](https://github.com/cesanta/str/blob/1f26b01c6277be329b264caa4597c6ad655ff135/test/Makefile#L47-L63)
used for measurements.

|                            | Cortex-M0 | Cortex-M7 |
| -------------------------- | --------- | --------- |
| `xsnprintf` (no float)    | 1844      | 2128      |
| `xsnprintf`               | 10996     | 5592      |
| Standard `snprintf`  (no float)  | 68368     | 68248     |
| Standard `snprintf`        | 87476     | 81420     |

Notes:
- by default, standard snrpintf does not support float, and `x*printf` does 
- to enable float for ARM GCC (newlib), use `-u _printf_float`
- to disable float for `x*printf`, use `-DNO_FLOAT`

## Licensing

This library is licensed under the dual license:
- Open source projects are covered by the GNU Affero 3.0 license
- Commercial projects/products are covered by the commercial,
  permanent, royalty free license:
  - Buy a [single product license](https://buy.stripe.com/9AQaHI1Uud1U4msaEG) - covers a single company's product
  - Buy a [company wide license](https://buy.stripe.com/3csaHIar09PI4ms8wx) - covers all company's products

- For licensing questions, [contact us](https://mongoose.ws/contact/)

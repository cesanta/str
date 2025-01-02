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
- `xmatch()` - glob pattern match with captures
- `json_get()` - find element in a JSON string
- `json_get_num()` - fetch numeric value from a JSON string
- `json_get_bool()` - fetch boolean value from a JSON string
- `json_get_str()` - fetch string value from a JSON string
- `xhexdump()` - print hex dump of the given memory buffer

## Features

- Ideal for implementing serial protocols, debug logging, JSON data exchange
- Source code is both ISO C and ISO C++ compliant
- Tiny size: see the [Footprint](#footprint) section below for exact numbers
- `*printf()` supports all major format specifiers, width, precition and alignment
- `*printf()` supports floating point `%f` and `%g` specifiers by default
- `*printf()` Supports non-standard `%M`,`%m` specifiers which allows for custom formats
- Can print to anything: to a memory buffer, file, socket, UART, etc
- Extensively [tested](/cesanta/str/actions) using VC98, VC22, ARM GCC, AVR GCC, x86 GCC, Clang

## Usage example

Printing to a buffer: https://github.com/cesanta/str/blob/873b39dd14b074bf0779f5d06f5c5bfe3bcb416b/test/main.c#L174-L177

Print to the UART. Output JSON, and base64-encoded data: https://github.com/cesanta/str/blob/42f0571f8de45e1acd63ffb93526333bd15630c5/test/main.ino#L1-L14

Parse JSON: https://github.com/cesanta/str/blob/813e08acf4e389690830bfb5ff525c9e79bdb362/test/main.c#L183-L190

Parse and print floating point: https://github.com/cesanta/str/blob/23ebc5c6aafe1b9b5141f97563ee57c50c72093b/test/main.c#L155-L158

## Code duplication

`str.h` is divided in two parts: API declaration, and implementation.
The implementation part is wrapped into the preprocessor conditional:

```c
// API declarations
size_t xprintf(void (*)(char, void *), void *, const char *, ...);
...

#ifndef STR_API_ONLY
// Implementation
......
#endif
```

If more than one file includes `str.h`, then the build will result in
duplicate symbols, because the implementation will end up duplicated in
several object files.
To avoid this, only one file should include the full
`str.h`, and the rest of the files should only include API declarations:

file1.c:
```c
#include "str.h"
...
```

file2.c, file3.c, ...:
```c
#define STR_API_ONLY
#include "str.h"
...
```

## API reference

### xprintf(), vxprintf()
```c
size_t vxprintf(void (*fn)(char, void *), void *arg, const char *fmt, va_list *);
size_t xprintf(void (*fn)(char, void *), void *arg, const char *fmt, ...);
```

Print formatted string using an output function `fn()`. The output function
outputs a single byte: `void fn(char ch, void *param) { ... }`. By using
different output functions, it is possible to print data to anywhere.
Parameters:
- `fn` - an output function
- `arg` - an parameter for the `fn()` output function
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
size_t (*ff)(void (*fn)(char, void *), void *arg, va_list *ap);
```
Parameters:
- `fn` - an output function
- `arg` - an parameter for the `fn()` output function
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

### json\_get()
```c
int json_get(const char *buf, int len, const char *path, int *size);
```

Parse JSON string `buf`, `len` and return the offset of the element
specified by the JSON `path`. The length of the element is stored in `size`.

Parameters:
- `buf` - a pointer to a JSON string
- `len` - a length of a JSON string
- `path` - a JSON path. Must start with `$`, e.g. `$.user`, `$[12]`, `$`, etc
- `size` - a pointer that receives element's length. Can be NULL

Return value: offset of the element, or negative value on error.

Usage example:

```c
// JSON string buf, len contains { "a": 1, "b": [2, 3] }
int size, ofs;

// Lookup "$", which is the whole JSON. Can be used for validation
ofs = json_get(buf, len, "$", &size);  // ofs == 0, size == 23

// Lookup attribute "a". Point to value "1"
ofs = json_get(buf, len, "$.a", &zize);  // ofs = 7, size = 1

// Lookup attribute "b". Point to array [2, 3]
ofs = json_get(buf, len, "$.b", &size);  // ofs = 15, size = 6

// Lookup attribute "b[1]". Point to value "3"
ofs = json_get(buf, len, "$.b[1]", &size); // ofs = 19, size = 1
```

### json\_get\_num()

```c
int json_get_num(const char *buf, int len, const char *path, double *val);
```

Fetch numeric (double) value from the json string `buf`, `len` at JSON path
`path` into a placeholder `val`. Return true if successful.

Parameters:
- `buf` - a pointer to a JSON string
- `len` - a length of a JSON string
- `path` - a JSON path. Must start with `$`
- `val` - a placeholder for value

Return value: 1 on success, 0 on error

Usage example:

```c
double d = 0.0;
json_get_num("[1,2,3]", 7, "$[1]", &d));        // d == 2
json_get_num("{\"a\":1.23}", 10, "$.a", &d));   // d == 1.23
```

### json\_get\_bool()

```c
int json_get_bool(const char *buf, int len, const char *path, int *val);
```

Fetch boolean (bool) value from the json string `buf` at JSON path
`path` into a placeholder `v`. Return true if successful.

Parameters:
- `buf` - a pointer to a JSON string
- `len` - a length of a JSON string
- `path` - a JSON path. Must start with `$`
- `val` - a placeholder for value

Return value: 1 on success, 0 on error

Usage example:

```c
int b = 0;
json_get_bool("[123]", 5, "$[0]", &b));   // Error. b == 0
json_get_bool("[true]", 6, "$[0]", &b));  // b == 1
```

### json\_get\_long()

```c
long json_get_long(const char *buf, int len, const char *path, long default_val);
```

Fetch integer numeric (long) value from the json string `buf`, `len` at JSON path
`path`. Return it if found, or `default_val` if not found.

Parameters:
- `buf` - a pointer to a JSON string
- `len` - a length of a JSON string
- `path` - a JSON path. Must start with `$`
- `default_val` - a default value for the failure case

Return value: found value, or `default_val` value

Usage example:

```c
long a = json_get_long("[123]", 5, "$a", -1));   // a == -1
long b = json_get_long("[123]", 5, "$[0]", -1)); // b == 123
```

### json\_get\_str()

```c
int json_get_str(const char *buf, int len, const char *path, char *dst, size_t dstlen);
```

Fetch string value from the json string `json` at JSON path
`path`. If found, a string is allocated using `calloc()`,
un-escaped, and returned to the caller. It is the caller's responsibility to
`free()` the returned string.

Parameters:
- `buf` - a pointer to a JSON string
- `len` - a length of a JSON string
- `path` - a JSON path. Must start with `$`
- `dst` - a pointer to a buffer that holds the result
- `dstlen` - a length of a result buffer

Return value: length of a decoded string. >= 0 on success, < 0 on error

Usage example:

```c
char dst[100];
json_get_str("[1,2,\"hi\"]", "$[2]", dst, sizeof(dst));  // dst contains "hi"
```

### xhexdump()

```c
void xhexdump(void (*fn)(char, void *), void *arg, const void *buf, size_t len);
```

Print hex dump of the given memory buffer.

Parameters:
- `fn` - an output function
- `arg` - an parameter for the `fn()` output function
- `buf` - a pointer to a buffer to print
- `len` - a length of a buffer

Usage example:

```c
xhexdump(xputchar, NULL, "hi", 2);
```


## Pre-defined `%M`, `%m` format functions

```c
size_t fmt_*(void (*fn)(char, void *), void *arg, va_list *ap);
```

Pre-defined helper functions for `%M` specifier:
- `fmt_ip4` - print IPv4 address. Expect a pointer to 4-byte IPv4 address
- `fmt_ip6` - print IPv6 address. Expect a pointer to 16-byte IPv6 address
- `fmt_mac` - print MAC address. Expect a pointer to 6-byte MAC address
- `fmt_b64` - print base64 encoded data. Expect `int`, `void *`
- `fmt_esc` - print a string, escaping `\n`, `\t`, `\r`, `"`. Espects `int`, `char *`

Examples:

```
uint32_t ip4 = 0x0100007f;                           // Print IPv4 address:
xsnprintf(buf, sizeof(buf), "%M", fmt_ip4, &ip4);    // 127.0.0.1

uint8_t ip6[16] = {1, 100, 33};                      // Print IPv6 address:
xsnprintf(buf, sizeof(buf), "%M", fmt_ip4, &ip6);    // [164:2100:0:0:0:0:0:0]

uint8_t mac[6] = {1, 2, 3, 4, 5, 6};                 // Print MAC address:
xsnprintf(buf, sizeof(buf), "%M", fmt_mac, &mac);    // 01:02:03:04:05:06

const char str[] = {'a', \n, '"', 0};                // Print escaped string:
xsnprintf(buf, sizeof(buf), "%M", fmt_esc, 0, str);  // a\n\"

const char *data = "xyz";                            // Print base64 data:
xsnprintf(buf, sizeof(buf), "%M", fmt_b64, 3, data); // eHl6
```

## Custom `%M`, `%m` format functions

It is easy to create your own format functions to format data that is
specific to your application. For example, if you want to print your
data structure as JSON string, just create your custom formatting function:

```c
struct foo { int a; double b; const char *c; };

size_t fmt_foo(void (*fn)(char, void *), void *arg, va_list *ap) {
  struct foo *foo = va_arg(*ap, struct foo *);
  return xxprintf(fn, arg, "{\"a\":%d, \"b\":%g, \"c\":%m}",
                  foo->a, foo->b, ESC(c));
}
```

And now, you can use that function:

```
struct foo foo = {1, 2.34, "hi"};
xsnprintf(buf, sizeof(buf), "%M", fmt_foo, &foo);
```

### xmatch()

```c
struct xstr {
  char *buf;
  size_t len;
};

bool xmatch(struct xstr str, struct xstr pattern, struct xstr *caps);
```

Check if string `str` matches glob pattern `pattern`, and optionally capture
wildcards into the provided array `caps`.

> NOTE: If `caps` is not NULL, then the `caps` array size must be at least the
> number of wildcard symbols in `pattern` plus 1. The last cap will be
> initialized to an empty string.

The glob pattern matching rules are as follows:

- `?` matches any single character
- `*` matches zero or more characters except `/`
- `#` matches zero or more characters
- any other character matches itself

Parameters:
- `str` - a string to match
- `pattern` - a pattern to match against
- `caps` - an optional array of captures for wildcard symbols `?`, `*`, '#'

Return value: `true` if matches, `false` otherwise

Usage example:

```c
// Assume that hm->uri holds /foo/bar. Then we can match the requested URI:
struct xstr caps[3];  // Two wildcard symbols '*' plus 1
struct xstr str = xstr_s("/hello/world");
struct xstr pattern = xstr_s{ "/*/*");
if (xmatch(str, pattern, caps)) {
  // caps[0] holds `hello`, caps[1] holds `world`.
}
```

## Printing to dynamic memory

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

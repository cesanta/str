#include "str.h"

void xputchar(char ch, void *arg) { Serial.write(ch); }
#define printf(...) xprintf(xputchar, NULL, __VA_ARGS__)

void setup() {
  Serial.begin(115200);
}

void loop() {
  printf("JSON: {%m:%g,[%d]}\n", XESC("value"), 1.234, 42);
  printf("Base64: %M\n", fmt_b64, 5, "hello");
  delay(1000);
}

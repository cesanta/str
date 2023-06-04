#define ENABLE_PRINTF
#include "str.h"

void xputchar(char ch, void *arg) { Serial.write(ch); }

void setup() {
  Serial.begin(115200);
}

void loop() {
  printf("hi! JSON: {%m:%g,[%d]}\n", ESC("value"), 1.234, 42);
  delay(1000);
}

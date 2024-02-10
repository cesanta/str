PROG ?= unit_test
CFLAGS = -O2 -I.. -coverage -W -Wall -Wextra -Werror -Wno-deprecated -Wundef -Wshadow -Wdouble-promotion -Wconversion
CWD ?= $(realpath $(CURDIR))
ROOT ?= $(realpath $(CURDIR)/..)
DOCKER = docker run --platform linux/amd64 $(ENV) --rm -e WINEDEBUG=-all -v $(ROOT):$(ROOT) -w $(CWD)
SOURCES = main.c

ifeq "$(findstring ++,$(CC))" ""
  CFLAGS += -Wmissing-prototypes -Wstrict-prototypes
else
  CFLAGS += -Wno-deprecated
endif

run: $(PROG)
	$(RUN) ./$(PROG) $(ARGS)

$(PROG): $(SOURCES) ../str.h
	$(CC) $(SOURCES) $(CFLAGS) $(CFLAGS_EXTRA) -o $@

vc22:
	$(DOCKER) mdashnet/vc22 wine64 cl /nologo /W3 /O2 /MD /I.. $(SOURCES) /Fe$@.exe
	$(DOCKER) mdashnet/vc22 wine64 $@.exe

vc98:
	$(DOCKER) mdashnet/vc98 wine cl /nologo /W3 /O2 /MD /I.. $(SOURCES) /Fe$@.exe
	$(DOCKER) mdashnet/vc98 wine $@.exe

arm:
	arm-none-eabi-gcc $(SOURCES) syscalls.c -I.. -W -Wall -Wextra -Os -mcpu=cortex-m0 -mfloat-abi=soft -nostdlib -lc -lgcc -e main

arduino:
	curl -Ls http://downloads.arduino.cc/arduino-1.8.13-linux64.tar.xz -o /tmp/a.tgz
	tar -xf /tmp/a.tgz
	mv arduino-* $@

uno: arduino
	rm -rf tmp; mkdir tmp; cp main.ino tmp/tmp.ino; cp ../*.[ch] tmp/
	$(DOCKER) mdashnet/cc2 ./arduino/arduino --verify --board arduino:avr:nano tmp/tmp.ino

coverage: run
	gcov -l -n *.gcno | sed '/^$$/d' | sed 'N;s/\n/ /'
	@gcov *.gcno >/dev/null

upload-coverage: coverage
	curl -s https://codecov.io/bash | /bin/bash

define build
  @arm-none-eabi-gcc footprint.c syscalls.c ../micro_printf.c -o $1.elf -ffunction-sections -fdata-sections -I.. $2 $3 -mthumb -e main -nostartfiles -nostdlib --specs nano.specs -lc -lgcc -Wl,--gc-sections -Wl,-Map=$1.map
  @arm-none-eabi-objcopy -O binary $1.elf $1.bin
  @ls -l $1.bin
endef

M7 = -Os -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv4-sp-d16
M0 = -Os -mcpu=cortex-m0 -mfloat-abi=soft
footprint:
	$(call build,m7_nofloat,$(M7),-DNO_FLOAT)
	$(call build,m7,$(M7),)
	$(call build,m7_std_nofloat,$(M7),-DSTD)
	$(call build,m7_std,$(M7),-DSTD -u _printf_float)
	$(call build,m0_nofloat,$(M0),-DNO_FLOAT)
	$(call build,m0,$(M0),)
	$(call build,m0_std_nofloat,$(M0),-DSTD)
	$(call build,m0_std,$(M0),-DSTD -u _printf_float)

clean:
	rm -rf $(PROG) tmp *.o *.obj *.exe *.dSYM *.elf *.bin *.map *.gcno *.gcda *.gcov

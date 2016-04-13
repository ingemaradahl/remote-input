SHELL=bash -o pipefail

remote-inputd:

OUT = out
DEPDIR = $(OUT)/deps
GENDIR = $(OUT)/gen

DIRECTORIES = $(OUT) $(DEPDIR) $(GENDIR)
$(DIRECTORIES):
	mkdir -p $@

CFLAGS += -std=c11 -Wall
CFLAGS += -Werror=format-security -Wshadow -Wformat
CFLAGS += -fstack-protector -fno-strict-aliasing
CFLAGS += -Wa,--noexecstack

CPPFLAGS += -D_XOPEN_SOURCE=700

SRCS = remote-inputd.c logging.c input_device.c server.c

ifeq ($(TARGET), ANDROID)

ifndef ANDROID_NDK
$(error $$ANDROID_NDK not set! Please install the Android ndk)
endif

GCC_VERSION = 4.9

PLATFORM_TARGET_VERSION=21
PLATFORM_TARGET_ARCH=arm

TOOLCHAIN = $(ANDROID_NDK)/toolchains/arm-linux-androideabi-$(GCC_VERSION)/prebuilt/linux-x86_64
SYSROOT = $(ANDROID_NDK)/platforms/android-$(PLATFORM_TARGET_VERSION)/arch-$(PLATFORM_TARGET_ARCH)

CC = $(TOOLCHAIN)/bin/arm-linux-androideabi-gcc

CFLAGS += -fdiagnostics-color=auto
CFLAGS += -fpic -fPIE
CFLAGS += -no-canonical-prefixes
CFLAGS += -march=armv7-a -mthumb

CPPFLAGS += -I$(SYSROOT)/usr/include
CPPFLAGS += -DANDROID

LDFLAGS += -march=armv7-a -fPIE -pie -mthumb
LDFLAGS += --sysroot=$(SYSROOT) -L$(SYSROOT)/usr/lib
LDFLAGS += -Wl,-rpath-link=$(SYSROOT)/usr/lib -Wl,-rpath-link=$(OUT)
LDFLAGS += -Wl,--gc-sections -Wl,-z,nocopyreloc -no-canonical-prefixes
LDFLAGS += -Wl,--fix-cortex-a8 -Wl,--no-undefined -Wl,-z,noexecstack
LDFLAGS += -Wl,-z,relro -Wl,-z,now
endif  # TARGET == ANDROID

DEPS = $(patsubst %, $(DEPDIR)/%.d, $(basename $(SRCS)))

$(DEPDIR)/%.d: %.c | $(DEPDIR)
	$(CC) $(CFLAGS) -MG -MM -MP -MT $@ -MT $(OUT)/$(<:.c=.o) -MF $@ $<

$(OUT)/gen/keymap.h: device_key_mapping.h generate_keymap.awk | $(GENDIR)
	$(CPP) $(CPPFLAGS) -P -imacros linux/input.h $< | sort -n | ./generate_keymap.awk > $@

$(OUT)/%.o: %.c | $(OUT)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.DEFAULT_GOAL: remote-inputd
remote-inputd: $(patsubst %, $(OUT)/%.o, $(basename $(SRCS)))
	$(CC) $(LDFLAGS) -o $@ $^

forward_input: forward_input.c keysym_to_linux_code.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(shell pkg-config --libs --cflags x11) $^ -o $@

all: remote-inputd forward_input

clean:
	rm -rf remote-inputd forward_input $(OUT)

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif

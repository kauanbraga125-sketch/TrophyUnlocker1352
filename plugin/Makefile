# Drop this directory into GoldHEN_Plugins_Repository/plugin_src/trophy_unlocker_1352
# and build with the repository's normal environment.
DEBUG_FLAGS = -D__FINAL__=1
LOG_TYPE = -D__USE_PRINTF__
BUILD_TYPE = _final
ifeq ($(DEBUG),1)
DEBUG_FLAGS = -D__FINAL__=0
BUILD_TYPE = _debug
endif
TYPE := $(BUILD_TYPE)
FINAL := $(DEBUG_FLAGS)
BUILD_FOLDER := $(shell pwd)/../../bin/plugins
OUTPUT_PRX := $(shell basename $(CURDIR))
TARGET := $(BUILD_FOLDER)/prx$(TYPE)/$(OUTPUT_PRX)
TARGET_ELF := $(BUILD_FOLDER)/elf$(TYPE)/$(OUTPUT_PRX)
TARGETSTUB := $(OUTPUT_PRX).so

LIBS := -lSceLibcInternal -lScePad -lGoldHEN_Hook -lkernel -lSceSysmodule -lSceUserService -lSceNpTrophy
EXTRAFLAGS := $(DEBUG_FLAGS) $(LOG_TYPE) -fcolor-diagnostics -Wall -Wextra
TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
GH_SDK := $(GOLDHEN_SDK)
PROJDIR := ../$(shell basename $(CURDIR))/source
INTDIR := ../$(shell basename $(CURDIR))/build
INCLUDEDIR := ../$(shell basename $(CURDIR))/include
COMMON_DIR := ../../common
CORE_DIR := ../$(shell basename $(CURDIR))/core

CFILES := $(wildcard $(PROJDIR)/*.c) $(wildcard $(CORE_DIR)/*.c)
CPPFILES := $(wildcard $(PROJDIR)/*.cpp)
COMMONFILES := $(wildcard $(COMMON_DIR)/*.cpp)
OBJS := $(patsubst $(PROJDIR)/%.c,$(INTDIR)/%.o,$(wildcard $(PROJDIR)/*.c)) \
        $(patsubst $(CORE_DIR)/%.c,$(INTDIR)/core_%.o,$(wildcard $(CORE_DIR)/*.c)) \
        $(patsubst $(PROJDIR)/%.cpp,$(INTDIR)/%.o,$(CPPFILES)) \
        $(patsubst $(COMMON_DIR)/%.cpp,$(INTDIR)/common_%.o,$(COMMONFILES))

CFLAGS := $(FINAL) --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) \
          -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -I$(GH_SDK)/include \
          -I$(INCLUDEDIR) -I$(COMMON_DIR) -I$(CORE_DIR) $(O_FLAG)
CXXFLAGS := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x -e _init --eh-frame-hdr \
           -L$(TOOLCHAIN)/lib -L$(GH_SDK) $(LIBS)
_unused := $(shell mkdir -p $(INTDIR) $(BUILD_FOLDER)/prx$(TYPE) $(BUILD_FOLDER)/elf$(TYPE))
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
CC := clang
CCX := clang++
LD := ld.lld
CDIR := linux
endif
ifeq ($(UNAME_S),Darwin)
CC := /usr/local/opt/llvm/bin/clang
CCX := /usr/local/opt/llvm/bin/clang++
LD := /usr/local/opt/llvm/bin/ld.lld
CDIR := macos
endif

$(TARGET): $(INTDIR) $(OBJS)
	$(LD) $(GH_SDK)/build/crtprx.o $(INTDIR)/*.o -o $(TARGET_ELF).elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(TARGET_ELF).elf -out=$(TARGET_ELF).oelf --lib=$(TARGET).prx --paid 0x3800000000000011

$(INTDIR)/%.o: $(PROJDIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<
$(INTDIR)/core_%.o: $(CORE_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<
$(INTDIR)/%.o: $(PROJDIR)/%.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

build-info:
	@echo "building $(OUTPUT_PRX)"
plugin_common:
	$(CC) $(CFLAGS) -o $(INTDIR)/plugin_common.o $(COMMON_DIR)/plugin_common.c

.PHONY: clean
.DEFAULT_GOAL := all
all: build-info plugin_common $(TARGET)
clean:
	rm -rf $(TARGET) $(TARGETSTUB) $(INTDIR)

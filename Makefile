# GNU Make build for the standalone TPM updater.
# Building or testing NEVER sends a firmware update to the TPM.
# Use `make V=1` to show the complete compiler and linker commands.

.DEFAULT_GOAL := all

CXX ?= c++
PKG_CONFIG ?= pkg-config
CXXFLAGS ?= -O2 -g -Wall -Wextra -std=c++17
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?=
PKGS ?= tss2-esys tss2-sys tss2-tcti-device openssl

SOURCES := $(wildcard src/*.cc)
OBJECTS := $(patsubst src/%.cc,build/%.o,$(SOURCES))
TARGET := build/tpm-firmware-updater
TEST := build/test_endian

PKG_CFLAGS = $(shell $(PKG_CONFIG) --cflags $(PKGS) 2>/dev/null)
PKG_LIBS = $(shell $(PKG_CONFIG) --libs $(PKGS) 2>/dev/null)

ifeq ($(V),1)
Q :=
else
Q := @
endif

.PHONY: all help check-deps test clean

all: $(TARGET)
	@printf '\n[OK] Executable: %s\n' '$(TARGET)'
	@printf '%s\n' '     No firmware update has been performed.'

help:
	@printf '\n%s\n%s\n\n' 'TPM Firmware Updater - build commands' '-------------------------------------'
	@printf '  %-20s %s\n' 'make' 'Build the updater (default)' \
	    'make test' 'Run hardware-free endian tests' \
	    'make check-deps' 'Verify TPM2-TSS and OpenSSL dependencies' \
	    'make clean' 'Remove generated build files' \
	    'make V=1' 'Show full compiler and linker commands'
	@printf '\n%s\n\n' 'Note: no Makefile target flashes TPM firmware.'

check-deps:
	@command -v $(PKG_CONFIG) >/dev/null || { printf '%s\n' '[ERROR] pkg-config not found'; exit 1; }
	@$(PKG_CONFIG) --exists $(PKGS) || { printf '%s\n' '[ERROR] Missing development dependencies:'; $(PKG_CONFIG) --print-errors --exists $(PKGS); exit 1; }
	@if [ '$(MAKECMDGOALS)' = 'check-deps' ]; then printf '%s\n' '[OK] TPM2-TSS and OpenSSL development dependencies found.'; fi

build:
	@mkdir -p $@

build/%.o: src/%.cc | build check-deps
	@printf '[CXX] %s\n' '$<'
	$(Q)$(CXX) $(CPPFLAGS) $(PKG_CFLAGS) $(CXXFLAGS) -pthread -MMD -MP -c $< -o $@

$(TARGET): $(OBJECTS)
	@printf '[LINK] %s\n' '$@'
	$(Q)$(CXX) $(LDFLAGS) $^ -o $@ $(PKG_LIBS) -pthread $(LDLIBS)

$(TEST): tests/test_endian.cc src/portable_endian.h | build
	@printf '[CXX] %s\n' '$<'
	$(Q)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc $< -o $@

test: $(TEST)
	@printf '[TEST] Portable endian conversions\n'
	$(Q)./$(TEST)

clean:
	@rm -rf build
	@printf '%s\n' '[CLEAN] Removed build/'

-include $(OBJECTS:.o=.d)

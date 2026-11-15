#!/usr/bin/env make
# SDM660 WinPort — Local build script (does NOT require full EDK II setup)
#
# This Makefile is a convenience wrapper for developers who want to:
#   (a) validate source code structure without compiling,
#   (b) generate ACPI .aml blobs with iASL before integrating into EDK II,
#   (c) do a quick syntax check on C sources via clang -fsyntax-only.
#
# Prerequisites:
#   make, python3, iasl (ACPICA compiler), clang or gcc (for syntax check)
#
# Targets:
#   all        — check everything (ACPI + C syntax + repo layout)
#   acpi       — compile SSDT/RSDP sources to .aml blobs
#   check      — clang/gcc syntax validation (no linking)
#   layout     — print the repository tree for review
#   clean      — remove generated artifacts

SHELL     := /bin/bash
PREFIX    ?= Build
PYTHON    ?= python3
IASL      ?= $(shell command -v iasl 2>/dev/null || echo "")

ASL_SRC   := AcpiTables/ssdt_cpu.asl AcpiTables/ssdt_periph.asl
ASL_OBJS  := $(patsubst AcpiTables/%.asl,$(PREFIX)/acpi/%.aml,$(ASL_SRC))
C_SRCS    := $(wildcard Drivers/*/*.c UefiPlatform/*.c AcpiTables/*.c)

.PHONY: all acpi check layout clean

all: acpi check layout
	@echo ""
	@echo "=== Build complete ==="
	@echo "ACPI blobs : $(PREFIX)/acpi/"
	@echo "Firmware   : run EDK2 build per Docs/build.md"

acpi: $(ASL_OBJS)

$(PREFIX)/acpi/%.aml: AcpiTables/%.asl | $(PREFIX)/acpi
	@if [ -z "$(IASL)" ]; then \
		echo "[ERROR] iASL not found in PATH."; \
		echo "       Install ACPICA tools: https://acpica.org/downloads"; \
		exit 1; \
	fi
	$(IASL) -p $(PREFIX)/acpi/$* $<

$(PREFIX)/acpi:
	mkdir -p $(PREFIX)/acpi

check:
	@echo "[CHECK] Validating $(words $(C_SRCS)) C sources ..."
	@for f in $(C_SRCS); do \
		if command -v clang >/dev/null 2>&1; then \
			clang -fsyntax-only $$f || exit 1; \
		elif command -v gcc >/dev/null 2>&1; then \
			gcc -fsyntax-only -x c -march=armv8-a $$f || exit 1; \
		else \
			echo "[WARN]  No clang or gcc found — skipping syntax check"; \
			break; \
		fi \
	done
	@echo "[OK]    All checked."

layout:
	@echo "[TREE]  Repository layout:"
	@find . -type f \( -name "*.c" -o -name "*.h" -o -name "*.asl" -o -name "*.dsc" \) \
	    -not -path "./$(PREFIX)/*" -not -path "./.git/*" \
	    | sort \
	    | sed 's#^#  #g'

clean:
	rm -rf $(PREFIX)

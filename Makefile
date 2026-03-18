#
# P3053 Makefile. 
#
# This makefile uses GNUMake to compile and link a hexadecimail programming file
# for a PIC32MZ microcontroller. The makefile finds all source files in the /src
# directory tree and compiles them into objects. It links them all together to
# create a binary executable. It converts this executable into a hexadecimal
# file for the Microship programmer.
#
# We do not use compiler flags to configure the build. We turn on and off
# internal code features, such as debug-level console reporting and provision of
# a Telnet command-line interface, using compiler macros in a config.h file.
# After modifying config.h, we build again using "make", and the build will take
# place in the build directory.
#
# We developed this makefile for the PIC32MZ2048EFH100. We compile for our
# Embedded Ethernet Module (A3053) target. We trust that the same Makefult
# structure can be adapted to other, similar embedded ethernet modules. The
# build uses the xc32 compiler, as well as a Microchip Device Package
# repository. Both must be present on your system for the build to complete.
#
# The makefile instructs the compiler to create dependency records for each
# object, and the makefile includes these in the dependency checking it does for
# each object it builds. In this way, the makefile will re-compile objects for
# which one or more required headers have been modified.
#
# The makefile compiles the source code in the SRC_DIRS directory tree and links
# them to one another and to a Microchip library using a a Microchip linker
# script. The library and linker script combined are what Microchip calls a
# "device package". We use devide package PIC32MZ-EF_DFP version 1.3.58. The
# P3053 repository does not include this device package, but you can download
# the package from our website using a link you will find the the A3053 manual.
#
# [08-MAR-26] Kevan Hashemi.
#
# Copyright Copyright (C) 2025-2026, Kevan Hashemi, Open Source Instruments Inc.
# 
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
# 
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <https://www.gnu.org/licenses/>.
#

#
# Define the exact processor, the location of its device pack, and point to its
# linker script. Do not use a relative path for the device pack. If you do, the
# linker will find a default device pack and use that to build the executable.
# The resulting code will fail in unpredictable ways and it will be a while
# before you figure out that it's a linker problem not a source code or hardware
# problem.
#
CPU := 32MZ2048EFH100
DFP_DIR := $(abspath ../Microchip/PIC32MZ-EF_DFP/1.3.58)
CPULD := $(DFP_DIR)/xc32/$(CPU)/p$(CPU).ld

#
# Assign a minimum heap size so that we can be sure to have enough RAM for our
# network connections.
#
HEAPSIZE=200000

#
# Colors for text output during the build.
#
RED    := \033[1;31m
YELLOW := \033[1;33m
BLUE   := \033[1;34m
CYAN   := \033[1;36m
RESET  := \033[0m

#
# All the x32 build tools: compilers, linkers, archivers, and assemblers.
#
MP_DIR := /Applications/microchip/xc32/v4.60/bin
MP_BIN2HEX := $(MP_DIR)/xc32-bin2hex
MP_CC := $(MP_DIR)/xc32-gcc
MP_CPPC := $(MP_DIR)/xc32-g++
MP_AS := $(MP_DIR)/xc32-as
MP_LD := $(MP_DIR)/xc32-ld
MP_AR := $(MP_DIR)/xc32-ar

#
# Define the build and distribution directories, the final output file and the
# map file names.
#
TARGET := P3053
BUILD_DIR := build
$(info BUILD_DIR = $(BUILD_DIR))
OUTPUT_FILE := $(BUILD_DIR)/$(TARGET).elf
$(info OUTPUT_FILE = $(OUTPUT_FILE))
MAP_FILE := $(BUILD_DIR)/$(TARGET).map

#
# Get a list of all the sources in the source directory. These are C and
# assembler files. Use these to generate a list of object files.
#
SRC_DIRS := src
SOURCEFILES := $(shell find $(SRC_DIRS) -type f \( -name '*.c' -o -name '*.S' \))
OBJECTFILES := $(patsubst src/%, $(BUILD_DIR)/src/%, $(SOURCEFILES:.c=.o))
OBJECTFILES := $(OBJECTFILES:.S=.o)

#
# Initialize our flag variables. If we specify any of these at the commmand line
# when we invoke Make, our local values will be over-written.
#
CFLAGS :=
ASFLAGS :=
LDFLAGS :=

#
# Flags that are shared by the release and debug builds.
#
CFLAGS += -mprocessor=$(CPU) \
	-ffunction-sections \
	-fdata-sections \
	-fno-common \
	-DHAVE_CONFIG_H \
	-DWOLFSSL_IGNORE_FILE_WARN \
	-O2 \
	-I"src" \
	-I"src/shim" \
	-I"src/microchip" \
	-I"src/microchip/library" \
	-I"src/microchip/library/tcpip/src" \
	-I"src/microchip/library/tcpip/src/common" \
	-Wno-error \
	-Wno-unused-function \
	-Wall
ASFLAGS += -mprocessor=$(CPU)  \
	-Wa,--gdwarf-2 \
	-mdfp="$(DFP_DIR)"
LDFLAGS += -mprocessor=$(CPU) \
	-Wl,--script=$(CPULD) \
	-Wl,--defsym=_min_heap_size=$(HEAPSIZE) \
	-Wl,--gc-sections \
	-Wl,--no-code-in-dinit \
	-Wl,--no-dinit-in-serial-mem \
	-Wl,-Map=$(MAP_FILE) \
	-Wl,--memorysummary,$(BUILD_DIR)/memoryfile.xml \
	-mdfp="$(DFP_DIR)"

#
# Recipe for compiling all C input files into objects, including creating dependency
# makefiles for each object produced.
#
$(BUILD_DIR)/%.o : %.c
	@mkdir -p $(dir $@)
	@printf "$(CYAN)Compiling $< $(RESET)\n"
	$(MP_CC) -x c -c $(CFLAGS) -MP -MMD -MF "$@.d" -o $@ $<
	
#
# Recipe for assembling all assembler input files into objects, including
# creating dependency makefiles for each object produced.
#
$(BUILD_DIR)/%.o : %.S
	@mkdir -p $(dir $@)
	@printf "$(GREEN)ASSEMBLE: $< $(RESET)\n"
	$(MP_CC) -x assembler-with-cpp -c $(ASFLAGS) -MP -MMD -MF "$@.d" -o $@ $<

#
# Recipe for linking all objects into the final output file.
#
$(OUTPUT_FILE): $(OBJECTFILES) $(CPULD) Makefile
	@mkdir -p $(BUILD_DIR)
	@printf "$(BLUE)Linking $@ $(RESET)\n"
	$(MP_CC) $(LDFLAGS) -o $@ $(OBJECTFILES)
	$(MP_BIN2HEX) $(OUTPUT_FILE)

#
# The default target: build the hexadecimal programming file.
#
build: $(OUTPUT_FILE)

#
# When we clean, we remove all files and directories in the object and
# distribution directory trees.
#
clean: remove
	@printf "$(YELLOW)Cleaning build directories$(RESET)\n"
	find build -mindepth 1 -delete

#
# The remove target removes the output file so that we can, by calling
# make, re-link the output file and test the link command.
#
remove:
	@printf "$(YELLOW)Removing output files$(RESET)\n"
	rm -f build/$(TARGET).elf
	rm -f build/$(TARGET).hex
	rm -f build/$(TARGET).map

#
# We have told the compiler to create a dependency makefiles for every object it
# creates, which lists the source and header files included in the compile.
# These dependency files contain a recipe for building the object that lists its
# dependencies. By this means, Make will look at all the header files required
# by an object and re-compile the object if any of the headers is modified.
#
DEPFILES := $(OBJECTFILES:.o=.o.d)
-include $(DEPFILES)
#
# The P3053A Makefile. 
#
# This makefile uses GNUMake to compile a hexadecimail programming file from all
# the source files in the local src directory. The "build" target adds some
# debugging flags to the compile, assemble, and link. We want to turn on the
# debugging console for debug builds, but disable it for production builds. The
# build produces a hex output file we can use to program a PIC32MZ
# microprocessor. We developed the makefile for a PIC32MZ2048EFH100, with TCP/IP
# and WOLFSSL code for our Embedded Ethernet Module (A3053A), but we trust that
# the same structure can be used to compile arbitrary projects from the command
# line for any PIC processor. The build uses an installation of the xc32
# compiler, as well as a Microchip Device Package repository, where it finds
# the CPU device package.
#
# Kevan Hashemi, Open Source Instruments Inc., 2025.
#

#
# Detect the build mode so that we can set flags before we get to the
# target definition. We will look in the MAKECMDGOALS reservbed variable
# for certain targets that dictate the build mode.
#
ifeq (debug,$(findstring debug,$(MAKECMDGOALS)))
BUILD_MODE=debug
else
BUILD_MODE=production
endif
$(info BUILD_MODE = $(BUILD_MODE))


#
# Define the exact processor, its family, the location of its device pack, and
# point to the linker script. We are using the linker script and device pack
# that are provided by our Microchip Device Package repository.
#
CPU=32MZ2048EFH100
CPU_FAMILY=pic32mz_ef_sk
DFP_DIR=/Users/kevan/Code/Microchip/PIC32MZ-EF_DFP/1.3.58
CPULD=$(DFP_DIR)/xc32/$(CPU)/p$(CPU).ld

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
MP_DIR=/Applications/microchip/xc32/v4.60/bin
MP_BIN2HEX=$(MP_DIR)/xc32-bin2hex
MP_CC=$(MP_DIR)/xc32-gcc
MP_CPPC=$(MP_DIR)/xc32-g++
MP_AS=$(MP_DIR)/xc32-as
MP_LD=$(MP_DIR)/xc32-ld
MP_AR=$(MP_DIR)/xc32-ar

#
# Define the build and distribution directories, the final output file and the
# map file names.
#
BUILD_DIR=build
OUTPUT_FILE=$(BUILD_DIR)/$(BUILD_MODE).elf
MAP_FILE=$(BUILD_DIR)/$(BUILD_MODE).map

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
CFLAGS=
ASFLAGS=
LDFLAGS=

#
# Flags that are shared by the production and debug builds.
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
	-I"src/config" \
	-I"src/config/library" \
	-I"src/config/library/tcpip/src" \
	-I"src/config/library/tcpip/src/common" \
	-I"src/wolfssl" \
	-I"src/wolfssl/wolfssl" \
	-Wno-error \
	-Wno-unused-function \
	-Wall
ASFLAGS += -mprocessor=$(CPU)  \
	-Wa,--gdwarf-2 \
	-mdfp="$(DFP_DIR)"
LDFLAGS += -mprocessor=$(CPU) \
	-Wl,--script=$(CPULD) \
	-Wl,--defsym=_min_heap_size=64960 \
	-Wl,--gc-sections \
	-Wl,--no-code-in-dinit \
	-Wl,--no-dinit-in-serial-mem \
	-Wl,-Map=$(MAP_FILE) \
	-Wl,--memorysummary,$(BUILD_DIR)/memoryfile.xml \
	-mdfp="$(DFP_DIR)"

#
# Add flags depending upon whether this is a debug or production build.
#
ifeq ($(BUILD_MODE),debug)
CFLAGS += -D__DEBUG
ASFLAGS += -D__DEBUG
LDFLAGS += -Wl,--defsym=__DEBUG=1
endif

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
# When we build, we create the compiled hex or elf file that we can 
# load into the processor.
#
build: $(OUTPUT_FILE)

#
# The debug target is the same as the build target, except we have added
# some compiler, assembler, and linker flags earlier in the makefile, so
# as to affect the build.
#
debug: build

#
# When we clean, we remove all files and directories in the object and
# distribution directory trees.
#
clean:
	@printf "$(YELLOW)Cleaning ${BUILD_DIR} directories$(RESET)\n"
	find ${BUILD_DIR} -mindepth 1 -delete

#
# The remove target removes the output file so that we can, by calling
# make, re-link the output file and test the link command.
#
remove:
	@printf "$(YELLOW)Removing $(OUTPUT_FILE)$(RESET)\n"
	rm $(OUTPUT_FILE)

#
# We have told the compiler to create a dependency makefiles for every object it
# creates, which lists the source and header files included in the compile.
# These dependency files contain a recipe for building the object that lists its
# dependencies. By this means, Make will look at all the header files required
# by an object and re-compile the object if any of the headers is modified.
#
DEPFILES := $(OBJECTFILES:.o=.o.d)
-include $(DEPFILES)
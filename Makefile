#
# The P3053A Makefile. 
#
# This makefile uses GNUMake to compile a hexadecimail programming file from all
# the source files in the local src directory. If we specify BUILD_MODE equal to
# "debug", the build produces a debugger "elf" output file. If we do not specify
# BUILD_MODE, or we specify "production", the build produces a non-debug elf
# binary and a hexadecimal version of the binary file that we can use to program
# a PIC microprocessor. We developed the makefile for a PIC32MZ2048EFH100, with
# TCP/IP and WOLFSSL code for our Embedded Ethernet Module (A3053A), but we
# trust that the same structure can be used to compile arbitrary projects from
# the command line for any PIC processor. The build uses an installation of the
# xc32 compiler, as well as a Harmony3 repository, where it finds the CPU device
# package.
#
# Kevan Hashemi, Open Source Instruments Inc., 2025.
#

#
# Define the exact processor, its family, the location of its device pack, and
# point to the linker script. We are using the linker script and device pack
# that are provided by our Harmony3 repository.
#
CPU=32MZ2048EFH100
CPU_FAMILY=pic32mz_ef_sk
DFP_DIR=/Users/kevan/Code/Harmony3/dev_packs/Microchip/PIC32MZ-EF_DFP/1.3.58
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
# Make sure the debug mode variable is defined as either "debug" or "production" to
# control our compile, assemble, and link stages.
#
ifeq ($(BUILD_MODE),debug)
BUILD_MODE=debug
else
BUILD_MODE=production
endif

#
# Define the build and distribution directories, the final output file and the
# map file names.
#
BUILD_DIR=build
DIST_DIR=dist
OUTPUT_FILE=$(DIST_DIR)/$(BUILD_MODE).elf
MAP_FILE=$(DIST_DIR)/$(BUILD_MODE).map

#
# We will translate the output file into hexadecimal if this is a production
# build.
#
ifeq ($(BUILD_MODE),debug)
POST_LINK :=
else
POST_LINK := $(MP_BIN2HEX) $(OUTPUT_FILE)
endif

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
CFLAGS += \
	-mprocessor=$(CPU) \
	-ffunction-sections \
	-fdata-sections \
	-O1 \
	-fno-common \
	-DHAVE_CONFIG_H \
	-DWOLFSSL_IGNORE_FILE_WARN \
	-I"src" \
	-I"src/config/$(CPU_FAMILY)" \
	-I"src/config/$(CPU_FAMILY)/library" \
	-I"src/config/$(CPU_FAMILY)/library/tcpip/src" \
	-I"src/config/$(CPU_FAMILY)/library/tcpip/src/common" \
	-I"src/third_party/wolfssl" \
	-I"src/third_party/wolfssl/wolfssl" \
	-Werror -Wall
ASFLAGS += \
	-mprocessor=$(CPU)  \
	-Wa,--defsym=__MPLAB_BUILD=1 \
	-Wa,--gdwarf-2 \
	-mdfp="$(DFP_DIR)"
LDFLAGS += \
	-mprocessor=$(CPU) \
	-Wl,--defsym=__MPLAB_BUILD=1 \
	-Wl,--script=$(CPULD) \
	-Wl,--defsym=_min_heap_size=64960 \
	-Wl,--gc-sections \
	-Wl,--no-code-in-dinit \
	-Wl,--no-dinit-in-serial-mem \
	-Wl,-Map=$(MAP_FILE) \
	-Wl,--memorysummary,$(DIST_DIR)/memoryfile.xml \
	-mdfp="$(DFP_DIR)"

#
# Add flags depending upon whether this is a debug or production build.
#
ifeq ($(BUILD_MODE),debug)
CFLAGS += \
	-D__DEBUG \
	-D__MPLAB_DEBUGGER_ICD4=1 \
	-fframe-base-loclist
ASFLAGS += \
	-D__DEBUG \
	-D__MPLAB_DEBUGGER_ICD4=1 \
    -Wa,--defsym=__MPLAB_DEBUG=1 \
    -Wa,--defsym=__MPLAB_DEBUGGER_ICD4=1 \
    -Wa,--gdwarf-2
LDFLAGS += \
	-g -mdebugger \
	-D__MPLAB_DEBUGGER_ICD4=1 \
	-Wl,--defsym=__MPLAB_DEBUG=1 \
	-Wl,--defsym=__DEBUG=1 \
	-Wl,--defsym=__MPLAB_DEBUGGER_ICD4=1
else
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
# Recipe for linking all objects into the final output file. For production build, we
# use the post-link tasks to create a hexadecimal version of our elf output.
#
$(OUTPUT_FILE): $(OBJECTFILES) $(CPULD) Makefile
	@mkdir -p $(DIST_DIR)
	@echo "$(BLUE)Linking $@ $(RESET)\n"
	$(MP_CC) $(LDFLAGS) -o $@ $(OBJECTFILES)
	$(POST_LINK)

#
# When we build, we create the compiled hex or elf file that we can 
# load into the processor.
#
build: $(OUTPUT_FILE)

#
# When we clean, we remove all files and directories in the object and
# distribution directory trees.
#
clean:
	@printf "$(YELLOW)Cleaning ${BUILD_DIR} and ${DIST_DIR} directories$(RESET)\n"
	find ${BUILD_DIR} ${DIST_DIR} -mindepth 1 -delete

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
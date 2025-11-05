#
# The P3053A Makefile.
#
# Kevan Hashemi, Open Source Instruments Inc., 2025.
#

#
# Location of the processor device pack. We are using the one in the Harmony3
# repository. We don't copy the device pack into our project because it is 
# about half a Gigabyte.
#
DFP_DIR=/Users/kevan/Code/Harmony3/dev_packs/Microchip/PIC32MZ-EF_DFP/1.3.58

#
# The location of the project files and the project name.
#
PROJECT=P3053
CONF=pic32mz_ef_sk
CPULD=src/config/pic32mz_ef_sk/p32MZ2048EFH100.ld
CPU=32MZ2048EFH100

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
MP_CC=/Applications/microchip/xc32/v4.60/bin/xc32-gcc
MP_CPPC=/Applications/microchip/xc32/v4.60/bin/xc32-g++
MP_AS=/Applications/microchip/xc32/v4.60/bin/xc32-as
MP_LD=/Applications/microchip/xc32/v4.60/bin/xc32-ld
MP_AR=/Applications/microchip/xc32/v4.60/bin/xc32-ar
MP_CC_DIR=/Applications/microchip/xc32/v4.60/bin
MP_CPPC_DIR=/Applications/microchip/xc32/v4.60/bin
MP_AS_DIR=/Applications/microchip/xc32/v4.60/bin
MP_LD_DIR=/Applications/microchip/xc32/v4.60/bin
MP_AR_DIR=/Applications/microchip/xc32/v4.60/bin

#
# Make sure the debug mode variable is defined as either "debug" or "production" to
# control our compile, assemble, and link stages.
#
ifeq ($(BUILD_MODE),debug)
BUILD_MODE=debug
else
BUILD_MODE=production
endif

OBJECTDIR=build
DISTDIR=dist
OUTPUT_FILE=$(DISTDIR)/$(BUILD_MODE).elf
MAP_FILE=$(DISTDIR)/$(BUILD_MODE).map

ifeq ($(BUILD_MODE),debug)
POST_LINK :=
else
POST_LINK := $(MP_CC_DIR)/xc32-bin2hex $(OUTPUT_FILE)
endif

SRC_DIRS := src
SOURCEFILES := $(shell find $(SRC_DIRS) -type f \( -name '*.c' -o -name '*.S' \))
OBJECTFILES := $(patsubst src/%, $(OBJECTDIR)/src/%, $(SOURCEFILES:.c=.o))
OBJECTFILES := $(OBJECTFILES:.S=.o)

CFLAGS=
ASFLAGS=
LDFLAGS=

CFLAGS += \
	-mprocessor=$(CPU) \
	-ffunction-sections \
	-fdata-sections \
	-O1 \
	-fno-common \
	-DHAVE_CONFIG_H \
	-DWOLFSSL_IGNORE_FILE_WARN \
	-I"src" \
	-I"src/config/pic32mz_ef_sk" \
	-I"src/config/pic32mz_ef_sk/library" \
	-I"src/config/pic32mz_ef_sk/library/tcpip/src" \
	-I"src/config/pic32mz_ef_sk/library/tcpip/src/common" \
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
	-Wl,--memorysummary,$(DISTDIR)/memoryfile.xml \
	-mdfp="$(DFP_DIR)"

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
	
endif

$(OBJECTDIR)/%.o : %.c
	@mkdir -p $(dir $@)
	@printf "$(CYAN)Compiling $< $(RESET)\n"
	$(MP_CC) -x c -c $(CFLAGS) -MP -MMD -MF "$@.d" -o $@ $<
	
$(OBJECTDIR)/%.o : %.S
	@mkdir -p $(dir $@)
	@printf "$(GREEN)ASSEMBLE: $< $(RESET)\n"
	$(MP_CC) -x assembler-with-cpp -c $(ASFLAGS) -MP -MMD -MF "$@.d" -o $@ $<

$(OUTPUT_FILE): $(OBJECTFILES) $(CPULD) Makefile
	@mkdir -p $(DISTDIR)
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
	@printf "$(YELLOW)Cleaning ${OBJECTDIR} and ${DISTDIR} directories$(RESET)\n"
	find ${OBJECTDIR} ${DISTDIR} -mindepth 1 -delete

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
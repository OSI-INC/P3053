# Define the path to the MPLAB binary directory and add to the path variable.
PATH_TO_IDE_BIN=/Applications/microchip/mplabx/v6.25/mplab_platform/platform/../mplab_ide/modules/../../bin/
PATH:=/Applications/microchip/mplabx/v6.25/mplab_platform/platform/../mplab_ide/modules/../../bin/:$(PATH)

# Define the path to the MPLAB X Java binary and use it do define a dependency tracker.
MP_JAVA_PATH="/Applications/microchip/mplabx/v6.25/sys/java/zulu8.80.0.17-ca-fx-jre8.0.422-macosx_x64/zulu-8.jre/Contents/Home/bin/"
DEP_GEN=${MP_JAVA_PATH}java -jar "/Applications/microchip/mplabx/v6.25/mplab_platform/platform/../mplab_ide/modules/../../bin/extractobjectdependencies.jar"

# Not sure if this is ever used.
OS_CURRENT="$(shell uname -s)"

# The x32 build tools.
MP_CC="/Applications/microchip/xc32/v4.60/bin/xc32-gcc"
MP_CPPC="/Applications/microchip/xc32/v4.60/bin/xc32-g++"
# MP_BC is not defined
MP_AS="/Applications/microchip/xc32/v4.60/bin/xc32-as"
MP_LD="/Applications/microchip/xc32/v4.60/bin/xc32-ld"
MP_AR="/Applications/microchip/xc32/v4.60/bin/xc32-ar"
MP_CC_DIR="/Applications/microchip/xc32/v4.60/bin"
MP_CPPC_DIR="/Applications/microchip/xc32/v4.60/bin"
# MP_BC_DIR is not defined
MP_AS_DIR="/Applications/microchip/xc32/v4.60/bin"
MP_LD_DIR="/Applications/microchip/xc32/v4.60/bin"
MP_AR_DIR="/Applications/microchip/xc32/v4.60/bin"

# Location of the processor device pack.
DFP_DIR=/Users/kevan/Code/Harmony3/dev_packs/Microchip/PIC32MZ-EF_DFP/1.3.58

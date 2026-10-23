# The Xbox kernel's own FATX driver against a volume vmx-firmware/core/fatx.c made: tools/kernel_fatx_test.sh.
# NEVER `make clean` here (nxdk's clean deletes its shared libraries).
XBE_TITLE = FATX_check
SRCS = main.c
CFLAGS += -O2
OUTPUT_DIR = ../../build/xbox/kernel_fatx
include $(NXDK_DIR)/Makefile

# VMX World's network loader: tools/build_xbox.sh --loader (through the symlink) -> build/xbox/loader/default.xbe,
# titled VMX World. NEVER `make clean` here (nxdk's clean deletes its shared libraries).
XBE_TITLE = VMX_World
SRCS = main.c xnet_http.c
CFLAGS += -O2
NXDK_NET = y
OUTPUT_DIR = ../build/xbox/loader
include $(NXDK_DIR)/Makefile

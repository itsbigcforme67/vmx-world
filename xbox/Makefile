# VMX World on the original Xbox (nxdk): tools/build_xbox.sh, which runs this through a symlink (make can't take the
# space in the repo's path; every path here is relative, so none of them has one).
# -> build/xbox/VMXWORLD/ (default.xbe, assets/, catalog/ = the VMX games the World "service" offers) and
#    build/xbox/vmxworld.iso of it (xemu). DEMO=file puts a pad script in as demo.txt (played first; tests).
# The platform layer is FOLD's (main_xbox.c: SDL pad, the GPU or the software picture to the framebuffer, COM1 log),
# the renderer FOLD's gfx.c, the VMX the firmware's hardware-free core with the .NET runtime.
# NEVER `make clean` here: nxdk's clean also deletes the shared nxdk libraries in ~/xboxdev/nxdk/lib.
XBE_TITLE = VMX_World
FOLD = ../../microgames/fold
FW = ../../vmx-firmware
SRCS = world.c hdd.c holo.c $(FOLD)/app/gfx.c $(FOLD)/app/main_xbox.c $(FOLD)/app/padscript.c $(FOLD)/xbox/gpu.c \
       $(FW)/core/ui.c $(FW)/core/surf.c $(FW)/core/sound.c $(FW)/core/records.c $(FW)/core/games.c $(FW)/core/game_fold.c \
       $(FW)/core/game_vx.c $(FW)/runtime/vxrt.c $(FOLD)/core/fold.c $(FOLD)/app/vmx.c
CFLAGS += -O2 -include $(FOLD)/xbox/xbox_compat.h -include nxdk_compat.h -DNXDK -Wno-misleading-indentation
LDFLAGS += -stack:0x40000
NXDK_SDL = y
SHADER_OBJS = $(FOLD)/xbox/vs.inl $(FOLD)/xbox/ps.inl
OUTPUT_DIR = build/xbox
include $(NXDK_DIR)/Makefile

package: $(OUTPUT_DIR)/default.xbe
	rm -rf $(OUTPUT_DIR)/VMXWORLD $(OUTPUT_DIR)/vmxworld.iso
	mkdir -p $(OUTPUT_DIR)/VMXWORLD/assets $(OUTPUT_DIR)/VMXWORLD/catalog
	cp $(OUTPUT_DIR)/default.xbe $(OUTPUT_DIR)/VMXWORLD/
	python3 ../tools/xbe_title_id.py $(OUTPUT_DIR)/VMXWORLD/default.xbe 0x56580001
	cp $(FOLD)/assets/vmx_small.rgba $(FOLD)/assets/vmx_large.rgba assets/vmx_holo.pak $(OUTPUT_DIR)/VMXWORLD/assets/
	cp $(FW)/build/games/*.VXP $(OUTPUT_DIR)/VMXWORLD/catalog/
	if [ -n "$(DEMO)" ]; then cp $(DEMO) $(OUTPUT_DIR)/VMXWORLD/demo.txt; fi
	$(EXTRACT_XISO) -c $(OUTPUT_DIR)/VMXWORLD $(OUTPUT_DIR)/vmxworld.iso >/dev/null
	@echo "built $(OUTPUT_DIR)/VMXWORLD and $(OUTPUT_DIR)/vmxworld.iso"

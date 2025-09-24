# Config
DEBUG_ENABLED := 1
MODEL_SCALE := 100
TICKRATE := 30
COMPRESS_LEVEL := 2

BUILD_DIR := build

include $(N64_INST)/include/n64.mk
include $(T3D_INST)/t3d.mk

SRC_DIRS := src
C_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
O_FILES := $(C_FILES:%.c=$(BUILD_DIR)/%.o)

TARGET := gjk-test
ROM := $(TARGET).z64
ELF := $(BUILD_DIR)/$(TARGET).elf
DFS := $(BUILD_DIR)/$(TARGET).dfs
N64_CFLAGS += -Wall -Wextra -Werror -Ofast -DMODEL_SCALE=$(MODEL_SCALE) \
	      -DTICKRATE=$(TICKRATE) -DCOMPRESS_LEVEL=$(COMPRESS_LEVEL)

ifeq ($(DEBUG_ENABLED),1)
	N64_CFLAGS += -DDEBUG
endif

ASSETS_PNG := $(wildcard assets/*.png)
ASSETS_GLTF := $(wildcard assets/*.gltf)
ASSETS_CONV := $(ASSETS_PNG:assets/%.png=filesystem/%.sprite) \
	       $(ASSETS_GLTF:assets/%.gltf=filesystem/%.t3dm) \
	       $(ASSETS_GLTF:assets/%.gltf=filesystem/%.cm)

final: $(ROM)
$(ROM): N64_ROM_TITLE="GJK Test"
$(ROM): $(DFS) 
$(DFS): $(ASSETS_CONV)
$(ELF): $(O_FILES)

MKSPRITE_FLAGS := --compress $(COMPRESS_LEVEL)
MKASSET_FLAGS := --compress $(COMPRESS_LEVEL)

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o $(dir $@) "$<"

filesystem/%.t3dm: assets/%.gltf
	@mkdir -p $(dir $@)
	@echo "    [T3D-MODEL] $@"
	$(T3D_GLTF_TO_3D) "$<" $@ --base-scale=$(MODEL_SCALE)
	$(N64_BINDIR)/mkasset $(MKASSET_FLAGS) -o $(dir $@) $@

GLTF_TO_CM := tools/gltf-to-coldat/gltf-to-coldat

$(GLTF_TO_CM):
	make -C $(dir $@)

filesystem/%.cm: assets/%.gltf $(GLTF_TO_CM)
	@mkdir -p $(dir $@)
	@echo "    [COLLISION] $@"
	$(GLTF_TO_CM) assets filesystem $(basename $(notdir $@))
	$(N64_BINDIR)/mkasset $(MKASSET_FLAGS) -o $(dir $@) $@

.PHONY: clean todo

clean:
	rm -rf $(BUILD_DIR) filesystem

todo: $(C_FILES)
	grep -i --color=always "todo" $^
	grep -i --color=always "fixme" $^

-include $(wildcard $(BUILD_DIR)/*.d)

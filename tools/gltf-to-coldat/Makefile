CC := gcc
CFLAGS := -Wall -Wextra -Werror -O3 -std=c99 -ggdb3
BUILD_DIR := build
PROG := gltf-to-coldat
SRC_FILES := main.c
OBJ_FILES := $(SRC_FILES:%.c=$(BUILD_DIR)/%.o)

all: $(OBJ_FILES)
	$(CC) $(CFLAGS) -o gltf-to-coldat $(OBJ_FILES)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(BUILD_DIR) $(PROG)

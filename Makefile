# Kernel Module Makefile

# Kernel version selection parameter, default is 6.14
KERNEL_VERSION ?= 6.14

# Select source file based on kernel version
ifeq ($(KERNEL_VERSION), 6.14)
    MODULE_NAME := ftrace_filter_inject
    SOURCE_FILE := ftrace_filter_inject_6.14.c
else ifeq ($(KERNEL_VERSION), 4.9)
    MODULE_NAME := ftrace_filter_inject
    SOURCE_FILE := ftrace_filter_inject_4.9.c
else
    $(error Unsupported kernel version: $(KERNEL_VERSION). Use KERNEL_VERSION=6.14 or KERNEL_VERSION=4.9)
endif

# Kernel source directory
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

# Current directory
PWD := $(shell pwd)

# Build directories
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
MODULE_SRC := $(BUILD_DIR)/$(MODULE_NAME).c
MODULE_KO := $(MODULE_NAME).ko
FINAL_KO := $(BIN_DIR)/$(MODULE_NAME).ko

# Default target: build module
all: $(FINAL_KO)

# Create build directory and copy source files
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

# Copy source file to build directory
$(MODULE_SRC): $(SOURCE_FILE) | $(BUILD_DIR)
	cp $(SOURCE_FILE) $(MODULE_SRC)

# Create Makefile in build directory
$(BUILD_DIR)/Makefile: | $(BUILD_DIR)
	echo "obj-m := $(MODULE_NAME).o" > $(BUILD_DIR)/Makefile

# Build module in build directory
$(BUILD_DIR)/$(MODULE_KO): $(MODULE_SRC) $(BUILD_DIR)/Makefile
	$(MAKE) -C $(KERNELDIR) M=$(PWD)/$(BUILD_DIR) modules
	@# Verify module was built
	@if [ ! -f $(BUILD_DIR)/$(MODULE_KO) ]; then \
		echo "Error: Module not built at $(BUILD_DIR)/$(MODULE_KO)"; \
		exit 1; \
	fi

# Move .ko file to bin directory
$(FINAL_KO): $(BUILD_DIR)/$(MODULE_KO)
	mv $(BUILD_DIR)/$(MODULE_KO) $(FINAL_KO)
	@echo "Module built at: $(FINAL_KO)"

# Create source file symbolic link (for development convenience)
$(MODULE_NAME).c: $(SOURCE_FILE)
	ln -sf $(SOURCE_FILE) $(MODULE_NAME).c

# Clean
clean:
	@if [ -d $(BUILD_DIR) ]; then \
		if [ -f $(BUILD_DIR)/Makefile ]; then \
			$(MAKE) -C $(KERNELDIR) M=$(PWD)/$(BUILD_DIR) clean || true; \
		fi; \
		rm -rf $(BUILD_DIR); \
	fi
	@echo "Cleaned build directory"

# Install module
install: $(FINAL_KO)
	sudo insmod $(FINAL_KO)

# Uninstall module
uninstall:
	sudo rmmod $(MODULE_NAME) 2>/dev/null || true

# View logs
log:
	sudo dmesg | grep "EIO-Inject"

# Set probability parameter
set-prob:
	sudo sh -c 'echo $(prob) > /sys/module/$(MODULE_NAME)/parameters/injection_probability'

.PHONY: all clean install uninstall log set-prob

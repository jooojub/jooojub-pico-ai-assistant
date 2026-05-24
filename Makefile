BUILD_DIR  = build
BOARD      = pico
PLATFORM   = rp2040
PICO_SDK   = $(CURDIR)/pico-sdk
MOUNT_PATH = $(CURDIR)/RPI-RP2

.PHONY: all clean flash

all: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR) --no-print-directory

$(BUILD_DIR)/Makefile: CMakeLists.txt
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DPICO_SDK_PATH=$(PICO_SDK) \
	                         -DPICO_BOARD=$(BOARD) \
	                         -DPICO_PLATFORM=$(PLATFORM) \
	                         -G "Unix Makefiles" ..

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/main.uf2
	@echo "Waiting for Pico in BOOTSEL mode (up to 30s)..."
	@DEVICE=""; \
	for i in $$(seq 1 30); do \
		DEVICE=$$(lsblk -o NAME,LABEL -rn | awk '$$2=="RPI-RP2"{print "/dev/"$$1}' | head -1); \
		if [ -n "$$DEVICE" ]; then break; fi; \
		sleep 1; \
	done; \
	if [ -z "$$DEVICE" ]; then \
		echo "Error: Pico not found. Hold BOOTSEL and reconnect USB."; \
		exit 1; \
	fi; \
	echo "Found: $$DEVICE"; \
	mkdir -p $(MOUNT_PATH); \
	sudo mount $$DEVICE $(MOUNT_PATH); \
	sudo cp $(BUILD_DIR)/main.uf2 $(MOUNT_PATH)/; \
	sudo sync; \
	sudo umount $(MOUNT_PATH); \
	echo "Flash complete."

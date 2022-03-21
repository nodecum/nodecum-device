
BOARD  ?= adafruit_feather_nrf52840
RUNNER ?= blackmagicprobe
RUNNER_ARGS ?= --gdb-serial /dev/ttyACM0

.PHONY: check flash_boot
MCUBOOT_DIR  := $(ZEPHYR_BASE)/../bootloader/mcuboot

BUILD_DIR := $(CURDIR)/build/$(BOARD)
SRC_DIR_BOOT   := $(MCUBOOT_DIR)/boot/zephyr
BUILD_DIR_BOOT := $(BUILD_DIR)/mcuboot
SRC_DIR_APP1   := $(CURDIR)/app1
BUILD_DIR_APP1 := $(BUILD_DIR)/app1
SRC_DIR_APP2   := $(CURDIR)/app2
BUILD_DIR_APP2 := $(BUILD_DIR)/app2
SRC_DIR_HELLO  := $(CURDIR)/hello
BUILD_DIR_HELLO:= $(BUILD_DIR)/hello
SRC_DIR_SMP    := $(CURDIR)/smp_svr
BUILD_DIR_SMP  := $(BUILD_DIR)/smp_svr

GDB := ~/bin/zephyr-sdk-0.13.2/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb

BOOTLOADER_CONFIG_OVERLAYS := "$(CURDIR)/boot/overlay-rsa.conf" 
# $(CURDIR)/boot/usb_cdc_acm.conf;
BOOTLOADER_DTC_OVERLAYS := "$(CURDIR)/boot/usb_cdc_acm.overlay"

help:
	@echo "make <target> BOARD=<board>"
	@echo "<target>: all, boot"
	@echo "<board>: adafruit_feather_nrf52840"

all: boot

boot: check
	(mkdir -p $(BUILD_DIR_BOOT) && \
		cd $(BUILD_DIR_BOOT) && \
		cmake -DOVERLAY_CONFIG=$(BOOTLOADER_CONFIG_OVERLAYS) \
			-G"Ninja" \
			-DBOARD=$(BOARD) \
			$(SRC_DIR_BOOT) && \
                       ninja)
	cp $(BUILD_DIR_BOOT)/zephyr/zephyr.bin mcuboot.bin

build_boot:
	west build -d $(BUILD_DIR_BOOT) -b $(BOARD) $(SRC_DIR_BOOT) \
		-- -DOVERLAY_CONFIG=$(BOOTLOADER_CONFIG_OVERLAYS) #\
#		   -DDTC_OVERLAY_FILE=$(BOOTLOADER_DTC_OVERLAYS)

flash_boot:
	west flash -d $(BUILD_DIR_BOOT) -r $(RUNNER) $(RUNNER_ARGS) \
		--skip-rebuild --elf-file $(BUILD_DIR_BOOT)/zephyr/zephyr.elf

build_app1:
	west build -d $(BUILD_DIR_APP1) -b $(BOARD) $(SRC_DIR_APP1)

sign_app1:
	west sign -d $(BUILD_DIR_APP1) -t imgtool -- --key $(MCUBOOT_DIR)/root-rsa-2048.pem

flash_app1:
	west flash -d $(BUILD_DIR_APP1) -r $(RUNNER) $(RUNNER_ARGS) \
		--skip-rebuild --elf-file $(BUILD_DIR_APP1)/zephyr/zephyr.signed.hex 

build_app2:
	west build -d $(BUILD_DIR_APP2) -b $(BOARD) $(SRC_DIR_APP2)

sign_app2:
	west sign -d $(BUILD_DIR_APP2) -t imgtool -- --key $(MCUBOOT_DIR)/root-rsa-2048.pem

flash_app2:
	west flash -d $(BUILD_DIR_APP2) -r $(RUNNER) $(RUNNER_ARGS) \
		--skip-rebuild --elf-file $(BUILD_DIR_APP2)/zephyr/zephyr.signed.hex 

build_hello:
	west build -d $(BUILD_DIR_HELLO) -b $(BOARD) $(SRC_DIR_HELLO)

flash_hello:
	west flash -d $(BUILD_DIR_HELLO) -r $(RUNNER) $(RUNNER_ARGS) \
		--skip-rebuild --hex-file $(BUILD_DIR_HELLO)/zephyr/zephyr.hex 

build_smp:
	west build -d $(BUILD_DIR_SMP) -b $(BOARD) $(SRC_DIR_SMP) \
		-- -DOVERLAY_CONFIG="$(SRC_DIR_SMP)/overlay-bt.conf;$(SRC_DIR_SMP)/display.conf" \
		   -DDTC_OVERLAY_FILE="$(SRC_DIR_SMP)/usb.overlay;$(SRC_DIR_SMP)/adafruit_feather_nrf52480.overlay;$(SRC_DIR_SMP)/ssd1306_128x32.overlay" 

#$(SRC_DIR_SMP)/overlay-cdc.conf;

sign_smp:
	west sign -d $(BUILD_DIR_SMP) -t imgtool -- --key $(MCUBOOT_DIR)/root-rsa-2048.pem

flash_smp:
	west flash -d $(BUILD_DIR_SMP) -r $(RUNNER) $(RUNNER_ARGS) \
		--skip-rebuild --elf-file $(BUILD_DIR_SMP)/zephyr/zephyr.signed.hex

upload_smp:
	sudo /home/robert/go/bin/mcumgr --conntype ble \
                --connstring 'peer_name=Zephyr' image upload \
                $(BUILD_DIR_SMP)/zephyr/zephyr.signed.bin


# flash_boot: boot
# 	$(GDB) \
# 		--batch \
# 		-ex 'set confirm off' \
# 		-ex 'target extended-remote /dev/ttyACM0' \
# 		-ex 'monitor swdp_scan' \
# 		-ex 'attach 1' \
# 		-ex 'file $(BUILD_DIR_BOOT)/zephyr/zephyr.elf' \
# 		-ex 'load' \
# 		-ex 'kill' \
# 		-ex 'quit' 


check:
	@if [ -z "$$ZEPHYR_BASE" ]; then echo "Zephyr environment not set up"; false; fi
	@if [ -z "$(BOARD)" ]; then echo "You must specify BOARD=<board>"; false; fi

# If you would like to choose a different path to the SDK, you can pass it as an
# argument.
#指定SDK库的位置
ifndef MICROKIT_SDK
	MICROKIT_SDK := ../microkit-sdk-2.0.1
endif

# In case the default compiler triple doesn't work for you or your package manager
# only has aarch64-none-elf or something, you can specifiy the toolchain.
ifndef TOOLCHAIN
	# Get whether the common toolchain triples exist
	TOOLCHAIN_AARCH64_LINUX_GNU := $(shell command -v aarch64-linux-gnu-gcc 2> /dev/null)
	TOOLCHAIN_AARCH64_UNKNOWN_LINUX_GNU := $(shell command -v aarch64-unknown-linux-gnu-gcc 2> /dev/null)
	TOOLCHAIN_AARCH64_NONE_ELF := $(shell command -v aarch64-none-elf-gcc 2> /dev/null)
	# Then check if they are defined and select the appropriate one
	ifdef TOOLCHAIN_AARCH64_LINUX_GNU
		TOOLCHAIN := aarch64-linux-gnu
	else ifdef TOOLCHAIN_AARCH64_UNKNOWN_LINUX_GNU
		TOOLCHAIN := aarch64-unknown-linux-gnu
	else ifdef TOOLCHAIN_AARCH64_NONE_ELF
		TOOLCHAIN := aarch64-none-elf
	else
		$(error "Could not find an AArch64 cross-compiler")
	endif
endif

BOARD := qemu_virt_aarch64
MICROKIT_CONFIG := debug
BUILD_DIR := build
SYSTEM_DESCRIPTION := light.system

CPU := cortex-a53
HOST_CC ?= cc
TEST_HOOKS ?= 0

CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as
MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit

PRINTF_OBJS := printf.o util.o
POLICY_OBJS := light_policy.o
PROTOCOL_OBJS := light_protocol.o
COMMAND_CODEC_OBJS := light_command_codec.o
TRANSPORT_OBJS := light_transport.o
SNAPSHOT_OBJS := light_status_snapshot.o
OUTPUT_POLICY_OBJS := light_output_policy.o
CONTROL_LOGIC_OBJS := light_control_logic.o
VEHICLE_LOGIC_OBJS := light_vehicle_state.o
RUNTIME_GUARD_OBJS := light_runtime_guard.o
FAULT_MODE_OBJS := light_fault_mode.o
GPIO_OBJS := $(PRINTF_OBJS) $(FAULT_MODE_OBJS) gpio.o
EXECUTION_PLAN_OBJS := light_execution_plan.o
LIGHTCTL_OBJS := $(PRINTF_OBJS) $(PROTOCOL_OBJS) $(EXECUTION_PLAN_OBJS) $(RUNTIME_GUARD_OBJS) $(FAULT_MODE_OBJS) lightctl.o
COMMANDIN_OBJS := $(PRINTF_OBJS) $(COMMAND_CODEC_OBJS) $(TRANSPORT_OBJS) $(SNAPSHOT_OBJS) $(FAULT_MODE_OBJS) commandin.o
FAULT_MG_OBJS := $(PRINTF_OBJS) $(PROTOCOL_OBJS) $(FAULT_MODE_OBJS) faultmg.o
SCHEDULER_OBJS := $(PRINTF_OBJS) $(PROTOCOL_OBJS) $(CONTROL_LOGIC_OBJS) $(OUTPUT_POLICY_OBJS) $(FAULT_MODE_OBJS) scheduler.o
VEHICLE_STATE_OBJS := $(PRINTF_OBJS) $(PROTOCOL_OBJS) $(VEHICLE_LOGIC_OBJS) vehicle_state.o
#VMM_OBJS := $(PRINTF_OBJS) vmm.o psci.o smc.o fault.o vgic.o global_data.o vgic_v2.o

BOARD_DIR := $(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)

IMAGES_PART_1 := gpio.elf
IMAGES_PART_2 := gpio.elf lightctl.elf
IMAGES_PART_3 := gpio.elf lightctl.elf commandin.elf
IMAGES_PART_4 := gpio.elf lightctl.elf commandin.elf faultmg.elf
IMAGES_PART_5 := gpio.elf lightctl.elf commandin.elf faultmg.elf scheduler.elf vehicle_state.elf
IMAGES_BUILD := $(IMAGES_PART_5)
LEGACY_TARGETS := part1 part2 part3 part4 part5
#IMAGES_PART_4 := serial_server.elf client.elf wordle_server.elf vmm.elf
# Note that these warnings being disabled is to avoid compilation errors while in the middle of completing each exercise part
ifeq ($(MICROKIT_CONFIG),release)
PROJECT_OPT_FLAGS := -O2 -DNDEBUG -g0
else
PROJECT_OPT_FLAGS := -O0 -g
endif

CFLAGS := -mcpu=$(CPU) -mstrict-align -nostdlib -ffreestanding $(PROJECT_OPT_FLAGS) -Wall -Wno-array-bounds -Wno-unused-variable -Wno-unused-function -Werror -I$(BOARD_DIR)/include -Ivmm/src/util -Iinclude -DBOARD_$(BOARD) -DLIGHT_ENABLE_TEST_HOOKS=$(TEST_HOOKS)
LDFLAGS := -L$(BOARD_DIR)/lib -L/usr/aarch64-linux-gnu/lib
LIBS := -lmicrokit -Tmicrokit.ld -lc -lrt

IMAGE_FILE_PART_1 = $(BUILD_DIR)/demo_part_one.img
IMAGE_FILE_PART_2 = $(BUILD_DIR)/demo_part_two.img
IMAGE_FILE_PART_3 = $(BUILD_DIR)/demo_part_three.img
IMAGE_FILE_PART_4 = $(BUILD_DIR)/demo_part_four.img
IMAGE_FILE_PART_5 = $(BUILD_DIR)/demo_part_five.img
#IMAGE_FILE_PART_4 = $(BUILD_DIR)/wordle_part_four.img
IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt
BUILD_ELFS := $(addprefix $(BUILD_DIR)/, $(IMAGES_BUILD))
CONFIG_STAMP := $(BUILD_DIR)/.microkit_config_$(MICROKIT_CONFIG)

# VMM defines
# KERNEL_IMAGE = vmm/images/linux
# DTB_IMAGE = vmm/images/linux.dtb
# INITRD_IMAGE = vmm/images/rootfs.cpio.gz

.PHONY: all build run clean debug release smoke test-policy test-runtime test-fault test-fault-transport test-integration-fault test-serial-e2e help $(LEGACY_TARGETS) legacy

all: build

build: $(IMAGE_FILE)

debug:
	$(MAKE) build MICROKIT_CONFIG=debug

release:
	$(MAKE) build MICROKIT_CONFIG=release

smoke: build
	./scripts/smoke_test.sh

test-integration-fault:
	$(MAKE) build BUILD_DIR=build-test-hooks TEST_HOOKS=1
	IMAGE_FILE=build-test-hooks/loader.img ./scripts/fault_injection_test.sh

test-serial-e2e: build
	./scripts/serial_e2e_test.sh

test-policy: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_policy.c light_policy.c -o build/test_light_policy
	./build/test_light_policy

test-protocol: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_protocol.c light_protocol.c -o build/test_light_protocol
	./build/test_light_protocol

test-command: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_command_codec.c light_command_codec.c -o build/test_light_command_codec
	./build/test_light_command_codec

test-transport: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_transport.c light_transport.c light_command_codec.c -o build/test_light_transport
	./build/test_light_transport

test-snapshot: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_status_snapshot.c light_status_snapshot.c light_fault_mode.c -o build/test_light_status_snapshot
	./build/test_light_status_snapshot

test-control: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_control_logic.c light_control_logic.c light_output_policy.c light_protocol.c -o build/test_light_control_logic
	./build/test_light_control_logic

test-vehicle: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_vehicle_state.c light_vehicle_state.c light_protocol.c -o build/test_light_vehicle_state
	./build/test_light_vehicle_state

test-execution: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_execution_plan.c light_execution_plan.c -o build/test_light_execution_plan
	./build/test_light_execution_plan

test-runtime: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_runtime_guard.c light_runtime_guard.c -o build/test_light_runtime_guard
	./build/test_light_runtime_guard

test-fault: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_fault_mode.c light_fault_mode.c light_output_policy.c light_runtime_guard.c -o build/test_light_fault_mode
	./build/test_light_fault_mode

test-fault-transport: | directories
	$(HOST_CC) -std=c11 -Wall -Werror -Iinclude tests/test_light_fault_mode_transport.c light_fault_mode.c -o build/test_light_fault_mode_transport
	./build/test_light_fault_mode_transport

directories:
	@mkdir -p $(BUILD_DIR)

$(CONFIG_STAMP): | directories
	@touch $@

run: build
	qemu-system-aarch64 -machine virt,virtualization=on \
		-cpu $(CPU) \
		-rtc base=localtime \
		-serial mon:stdio \
		-device loader,file=$(IMAGE_FILE),addr=0x70000000,cpu-num=0 \
		-m size=2G \
		-nographic \
		-netdev user,id=mynet0 \
		-device virtio-net-device,netdev=mynet0,mac=52:55:00:d1:55:01

clean:
	$(info Cleaning $(BUILD_DIR))
	$(RM) $(BUILD_DIR)/*.o
	$(RM) $(BUILD_DIR)/*.elf
	$(RM) $(IMAGE_FILE)
	$(RM) $(REPORT_FILE)
	$(RM) $(BUILD_DIR)/.microkit_config_*
	$(RM) $(BUILD_DIR)/demo_part_one.img
	$(RM) $(BUILD_DIR)/demo_part_two.img
	$(RM) $(BUILD_DIR)/demo_part_three.img
	$(RM) $(BUILD_DIR)/demo_part_four.img
	$(RM) $(BUILD_DIR)/demo_part_five.img

help:
	@echo "Recommended targets:"
	@echo "  build    Build the full project image (equivalent to legacy part5)"
	@echo "  run      Run build/loader.img with qemu-system-aarch64"
	@echo "  clean    Remove known build artifacts under build/"
	@echo "  debug    Build the full image with MICROKIT_CONFIG=debug"
	@echo "  release  Build the full image with MICROKIT_CONFIG=release"
	@echo "  smoke    Run the minimal automated smoke test"
	@echo "  test-policy Run host-side policy unit tests"
	@echo "  test-protocol Run shared-state compatibility tests"
	@echo "  test-command Run command decoding tests"
	@echo "  test-transport Run transport parser and dispatch tests"
	@echo "  test-snapshot Run unified status snapshot tests"
	@echo "  test-control Run target_output control-logic tests"
	@echo "  test-vehicle Run vehicle_state update tests"
	@echo "  test-execution Run lightctl execution diff tests"
	@echo "  test-runtime Run host-side runtime guard unit tests"
	@echo "  test-fault Run host-side fault mode tests"
	@echo "  test-fault-transport Run host-side fault mode transport tests"
	@echo "  test-integration-fault Run QEMU fault-injection integration test (test hooks enabled)"
	@echo "  test-serial-e2e Run a minimal serial end-to-end transport/query scenario"
	@echo "  help     Show this help message"
	@echo ""
	@echo "Legacy compatibility targets:"
	@echo "  all      Alias of build"
	@echo "  part1    Legacy compatibility alias, emits demo_part_one.img"
	@echo "  part2    Legacy compatibility alias, emits demo_part_two.img"
	@echo "  part3    Legacy compatibility alias, emits demo_part_three.img"
	@echo "  part4    Legacy compatibility alias, emits demo_part_four.img"
	@echo "  part5    Legacy staged build target, equivalent to build"
	@echo "  legacy   Build all legacy staged images"
	@echo ""
	@echo "Common overrides:"
	@echo "  make build MICROKIT_SDK=/path/to/microkit-sdk-2.0.1"
	@echo "  make release MICROKIT_SDK=/path/to/microkit-sdk-2.0.1"
	@echo ""
	@echo "Notes:"
	@echo "  release selects the Microkit SDK release configuration and uses -O2 -DNDEBUG -g0."
	@echo "  test-integration-fault builds into build-test-hooks/ with test-only fault injection hooks enabled."

legacy: part1 part2 part3 part4 part5

part1: $(IMAGE_FILE_PART_1)
part2: $(IMAGE_FILE_PART_2)
part3: $(IMAGE_FILE_PART_3)
part4: $(IMAGE_FILE_PART_4)
part5: build
# part4: directories $(BUILD_DIR)/vmm.elf $(IMAGE_FILE_PART_4)

$(BUILD_DIR)/%.o: %.c Makefile $(CONFIG_STAMP)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/printf.o: include/printf.c Makefile $(CONFIG_STAMP)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/util.o: vmm/src/util/util.c Makefile $(CONFIG_STAMP)
	$(CC) -c $(CFLAGS) $< -o $@

# $(BUILD_DIR)/%.o: vmm/src/%.c Makefile
# 	$(CC) -c $(CFLAGS) $< -o $@

# $(BUILD_DIR)/%.o: vmm/src/util/%.c Makefile
# 	$(CC) -c $(CFLAGS) $< -o $@

# $(BUILD_DIR)/%.o: vmm/src/vgic/%.c Makefile
# 	$(CC) -c $(CFLAGS) $< -o $@

# $(BUILD_DIR)/global_data.o: vmm/src/global_data.S $(KERNEL_IMAGE) $(INITRD_IMAGE) $(DTB_IMAGE)
# 	$(CC) -c -g -x assembler-with-cpp \
# 					-DVM_KERNEL_IMAGE_PATH=\"$(KERNEL_IMAGE)\" \
# 					-DVM_DTB_IMAGE_PATH=\"$(DTB_IMAGE)\" \
# 					-DVM_INITRD_IMAGE_PATH=\"$(INITRD_IMAGE)\" \
# 					$< -o $@

$(BUILD_DIR)/gpio.elf: $(addprefix $(BUILD_DIR)/, $(GPIO_OBJS)) $(CONFIG_STAMP)
	$(LD) $(LDFLAGS) $(filter %.o,$^) $(LIBS) -o $@

$(BUILD_DIR)/lightctl.elf: $(addprefix $(BUILD_DIR)/, $(LIGHTCTL_OBJS)) $(CONFIG_STAMP)
	$(LD) $(LDFLAGS) $(filter %.o,$^) $(LIBS) -o $@

$(BUILD_DIR)/commandin.elf: $(addprefix $(BUILD_DIR)/, $(COMMANDIN_OBJS)) $(CONFIG_STAMP)
	$(LD) $(LDFLAGS) $(filter %.o,$^) $(LIBS) -o $@

$(BUILD_DIR)/faultmg.elf: $(addprefix $(BUILD_DIR)/, $(FAULT_MG_OBJS)) $(CONFIG_STAMP)
	$(LD) $(LDFLAGS) $(filter %.o,$^) $(LIBS) -o $@

$(BUILD_DIR)/scheduler.elf: $(addprefix $(BUILD_DIR)/, $(SCHEDULER_OBJS)) $(CONFIG_STAMP)
	$(LD) $(LDFLAGS) $(filter %.o,$^) $(LIBS) -o $@

$(BUILD_DIR)/vehicle_state.elf: $(addprefix $(BUILD_DIR)/, $(VEHICLE_STATE_OBJS)) $(CONFIG_STAMP)
	$(LD) $(LDFLAGS) $(filter %.o,$^) $(LIBS) -o $@

# $(BUILD_DIR)/vmm.elf: $(addprefix $(BUILD_DIR)/, $(VMM_OBJS))
# 	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE): $(BUILD_ELFS) $(SYSTEM_DESCRIPTION) $(CONFIG_STAMP) | directories
	$(MICROKIT_TOOL) $(SYSTEM_DESCRIPTION) --search-path $(BUILD_DIR) --board $(BOARD) --config $(MICROKIT_CONFIG) -o $@ -r $(REPORT_FILE)

$(IMAGE_FILE_PART_1): $(IMAGE_FILE)
	cp $(IMAGE_FILE) $@

$(IMAGE_FILE_PART_2): $(IMAGE_FILE)
	cp $(IMAGE_FILE) $@

$(IMAGE_FILE_PART_3): $(IMAGE_FILE)
	cp $(IMAGE_FILE) $@

$(IMAGE_FILE_PART_4): $(IMAGE_FILE)
	cp $(IMAGE_FILE) $@

$(IMAGE_FILE_PART_5): $(IMAGE_FILE)
	cp $(IMAGE_FILE) $@

# $(IMAGE_FILE_PART_4): $(addprefix $(BUILD_DIR)/, $(IMAGES_PART_4)) wordle.system
# 	$(MICROKIT_TOOL) wordle.system --search-path $(BUILD_DIR) --board $(BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

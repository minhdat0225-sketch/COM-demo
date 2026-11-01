# Makefile – AUTOSAR Classic COM Demo (STM32F103)
# Build: make
# Clean: make clean

TARGET_TX = node_tx
TARGET_RX = node_rx

# Default target
all: com_demo.elf

CC     = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# STM32F103C8 (Medium-density)
CFLAGS  = -mcpu=cortex-m3 -mthumb -O0 -g3 -Wall -Wextra
CFLAGS += -ffreestanding -ffunction-sections -fdata-sections
CFLAGS += -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER -DRUN_ON_RENODE
LDFLAGS = -T stm32f103.ld --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections

# ===== Include paths =====
INC  = -Iautosar/include
INC += -Iautosar/com
INC += -Iautosar/pdur
INC += -Iautosar/canif
INC += -Iautosar/can
INC += -Iautosar/lin
INC += -Iautosar/linif
INC += -Iautosar/cantp
INC += -Iconfig
INC += -Ispl/inc
INC += -I../bsp/cmsis

# ===== Source files =====
# Application
# Bỏ file main.c cũ, các file tx_main.c và rx_main.c được chèn vào OBJS_TX/RX
SRCS  = 

# AUTOSAR COM stack
SRCS += autosar/com/Com.c # Đã dọn đường cho COPL
# SRCS += ../COM_COPL/build_c/copl_bsw_com.c
SRCS += autosar/pdur/PduR.c
SRCS += autosar/canif/CanIf.c
SRCS += autosar/can/Can.c
SRCS += autosar/lin/Lin.c
SRCS += autosar/linif/LinIf.c
SRCS += autosar/cantp/CanTp.c
SRCS += autosar/dcm/Dcm.c

# Config
SRCS += config/Com_Cfg.c
SRCS += config/CanIf_Cfg.c
SRCS += config/PduR_Cfg.c
SRCS += config/CanTp_Cfg.c

# SPL (chỉ compile các module cần thiết)
SRCS += spl/src/stm32f10x_rcc.c
SRCS += spl/src/stm32f10x_gpio.c
SRCS += spl/src/stm32f10x_can.c
SRCS += spl/src/stm32f10x_usart.c
SRCS += spl/src/stm32f10x_adc.c
SRCS += spl/src/stm32f10x_dma.c
SRCS += spl/src/stm32f10x_tim.c
SRCS += spl/src/stm32f10x_exti.c
SRCS += spl/src/stm32f10x_spi.c
SRCS += spl/src/stm32f10x_i2c.c
SRCS += spl/src/misc.c
SRCS += spl/src/system_stm32f10x.c

# Startup
ASM_SRCS = startup_stm32f103.s

# ===== Object files (chung) =====
# Không dùng file chung cho mảng có compile flag nên compile thẳng.
# Để đơn giản Makefile, ta sẽ build lại toàn bộ cho từng node với flag tương ứng.

# ===== Build rules =====
BUILD_DIR = build
APP_SRCS = $(wildcard example/*.c)

# Template tự động build cho mọi ứng dụng trong thư mục example
define MAKE_APP_TEMPLATE
# Lấy tên file ứng dụng (vd: lin_tx_demo)
APP_BASENAME_$(1) = $$(basename $$(notdir $(1)))
# File đích sẽ nằm trong build/example/tên_app
APP_TARGET_$(1) = $$(BUILD_DIR)/example/$$(APP_BASENAME_$(1))
# Đặt cờ tự động dựa trên tên app.
APP_NODE_$(1) = $$(if $$(findstring evcu_com_ecu,$$(APP_BASENAME_$(1))),-DEVCU_COM_ECU -DNODE_TX,$$(if $$(findstring evcu_diag_ecu,$$(APP_BASENAME_$(1))),-DEVCU_DIAG_ECU -DNODE_TX,$$(if $$(findstring rx,$$(APP_BASENAME_$(1))),-DNODE_RX,-DNODE_TX)))

# Danh sách Object file riêng rẽ cho từng app
OBJS_$(1) = $$(patsubst %.c,$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o,$(1)) \
            $$(patsubst %.c,$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o,$$(SRCS)) \
            $$(patsubst %.s,$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o,$$(ASM_SRCS))

$$(APP_TARGET_$(1)).elf: $$(OBJS_$(1))
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) $$^ $$(LDFLAGS) -o $$@

$$(APP_TARGET_$(1)).bin: $$(APP_TARGET_$(1)).elf
	$$(OBJCOPY) -O binary $$< $$@

$$(APP_TARGET_$(1)).hex: $$(APP_TARGET_$(1)).elf
	$$(OBJCOPY) -O ihex $$< $$@

$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o: %.c
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) $$(APP_NODE_$(1)) $$(INC) -c $$< -o $$@

$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o: %.s
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -c $$< -o $$@

ALL_TARGETS += $$(APP_TARGET_$(1)).elf $$(APP_TARGET_$(1)).bin $$(APP_TARGET_$(1)).hex
endef

$(foreach app,$(APP_SRCS),$(eval $(call MAKE_APP_TEMPLATE,$(app))))

# ===== com_demo (main.c) build rules =====
COM_DEMO_OBJS = $(patsubst %.c,$(BUILD_DIR)/com_demo/%.o,main.c) \
                $(patsubst %.c,$(BUILD_DIR)/com_demo/%.o,$(SRCS)) \
                $(patsubst %.s,$(BUILD_DIR)/com_demo/%.o,$(ASM_SRCS))

com_demo.elf: $(COM_DEMO_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════════╗"
	@echo "║     BUILD THANH CONG com_demo.elf (STM32F103)               ║"
	@echo "╚══════════════════════════════════════════════════════════════╝"
	$(SIZE) $@
	@echo ""

$(BUILD_DIR)/com_demo/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(BUILD_DIR)/com_demo/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Target to build all examples
examples: $(ALL_TARGETS)
	@echo "All examples built successfully."

# Debug target using Renode and GDB
debug: com_demo.elf
	@echo "Stopping any running Renode/GDB processes..."
	@pkill -9 -i renode || true; pkill -9 -i mono || true; lsof -ti:3333 | xargs kill -9 2>/dev/null || true
	@echo "Starting Renode simulation in background..."
	@renode --disable-xwt -e "include @scripts/stm32_com.resc" > renode_system.log 2>&1 & \
	RENODE_PID=$$! ; \
	echo "Waiting for GDB server to start on port 3333..." ; \
	for i in {1..10}; do \
		if lsof -i :3333 >/dev/null 2>&1; then \
			break; \
		fi; \
		sleep 0.5; \
	done; \
	echo "Launching GDB..." ; \
	arm-none-eabi-gdb com_demo.elf -ex "target extended-remote 127.0.0.1:3333" -ex "break main" -ex "continue" ; \
	echo "Stopping Renode (PID $$RENODE_PID)..." ; \
	kill -9 $$RENODE_PID 2>/dev/null || true

clean:
	rm -rf $(BUILD_DIR) com_demo.elf

.PHONY: all examples debug clean print_targets
print_targets:
	@echo $(ALL_TARGETS)

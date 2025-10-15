### 描述
这是从SEGGER官网获取的8.64a版本的RTT文件,并加入了jlink烧录、擦除的脚本,两个文件都在`jlinkscript`中,建议直接引用本文件为submodbule,'并在MakeFile中加入:
```makefile
# *** EOF ***

# #自动获取当前MCU的型号,仅限于STM32CubeMX生成的Makefile
# MCU_ID = $(shell echo $(LDSCRIPT) | cut -c1-11)
#由于STM32CubeMX 版本更新后采用族类ld文件,无法自动获取型号,改为采用自定义的方式

MCU_ID = 

ifeq ($(MCU_ID),)
    $(error 请配置 MCU 值)
endif

print:
	@echo "Target Chip: $(MCU_ID)"

flash: all
	@echo "Uploading to firmware..."
	sed "s/{{TARGET}}/$(TARGET)/g" ./ARM_SEGGER_RTT/jlinkscript/flash.jlink > flash.jlink
	-JLinkExe  -Device $(MCU_ID) -CommandFile flash.jlink

erase:
	@echo "Erase chip..."
	-JLinkExe  -Device $(MCU_ID) -CommandFile ./ARM_SEGGER_RTT/jlinkscript/erase.jlink

run:
	@echo "Try to run MCU"
	-JLinkExe  -Device $(MCU_ID) -if SWD -Speed 2400 -RTTTelnetPort 9999 -autoconnect 1
rtt:
	@echo "rtt..."
	while true; do sleep 1; telnet 127.0.0.1 9999; done
fr: all
	@echo "flash & run"
	$(MAKE) flash
	$(MAKE) run
rttts:
	@make rtt | ts '%H:%M:%S'
LOGDIR := logs
RTT_LOGFILE := $(LOGDIR)/$(shell date +%Y%m%d_%H%M%S).log

rttlog:
	@mkdir -p "$(LOGDIR)"
	@make rtt | ts '%Y-%m-%d %H:%M:%S' | tee "$(RTT_LOGFILE)"
.PHONY: rttlog

# ============ Flash/RAM Analysis Targets ============
# .PHONY: analyze analyze-printf analyze-symbols analyze-flash

# # 主分析命令：显示 printf 相关符号 + 按大小排序的符号表 + 内存摘要
# analyze: analyze-flash analyze-printf analyze-symbols

# # 显示 Flash/RAM 使用摘要
# analyze-flash:
# 	@echo
# 	@echo "Memory Usage Summary for $(TARGET).elf"
# 	@$(SZ) $(BUILD_DIR)/$(TARGET).elf
# 	@echo
# 	@$(PREFIX)size -A $(BUILD_DIR)/$(TARGET).elf | grep -E "\.(text|data|bss)" | \
# 		awk '{printf "  \033[0;34m%-8s\033[0m %6d bytes (%.1f KB)\n", $$1, $$2, $$2/1024}'

# # 分析 printf/vsnprintf 相关符号
# analyze-printf:
# 	@echo
# 	@echo "Searching for printf/vsnprintf related symbols:"
# 	@$(PREFIX)objdump -t $(BUILD_DIR)/$(TARGET).elf 2>/dev/null | \
# 		grep -i "printf\|vsnprintf" | \
# 		sed 's/^/   /' || echo "   \033[0;32m✓ No printf/vsnprintf symbols found.\033[0m"

# # 分析最大符号（按大小排序，显示最大的20个）
# analyze-symbols:
# 	@echo
# 	@echo "Top 20 Largest Symbols by Size:"
# 	@$(PREFIX)nm --print-size -S $(BUILD_DIR)/$(TARGET).elf 2>/dev/null | \
# 		sort -k2 -g | tail -20 | \
# 		awk '{printf "   \033[0;35m%6s B\033[0m | %s\n", $$2, $$4}' || echo "   \033[0;31m✗ Failed to analyze symbols (check .elf exists)\033[0m"
```

### 目录
- [描述](#描述)
- [目录](#目录)
- [修订记录:](#修订记录)
- [1.1 更新](#11-更新)

### 修订记录:
| 文档版本 | 修订时间 | 修改内容 | 备注 |
|--|--|--|--|

---

### 1.1 更新
* v1.0.0
  1. 源自于SEGGER_RTT_V864a
  2. 添加了分级日志功能,全部开启Flash占用约7.7K
     1. 添加了浮点打印支持
     2. 拥有超时机制
     3. 拥有颜色等级区分
  3. 添加了lite等级,Flash占用约3.9K
     1. 仅保留基础打印功能
     2. 该功能开启后除浮点外的分级日志将全部关闭
* v1.0.1
  1. 添加有FPU的MCU浮点型处理逻辑,提高性能
  2. 增加浮点型NAN等特殊值的处理
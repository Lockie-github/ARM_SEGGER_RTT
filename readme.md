### Readme
这是从SEGGER官网获取的6.14版本的RTT文件,并加入了jlink烧录、擦除的脚本,两个文件都在`jlinkscript`中,建议直接引用本文件为submodbule,'并在MakeFile中加入以下代码,并修改MCU_ID为你使用的芯片:
```makefile
# *** EOF ***

#自动获取当前MCU的型号,仅限于STM32CubeMX生成的Makefile
# MCU_ID = $(shell echo $(LDSCRIPT) | cut -c1-11)
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
```
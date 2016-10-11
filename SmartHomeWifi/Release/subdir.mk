################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
INO_SRCS += \
../SmartHome.ino 

CPP_SRCS += \
../.ino.cpp 

LINK_OBJ += \
./.ino.cpp.o 

INO_DEPS += \
./SmartHome.ino.d 

CPP_DEPS += \
./.ino.cpp.d 


# Each subdirectory must supply rules for building sources it contributes
.ino.cpp.o: ../.ino.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/../../../tools/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2/bin/xtensa-lx106-elf-g++" -D__ets__ -DICACHE_FLASH -U__STRICT_ANSI__ "-I/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/tools/sdk/include" -c -Os -g -mlongcalls -mtext-section-literals -fno-exceptions -fno-rtti -falign-functions=4 -std=c++11 -MMD -ffunction-sections -fdata-sections -DF_CPU=80000000L  -DARDUINO=10609 -DARDUINO_ESP8266_ESP12 -DARDUINO_ARCH_ESP8266 -DESP8266  -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/cores/esp8266" -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/variants/nodemcu" -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/libraries/ESP8266WiFi" -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/libraries/ESP8266WiFi/src" -I"/home/felix/Arduino/libraries/EEPROMAnything" -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/libraries/EEPROM" -I"/home/felix/Arduino/libraries/DebugUtils" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/Product/WaterHeater" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/Product/WaterHeater/Configurations/eeprom" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/Product/WaterHeater/MainApp/inc" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/Product/WaterHeater/MainApp/src" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Boot/inc" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Boot/src" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Data/inc" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Data/src" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Utils/inc" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Utils/src" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -D__IN_ECLIPSE__=1 -x c++ "$<" -o "$@"  -Wall
	@echo 'Finished building: $<'
	@echo ' '

SmartHome.o: ../SmartHome.ino
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/../../../tools/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2/bin/xtensa-lx106-elf-g++" -D__ets__ -DICACHE_FLASH -U__STRICT_ANSI__ "-I/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/tools/sdk/include" -c -Os -g -mlongcalls -mtext-section-literals -fno-exceptions -fno-rtti -falign-functions=4 -std=c++11 -MMD -ffunction-sections -fdata-sections -DF_CPU=80000000L  -DARDUINO=10609 -DARDUINO_ESP8266_ESP12 -DARDUINO_ARCH_ESP8266 -DESP8266  -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/cores/esp8266" -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/variants/nodemcu" -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/libraries/ESP8266WiFi" -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/libraries/ESP8266WiFi/src" -I"/home/felix/Arduino/libraries/EEPROMAnything" -I"/home/felix/.arduino15/packages/esp8266/hardware/esp8266/2.1.0/libraries/EEPROM" -I"/home/felix/Arduino/libraries/DebugUtils" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/Product/WaterHeater" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/Product/WaterHeater/Configurations/eeprom" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/Product/WaterHeater/MainApp/inc" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/Product/WaterHeater/MainApp/src" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Boot/inc" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Boot/src" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Data/inc" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Data/src" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Utils/inc" -I"/home/felix/Projects/SmartHome/github/SmartHome/SmartHomeWifi/System/Utils/src" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -D__IN_ECLIPSE__=1 -x c++ "$<" -o "$@"  -Wall
	@echo 'Finished building: $<'
	@echo ' '



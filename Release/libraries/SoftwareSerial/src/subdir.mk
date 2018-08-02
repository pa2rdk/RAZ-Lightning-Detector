################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/libraries/SoftwareSerial/src/SoftwareSerial.cpp 

LINK_OBJ += \
./libraries/SoftwareSerial/src/SoftwareSerial.cpp.o 

CPP_DEPS += \
./libraries/SoftwareSerial/src/SoftwareSerial.cpp.d 


# Each subdirectory must supply rules for building sources it contributes
libraries/SoftwareSerial/src/SoftwareSerial.cpp.o: /Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/libraries/SoftwareSerial/src/SoftwareSerial.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	"/Applications/Eclipse/arduino-oxygen/Eclipse.app/Contents/MacOS//arduinoPlugin/packages/arduino/tools/avr-gcc/4.9.2-atmel3.5.4-arduino2/bin/avr-g++" -c -g -Os -Wall -Wextra -std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics -flto -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10802 -DARDUINO_AVR_NANO -DARDUINO_ARCH_AVR   -I"/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/cores/arduino" -I"/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/variants/eightanaloginputs" -I"/Users/robertdekok/Library/Mobile Documents/com~apple~CloudDocs/ARDUINO/libraries/Adafruit-ST7735-Library-master" -I"/Users/robertdekok/Library/Mobile Documents/com~apple~CloudDocs/ARDUINO/libraries/Adafruit_GFX_Library" -I"/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/libraries/EEPROM" -I"/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/libraries/EEPROM/src" -I"/Users/robertdekok/Library/Mobile Documents/com~apple~CloudDocs/ARDUINO/libraries/RDKESP8266" -I"/Users/robertdekok/Library/Mobile Documents/com~apple~CloudDocs/ARDUINO/libraries/sen39001_arduino_i2c_r00" -I"/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/libraries/SoftwareSerial" -I"/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/libraries/SoftwareSerial/src" -I"/Users/robertdekok/Library/Mobile Documents/com~apple~CloudDocs/ARDUINO/libraries/I2C" -I"/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/libraries/SPI" -I"/Applications/Arduino-1.8.3.app/Contents/Java/hardware/arduino/avr/libraries/SPI/src" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -D__IN_ECLIPSE__=1 -x c++ "$<"  -o  "$@"
	@echo 'Finished building: $<'
	@echo ' '



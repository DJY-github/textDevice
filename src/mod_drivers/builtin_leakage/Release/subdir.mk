################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../im_dm_devices_modbus.cpp \
../modbus_packet.cpp 

OBJS += \
./im_dm_devices_modbus.o \
./modbus_packet.o 

CPP_DEPS += \
./im_dm_devices_modbus.d \
./modbus_packet.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-none-linux-gnueabi-g++ -I"/home/hz/xgapps/imds/imdm/include" -I"/home/hz/xgapps/imds/imutil/include" -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



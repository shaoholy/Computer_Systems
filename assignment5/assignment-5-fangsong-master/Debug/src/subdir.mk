################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/file_util.c \
../src/http_methods.c \
../src/http_request.c \
../src/http_server.c \
../src/http_util.c \
../src/mime_util.c \
../src/network_util.c \
../src/properties.c \
../src/thpool.c \
../src/time_util.c 

OBJS += \
./src/file_util.o \
./src/http_methods.o \
./src/http_request.o \
./src/http_server.o \
./src/http_util.o \
./src/mime_util.o \
./src/network_util.o \
./src/properties.o \
./src/thpool.o \
./src/time_util.o 

C_DEPS += \
./src/file_util.d \
./src/http_methods.d \
./src/http_request.d \
./src/http_server.d \
./src/http_util.d \
./src/mime_util.d \
./src/network_util.d \
./src/properties.d \
./src/thpool.d \
./src/time_util.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



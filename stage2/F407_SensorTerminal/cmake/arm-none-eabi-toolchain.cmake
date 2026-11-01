set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(CMAKE_C_COMPILER arm-none-eabi-gcc REQUIRED)
find_program(CMAKE_ASM_COMPILER arm-none-eabi-gcc REQUIRED)
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(CMAKE_SIZE arm-none-eabi-size REQUIRED)

get_filename_component(ARM_GCC_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(ARM_GCC_ROOT "${ARM_GCC_BIN_DIR}/../arm-none-eabi" ABSOLUTE)
set(CMAKE_FIND_ROOT_PATH "${ARM_GCC_ROOT}")

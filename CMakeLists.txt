cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project("Linux Kernel Module with CLion IDE support / CMake" VERSION 0.1.0 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(KernelHeaders REQUIRED)


# find MODULE_LICENSE("GPL"), MODULE_AUTHOR() etc.
# thanks to "merseyviking" from stack overflow
add_definitions(-D__KERNEL__ -DMODULE)


add_executable(phone_handbook_module
        phone_module_main.c
        phone_book.c
)
add_executable(keyboard_module keyboard_main.c)

target_include_directories("phone_handbook_module" PRIVATE ${KERNELHEADERS_INCLUDE_DIRS})
target_include_directories("keyboard_module" PRIVATE ${KERNELHEADERS_INCLUDE_DIRS})


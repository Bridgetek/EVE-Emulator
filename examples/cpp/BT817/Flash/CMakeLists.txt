cmake_minimum_required(VERSION 3.15)
project(BT817FlashExample LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT WIN32)
    message(FATAL_ERROR "The EVE emulator release package supports this example on Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The EVE emulator release package requires an x64 build.")
endif()

set(REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(EMULATOR_INCLUDE "${REPOSITORY_ROOT}/include")
set(EMULATOR_LIBRARY "${REPOSITORY_ROOT}/lib/bt8xxemu.lib")
set(RUNTIME_FILES
    "${REPOSITORY_ROOT}/bin/bt8xxemu.dll"
    "${REPOSITORY_ROOT}/bin/btflashemu.dll"
    "${REPOSITORY_ROOT}/bin/zlib.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/flash-817-default.bin")

foreach(REQUIRED_FILE IN LISTS RUNTIME_FILES)
    if(NOT EXISTS "${REQUIRED_FILE}")
        message(FATAL_ERROR "Required runtime file is missing: ${REQUIRED_FILE}")
    endif()
endforeach()

if(NOT EXISTS "${EMULATOR_LIBRARY}")
    message(FATAL_ERROR "Required import library is missing: ${EMULATOR_LIBRARY}")
endif()

add_executable(BT817FlashExample
    main.cpp
    eve_flash.cpp
    eve_flash.h)
target_compile_features(BT817FlashExample PRIVATE cxx_std_17)
target_compile_definitions(BT817FlashExample PRIVATE UNICODE _UNICODE)
target_include_directories(BT817FlashExample PRIVATE "${EMULATOR_INCLUDE}")
target_link_libraries(BT817FlashExample PRIVATE "${EMULATOR_LIBRARY}")

foreach(RUNTIME_FILE IN LISTS RUNTIME_FILES)
    add_custom_command(TARGET BT817FlashExample POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RUNTIME_FILE}" "$<TARGET_FILE_DIR:BT817FlashExample>"
        VERBATIM)
endforeach()

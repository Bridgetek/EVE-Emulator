cmake_minimum_required(VERSION 3.15)
project(BT820SDCardExample LANGUAGES CXX)

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
set(SD_ASSET "${CMAKE_CURRENT_SOURCE_DIR}/sd/bicycle.anim.reloc")
set(RUNTIME_FILES
    "${REPOSITORY_ROOT}/bin/bt8xxemu.dll"
    "${REPOSITORY_ROOT}/bin/libz.dll")

foreach(REQUIRED_FILE IN LISTS RUNTIME_FILES)
    if(NOT EXISTS "${REQUIRED_FILE}")
        message(FATAL_ERROR "Required runtime file is missing: ${REQUIRED_FILE}")
    endif()
endforeach()

if(NOT EXISTS "${EMULATOR_LIBRARY}")
    message(FATAL_ERROR "Required import library is missing: ${EMULATOR_LIBRARY}")
endif()

if(NOT EXISTS "${SD_ASSET}")
    message(FATAL_ERROR "Required SD-card asset is missing: ${SD_ASSET}")
endif()

add_executable(BT820SDCardExample
    main.cpp
    eve_sdcard.cpp
    eve_sdcard.h)
target_compile_features(BT820SDCardExample PRIVATE cxx_std_17)
target_compile_definitions(BT820SDCardExample PRIVATE UNICODE _UNICODE)
target_include_directories(BT820SDCardExample PRIVATE "${EMULATOR_INCLUDE}")
target_link_libraries(BT820SDCardExample PRIVATE "${EMULATOR_LIBRARY}")
if(MSVC)
    target_compile_options(BT820SDCardExample PRIVATE /W4)
endif()

foreach(RUNTIME_FILE IN LISTS RUNTIME_FILES)
    add_custom_command(TARGET BT820SDCardExample POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RUNTIME_FILE}" "$<TARGET_FILE_DIR:BT820SDCardExample>"
        VERBATIM)
endforeach()

add_custom_command(TARGET BT820SDCardExample POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:BT820SDCardExample>/sd"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SD_ASSET}"
            "$<TARGET_FILE_DIR:BT820SDCardExample>/sd/bicycle.anim.reloc"
    VERBATIM)

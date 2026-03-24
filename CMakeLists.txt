cmake_minimum_required(VERSION 3.15)
project(PictoByteConverter VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Options
option(BUILD_CLI           "Build the command-line executable"            ON)
option(ENABLE_NATIVE_ARCH  "Enable native CPU SIMD (AVX2 / -march=native)" ON)

#Compiler Flags
if(MSVC)
    add_compile_options(/W4 /O2 /MP)
    if(ENABLE_NATIVE_ARCH)
        add_compile_options(/arch:AVX2)
    endif()
else()
    add_compile_options(-Wall -Wextra -O3)
    if(ENABLE_NATIVE_ARCH)
        add_compile_options(-march=native)
    endif()
endif()

#Shared library: pictobyte
set(LIB_SOURCES
    src/API.cpp
    src/Encoder.cpp
    src/Decoder.cpp
)

add_library(pictobyte_obj OBJECT ${LIB_SOURCES})
target_compile_definitions(pictobyte_obj PRIVATE PICTOBYTE_EXPORTS)

target_include_directories(pictobyte_obj PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/headers>
)

add_library(pictobyte SHARED $<TARGET_OBJECTS:pictobyte_obj>)

# Include directories
target_include_directories(pictobyte PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/headers>
)

# On Windows, link with system libraries
if(WIN32)
    target_link_libraries(pictobyte PRIVATE kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32)
endif()

# Set output names and directories
set_target_properties(pictobyte PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

#Command-line tool: pictobyte_cli
if(BUILD_CLI)
    add_executable(pictobyte_cli cli/main.cpp)

    if(WIN32)
        add_library(pictobyte_static STATIC $<TARGET_OBJECTS:pictobyte_obj>)
        target_include_directories(pictobyte_static PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/headers>
        )
        target_compile_definitions(pictobyte_cli PRIVATE PICTOBYTE_STATIC)
        target_link_libraries(pictobyte_static PRIVATE kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32)

        # Keep the statically linked CLI, but always build the shared DLL too.
        add_dependencies(pictobyte_cli pictobyte)
        target_link_libraries(pictobyte_cli PRIVATE pictobyte_static)

        if(NOT MSVC)
            target_link_options(pictobyte_cli PRIVATE -static -static-libgcc -static-libstdc++)
        endif()
    else()
        target_link_libraries(pictobyte_cli PRIVATE pictobyte)
    endif()
    
    # Ensure CLI goes to bin/ on all supported platforms
    set_target_properties(pictobyte_cli PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

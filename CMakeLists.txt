cmake_minimum_required(VERSION 3.15)
project(PictoByteConverter VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Options ──────────────────────────────────────────────────────────────────
option(BUILD_CLI "Build the command-line executable" ON)

# ─── Compiler Flags ───────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /O2 /MP)
else()
    add_compile_options(-Wall -Wextra -O3)
endif()

# ─── Shared library: pictobyte ────────────────────────────────────────────────
set(LIB_SOURCES
    src/API.cpp
    src/Encoder.cpp
    src/Decoder.cpp
)

add_library(pictobyte SHARED ${LIB_SOURCES})

# When building the DLL, export symbols; consumers see dllimport automatically.
target_compile_definitions(pictobyte PRIVATE PICTOBYTE_EXPORTS)

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
if(WIN32)
    set_target_properties(pictobyte PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
endif()

# ─── Command-line tool: pictobyte_cli ─────────────────────────────────────────
if(BUILD_CLI)
    add_executable(pictobyte_cli cli/main.cpp)
    target_link_libraries(pictobyte_cli PRIVATE pictobyte)
    
    # Ensure CLI also goes to bin/
    if(WIN32)
        set_target_properties(pictobyte_cli PROPERTIES 
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
    endif()
endif()

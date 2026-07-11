cmake_minimum_required(VERSION 3.11)
include(FetchContent)
project(ffl-raylib-samples LANGUAGES C)

option(USE_RAYLIB_WITH_RGFW "When building raylib, use a specific version supporting RGFW. This is NEEDED for Windows XP support." OFF)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use Debug as default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Configure the target binary to build outside of the build directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../") # Main source directory.

# ------------------ Compiler Settings -------------------

if(NOT MSVC)
    # - Ignore unused parameters from callbacks.
    # - Ignore pointer/int casting from pTexture.
    # - Ignore warnings about C23 extensions, namely static_assert, used in FFL.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-unused-parameter -Wno-c23-extensions")
endif()

# -------------------- Dependencies --------------------

# raylib -----------------------------------
# https://github.com/pongasoft/re-edit/blob/73158b3c5536a0a5d43f32a1b0a3423ebe8118d7/CMakeLists.txt#L65

# Fetch/build raylib locally. Disable examples.
set(FETCHCONTENT_QUIET OFF)
set(BUILD_EXAMPLES OFF CACHE INTERNAL "")

if(USE_RAYLIB_WITH_RGFW)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib
        GIT_SHALLOW 1
        # THIS commit is required because
        # - the repo uses pre-6.0 skeletal animation logic
        # - this commit supports RGFW unlike 5.5
        GIT_TAG 703bd9caab50b139428cea1aaff9974ebee5742e
    )
else()
    set(RAYLIB_VERSION 5.5)
    # This will ACTUALLY find_package() before downloading it.
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.zip
        FIND_PACKAGE_ARGS ${RAYLIB_VERSION} EXACT
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
    )
endif()

if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
    FetchContent_MakeAvailable(raylib)
endif()
if(DEFINED raylib_SOURCE_DIR) # ONLY works if we are building raylib.
    target_compile_definitions(raylib PRIVATE SUPPORT_GPU_SKINNING=1)
endif()

# raygui -----------------------------------
FetchContent_Declare(
    raygui_h
    URL https://raw.githubusercontent.com/raysan5/raygui/refs/tags/4.0/src/raygui.h
    DOWNLOAD_NAME raygui.h
    DOWNLOAD_NO_EXTRACT ON
)
FetchContent_MakeAvailable(raygui_h)

# FFL Project ------------------------------

set(FFL_BUILD_SHARED OFF)
set(FFL_BUILD_STATIC ON)  # Build FFL as a static library.
set(FFL_NO_DATABASE_DEFAULT ON) # Exclude default/guest Miis.
#set(FFL_NO_NINTEXUTILS ON)

add_subdirectory(FFL-Testing/ffl) # Use FFL's CMakeLists.

# C++ Support ------------------------------

# This file includes stubs for C++ language functions.
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/cppsupport.cpp CONTENT
"#include <stdlib.h>
extern \"C\" void __cxa_pure_virtual() { abort(); }
void* operator new(size_t s) { return malloc(s); }
void* operator new[](size_t s) { return malloc(s); }
void operator delete(void* p) { free(p); }
void operator delete(void* p, size_t sz) { (void)sz; free(p); }
void operator delete[](void* p) { free(p); }
void operator delete[](void* p, size_t sz) { (void)sz; free(p); }
")
add_library(cppsupport STATIC ${CMAKE_BINARY_DIR}/cppsupport.cpp)
set_source_files_properties(${CMAKE_BINARY_DIR}/cppsupport.cpp PROPERTIES LANGUAGE C)

# Linking Options --------------------------

# Disable features requiring libstdc++.
if(CMAKE_C_COMPILER_ID MATCHES "MSVC" OR (CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_C_SIMULATE_ID MATCHES "MSVC" AND CMAKE_C_COMPILER_FRONTEND_VARIANT MATCHES "MSVC"))
    set(COMPILE_OPTIONS_FOR_NO_LIBSTDCXX /GR- /EHsc)
else()
    set(COMPILE_OPTIONS_FOR_NO_LIBSTDCXX -fno-exceptions -fno-rtti -fno-threadsafe-statics)
endif()
target_compile_options(ffl PRIVATE ${COMPILE_OPTIONS_FOR_NO_LIBSTDCXX})
target_compile_options(cppsupport PRIVATE ${COMPILE_OPTIONS_FOR_NO_LIBSTDCXX})

# Common link libraries
set(COMMON_LIBRARIES
    raylib
    ffl
    cppsupport
)
if (NOT WIN32)
    # Add libm for non-Windows targets.
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} m)
endif()

if(APPLE)
    # Checks if OSX and links appropriate frameworks (Only required on MacOS)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# -------------------- Common Settings --------------------

# Common compile definitions
set(COMMON_DEFS
    FFL_NO_RIO
    SUPPORT_TRACELOG
)

# Common include directories
set(COMMON_INCLUDES
    #${raylib_INCLUDE_DIRS}
    # Add RIO includes for glad.
    ${FFL_WITH_RIO}/include/
)

message(STATUS "includes: ${COMMON_INCLUDES}")

# Emscripten-specific configuration
if(EMSCRIPTEN)
    set(COMMON_DEFS ${COMMON_DEFS} PLATFORM_WEB GRAPHICS_API_OPENGL_ES2)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include GLES2/gl2.h")
else()
    set(COMMON_DEFS ${COMMON_DEFS} PLATFORM_DESKTOP GRAPHICS_API_OPENGL_33)
endif()

# -------------------- Targets --------------------

add_executable(ffl_raylib_shader_basic ffl_raylib_shader_basic.c)
target_include_directories(ffl_raylib_shader_basic PRIVATE ${COMMON_INCLUDES})
target_link_libraries(ffl_raylib_shader_basic PRIVATE ${COMMON_LIBRARIES})
target_compile_definitions(ffl_raylib_shader_basic PRIVATE ${COMMON_DEFS})

add_executable(ffl_raylib_shader_fflshader ffl_raylib_shader_fflshader.c)
target_include_directories(ffl_raylib_shader_fflshader PRIVATE
    ${COMMON_INCLUDES}
    ${raygui_h_SOURCE_DIR})
target_link_libraries(ffl_raylib_shader_fflshader PRIVATE ${COMMON_LIBRARIES})
target_compile_definitions(ffl_raylib_shader_fflshader PRIVATE ${COMMON_DEFS})

# -------------------- Emscripten --------------------

if(EMSCRIPTEN)
    set(EM_FLAGS
        -sUSE_ZLIB=1 -sALLOW_MEMORY_GROWTH
        -sASYNCIFY -sEXPORTED_RUNTIME_METHODS=ccall
        --preload-file ../FFLResHigh.dat@FFLResHigh.dat
    )
    string(REPLACE ";" " " EM_FLAGS_STR "${EM_FLAGS}")

    foreach(TARGET ffl_raylib_shader_basic ffl_raylib_shader_fflshader)
        set_target_properties(${TARGET} PROPERTIES
            SUFFIX ".html"
            LINK_FLAGS "${EM_FLAGS_STR}"
        )
    endforeach()
endif()

# -------------------- Summary --------------------

message(STATUS "ffl-raylib-samples: Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ffl-raylib-samples: Raylib: ${RAYLIB_LIBRARIES}")
if(EMSCRIPTEN)
    message(STATUS "ffl-raylib-samples: Building for Emscripten/WebGL")
endif()

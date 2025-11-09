cmake_minimum_required(VERSION 3.15)
project(ffl-raylib-samples LANGUAGES C CXX)

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

# raylib
if(raylib_DIR) # raylib's CMake build directory
    # Direct import from build directory
    add_library(raylib STATIC IMPORTED)
    set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION "${raylib_DIR}/libraylib.a"
        INTERFACE_INCLUDE_DIRECTORIES "${raylib_DIR}/include"
    )
else()
    find_package(raylib REQUIRED)
endif()

# FFL Project ----------------------------

set(FFL_BUILD_SHARED OFF)
set(FFL_BUILD_STATIC ON)  # Build FFL as a static library.
set(FFL_BUILD_EMSCRIPTEN_MODULE OFF) # Needed for Emscripten compatibility, when importing the lib
#set(FFL_NO_NINTEXUTILS ON)

add_subdirectory(FFL-Testing/ffl) # Use FFL's CMakeLists.

# Common link libraries
set(COMMON_LIBRARIES
    raylib
    ffl
)

if(APPLE)
    # Checks if OSX and links appropriate frameworks (Only required on MacOS)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# -------------------- Common Settings --------------------

# Common compile definitions
set(COMMON_DEFS
    FFL_USE_TEXTURE_CALLBACK
    SUPPORT_TRACELOG
)

# Common include directories
set(COMMON_INCLUDES
    ${raylib_INCLUDE_DIRS}
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
target_include_directories(ffl_raylib_shader_fflshader PRIVATE ${COMMON_INCLUDES})
target_link_libraries(ffl_raylib_shader_fflshader PRIVATE ${COMMON_LIBRARIES})
target_compile_definitions(ffl_raylib_shader_fflshader PRIVATE ${COMMON_DEFS})

# -------------------- Emscripten --------------------

if(EMSCRIPTEN)
    set(EM_FLAGS
        -sUSE_GLFW=3 -sUSE_ZLIB=1 -sALLOW_MEMORY_GROWTH
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

# Summary
message(STATUS "ffl-raylib-samples: Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ffl-raylib-samples: Raylib: ${RAYLIB_LIBRARIES}")
if(EMSCRIPTEN)
    message(STATUS "ffl-raylib-samples: Building for Emscripten/WebGL")
endif()

# SPDX-FileCopyrightText: 2024 Crow Translate contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# FindONNXRuntime.cmake
# 
# Finds ONNX Runtime using system packages first, then falls back to static linking
# Handles platform-specific library configurations and dependencies
#
# Usage: find_package(ONNXRuntime 1.22.0 REQUIRED)
#
# Variables set:
#   ONNXRuntime_FOUND - True if ONNX Runtime found
#   ONNXRuntime_INCLUDE_DIRS - Include directories
#   ONNXRuntime_LIBRARIES - Libraries to link (for dynamic linking)
#   ONNXRuntime_USE_STATIC - True if using static linking fallback

# Get version from find_package() call
set(ONNX_VERSION ${ONNXRuntime_FIND_VERSION})
if(NOT ONNX_VERSION)
    set(ONNX_VERSION "1.22.0")  # fallback
endif()

# First try to find ONNX Runtime via pkg-config or system paths
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_ONNXRUNTIME QUIET libonnxruntime)
endif()

# Try to find onnxruntime directly if pkg-config didn't work
find_path(ONNXRuntime_INCLUDE_DIR onnxruntime_cxx_api.h
    PATHS 
        ${PC_ONNXRUNTIME_INCLUDE_DIRS}
        /usr/include/onnxruntime 
        /usr/local/include/onnxruntime
)

find_library(ONNXRuntime_LIBRARY onnxruntime
    PATHS 
        ${PC_ONNXRUNTIME_LIBRARY_DIRS}
        /usr/lib 
        /usr/local/lib
)

if(ONNXRuntime_INCLUDE_DIR AND ONNXRuntime_LIBRARY)
    set(ONNXRuntime_FOUND TRUE)
    set(ONNXRuntime_INCLUDE_DIRS ${ONNXRuntime_INCLUDE_DIR})
    set(ONNXRuntime_LIBRARIES ${ONNXRuntime_LIBRARY})
    set(ONNXRuntime_USE_STATIC FALSE)
    message(STATUS "Found system ONNX Runtime: ${ONNXRuntime_LIBRARIES}")
    
    # For compatibility with old variable names
    set(ONNXRUNTIME_FOUND TRUE)
    set(ONNXRUNTIME_INCLUDE_DIRS ${ONNXRuntime_INCLUDE_DIRS})
    set(ONNXRUNTIME_LIBRARIES ${ONNXRuntime_LIBRARIES})
    set(ONNXRUNTIME_USE_DYNAMIC TRUE)
    
    return()
endif()

# Static linking fallback configuration
message(STATUS "System ONNX Runtime not found, falling back to static linking")
set(ONNXRuntime_USE_STATIC TRUE)

# Set ONNX Runtime root path for static libraries
if(NOT DEFINED ONNXRUNTIME_ROOT_PATH)
    if(WIN32)
        set(ONNXRUNTIME_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/onnxruntime-win-x64-${ONNX_VERSION}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(ONNXRUNTIME_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/onnxruntime-freebsd-x64-${ONNX_VERSION}")
    else() # Linux
        set(ONNXRUNTIME_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/onnxruntime-linux-x64-${ONNX_VERSION}")
    endif()
endif()

# Function to configure static linking for ONNX Runtime
function(configure_onnxruntime_static TARGET_NAME)
    if(WIN32)
        # Use a link script that discovers libraries at build time (Windows)
        set(LINK_SCRIPT "${CMAKE_BINARY_DIR}/onnx_link_script_win.txt")
        set(BUILD_DIR "${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime_build-build")
        
        # Create a script that generates the Windows link script at build time
        set(GENERATE_LINK_SCRIPT "${CMAKE_BINARY_DIR}/generate_onnx_link_win.cmake")
        file(WRITE ${GENERATE_LINK_SCRIPT} "
# Set CMake policies to avoid warnings
cmake_policy(SET CMP0009 NEW)

# Discover all ONNX Runtime libraries at build time (Windows .lib files)
file(GLOB_RECURSE ALL_LIBS \"${BUILD_DIR}/*.lib\")
list(FILTER ALL_LIBS INCLUDE REGEX \".*/((onnxruntime_|re2|onnx|protobuf|absl_|cpuinfo).*|libprotobuf)\\\\.lib$\")
list(FILTER ALL_LIBS EXCLUDE REGEX \".*protobuf-lite.*\")

# Order libraries: common first, session last
set(ORDERED_LIBS)
set(SESSION_LIB)
set(COMMON_LIB)

foreach(lib IN LISTS ALL_LIBS)
    if(lib MATCHES \"onnxruntime_session\")
        set(SESSION_LIB \${lib})
    elseif(lib MATCHES \"onnxruntime_common\")
        set(COMMON_LIB \${lib})
    else()
        list(APPEND ORDERED_LIBS \${lib})
    endif()
endforeach()

if(COMMON_LIB)
    list(PREPEND ORDERED_LIBS \${COMMON_LIB})
endif()
if(SESSION_LIB)
    list(APPEND ORDERED_LIBS \${SESSION_LIB})
endif()

# Write link script with one library per line (Windows doesn't need --whole-archive)
file(WRITE \"${LINK_SCRIPT}\" \"\")
foreach(lib IN LISTS ORDERED_LIBS)
    file(APPEND \"${LINK_SCRIPT}\" \"\${lib}\\n\")
endforeach()
message(STATUS \"Generated Windows link script with \${ORDERED_LIBS}\")
")

        # Add custom command to generate link script
        add_custom_command(
            OUTPUT ${LINK_SCRIPT}
            COMMAND ${CMAKE_COMMAND} -P ${GENERATE_LINK_SCRIPT}
            DEPENDS onnxruntime_ready
            COMMENT "Generating ONNX Runtime Windows link script"
        )
        
        # Add custom target to ensure link script is created
        add_custom_target(onnx_link_script_win
            DEPENDS ${LINK_SCRIPT}
        )
        
        # Add dependency and use the link script
        add_dependencies(${TARGET_NAME} onnx_link_script_win)
        
        # Use response file and add Windows system libraries
        target_link_options(${TARGET_NAME} PRIVATE "@${LINK_SCRIPT}")
        target_link_libraries(${TARGET_NAME} PRIVATE
            Shlwapi.lib Advapi32.lib Shell32.lib User32.lib Bcrypt.lib
        )
        
        target_compile_definitions(${TARGET_NAME} PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        )
        
    else() # Linux and FreeBSD
        # Check for lib64 vs lib directory
        if(EXISTS ${ONNXRUNTIME_ROOT_PATH}/lib64)
            set(ONNX_LIB_DIR ${ONNXRUNTIME_ROOT_PATH}/lib64)
        else()
            set(ONNX_LIB_DIR ${ONNXRUNTIME_ROOT_PATH}/lib)
        endif()
        
        # Use individual libraries (find all available static libraries)
        message(STATUS "Using individual ONNX Runtime static libraries")
        
        # Create an interface library that will be populated after build
        add_library(onnxruntime_discovered INTERFACE)
        
        # Create a custom target that discovers libraries after ONNX Runtime is built
        set(BUILD_DIR "${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime_build-build")
        set(DISCOVERY_SCRIPT "${CMAKE_BINARY_DIR}/discover_onnx_libs.cmake")
        
        # Write the discovery script
        file(WRITE ${DISCOVERY_SCRIPT} "
# Discover all relevant static libraries
file(GLOB_RECURSE ALL_LIBS \"${BUILD_DIR}/*.a\")

# Filter for relevant libraries
list(FILTER ALL_LIBS INCLUDE REGEX \".*/lib(onnxruntime_|re2|onnx|protobuf|absl_|cpuinfo).*\\\\.a$\")
list(FILTER ALL_LIBS EXCLUDE REGEX \".*protobuf-lite.*\")

# Basic ordering: common first, session last
set(ORDERED_LIBS)
set(SESSION_LIB)
set(COMMON_LIB)

foreach(lib IN LISTS ALL_LIBS)
    if(lib MATCHES \"libonnxruntime_session\")
        set(SESSION_LIB \\\${lib})
    elseif(lib MATCHES \"libonnxruntime_common\") 
        set(COMMON_LIB \\\${lib})
    else()
        list(APPEND ORDERED_LIBS \\\${lib})
    endif()
endforeach()

# Put common first, session last
if(COMMON_LIB)
    list(PREPEND ORDERED_LIBS \\\${COMMON_LIB})
endif()
if(SESSION_LIB)
    list(APPEND ORDERED_LIBS \\\${SESSION_LIB})
endif()

message(STATUS \"Discovered \\\${ORDERED_LIBS}\")

# Write library list to a CMake file
set(LIB_FILE \"${CMAKE_BINARY_DIR}/onnx_discovered_libs.cmake\")
file(WRITE \\\${LIB_FILE} \"set(DISCOVERED_ONNX_LIBS \\\"\\\${ORDERED_LIBS}\\\")\")
")

        # Add custom command to run discovery after build
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/onnx_libs_ready.stamp
            COMMAND ${CMAKE_COMMAND} -P ${DISCOVERY_SCRIPT}
            COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/onnx_libs_ready.stamp
            DEPENDS onnxruntime_ready
            COMMENT "Discovering ONNX Runtime static libraries"
        )
        
        # Custom target that depends on discovery
        add_custom_target(onnx_discovery_complete
            DEPENDS ${CMAKE_BINARY_DIR}/onnx_libs_ready.stamp
        )
        
        # For the immediate link, we'll need to set ONNXRUNTIME_STATIC_LIBS to the discovered ones
        # This is a chicken-and-egg problem, so let's use a different approach...
        set(ONNXRUNTIME_STATIC_LIBS)
        
        message(STATUS "Using ONNX Runtime libraries from build directory and dependencies")
        
        list(LENGTH ONNXRUNTIME_STATIC_LIBS LIB_COUNT)
        message(STATUS "Found ${LIB_COUNT} ONNX Runtime and dependency static libraries")
        
        # Platform-specific system libraries
        if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
            set(SYSTEM_LIBS
                pthread
                m
                dl
                rt
                execinfo  # FreeBSD-specific
            )
        else() # Linux
            set(SYSTEM_LIBS
                pthread
                m
                dl
                rt
                z  # zlib for protobuf gzip support
                stdc++fs  # For filesystem support on older GCC
            )
        endif()
        
        # Use a link script that discovers libraries at build time
        set(LINK_SCRIPT "${CMAKE_BINARY_DIR}/onnx_link_script.txt")
        set(BUILD_DIR "${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime_build-build")
        
        # Create a script that generates the link script at build time
        set(GENERATE_LINK_SCRIPT "${CMAKE_BINARY_DIR}/generate_onnx_link.cmake")
        file(WRITE ${GENERATE_LINK_SCRIPT} "
# Set CMake policies to avoid warnings
cmake_policy(SET CMP0009 NEW)

# Discover all ONNX Runtime libraries at build time
file(GLOB_RECURSE ALL_LIBS \"${BUILD_DIR}/*.a\")
list(FILTER ALL_LIBS INCLUDE REGEX \".*/lib(onnxruntime_|re2|onnx|protobuf|absl_|cpuinfo).*\\\\.a$\")
list(FILTER ALL_LIBS EXCLUDE REGEX \".*protobuf-lite.*\")

# Order libraries: common first, session last
set(ORDERED_LIBS)
set(SESSION_LIB)
set(COMMON_LIB)

foreach(lib IN LISTS ALL_LIBS)
    if(lib MATCHES \"libonnxruntime_session\")
        set(SESSION_LIB \${lib})
    elseif(lib MATCHES \"libonnxruntime_common\")
        set(COMMON_LIB \${lib})
    else()
        list(APPEND ORDERED_LIBS \${lib})
    endif()
endforeach()

if(COMMON_LIB)
    list(PREPEND ORDERED_LIBS \${COMMON_LIB})
endif()
if(SESSION_LIB)
    list(APPEND ORDERED_LIBS \${SESSION_LIB})
endif()

# Write link script with one library per line
file(WRITE \"${LINK_SCRIPT}\" \"-Wl,--whole-archive\\n\")
foreach(lib IN LISTS ORDERED_LIBS)
    file(APPEND \"${LINK_SCRIPT}\" \"\${lib}\\n\")
endforeach()
file(APPEND \"${LINK_SCRIPT}\" \"-Wl,--no-whole-archive\\n\")
message(STATUS \"Generated link script with \${ORDERED_LIBS}\")
")

        # Add custom command to generate link script
        add_custom_command(
            OUTPUT ${LINK_SCRIPT}
            COMMAND ${CMAKE_COMMAND} -P ${GENERATE_LINK_SCRIPT}
            DEPENDS onnxruntime_ready
            COMMENT "Generating ONNX Runtime link script"
        )
        
        # Add custom target to ensure link script is created
        add_custom_target(onnx_link_script
            DEPENDS ${LINK_SCRIPT}
        )
        
        # Add dependency and use the link script
        add_dependencies(${TARGET_NAME} onnx_link_script)
        
        # Use response file to include discovered libraries
        target_link_options(${TARGET_NAME} PRIVATE "@${LINK_SCRIPT}")
        target_link_libraries(${TARGET_NAME} PRIVATE ${SYSTEM_LIBS})
    endif()
    
    # Include directories for static linking - always use source directory
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include
        ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include/onnxruntime
        ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include/onnxruntime/core/session
    )
    
    # Note: Static libraries don't exist at configure time - they're discovered at build time
    if(NOT ONNXRUNTIME_STATIC_LIBS)
        message(STATUS "ONNX Runtime static libraries will be discovered after build")
    endif()
    
    message(STATUS "Using static ONNX Runtime libraries from build directory")
    message(STATUS "Linking ${LIB_COUNT} libraries for target: ${TARGET_NAME}")
    
    # Debug: show first few libraries being linked
    list(LENGTH ONNXRUNTIME_STATIC_LIBS TOTAL_LIBS)
    if(TOTAL_LIBS GREATER 0)
        list(GET ONNXRUNTIME_STATIC_LIBS 0 FIRST_LIB)
        message(STATUS "First library: ${FIRST_LIB}")
        if(TOTAL_LIBS GREATER 5)
            list(SUBLIST ONNXRUNTIME_STATIC_LIBS 0 5 FIRST_FIVE)
            message(STATUS "First 5 libraries: ${FIRST_FIVE}")
        endif()
    endif()
endfunction()

# Function to build ONNX Runtime from source with telemetry disabled
function(build_onnxruntime_static)
    message(STATUS "Building ONNX Runtime v${ONNX_VERSION} from source (telemetry disabled)...")
    
    include(ExternalProject)
        
        # Build ONNX Runtime using its cmake subdirectory with telemetry disabled
        ExternalProject_Add(
            onnxruntime_build
            URL https://github.com/microsoft/onnxruntime/archive/refs/tags/v${ONNX_VERSION}.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            PREFIX ${CMAKE_BINARY_DIR}/onnxruntime-src
            SOURCE_DIR ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime
            SOURCE_SUBDIR cmake
            # Patches required because ONNX Runtime isn't designed for static linking in external projects:
            PATCH_COMMAND 
                # ONNX Runtime assumes controlled build environment, but CI hosts vary in compiler strictness
                sed -i [=[/include(\${target_name}\.cmake)/a set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -Wno-error\")\nset(CMAKE_C_FLAGS \"${CMAKE_C_FLAGS} -Wno-error\")]=] <SOURCE_DIR>/cmake/CMakeLists.txt
                # MLAS library designed for internal ONNX Runtime use, not external static linking
                && sed -i [=[49a set_target_properties(onnxruntime_mlas PROPERTIES COMPILE_WARNING_AS_ERROR OFF)]=] <SOURCE_DIR>/cmake/onnxruntime_mlas.cmake
                # Optimizer components assume warnings won't be treated as errors in external builds
                && sed -i [=[/selector_action_transformer\.cc.*COMPILE_FLAGS.*-Wno-maybe-uninitialized/a set_source_files_properties(\"$\{ONNXRUNTIME_ROOT}/core/optimizer/nchwc_transformer.cc\" PROPERTIES COMPILE_FLAGS \"-Wno-maybe-uninitialized\")]=] <SOURCE_DIR>/cmake/onnxruntime_optimizer.cmake
                # Header files assume internal build system will provide standard library includes. Gcc-13 changed standard includes.
                && sed -i [=[1i #include <cstdint>]=] <SOURCE_DIR>/onnxruntime/core/optimizer/transpose_optimization/optimizer_api.h
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=Release
                -Donnxruntime_BUILD_SHARED_LIB=OFF
                -Donnxruntime_USE_TELEMETRY=OFF
                -Donnxruntime_BUILD_UNIT_TESTS=OFF
                -Donnxruntime_RUN_ONNX_TESTS=OFF
                -DBUILD_ONNX_PYTHON=OFF
                -Donnxruntime_USE_CUDA=OFF
                -Donnxruntime_USE_DNNL=OFF
                -Donnxruntime_USE_TENSORRT=OFF
                -Donnxruntime_USE_MIGRAPHX=OFF
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                -Donnxruntime_USE_AVX=OFF
                -Donnxruntime_USE_AVX2=OFF
                -Donnxruntime_USE_AVX512=OFF
                -DCMAKE_CXX_STANDARD=17
                -DCMAKE_CXX_STANDARD_REQUIRED=ON
                -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER
                -DBUILD_SHARED_LIBS=OFF
            BUILD_COMMAND ${CMAKE_COMMAND} --build . --config Release --parallel 6 --target onnxruntime_common onnxruntime_session onnxruntime_providers onnxruntime_framework onnxruntime_mlas onnxruntime_optimizer onnxruntime_util onnxruntime_graph onnxruntime_flatbuffers onnxruntime_lora re2 cpuinfo
            INSTALL_COMMAND ""
            UPDATE_DISCONNECTED TRUE
            LOG_DOWNLOAD ON
            LOG_CONFIGURE ON
            LOG_BUILD ON
            LOG_OUTPUT_ON_FAILURE ON
        )
        
        # Set up a custom target to ensure build completion before main target
        add_custom_target(onnxruntime_ready
            DEPENDS onnxruntime_build
            COMMENT "Waiting for ONNX Runtime build to complete"
        )
        
        message(STATUS "ONNX Runtime will be built in build directory")
endfunction()

# FreeBSD has ONNX Runtime in ports - require system installation
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    if(ONNXRuntime_USE_STATIC)
        message(FATAL_ERROR "Static ONNX Runtime linking is not supported on FreeBSD. Please install the system package: pkg install misc/onnxruntime")
    endif()
    # Use system ONNX Runtime on FreeBSD
    find_package(ONNXRuntime REQUIRED)
    return()
endif()

# Set up static linking configuration and mark as found  
if(ONNXRuntime_USE_STATIC)
    set(ONNXRuntime_FOUND TRUE)
    # For compatibility with old variable names
    set(ONNXRUNTIME_FOUND TRUE)
    set(ONNXRUNTIME_USE_DYNAMIC FALSE)
    message(STATUS "ONNX Runtime configured for static linking (version ${ONNX_VERSION})")
endif()

# Use standard find_package result checking
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ONNXRuntime
    FOUND_VAR ONNXRuntime_FOUND
    REQUIRED_VARS ONNX_VERSION
    VERSION_VAR ONNX_VERSION
)

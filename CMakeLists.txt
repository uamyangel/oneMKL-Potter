cmake_minimum_required(VERSION 3.17)

# =============================================================================
# Intel Compiler Configuration
# =============================================================================
# Prefer Intel C++ Compiler (icpx/icpc) for optimal oneMKL performance
# icpx: Modern Intel compiler based on LLVM (oneAPI 2021.1+)
# icpc: Classic Intel compiler (legacy, but widely available)
#
# This will use the Intel compiler if available, otherwise fallback to default
if(NOT DEFINED CMAKE_CXX_COMPILER)
    # Try to find Intel compilers in order of preference
    find_program(ICPX_COMPILER icpx)
    find_program(ICPC_COMPILER icpc)

    if(ICPX_COMPILER)
        set(CMAKE_CXX_COMPILER "${ICPX_COMPILER}")
        message(STATUS "Using Intel LLVM-based C++ Compiler: ${ICPX_COMPILER}")
    elseif(ICPC_COMPILER)
        set(CMAKE_CXX_COMPILER "${ICPC_COMPILER}")
        message(STATUS "Using Intel Classic C++ Compiler: ${ICPC_COMPILER}")
    else()
        message(WARNING "Intel compiler not found. Will use default compiler.")
        message(WARNING "For best performance, install Intel oneAPI and source setvars.sh")
    endif()
endif()

message(STATUS "CUHK FPGA ROUTE with oneMKL Optimization")
message(STATUS "project directory ${PROJECT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_VERBOSE_MAKEFILE ON)

project(FPGA_ROUTE_ONEMKL)
set(PATH_SRC ${PROJECT_SOURCE_DIR}/src)
set(PATH_LIB ${PROJECT_SOURCE_DIR}/libs)

# =============================================================================
# Intel Compiler Optimization Flags
# =============================================================================
# Apply aggressive optimizations when using Intel compilers in Release mode
# Note: icpx (IntelLLVM) and icpc (Intel Classic) use different flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    message(STATUS "Detected Intel compiler: ${CMAKE_CXX_COMPILER_ID}")

    # Common optimizations for both icpx and icpc
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")        # Maximum optimization

    if(CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
        # icpx: Intel LLVM-based compiler (modern, uses Clang-like flags)
        message(STATUS "Using Intel LLVM compiler (icpx) optimization flags")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")  # Optimize for current CPU
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math")    # Fast floating point
        # Note: -flto for LTO, but can cause issues with large projects, so commented out
        # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
    else()
        # icpc: Intel Classic compiler (legacy, uses traditional Intel flags)
        message(STATUS "Using Intel Classic compiler (icpc) optimization flags")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -xHost")         # Optimize for current CPU
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ipo")           # Interprocedural optimization
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -no-prec-div")   # Fast division
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fp-model fast=2")  # Fast floating point
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -qopt-prefetch") # Enable prefetching
    endif()

    message(STATUS "Intel compiler optimization flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

file(GLOB_RECURSE SRC_FILES ${PATH_SRC}/*.cpp)

# =============================================================================
# Third-party Library Configuration - capnproto
# =============================================================================
# Disable capnproto tests to avoid compatibility issues with Intel compiler
# We only need the capnproto library, not its tests
set(BUILD_TESTING OFF CACHE BOOL "Disable capnproto tests" FORCE)

add_subdirectory(${PATH_LIB}/capnproto)

# Find required libraries
find_package(ZLIB REQUIRED)
find_package(Boost COMPONENTS serialization REQUIRED)

# Find Intel oneMKL
# Method 1: Try to find MKL via pkg-config or standard paths
if(DEFINED ENV{MKLROOT})
    message(STATUS "Found MKLROOT: $ENV{MKLROOT}")
    set(MKL_ROOT $ENV{MKLROOT})

    # Set MKL include directories
    set(MKL_INCLUDE_DIRS "${MKL_ROOT}/include")

    # Set MKL library directories
    if(APPLE)
        set(MKL_LIB_DIR "${MKL_ROOT}/lib")
    else()
        set(MKL_LIB_DIR "${MKL_ROOT}/lib/intel64")
    endif()

    message(STATUS "MKL Include: ${MKL_INCLUDE_DIRS}")
    message(STATUS "MKL Library Dir: ${MKL_LIB_DIR}")

    # MKL libraries for C/C++ with Intel threading
    # Using LP64 interface (32-bit integers)
    # Intel threading layer is optimal for Intel compilers (icpx/icpc)
    set(MKL_LIBRARIES
        ${MKL_LIB_DIR}/libmkl_intel_lp64${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${MKL_LIB_DIR}/libmkl_intel_thread${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${MKL_LIB_DIR}/libmkl_core${CMAKE_SHARED_LIBRARY_SUFFIX}
    )

    set(MKL_FOUND TRUE)
else()
    # Try to find MKL via CMake config
    find_package(MKL CONFIG)
    if(MKL_FOUND)
        message(STATUS "Found MKL via CMake config")
    else()
        message(WARNING "oneMKL not found. Please source oneAPI environment or set MKLROOT.")
        message(WARNING "Run: source /opt/intel/oneapi/setvars.sh")
        set(MKL_FOUND FALSE)
    endif()
endif()

add_executable(route ${SRC_FILES})

target_include_directories(route PRIVATE ${PATH_SRC})
target_include_directories(route PRIVATE ${PATH_LIB}/capnproto/c++/src/)
target_include_directories(route PRIVATE ${PATH_LIB}/interchange/)
target_include_directories(route PRIVATE ${PATH_LIB}/cxxopts/)
target_include_directories(route PRIVATE ${Boost_INCLUDE_DIRS})

# Link base libraries
target_link_libraries(route capnp)
target_link_libraries(route Boost::serialization)
target_link_libraries(route z)

# Link oneMKL if found
if(MKL_FOUND)
    if(DEFINED MKL_INCLUDE_DIRS)
        target_include_directories(route PRIVATE ${MKL_INCLUDE_DIRS})
        target_link_directories(route PRIVATE ${MKL_LIB_DIR})

        # Link MKL libraries (Intel threading layer)
        target_link_libraries(route mkl_intel_lp64 mkl_intel_thread mkl_core)

        # Link threading libraries
        find_package(Threads REQUIRED)
        target_link_libraries(route Threads::Threads)

        # Link math library
        target_link_libraries(route m dl)

        # Add compiler flags for OpenMP (required by MKL Intel threading)
        # Use -qopenmp for Intel compilers (icpx/icpc)
        # Note: -fopenmp also works but -qopenmp is the Intel standard
        if(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
            target_compile_options(route PRIVATE -qopenmp)
            target_link_options(route PRIVATE -qopenmp)
        else()
            # Fallback to GCC-style for other compilers
            target_compile_options(route PRIVATE -fopenmp)
            target_link_options(route PRIVATE -fopenmp)
        endif()

        # Define preprocessor macro to enable oneMKL code
        target_compile_definitions(route PRIVATE USE_ONEMKL)

        message(STATUS "oneMKL optimization ENABLED")
    else()
        # Using MKL from CMake config
        target_link_libraries(route MKL::MKL)
        target_compile_definitions(route PRIVATE USE_ONEMKL)
        message(STATUS "oneMKL optimization ENABLED (via CMake config)")
    endif()
else()
    message(STATUS "Building WITHOUT oneMKL optimization")
endif()

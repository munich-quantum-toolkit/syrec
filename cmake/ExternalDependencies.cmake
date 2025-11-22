# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# Declare all external dependencies and make sure that they are available.

include(FetchContent)
set(FETCH_PACKAGES "")

if(BUILD_MQT_SYREC_BINDINGS)
  # Manually detect the installed mqt-core package.
  execute_process(
    COMMAND "${Python_EXECUTABLE}" -m mqt.core --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE mqt-core_DIR
    ERROR_QUIET)

  # Add the detected directory to the CMake prefix path.
  if(mqt-core_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${mqt-core_DIR}")
    message(STATUS "Found mqt-core package: ${mqt-core_DIR}")
  endif()

  if(NOT SKBUILD)
    # Manually detect the installed pybind11 package.
    execute_process(
      COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
      OUTPUT_STRIP_TRAILING_WHITESPACE
      OUTPUT_VARIABLE pybind11_DIR)

    # Add the detected directory to the CMake prefix path.
    list(APPEND CMAKE_PREFIX_PATH "${pybind11_DIR}")
  endif()

  # add pybind11 library
  find_package(pybind11 3.0.0 CONFIG REQUIRED)
endif()

# cmake-format: off
set(MQT_CORE_MINIMUM_VERSION 3.3.1
    CACHE STRING "MQT Core minimum version")
set(MQT_CORE_VERSION 3.3.3
    CACHE STRING "MQT Core version")
set(MQT_CORE_REV "8c9f6ab24968401e450812fc0ff7d05b5ae07a63"
    CACHE STRING "MQT Core identifier (tag, branch or commit hash)")
set(MQT_CORE_REPO_OWNER "munich-quantum-toolkit"
	CACHE STRING "MQT Core repository owner (change when using a fork)")
# cmake-format: on
FetchContent_Declare(
  mqt-core
  GIT_REPOSITORY https://github.com/${MQT_CORE_REPO_OWNER}/core.git
  GIT_TAG ${MQT_CORE_REV}
  FIND_PACKAGE_ARGS ${MQT_CORE_MINIMUM_VERSION})
list(APPEND FETCH_PACKAGES mqt-core)

if(BUILD_MQT_SYREC_TESTS)
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "" FORCE)
  set(GTEST_VERSION
      1.17.0
      CACHE STRING "Google Test version")
  set(GTEST_URL https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz)
  FetchContent_Declare(googletest URL ${GTEST_URL} FIND_PACKAGE_ARGS ${GTEST_VERSION} NAMES GTest)
  list(APPEND FETCH_PACKAGES googletest)
endif()

# The original CMake configuration in the ANTLR C++ git repository
# (https://github.com/antlr/antlr4/blob/master/runtime/Cpp/cmake/ExternalAntlr4Cpp.cmake) uses the
# ExternalProject_XX functions to configure the built of the ANTLR runtime and serves as a reference
# from which this configuration file was built using the FetchContent_XX functions instead.
set(ANTLR4_GIT_REPOSITORY "https://github.com/antlr/antlr4.git")

# ANTLR v4.13.2 - minor version update could include "minor" breaking changes (see
# https://github.com/antlr/antlr4?tab=readme-ov-file#versioning)
set(ANTLR4_VERSION
    4.13.2
    CACHE STRING "ANTLR4 runtime version")

# Note that the specified hash value refers to a commit in the 'origin/dev' branch of the ANTLR4
# runtime git repository that is currently needed to be able to compile the runtime on windows using
# the mvsc compiler (see https://github.com/antlr/antlr4/pull/4738). Due to the GIT_TAG option of
# the FetchContent_Declare CMake function (inherited from the ExternalProject_Add(...) function)
# only allowing commit hashes if the GIT_SHALLOW argument is disabled
# (https://cmake.org/cmake/help/latest/module/ExternalProject.html#git), a full-checkout of the
# ANTLR4 git repository is performed. If the needed changes are merge into the 'origin/master'
# branch of the ANTLR runtime, the ANTLR4_TAG should be updated to the 'new' version number and the
# GIT_SHALLOW argument enabled (i.e. set to ON) to only perform a clone of the git repository of
# depth 1.
set(ANTLR4_TAG
    "7b53e13ba005b978e2603f3ff81a0cb7cc98f689"
    CACHE STRING "Antlr4 runtime identifier (tag, branch or commit hash)")
set(ANTLR_BUILD_CPP_TESTS
    OFF
    CACHE BOOL "Should the ANTLR4 C++ runtime tests be built")
set(DISABLE_WARNINGS ON BOOL) # Do not report compiler warnings for build of ANTLR runtime
set(ANTLR4_BUILD_AS_STATIC_LIBRARY
    ON
    CACHE BOOL "Build the ANTLR4 runtime as a static library (turned on by default)")
set(ANTLR4_GIT_SHALLOW_CLONE
    OFF
    CACHE
      BOOL
      "Should a shallow clone of the ANTLR4 runtime be performed (only required when referring to branch or tag)"
)

message(STATUS "ANTLR git repo: ${ANTLR4_GIT_REPOSITORY}")
message(STATUS "ANTLR git tag: ${ANTLR4_TAG}")

if(NOT DEFINED WITH_STATIC_CRT AND (MSVC OR WIN32))
  # using /MD flag for antlr4_runtime (for Visual C++ compilers only)
  set(WITH_STATIC_CRT OFF)
endif()

if(ANTLR4_BUILD_AS_STATIC_LIBRARY)
  set(ANTLR_BUILD_STATIC
      ON
      CACHE INTERNAL BOOL)
  set(ANTLR_BUILD_SHARED
      OFF
      CACHE INTERNAL BOOL)
  message(STATUS "ANTLR runtime library type: STATIC")

  FetchContent_Declare(
    antlr4_static
    GIT_REPOSITORY ${ANTLR4_GIT_REPOSITORY}
    GIT_SHALLOW ${ANTLR4_GIT_SHALLOW_CLONE}
    GIT_TAG ${ANTLR4_TAG}
    SOURCE_SUBDIR runtime/Cpp FIND_PACKAGE_ARGS ${ANTLR4_VERSION})
  list(APPEND FETCH_PACKAGES antlr4_static)
else()
  set(ANTLR_BUILD_STATIC
      OFF
      CACHE INTERNAL BOOL)
  set(ANTLR_BUILD_SHARED
      ON
      CACHE INTERNAL BOOL)
  message(STATUS "ANTLR runtime library type: SHARED")

  FetchContent_Declare(
    antlr4_shared
    GIT_REPOSITORY ${ANTLR4_GIT_REPOSITORY}
    GIT_SHALLOW ${ANTLR4_GIT_SHALLOW_CLONE}
    GIT_TAG ${ANTLR4_TAG}
    SOURCE_SUBDIR runtime/Cpp FIND_PACKAGE_ARGS ${ANTLR4_VERSION})
  list(APPEND FETCH_PACKAGES antlr4_shared)
endif()

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})

if(ANTLR4_BUILD_AS_STATIC_LIBRARY)
  set(ANTLR4_INCLUDE_DIRS ${antlr4_static_SOURCE_DIR}/runtime/Cpp/runtime/src)
  # When linking the ANTLR4 static runtime to a shared library or executable, the position
  # independent code compiler options needs to be set otherwise the linker will fail
  # https://github.com/antlr/antlr4/issues/2776
  set_target_properties(antlr4_static PROPERTIES CMAKE_POSITION_INDEPENDENT_CODE ON)

  # Dlls do not use position independent code compiler option
  # (https://github.com/BVLC/caffe/issues/5992)
  if(NOT WIN32)
    target_compile_options(antlr4_static PUBLIC -fPIC)
  endif()
else()
  set(ANTLR4_INCLUDE_DIRS ${antlr4_shared_SOURCE_DIR}/runtime/Cpp/runtime/src)
endif()

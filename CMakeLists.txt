# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# set required cmake version
cmake_minimum_required(VERSION 3.24...4.0)

project(
  mqt-syrec
  LANGUAGES C CXX
  DESCRIPTION "MQT SyReC Synthesizer: A Tool for HDL-based Synthesis of Reversible Circuits")

option(BUILD_MQT_SYREC_BINDINGS "Build the MQT SYREC Python bindings" OFF)
if(BUILD_MQT_SYREC_BINDINGS)
  # ensure that the BINDINGS option is set
  set(BINDINGS
      ON
      CACHE INTERNAL "Enable settings related to Python bindings")
  # Some common settings for finding Python
  set(Python_FIND_VIRTUALENV
      FIRST
      CACHE STRING "Give precedence to virtualenvs when searching for Python")
  set(Python_FIND_FRAMEWORK
      LAST
      CACHE STRING "Prefer Brew/Conda to Apple framework Python")
  set(Python_ARTIFACTS_INTERACTIVE
      ON
      CACHE BOOL "Prevent multiple searches for Python and instead cache the results.")

  if(DISABLE_GIL)
    message(STATUS "Disabling Python GIL")
    add_compile_definitions(Py_GIL_DISABLED)
  endif()

  # top-level call to find Python
  find_package(
    Python 3.9 REQUIRED
    COMPONENTS Interpreter Development.Module
    OPTIONAL_COMPONENTS Development.SABIModule)
endif()

# check if this is the master project or used via add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(MQT_SYREC_MASTER_PROJECT ON)
else()
  set(MQT_SYREC_MASTER_PROJECT OFF)
endif()

option(BUILD_MQT_SYREC_TESTS "Also build tests for the MQT SYREC project"
       ${MQT_SYREC_MASTER_PROJECT})

include(cmake/ExternalDependencies.cmake)

# set the include directory for the build tree
set(MQT_SYREC_INCLUDE_BUILD_DIR "${CMAKE_SOURCE_DIR}/include")

# add main library code
add_subdirectory(src)

# add test code
if(BUILD_MQT_SYREC_TESTS)
  enable_testing()
  include(GoogleTest)
  add_subdirectory(test)
endif()

# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

set(MQT_SYREC_TARGET_NAME "mqt-syrec")

if(NOT TARGET ${MQT_SYREC_TARGET_NAME}-ir)
  add_library(${MQT_SYREC_TARGET_NAME}-ir)
  target_sources(
    ${MQT_SYREC_TARGET_NAME}-ir
    PUBLIC ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/expression.hpp
           ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/module.hpp
           ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/number.hpp
           ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/statement.hpp
           ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/variable.hpp
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/syrec/variable.cpp)

  target_include_directories(${MQT_SYREC_TARGET_NAME}-ir PUBLIC ${MQT_SYREC_INCLUDE_BUILD_DIR})

  target_link_libraries(${MQT_SYREC_TARGET_NAME}-ir PRIVATE MQT::ProjectWarnings
                                                            MQT::ProjectOptions)

  add_library(MQT::SyReC-IR ALIAS ${MQT_SYREC_TARGET_NAME}-ir)
  target_compile_features(${MQT_SYREC_TARGET_NAME}-ir PUBLIC cxx_std_17)
endif()

if(NOT TARGET ${MQT_SYREC_TARGET_NAME}-antlr-parser)
  file(GLOB_RECURSE SYREC_PARSER_HEADERS ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/parser/*.hpp)
  list(APPEND SYREC_PARSER_HEADERS ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/program.hpp
       ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/parser/antlr/TSyrecLexer.h
       ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/syrec/parser/antlr/TSyrecParser.h)

  file(GLOB_RECURSE SYREC_PARSER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/core/syrec/parser/*.cpp)
  list(APPEND SYREC_PARSER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/core/syrec/program.cpp)

  add_library(${MQT_SYREC_TARGET_NAME}-antlr-parser)
  target_sources(
    ${MQT_SYREC_TARGET_NAME}-antlr-parser
    PUBLIC ${SYREC_PARSER_HEADERS}
    PRIVATE ${SYREC_PARSER_SOURCES})

  target_include_directories(
    ${MQT_SYREC_TARGET_NAME}-antlr-parser
    PUBLIC ${MQT_SYREC_INCLUDE_BUILD_DIR} ${CMAKE_SOURCE_DIR}/include/core/syrec/parser/antlr
    PRIVATE ${ANTLR4_INCLUDE_DIRS})

  target_link_libraries(
    ${MQT_SYREC_TARGET_NAME}-antlr-parser
    PUBLIC ${MQT_SYREC_TARGET_NAME}-ir
    PRIVATE MQT::ProjectWarnings MQT::ProjectOptions fmt::fmt)

  if(ANTLR4_BUILD_AS_STATIC_LIBRARY)
    target_link_libraries(${MQT_SYREC_TARGET_NAME}-antlr-parser PRIVATE antlr4_static)
  else()
    target_link_libraries(${MQT_SYREC_TARGET_NAME}-antlr-parser PRIVATE antlr4_shared)
  endif()

  add_library(MQT::SyReC-Antlr ALIAS ${MQT_SYREC_TARGET_NAME}-antlr-parser)
  target_compile_features(${MQT_SYREC_TARGET_NAME}-antlr-parser PUBLIC cxx_std_17)
endif()

if(NOT TARGET ${MQT_SYREC_TARGET_NAME}-parsers)
  add_library(${MQT_SYREC_TARGET_NAME}-parsers)
  target_sources(
    ${MQT_SYREC_TARGET_NAME}-parsers
    PUBLIC ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/io/pla_parser.hpp
           ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/real/parser.hpp
           ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/truthTable/truth_table.hpp
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/io/pla_parser.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/real/parser.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/truthTable/truth_table.cpp)

  target_include_directories(${MQT_SYREC_TARGET_NAME}-parsers PUBLIC ${MQT_SYREC_INCLUDE_BUILD_DIR})

  target_link_libraries(
    ${MQT_SYREC_TARGET_NAME}-parsers
    PUBLIC MQT::CoreDD MQT::CoreIR
    PRIVATE MQT::ProjectWarnings MQT::ProjectOptions)

  add_library(MQT::SyReC-Parsers ALIAS ${MQT_SYREC_TARGET_NAME}-parsers)

endif()

if(NOT TARGET ${MQT_SYREC_TARGET_NAME}-synthesis)
  file(GLOB_RECURSE SYREC_SYNTHESIS_HEADERS ${MQT_SYREC_INCLUDE_BUILD_DIR}/algorithms/*.hpp)
  list(
    APPEND
    SYREC_SYNTHESIS_HEADERS
    ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/annotatable_quantum_computation.hpp
    ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/n_bit_values_container.hpp
    ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/properties.hpp
    ${MQT_SYREC_INCLUDE_BUILD_DIR}/core/truthTable/truth_table.hpp)

  file(GLOB_RECURSE SYREC_SYNTHESIS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/algorithms/*.cpp)
  list(
      APPEND 
      SYREC_SYNTHESIS_SOURCES 
      ${CMAKE_CURRENT_SOURCE_DIR}/core/annotatable_quantum_computation.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/core/truthTable/truth_table.cpp)

  add_library(${MQT_SYREC_TARGET_NAME}-synthesis)
  target_sources(
    ${MQT_SYREC_TARGET_NAME}-synthesis
    PUBLIC ${SYREC_SYNTHESIS_HEADERS}
    PRIVATE ${SYREC_SYNTHESIS_SOURCES})

  target_include_directories(${MQT_SYREC_TARGET_NAME}-synthesis
                             PUBLIC ${MQT_SYREC_INCLUDE_BUILD_DIR})

  # add header-only part of the Boost library
  set(BOOST_USE_MULTITHREADED ON)
  set(BOOST_USE_STATIC_RUNTIME OFF)
  find_package(Boost 1.71 REQUIRED)

  target_link_libraries(
    ${MQT_SYREC_TARGET_NAME}-synthesis
    PUBLIC MQT::CoreDD Boost::boost
    PRIVATE MQT::ProjectWarnings MQT::ProjectOptions)

  add_library(MQT::SyReC-Synthesis ALIAS ${MQT_SYREC_TARGET_NAME}-synthesis)
endif()

if(BUILD_MQT_SYREC_BINDINGS)
  add_subdirectory(python)
endif()

# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# collect all test files
file(GLOB_RECURSE SYREC_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Due to the package_add_test macro is only accepting a single library that is can link to the
# generated test executable, a wrapper library to which all of the generated MQT::SyReC libraries
# are linked to is needed. Note that the test source files are assumed to be defined after the last
# required parameter of the package_add_test macro.
add_library(${PROJECT_NAME}-test-wrapper INTERFACE)
target_link_libraries(${PROJECT_NAME}-test-wrapper INTERFACE MQT::SyReC-Antlr MQT::SyReC-Parsers
                                                             MQT::SyReC-IR MQT::SyReC-Synthesis)

# add test executable
package_add_test(${PROJECT_NAME}-test ${PROJECT_NAME}-test-wrapper ${SYREC_TEST_SOURCES})
target_link_libraries(${PROJECT_NAME}-test PRIVATE fmt::fmt)
target_include_directories(${PROJECT_NAME}-test
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unittests/syrec/parser/utils)

add_custom_command(
  TARGET ${PROJECT_NAME}-test
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/${PROJECT_NAME}-test
    ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-test
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/circuits
          $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/circuits
  COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/circuits
          ${CMAKE_BINARY_DIR}/circuits
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/configs
          $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/configs
  COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/configs
          ${CMAKE_BINARY_DIR}/configs
  # Solution is sufficient for now but surely there is a better way where the explicit recreation of
  # the folder structure is not necessary, i.e. we might only need to update the paths for creation
  # of the symlinks correctly since files are only copied to the output directory?
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/unittests
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/unittests/syrec
  COMMAND ${CMAKE_COMMAND} -E make_directory
          $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/unittests/syrec/parser
  COMMAND ${CMAKE_COMMAND} -E make_directory
          $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/unittests/syrec/parser/data
  COMMAND ${CMAKE_COMMAND} -E make_directory
          $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/unittests/syrec/parser/success
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/unittests
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/unittests/syrec
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/unittests/syrec/parser
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/unittests/syrec/parser/data
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/unittests/syrec/parser/success
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/unittests/syrec/parser/data/success
    $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/unittests/syrec/parser/data/success
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    $<TARGET_FILE_DIR:${PROJECT_NAME}-test>/unittests/syrec/parser/data/success
    ${CMAKE_BINARY_DIR}/unittests/syrec/parser/data/success
  COMMENT "Copying circuits and creating symlinks for ${PROJECT_NAME}-test"
  VERBATIM)

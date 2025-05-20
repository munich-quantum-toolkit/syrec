# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# collect all test files
file(GLOB_RECURSE TEST_FILES "*.cpp")

# add test executable
package_add_test(mqt-syrec-test MQT::SyReC ${TEST_FILES})

add_custom_command(
  TARGET mqt-syrec-test
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_DIR:mqt-syrec-test>/mqt-syrec-test
          ${CMAKE_BINARY_DIR}/mqt-syrec-test
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/circuits
          $<TARGET_FILE_DIR:mqt-syrec-test>/circuits
  COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_DIR:mqt-syrec-test>/circuits
          ${CMAKE_BINARY_DIR}/circuits
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/configs
          $<TARGET_FILE_DIR:mqt-syrec-test>/configs
  COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_DIR:mqt-syrec-test>/configs
          ${CMAKE_BINARY_DIR}/configs
  COMMENT "Copying circuits and creating symlinks for mqt-syrec-test"
  VERBATIM)

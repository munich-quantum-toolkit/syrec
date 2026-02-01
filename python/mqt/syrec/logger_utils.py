# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

import logging
from typing import Final

DEFAULT_LOGGER_NAME: Final[str] = "syrec-console-logger"


def configure_default_console_logger() -> None:
    # For supported log message formats (see https://docs.python.org/3/library/logging.html#formatter-objects)
    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s-%(levelname)s-[%(filename)s:%(lineno)s - %(funcName)20s()]-%(message)s",
        datefmt="%H:%M:%S",
        force=True,
    )


def log_debug_to_console(
    info_msg: str, num_additionally_skipped_stack_frames_starting_from_caller_function: int = 0
) -> None:
    logger = logging.getLogger(DEFAULT_LOGGER_NAME)
    # We do not want to log the origin of the helper function but of the caller of the function itself.
    # The origin of the log entry is set to the stack frame of the caller but can be advances further up in the stack trace
    logger.debug(msg=info_msg, stacklevel=(2 + num_additionally_skipped_stack_frames_starting_from_caller_function))


def log_info_to_console(
    info_msg: str, num_additionally_skipped_stack_frames_starting_from_caller_function: int = 0
) -> None:
    logger = logging.getLogger(DEFAULT_LOGGER_NAME)
    # We do not want to log the origin of the helper function but of the caller of the function itself.
    # The origin of the log entry is set to the stack frame of the caller but can be advances further up in the stack trace
    logger.info(msg=info_msg, stacklevel=(2 + num_additionally_skipped_stack_frames_starting_from_caller_function))


def log_warning_to_console(
    warn_msg: str, num_additionally_skipped_stack_frames_starting_from_caller_function: int = 0
) -> None:
    logger = logging.getLogger(DEFAULT_LOGGER_NAME)
    # We do not want to log the origin of the helper function but of the caller of the function itself.
    # The origin of the log entry is set to the stack frame of the caller but can be advances further up in the stack trace
    logger.warning(msg=warn_msg, stacklevel=(2 + num_additionally_skipped_stack_frames_starting_from_caller_function))


def log_error_to_console(
    err_msg: str, num_additionally_skipped_stack_frames_starting_from_caller_function: int = 0
) -> None:
    logger = logging.getLogger(DEFAULT_LOGGER_NAME)
    # We do not want to log the origin of the helper function but of the caller of the function itself.
    # The origin of the log entry is set to the stack frame of the caller but can be advances further up in the stack trace
    logger.error(msg=err_msg, stacklevel=(2 + num_additionally_skipped_stack_frames_starting_from_caller_function))

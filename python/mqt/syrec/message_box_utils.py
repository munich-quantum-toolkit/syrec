# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
from enum import Enum

from PyQt6 import QtWidgets

if sys.version_info >= (3, 11):
    from typing import assert_never
else:
    from typing_extensions import assert_never

from .logger_utils import log_debug_to_console, log_error_to_console, log_info_to_console, log_warning_to_console


class MessageBoxType(Enum):
    QUESTION = 0
    INFO = 1
    WARNING = 2
    ERROR = 3


def show_and_request_ok_in_optionally_cancellable_notification(
    message_box_type: MessageBoxType,
    message_box_parent: QtWidgets.QWidget,
    message_box_title: str,
    message_box_content: str,
    *,
    is_cancellable: bool,
    log_contents: bool = True,
) -> bool:
    clicked_message_box_button: QtWidgets.QMessageBox.StandardButton | None = None
    match message_box_type:
        case MessageBoxType.QUESTION:
            if log_contents:
                log_debug_to_console(
                    f"{message_box_title} - {message_box_content}",
                    num_additionally_skipped_stack_frames_starting_from_caller_function=1,
                )

            clicked_message_box_button = QtWidgets.QMessageBox.question(
                message_box_parent,
                message_box_title,
                message_box_content,
                buttons=_get_buttons_for_message_box_type(message_box_type, is_cancellable=is_cancellable),
                defaultButton=_get_default_button_for_message_box_type(message_box_type, is_cancellable=is_cancellable),
            )
            return _check_whether_message_ok_was_clicked(message_box_type, clicked_message_box_button)
        case MessageBoxType.INFO:
            if log_contents:
                log_info_to_console(
                    f"{message_box_title} - {message_box_content}",
                    num_additionally_skipped_stack_frames_starting_from_caller_function=1,
                )

            clicked_message_box_button = QtWidgets.QMessageBox.information(
                message_box_parent,
                message_box_title,
                message_box_content,
                buttons=_get_buttons_for_message_box_type(message_box_type, is_cancellable=is_cancellable),
                defaultButton=_get_default_button_for_message_box_type(message_box_type, is_cancellable=is_cancellable),
            )
            return _check_whether_message_ok_was_clicked(message_box_type, clicked_message_box_button)
        case MessageBoxType.WARNING:
            if log_contents:
                log_warning_to_console(
                    f"{message_box_title} - {message_box_content}",
                    num_additionally_skipped_stack_frames_starting_from_caller_function=1,
                )

            clicked_message_box_button = QtWidgets.QMessageBox.warning(
                message_box_parent,
                message_box_title,
                message_box_content,
                buttons=_get_buttons_for_message_box_type(message_box_type, is_cancellable=is_cancellable),
                defaultButton=_get_default_button_for_message_box_type(message_box_type, is_cancellable=is_cancellable),
            )
            return _check_whether_message_ok_was_clicked(message_box_type, clicked_message_box_button)
        case MessageBoxType.ERROR:
            if log_contents:
                log_error_to_console(
                    f"{message_box_title} - {message_box_content}",
                    num_additionally_skipped_stack_frames_starting_from_caller_function=1,
                )

            clicked_message_box_button = QtWidgets.QMessageBox.critical(
                message_box_parent,
                message_box_title,
                message_box_content,
                buttons=_get_buttons_for_message_box_type(message_box_type, is_cancellable=is_cancellable),
                defaultButton=_get_default_button_for_message_box_type(message_box_type, is_cancellable=is_cancellable),
            )
            return _check_whether_message_ok_was_clicked(message_box_type, clicked_message_box_button)
        case _:
            # Added guard to handle new message box types
            assert_never(message_box_type)


def _get_buttons_for_message_box_type(
    message_box_type: MessageBoxType, *, is_cancellable: bool
) -> QtWidgets.QMessageBox.StandardButton:
    if message_box_type == MessageBoxType.QUESTION:
        return (
            (QtWidgets.QMessageBox.StandardButton.Yes | QtWidgets.QMessageBox.StandardButton.No)
            if is_cancellable
            else QtWidgets.QMessageBox.StandardButton.Yes
        )
    return (
        (QtWidgets.QMessageBox.StandardButton.Ok | QtWidgets.QMessageBox.StandardButton.Cancel)
        if is_cancellable
        else QtWidgets.QMessageBox.StandardButton.Ok
    )


# Get the default button that will be pressed if the user presses ENTER in the open message box
def _get_default_button_for_message_box_type(
    message_box_type: MessageBoxType, *, is_cancellable: bool
) -> QtWidgets.QMessageBox.StandardButton:
    if message_box_type == MessageBoxType.QUESTION:
        return QtWidgets.QMessageBox.StandardButton.No if is_cancellable else QtWidgets.QMessageBox.StandardButton.Yes
    return QtWidgets.QMessageBox.StandardButton.Cancel if is_cancellable else QtWidgets.QMessageBox.StandardButton.Ok


def _check_whether_message_ok_was_clicked(
    message_box_type: MessageBoxType,
    clicked_message_box_button: QtWidgets.QMessageBox.StandardButton | None,
) -> bool:
    # Pressing the ESC key in a QMessageBox can return None is no escape button can be determined or was configured (see https://doc.qt.io/qt-6/qmessagebox.html#default-and-escape-keys)
    if message_box_type == MessageBoxType.QUESTION:
        return (
            clicked_message_box_button is not None
            and clicked_message_box_button == QtWidgets.QMessageBox.StandardButton.Yes
        )
    return (
        clicked_message_box_button is not None and clicked_message_box_button == QtWidgets.QMessageBox.StandardButton.Ok
    )

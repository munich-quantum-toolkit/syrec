# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

from .logger_utils import log_error_to_console
from .message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification

if TYPE_CHECKING:
    from collections.abc import Iterable

    from PyQt6 import QtWidgets


def assert_all_required_widgets_found_or_close_dialog(
    error_notification_parent_widget: QtWidgets.QWidget,
    required_widgets: Iterable[QtWidgets.QWidget],
    error_dialog_content: str,
    num_additionally_skipped_stack_frames_starting_from_caller_function: int = 0,
) -> bool:
    if all(widget is not None for widget in required_widgets):
        return True

    show_and_request_ok_in_optionally_cancellable_notification(
        message_box_type=MessageBoxType.ERROR,
        message_box_parent=error_notification_parent_widget,
        message_box_title="Not all required Qt widgets found!",
        message_box_content=f"{error_dialog_content}\nUnsaved changed will be lost and edit dialog will be closed!",
        is_cancellable=False,
        log_contents=False,
    )

    stringified_found_widgets_object_names: Final[str] = "Object names of found widgets: " + (
        ",".join([widget.objectName() for widget in filter(lambda widget: widget is not None, required_widgets)])
    )
    # We want to log the caller of this function as the origin of the error instead of this function itself.
    log_error_to_console(
        f"{error_dialog_content}\n{stringified_found_widgets_object_names}",
        num_additionally_skipped_stack_frames_starting_from_caller_function=1
        + num_additionally_skipped_stack_frames_starting_from_caller_function,
    )
    return False

# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import Final, Generic, TypeVar

from PyQt6 import QtCore, QtGui, QtWidgets

from ...logger_utils import log_error_to_console, log_info_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ..workers.cancellable_base_worker import CancellableBaseWorker

T = TypeVar("T", bound=CancellableBaseWorker)

DEFAULT_TOTAL_RUNTIME_INFO_TEXT_FORMAT: Final[str] = (
    "Total runtime [in seconds] (excluding model updates, internal waits): {total_runtime_in_seconds:f}"
)
DEFAULT_BATCH_RUNTIME_INFO_TEXT_FORMAT: Final[str] = (
    "Batch of {n_batch_elements:d} completed! Runtime [in seconds]: {batch_duration_in_seconds:f}"
)

SMALL_DIALOG_WIDTH: Final[int] = 600
SMALL_DIALOG_HEIGHT: Final[int] = 300


class BaseProgressDialog(QtWidgets.QDialog, Generic[T]):  # type: ignore[misc]
    def __init__(
        self,
        parent: QtWidgets.QWidget,
        dialog_title: str,
        optional_progress_bar_text_format: str | None = None,
        create_default_layout: bool = True,
        user_provided_dialog_size: QtCore.QSize | None = None,
        center_dialog: bool = True,
    ):
        super().__init__(parent)

        self.worker_thread: QtCore.QThread | None = None
        self.worker: T | None = None
        self.stop_processing_recv_batches: bool = False
        self.total_runtime_in_seconds: float = 0

        self.setModal(True)
        self.setSizeGripEnabled(True)
        self.setWindowTitle(dialog_title)

        dialog_x_pos: int = 0
        dialog_y_pos: int = 0

        to_be_used_dialog_size: Final[QtCore.QSize] = (
            QtCore.QSize(SMALL_DIALOG_WIDTH, SMALL_DIALOG_HEIGHT)
            if user_provided_dialog_size is None
            else user_provided_dialog_size
        )
        if center_dialog:
            dialog_pos: Final[QtCore.QPoint] = BaseProgressDialog.get_center_screen_position_for_size(
                to_be_used_dialog_size
            )
            dialog_x_pos = dialog_pos.x()
            dialog_y_pos = dialog_pos.y()
        self.setGeometry(dialog_x_pos, dialog_y_pos, to_be_used_dialog_size.width(), to_be_used_dialog_size.height())

        layout = QtWidgets.QVBoxLayout()
        self.title_lbl = QtWidgets.QLabel()
        self.title_lbl.setStyleSheet("QLabel { font-size : 16px; font-weight: bold; }")
        self.title_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.progress_info_text_lbl = QtWidgets.QLabel()
        self.progress_info_text_lbl.setStyleSheet("QLabel { color : gray; }")
        self.progress_info_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.error_text_lbl = QtWidgets.QLabel()
        self.error_text_lbl.setStyleSheet("QLabel { color : red; }")
        self.error_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.progress_bar: QtWidgets.QProgressBar | None = None
        if optional_progress_bar_text_format is not None:
            self.progress_bar = QtWidgets.QProgressBar()
            # For placeholder values see: https://doc.qt.io/qtforpython-6/PySide6/QtWidgets/QProgressBar.html#PySide6.QtWidgets.QProgressBar.format
            # self.progress_bar.setFormat("Generated %v out of %m input states")
            self.progress_bar.setFormat(optional_progress_bar_text_format)
            self.progress_bar.setFormat(optional_progress_bar_text_format)
            self.progress_bar.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.total_runtime_info_text_lbl = QtWidgets.QLabel()
        self.total_runtime_info_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.dialog_button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Ok | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        self.dialog_button_box.setCenterButtons(True)
        self._change_dialog_ok_button_enable_state(False)
        self._change_dialog_cancel_button_enable_state(False)

        if create_default_layout:
            layout.addWidget(self.title_lbl)
            layout.addWidget(self.progress_info_text_lbl)
            layout.addWidget(self.error_text_lbl)
            layout.addStretch()
            layout.addWidget(self.progress_bar)
            layout.addWidget(self.total_runtime_info_text_lbl)
            layout.addWidget(self.dialog_button_box)
            self.setLayout(layout)

    @staticmethod
    def get_default_big_dialog_size() -> QtCore.Size:
        return QtCore.QSize(
            int(QtGui.QGuiApplication.primaryScreen().availableSize().width() / 1.5),
            int(QtGui.QGuiApplication.primaryScreen().availableSize().height() / 1.5),
        )

    @staticmethod
    def get_center_screen_position_for_size(dialog_size: QtCore.Size) -> QtCore.QPoint:
        return QtCore.QPoint(
            (QtGui.QGuiApplication.primaryScreen().availableSize().width() // 2) - (dialog_size.width() // 2),
            (QtGui.QGuiApplication.primaryScreen().availableSize().height() // 2) - (dialog_size.height() // 2),
        )

    def _update_progress_text_with_batch_info(self, n_batch_elements: int, batch_duration_in_seconds: float) -> None:
        self.progress_info_text_lbl.setText(
            DEFAULT_BATCH_RUNTIME_INFO_TEXT_FORMAT.format(
                n_batch_elements=n_batch_elements, batch_duration_in_seconds=batch_duration_in_seconds
            )
        )

    def _accumulate_and_update_total_runtime(self, batch_runtime_in_seconds: float) -> None:
        if batch_runtime_in_seconds < 0:
            return

        self.total_runtime_in_seconds += batch_runtime_in_seconds
        self.total_runtime_info_text_lbl.setText(
            DEFAULT_TOTAL_RUNTIME_INFO_TEXT_FORMAT.format(total_runtime_in_seconds=self.total_runtime_in_seconds)
        )

    def _shutdown_worker_thread_and_await_completion(self) -> None:
        if self.worker_thread is None:
            return

        log_info_to_console(
            "Shutting down worker thread!", num_additionally_skipped_stack_frames_starting_from_caller_function=1
        )
        self.worker_thread.quit()
        log_info_to_console(
            "Waiting on worker thread completion...",
            num_additionally_skipped_stack_frames_starting_from_caller_function=1,
        )
        self.worker_thread.wait()
        log_info_to_console(
            "Worker thread finished!", num_additionally_skipped_stack_frames_starting_from_caller_function=1
        )
        self.progress_info_text_lbl.setText("Worker thread finished!")

    def _request_worker_cancellation(self) -> None:
        self.stop_processing_recv_batches = True
        self.progress_info_text_lbl.setText("Requesting cancellation of long running worker!")
        if self.worker is not None:
            log_info_to_console(
                "Requesting cancellation of long running worker",
                num_additionally_skipped_stack_frames_starting_from_caller_function=1,
            )
            self.worker.request_cancellation()
        self._change_dialog_cancel_button_enable_state(should_button_be_enabled=False)

    def _change_dialog_cancel_button_enable_state(self, should_button_be_enabled: bool) -> None:
        BaseProgressDialog._change_dialog_button_enable_state(
            self.dialog_button_box,
            QtWidgets.QDialogButtonBox.StandardButton.Cancel,
            should_button_be_enabled,
            btn_not_found_notification_parent=self,
        )

    def _change_dialog_ok_button_enable_state(self, should_button_be_enabled: bool) -> None:
        BaseProgressDialog._change_dialog_button_enable_state(
            self.dialog_button_box,
            QtWidgets.QDialogButtonBox.StandardButton.Ok,
            should_button_be_enabled,
            btn_not_found_notification_parent=self,
        )

    def _update_displayed_error_text(
        self,
        error: Exception | str,
        log_error: bool = True,
        num_additionally_skipped_stack_frames_starting_from_this_function: int = 0,
    ) -> None:
        err_msg: Final[str] = BaseProgressDialog._stringify_error(error) if isinstance(error, Exception) else error
        if log_error:
            log_error_to_console(
                err_msg,
                num_additionally_skipped_stack_frames_starting_from_caller_function=num_additionally_skipped_stack_frames_starting_from_this_function,
            )
        self.error_text_lbl.setText(err_msg)

    @staticmethod
    def _change_dialog_button_enable_state(
        dialog_button_box: QtWidgets.QDialogButtonBox,
        to_be_modified_button: QtWidgets.QDialogButtonBox.StandardButton,
        should_button_be_enabled: bool,
        btn_not_found_notification_parent: QtWidgets.QWidget,
    ) -> None:
        dialog_button: QtWidgets.QPushButton | None = dialog_button_box.button(to_be_modified_button)

        if dialog_button is not None:
            dialog_button.setEnabled(should_button_be_enabled)
        else:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=btn_not_found_notification_parent,
                message_box_title="Internal error",
                message_box_content=f"Could not find {to_be_modified_button.name} button of dialog, this should not happen",
                is_cancellable=False,
            )

    def _reset_workers(self) -> None:
        self.worker_thread = None
        self.worker = None

    @staticmethod
    def _stringify_error(error: Exception) -> str:
        return f"Error during long running worker operation! Reason: {type(error)=}, {error=}"

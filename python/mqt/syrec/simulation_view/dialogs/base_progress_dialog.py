# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from abc import abstractmethod
from typing import TYPE_CHECKING, Any, Final, Generic, TypeVar

from PyQt6 import QtCore, QtGui, QtWidgets

from ...logger_utils import log_error_to_console, log_info_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ..workers.cancellable_worker_variants import CancellableProducerConsumerWorker, CancellableProducerWorker

if TYPE_CHECKING:
    from ..simulation_run_model import QtSimulationRunModel

DEFAULT_TOTAL_RUNTIME_INFO_TEXT_FORMAT: Final[str] = (
    "Total runtime [in seconds] (excluding model updates, internal waits): {total_runtime_in_seconds:f}"
)
DEFAULT_BATCH_RUNTIME_INFO_TEXT_FORMAT: Final[str] = (
    "Batch of {n_batch_elements:d} completed! Runtime [in ms]: {batch_duration_in_ms:f}"
)

SMALL_DIALOG_WIDTH: Final[int] = 600
SMALL_DIALOG_HEIGHT: Final[int] = 300
DEFAULT_SMALL_QUEUE_SIZE: Final[int] = 500
DEFAULT_MEDIUM_QUEUE_SIZE: Final[int] = 1000
DEFAULT_WORKER_CONTINUE_DELAY_IN_MS: Final[int] = 250

WorkerType = TypeVar("WorkerType", bound=CancellableProducerWorker[Any] | CancellableProducerConsumerWorker[Any, Any])


class BaseProgressDialog(QtWidgets.QDialog, Generic[WorkerType]):  # type: ignore[misc]
    """
    Base class for progress dialogs with worker thread management.

    Note: Instances of this dialog are designed to be used only once.
    Create a new instance for each operation rather than reusing the same dialog.

    If not specified, the dialog will be opened as a modal dialog that is centered over the parent window with a default widget layout defined as (read from top to bottom):
            <TITLE>
        <PROGRESS_INFO>
          <ERROR_TEXT>
      <OPTIONAL_PROGRESS_BAR>
       <TOTAL_RUNTIME_INFO>
    <OPEN_BTN>  <CLOSE_BTN>

    Inputs:
        WorkerType: Defines the type of worker employed by the dialog to perform its long running operation.
    """

    def __init__(
        self,
        parent: QtWidgets.QWidget,
        shared_simulation_runs_model: QtSimulationRunModel,
        dialog_title: str,
        optional_progress_bar_text_format: str | None = None,
        create_default_layout: bool = True,
        user_provided_dialog_size: QtCore.QSize | None = None,
        center_dialog: bool = True,
    ) -> None:
        super().__init__(parent)

        self._worker_thread: QtCore.QThread | None = None
        self._worker: WorkerType | None = None
        self._shared_simulation_runs_model: QtSimulationRunModel = shared_simulation_runs_model

        self._stop_processing_recv_batches: bool = False
        self._total_runtime_in_seconds: float = 0

        # Ensure the dialog is deleted when closed this may not be strictly necessary but seems to be a good cleanup practice
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_DeleteOnClose)
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
        self._title_lbl = QtWidgets.QLabel()
        self._title_lbl.setStyleSheet("QLabel { font-size : 16px; font-weight: bold; }")
        self._title_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self._progress_info_text_lbl = QtWidgets.QLabel()
        self._progress_info_text_lbl.setStyleSheet("QLabel { color : gray; }")
        self._progress_info_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self._error_text_lbl = QtWidgets.QLabel()
        self._error_text_lbl.setStyleSheet("QLabel { color : red; }")
        self._error_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self._progress_bar: QtWidgets.QProgressBar | None = None
        if optional_progress_bar_text_format is not None:
            self._progress_bar = QtWidgets.QProgressBar()
            # For placeholder values see: https://doc.qt.io/qtforpython-6/PySide6/QtWidgets/QProgressBar.html#PySide6.QtWidgets.QProgressBar.format
            # self._progress_bar.setFormat("Generated %v out of %m input states")
            # An invalid pattern (e.g. unknown placeholders, etc.) will not cause an error.
            self._progress_bar.setFormat(optional_progress_bar_text_format)
            self._progress_bar.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self._total_runtime_info_text_lbl = QtWidgets.QLabel()
        self._total_runtime_info_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self._dialog_button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Ok | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        self._dialog_button_box.setCenterButtons(True)
        self._change_dialog_ok_button_enable_state(False)
        self._change_dialog_cancel_button_enable_state(False)

        if create_default_layout:
            layout.addWidget(self._title_lbl)
            layout.addWidget(self._progress_info_text_lbl)
            layout.addWidget(self._error_text_lbl)
            layout.addStretch()
            if optional_progress_bar_text_format is not None:
                layout.addWidget(self._progress_bar)
            layout.addWidget(self._total_runtime_info_text_lbl)
            layout.addWidget(self._dialog_button_box)
            self.setLayout(layout)

    @staticmethod
    def get_default_big_dialog_size() -> QtCore.QSize:
        # None could be returned when running the application in headless mode which should not happen but we cover this case nevertheless
        optional_primary_screen: QtGui.QScreen | None = QtGui.QGuiApplication.primaryScreen()
        if optional_primary_screen is None:
            return QtCore.QSize(0, 0)

        return QtCore.QSize(
            int(optional_primary_screen.availableSize().width() / 1.5),
            int(optional_primary_screen.availableSize().height() / 1.5),
        )

    @staticmethod
    def get_center_screen_position_for_size(dialog_size: QtCore.QSize) -> QtCore.QPoint:
        # None could be returned when running the application in headless mode which should not happen but we cover this case nevertheless
        optional_primary_screen: QtGui.QScreen | None = QtGui.QGuiApplication.primaryScreen()
        if optional_primary_screen is None:
            return QtCore.QPoint(0, 0)

        return QtCore.QPoint(
            (optional_primary_screen.availableSize().width() // 2)
            - ((dialog_size.width() // 2) if dialog_size.width() > 0 else 0),
            (optional_primary_screen.availableSize().height() // 2)
            - ((dialog_size.height() // 2) if dialog_size.height() > 0 else 0),
        )

    @abstractmethod
    def _handle_non_recoverable_error(self, err: Exception | str | None) -> None:
        """Handle non-recoverable errors. Must be implemented by subclasses."""
        return

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _allow_worker_to_continue(self) -> None:
        """Signal to the worker that it can continue with its long running operation."""
        if self._worker is None:
            return

        try:
            self._worker.notify_to_continue_processing()
        except Exception as err:
            self._handle_non_recoverable_error(
                f"Error while trying to notify simulation run execution worker about new batch data being available, reason: {BaseProgressDialog._stringify_error(err)}"
            )

    def _update_progress_text_with_batch_info(self, n_batch_elements: int, batch_duration_in_seconds: float) -> None:
        self._progress_info_text_lbl.setText(
            DEFAULT_BATCH_RUNTIME_INFO_TEXT_FORMAT.format(
                n_batch_elements=n_batch_elements, batch_duration_in_ms=batch_duration_in_seconds * 1000
            )
        )

    def _accumulate_and_update_total_runtime(self, batch_runtime_in_seconds: float) -> None:
        if batch_runtime_in_seconds < 0:
            return

        self._total_runtime_in_seconds += batch_runtime_in_seconds
        self._total_runtime_info_text_lbl.setText(
            DEFAULT_TOTAL_RUNTIME_INFO_TEXT_FORMAT.format(total_runtime_in_seconds=self._total_runtime_in_seconds)
        )

    def _shutdown_worker_thread_and_await_completion(self) -> None:
        """
        Stop the work threads event queue (QThread) and await the completion of the associated system thread of the worker.

        Note: This call will block until the system thread of the worker finishes execution.
        """
        if self._worker_thread is None:
            return

        log_info_to_console(
            "Shutting down worker thread!", num_additionally_skipped_stack_frames_starting_from_caller_function=1
        )
        self._worker_thread.quit()
        log_info_to_console(
            "Waiting on worker thread completion...",
            num_additionally_skipped_stack_frames_starting_from_caller_function=1,
        )
        self._worker_thread.wait()
        log_info_to_console(
            "Worker thread finished!", num_additionally_skipped_stack_frames_starting_from_caller_function=1
        )
        self._progress_info_text_lbl.setText("Worker thread finished!")

    def _request_worker_cancellation(self) -> None:
        """
        Request the cancellation of the long running operation of the worker.

        Note: It is the responsibility of the worker to support cooperative cancellation with this function giving
        no guarantee whether the worker supports such behaviour. Depending on the implementation of the worker, this call might block
        the calling thread.
        """
        if self._worker is None:
            return

        self._stop_processing_recv_batches = True
        self._progress_info_text_lbl.setText("Requesting cancellation of long running worker!")
        log_info_to_console(
            "Requesting cancellation of long running worker",
            num_additionally_skipped_stack_frames_starting_from_caller_function=1,
        )
        self._worker.request_cancellation()
        self._change_dialog_cancel_button_enable_state(should_button_be_enabled=False)

    def _change_dialog_cancel_button_enable_state(self, should_button_be_enabled: bool) -> None:
        BaseProgressDialog._change_dialog_button_enable_state(
            self._dialog_button_box,
            QtWidgets.QDialogButtonBox.StandardButton.Cancel,
            should_button_be_enabled,
            btn_not_found_notification_parent=self,
        )

    def _change_dialog_ok_button_enable_state(self, should_button_be_enabled: bool) -> None:
        BaseProgressDialog._change_dialog_button_enable_state(
            self._dialog_button_box,
            QtWidgets.QDialogButtonBox.StandardButton.Ok,
            should_button_be_enabled,
            btn_not_found_notification_parent=self,
        )

    def _update_displayed_error_text(
        self,
        error: Exception | str,
        *,
        log_error: bool = True,
        num_additionally_skipped_stack_frames_starting_from_this_function: int = 0,
    ) -> None:
        err_msg: Final[str] = BaseProgressDialog._stringify_error(error) if isinstance(error, Exception) else error
        if log_error:
            log_error_to_console(
                err_msg,
                num_additionally_skipped_stack_frames_starting_from_caller_function=num_additionally_skipped_stack_frames_starting_from_this_function,
            )
        self._error_text_lbl.setText(err_msg)

    def _reset_workers(self) -> None:
        """Reset the worker and worker thread instances by setting them to None."""
        self._worker_thread = None
        self._worker = None

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

    @staticmethod
    def _stringify_error(error: Exception) -> str:
        return f"Error during long running worker operation! Reason: {type(error)=}, {error=}"

    def _can_value_can_be_used_as_progress_bar_max_value(self, value: int) -> bool:
        max_allowed_value: Final[int] = (1 << 31) - 1
        if value > max_allowed_value:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Number not supported as maximum value of progress bar!",
                message_box_content=f"Attempted to use value {value} as maximum value of progress bar that was larger than the maximum supported value of {max_allowed_value}!",
                is_cancellable=False,
            )
            return False
        return True

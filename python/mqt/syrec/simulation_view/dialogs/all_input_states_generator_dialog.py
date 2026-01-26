# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore

if TYPE_CHECKING:
    from PyQt6 import QtGui, QtWidgets

    from ..simulation_run_model import QtSimulationRunModel

from ...logger_utils import log_error_to_console, log_info_to_console
from ...message_box_utils import MessageBoxType, show_optionally_cancellable_notification
from ..simulation_run_model import SimulationRunModel
from ..workers.all_input_states_generator_worker import AllInputStatesGeneratorWorker
from .base_progress_dialog import BaseProgressDialog


class AllInputStatesGeneratorDialog(BaseProgressDialog[AllInputStatesGeneratorWorker]):
    def __init__(self, parent: QtWidgets.QWidget):
        super().__init__(
            parent,
            dialog_title="Generating simulation runs...",
            optional_progress_bar_text_format="Generated %v out of %m input states",
        )
        self.shared_simulation_runs_model: QtSimulationRunModel | None = None
        self.num_generated_input_states: int = 0

        self.dialog_button_box.accepted.connect(self.accept)
        self.dialog_button_box.rejected.connect(self._handle_input_state_generation_cancel_button_click)

    def start_generation(
        self, shared_simulation_runs_model: QtSimulationRunModel, expected_input_state_size: int, batch_size: int = 1000
    ) -> None:
        self.shared_simulation_runs_model = shared_simulation_runs_model
        self.title_lbl.setText(f"Generating simulation runs with batch size {batch_size}!")
        # TODO: Validation that maximum value can actually be stored in progress bar maximum (should validation be performed in dialog or by caller?)
        if self.progress_bar is not None:
            self.progress_bar.setMinimum(0)
            self.progress_bar.setMaximum(2**expected_input_state_size)
            self.progress_bar.setValue(0)
        else:
            show_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Required widget not found",
                message_box_content="Input states generator was initialized without a progress bar! This should not happen.",
                is_cancellable=False,
            )

        # To avoid redundant comments we refer to the SimulationRunJsonImportDialog.start_import(...) function for details regarding the worker-object to perform a long running operation
        self.worker = AllInputStatesGeneratorWorker(expected_input_state_size, batch_size)
        self.worker_thread = QtCore.QThread()
        self.worker.moveToThread(self.worker_thread)
        self.worker.batchCompleted.connect(
            self._handle_generated_input_state_batch, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.finished.connect(
            self._handle_input_state_generator_finished, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.failed.connect(
            self._handle_input_state_generator_failure, QtCore.Qt.ConnectionType.QueuedConnection
        )

        self.worker_thread.started.connect(self.worker.start_generation, QtCore.Qt.ConnectionType.QueuedConnection)
        self.worker_thread.finished.connect(self.worker_thread.deleteLater)
        self.worker_thread.finished.connect(self._reset_workers)
        self.worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancel_button_enable_state(True)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802
        # Ask for confirmation before closing
        if self._handle_input_state_generation_cancel_button_click():
            if not self.error_text_lbl.text():
                self.accept()
            else:
                self.reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_input_state_generator_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot(float, object)  # type: ignore[untyped-decorator]
    def _handle_generated_input_state_batch(
        self, batch_generation_duration_in_seconds: float, batch_data: object
    ) -> None:
        if self.stop_processing_recv_batches:
            return

        if not AllInputStatesGeneratorWorker.is_batch_data_list_of_expected_type(
            batch_data, SimulationRunModel, parent_widget_for_error_notification=self
        ):
            if self.worker is not None:
                self.worker.ack_batch_processed()
            return

        generated_simulation_run_models: Final[list[SimulationRunModel]] = batch_data  # type: ignore[assignment]
        self._update_progress_text_with_batch_info(
            len(generated_simulation_run_models), batch_generation_duration_in_seconds
        )

        if self.shared_simulation_runs_model is None:
            log_error_to_console("Shared simulation runs model was not initialized during handling of batch!")
            self._handle_non_recoverable_error(None)
            return

        try:
            self.shared_simulation_runs_model.add_simulation_run_models(generated_simulation_run_models)
        except Exception as sim_run_model_err:
            self._handle_non_recoverable_error(sim_run_model_err)
            return

        if self.worker is not None:
            self.worker.ack_batch_processed()

        self._accumulate_and_update_total_runtime(batch_generation_duration_in_seconds)
        self.num_generated_input_states += len(generated_simulation_run_models)
        if self.progress_bar is not None:
            self.progress_bar.setValue(self.num_generated_input_states)
        self.progress_info_text_lbl.setText("")

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_input_state_generator_finished(self, was_cancellation_requested: bool) -> None:
        self.progress_info_text_lbl.setText("Input state generator finished!")
        log_info_to_console("Input state generator finished!")
        if self.progress_bar is not None:
            self.progress_bar.setVisible(False)

        if not was_cancellation_requested:
            if self.worker is not None:
                self._request_worker_cancellation()
            if self.worker_thread is not None:
                self._shutdown_worker_thread_and_await_completion()

        self._change_dialog_ok_button_enable_state(should_button_be_enabled=True)
        self._change_dialog_cancel_button_enable_state(should_button_be_enabled=False)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_input_state_generation_cancel_button_click(self) -> bool:
        if self.worker is None:
            return True

        if show_optionally_cancellable_notification(
            message_box_type=MessageBoxType.QUESTION,
            message_box_parent=self,
            message_box_title="Cancellation of generation of input states requested!",
            message_box_content="Are you sure that you want to stop the generation of the input states? This will cause the deletion of all already generated input states.",
            is_cancellable=True,
            log_contents=False,
        ):
            log_info_to_console("Cancellation of input state generation requested!")
            self._handle_non_recoverable_error(None)
            return True
        return False

    def _handle_non_recoverable_error(self, err: Exception | None) -> None:
        self.progress_info_text_lbl.setText("")
        if err is not None:
            # We want to log the source of the error as close as possible to the origin of the actual error thus we need to skip a few stack frames
            # to determine the "source" stack frame. The skip stack frames would be (read from left to right with the leftmost stackframe being at the
            # lowest level in the stacktrace): logger (std) -> logger_utils (custom) -> update_displayed_error_text (custom) -> the current function.
            self._update_displayed_error_text(err, num_additionally_skipped_stack_frames_starting_from_this_function=2)

        if self.shared_simulation_runs_model is not None:
            try:
                self.shared_simulation_runs_model.delete_all_simulation_run_models()
            except Exception:
                show_optionally_cancellable_notification(
                    message_box_type=MessageBoxType.ERROR,
                    message_box_parent=self,
                    message_box_title="Internal error!",
                    message_box_content="Failed to delete all simulation run models during handling of non-recoverable error!\nThis should not happen, cancelling long running operation!",
                    is_cancellable=False,
                )
        else:
            show_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Internal state error!",
                message_box_content="Shared simulation runs model was not initialized during handling of non-recoverable error!\nThis should not happen, cancelling long running operation!",
                is_cancellable=False,
            )

        if self.worker is not None:
            self._request_worker_cancellation()
        if self.worker_thread is not None:
            self._shutdown_worker_thread_and_await_completion()

# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import queue
import sys
from typing import TYPE_CHECKING, Final

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

from PyQt6 import QtCore

if TYPE_CHECKING:
    from PyQt6 import QtGui, QtWidgets

    from ..simulation_run_model import DataQubitsLookup, QtSimulationRunModel, SimulationRunModel

from ...logger_utils import log_info_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ..workers.all_input_states_generator_worker import AllInputStatesGeneratorWorker
from ..workers.cancellable_worker_variants import QueueConfig
from .base_progress_dialog import DEFAULT_MEDIUM_QUEUE_SIZE, DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, BaseProgressDialog


class AllInputStatesGeneratorDialog(BaseProgressDialog[AllInputStatesGeneratorWorker]):
    def __init__(self, parent: QtWidgets.QWidget, shared_simulation_runs_model: QtSimulationRunModel) -> None:
        super().__init__(
            parent,
            shared_simulation_runs_model,
            dialog_title="Generating simulation runs...",
            optional_progress_bar_text_format="Generated %v out of %m input states",
        )
        self._data_qubits_lookup: Final[DataQubitsLookup] = self._shared_simulation_runs_model.get_data_qubits_lookup()
        self._worker_send_queue: queue.SimpleQueue[SimulationRunModel] = queue.SimpleQueue()
        self._worker_send_queue_batch_size: int = 0
        self._num_generated_input_states: int = 0

        self._dialog_button_box.accepted.connect(self.accept)
        self._dialog_button_box.rejected.connect(self._handle_input_state_generation_cancel_button_click)

    def start_generation(
        self,
        num_qubits_per_generated_state: int,
        worker_send_queue_batch_size: int = DEFAULT_MEDIUM_QUEUE_SIZE,
    ) -> None:
        if worker_send_queue_batch_size < 1 or num_qubits_per_generated_state < 1:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Invalid input parameters detected",
                message_box_content=f"Expected worker send queue batch size (value={worker_send_queue_batch_size}) and number of qubits per generated state (value={num_qubits_per_generated_state}) to be positive integers!",
                is_cancellable=False,
            )
            super().reject()
            return

        # During the transition from Python 2 to 3 the two python integer types int and long were unified and the latter renamed to the former by PEP 237 (https://peps.python.org/pep-0237/). However,
        # the maximum value of a QProgressBar is capped to a 32bit integer thus we need to manually check whether our python integer fits into an 32bit integer. Otherwise, an error would be raised when
        # attempting to set the QProgressVar maximum value.
        n_expected_sim_runs_to_generate: Final[int] = 2 ** len(
            self._data_qubits_lookup.ascendingly_ordered_data_qubits_lookup
        )
        if self._progress_bar is not None:
            if not self._can_value_can_be_used_as_progress_bar_max_value(n_expected_sim_runs_to_generate):
                # We do not ask for confirmation to close the dialog since we faulted before the input state generation started.
                super().reject()
                return

            self._progress_bar.setMinimum(0)
            self._progress_bar.setMaximum(n_expected_sim_runs_to_generate)
            self._progress_bar.setValue(0)
        else:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Required widget not found",
                message_box_content="Input states generator was initialized without a progress bar! This should not happen.",
                is_cancellable=False,
            )

        self._title_lbl.setText(f"Generating simulation runs with batch size {worker_send_queue_batch_size}!")

        # To avoid redundant comments we refer to the SimulationRunJsonImportDialog.start_import(...) function for details regarding the worker-object to perform a long running operation
        self._worker_send_queue_batch_size = worker_send_queue_batch_size
        self._worker = AllInputStatesGeneratorWorker(
            num_qubits_per_generated_state,
            self._data_qubits_lookup,
            worker_send_queue_config=QueueConfig(
                queue_instance=self._worker_send_queue, queue_batch_size=self._worker_send_queue_batch_size
            ),
        )
        self._worker_thread = QtCore.QThread()
        self._worker.moveToThread(self._worker_thread)
        self._worker.batchCompleted.connect(
            self._handle_generated_input_state_batch, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self._worker.finished.connect(
            self._handle_input_state_generator_finished, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self._worker.failed.connect(
            self._handle_input_state_generator_failure, QtCore.Qt.ConnectionType.QueuedConnection
        )

        self._worker_thread.started.connect(self._worker.start_generation, QtCore.Qt.ConnectionType.QueuedConnection)
        self._worker_thread.finished.connect(self._worker_thread.deleteLater)
        self._worker_thread.finished.connect(self._reset_workers)
        self._worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancel_button_enable_state(should_button_be_enabled=True)

    @override
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        # Ask for confirmation before closing
        if self._handle_input_state_generation_cancel_button_click():
            if not self._error_text_lbl.text():
                self.accept()
            else:
                # Avoid requiring duplicate confirmation of close operation by calling reject() function of super class instead of overridden reject function.
                super().reject()
        else:
            event.ignore()

    # Pressing the ESC key will only close the dialog but not close it thus no closeEvent will be triggered.
    @override
    def reject(self) -> None:
        if self._handle_input_state_generation_cancel_button_click():
            super().reject()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_input_state_generator_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot(float)  # type: ignore[untyped-decorator]
    def _handle_generated_input_state_batch(self, batch_generation_duration_in_seconds: float) -> None:
        if self._stop_processing_recv_batches:
            return

        n_dequeued_batch_elems: int = 0
        try:
            for _ in range(self._worker_send_queue_batch_size):
                self._shared_simulation_runs_model.add_simulation_run_model(self._worker_send_queue.get_nowait())
                n_dequeued_batch_elems += 1
        except queue.Empty:
            # The last batch generated by the worker could contain less than the expected batch size elements thus an empty queue should not be treated as an error
            pass
        except Exception as sim_run_model_addition_err:  # noqa: BLE001
            self._handle_non_recoverable_error(sim_run_model_addition_err)
            return

        self._update_progress_text_with_batch_info(n_dequeued_batch_elems, batch_generation_duration_in_seconds)
        self._accumulate_and_update_total_runtime(batch_generation_duration_in_seconds)
        self._num_generated_input_states += n_dequeued_batch_elems
        if self._progress_bar is not None:
            self._progress_bar.setValue(self._num_generated_input_states)
        self._progress_info_text_lbl.setText("")

        QtCore.QTimer.singleShot(DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, self._allow_worker_to_continue)

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_input_state_generator_finished(self, was_cancellation_requested: bool) -> None:
        info_msg: Final[str] = "Input state generator finished!"
        self._progress_info_text_lbl.setText(info_msg)
        log_info_to_console(info_msg)
        if self._progress_bar is not None:
            self._progress_bar.setVisible(False)

        # Cancelling the long running operation through a click on the cancel button of the dialog will already request a shutdown of the worker
        # and its associated thread but the same operation also needs to be execute when the worker completes successfully. However, when cancellation
        # was already requested, skip this operation.
        if not was_cancellation_requested:
            self._request_worker_cancellation()
            self._shutdown_worker_thread_and_await_completion()

        self._change_dialog_ok_button_enable_state(should_button_be_enabled=True)
        self._change_dialog_cancel_button_enable_state(should_button_be_enabled=False)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_input_state_generation_cancel_button_click(self) -> bool:
        if self._worker is None:
            return True

        if show_and_request_ok_in_optionally_cancellable_notification(
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

    def _handle_non_recoverable_error(self, err: Exception | str | None) -> None:
        self._progress_info_text_lbl.setText("")
        if err is not None:
            # We want to log the source of the error as close as possible to the origin of the actual error thus we need to skip a few stack frames
            # to determine the "source" stack frame. The skip stack frames would be (read from left to right with the leftmost stackframe being at the
            # lowest level in the stacktrace): logger (std) -> logger_utils (custom) -> update_displayed_error_text (custom) -> the current function.
            self._update_displayed_error_text(err, num_additionally_skipped_stack_frames_starting_from_this_function=2)

        try:
            self._shared_simulation_runs_model.delete_all_simulation_run_models()
        except Exception:  # noqa: BLE001
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Internal error!",
                message_box_content="Failed to delete all simulation run models during handling of non-recoverable error!\nThis should not happen, cancelling long running operation!",
                is_cancellable=False,
            )

        self._request_worker_cancellation()
        self._shutdown_worker_thread_and_await_completion()

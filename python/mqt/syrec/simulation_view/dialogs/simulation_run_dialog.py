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

from PyQt6 import QtCore, QtGui, QtWidgets

if TYPE_CHECKING:
    from PyQt6 import QtGui

    from mqt.syrec import AnnotatableQuantumComputation

    from ..simulation_run_model import QtSimulationRunModel, SimulationRunModel
    from ..workers.simulation_run_worker import SimulationRunResult

from ...logger_utils import log_info_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ..simulation_run_model import SIMULATION_RUN_IO_STATE_QT_ROLE
from ..styled_item_delegates.simulation_run_execution_styled_item_delegate import (
    SimulationRunExecutionStyledItemDelegate,
)
from ..workers.cancellable_worker_variants import QueueConfig
from ..workers.simulation_run_worker import SimulationRunWorker
from .base_progress_dialog import DEFAULT_SMALL_QUEUE_SIZE, DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, BaseProgressDialog

MODEL_UPDATE_RUNTIME_FORMAT: Final[str] = (
    "Total model update runtime [in seconds]: {total_model_update_runtime_in_seconds:f}"
)


# Instead of iterating through all rows of a QAbstractItemView (in our case the QListView displaying all simulation run models) and setting them hidden, implement a proxy model for the
# QAbstractItemView that does only display the simulation run model of interest.
class SimulationRunFilterModel(QtCore.QSortFilterProxyModel):  # type: ignore[misc]
    def __init__(self, parent: QtCore.QObject, idx_of_sim_run_model_of_interest: QtCore.QModelIndex) -> None:
        super().__init__(parent)
        self._idx_of_sim_run_model_of_interest: QtCore.QModelIndex = idx_of_sim_run_model_of_interest

    @override
    def filterAcceptsRow(self, source_row: int, _: QtCore.QModelIndex) -> bool:
        return (
            source_row == self._idx_of_sim_run_model_of_interest.row()
            if self._idx_of_sim_run_model_of_interest.isValid()
            else False
        )


class SimulationRunDialog(BaseProgressDialog[SimulationRunWorker]):
    def __init__(
        self,
        parent: QtWidgets.QWidget,
        shared_simulation_runs_model: QtSimulationRunModel,
        annotatable_quantum_computation: AnnotatableQuantumComputation,
    ) -> None:
        super().__init__(
            parent,
            shared_simulation_runs_model,
            dialog_title="Executing simulation runs...",
            optional_progress_bar_text_format="Executed simulation run %v of %m",
            create_default_layout=False,
            user_provided_dialog_size=SimulationRunDialog.get_default_big_dialog_size(),
        )
        self._annotatable_quantum_computation: Final[AnnotatableQuantumComputation] = annotatable_quantum_computation
        self._optional_filtered_shared_sim_run_model: SimulationRunFilterModel | None = None
        self._stop_at_first_output_state_mismatch: bool = False
        self._num_executed_simulation_runs: int = 0
        self._last_fetched_simulation_run_idx: int = 0
        self._total_model_update_runtime_in_seconds: float = 0

        self._sim_run_model_queue_batch_size: int = 0
        self._sim_run_model_queue: queue.SimpleQueue[SimulationRunModel | None] = queue.SimpleQueue()

        self._sim_run_result_queue_batch_size: int = 0
        self._sim_run_result_queue: queue.SimpleQueue[SimulationRunResult] = queue.SimpleQueue()

        self._dialog_button_box.accepted.connect(self.accept)
        self._dialog_button_box.rejected.connect(self._handle_simulation_runs_cancel_button_click)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self._title_lbl)
        layout.addWidget(self._progress_info_text_lbl)
        layout.addWidget(self._error_text_lbl)

        simulation_runs_list_layout = QtWidgets.QHBoxLayout()
        self._simulation_runs_list_view: QtWidgets.QListView = QtWidgets.QListView()
        self._simulation_runs_list_view.setItemDelegate(SimulationRunExecutionStyledItemDelegate())
        self._simulation_runs_list_view.setUniformItemSizes(True)
        self._simulation_runs_list_view.setResizeMode(QtWidgets.QListView.ResizeMode.Adjust)
        self._simulation_runs_list_view.setAutoFillBackground(False)
        self._simulation_runs_list_view.setSpacing(5)
        self._simulation_runs_list_view.setFlow(QtWidgets.QListView.Flow.TopToBottom)
        # By default the vertical scroll mode is set to ScrollPerItem which will prevent the user to view not displayed if the vertical viewport size is larger than the required height of the list view item.
        self._simulation_runs_list_view.setVerticalScrollMode(QtWidgets.QAbstractItemView.ScrollMode.ScrollPerPixel)
        # Select with click on item, unselect with Ctrl+Click on already selected item (see https://doc.qt.io/qt-6/qabstractitemview.html#SelectionMode-enum)
        self._simulation_runs_list_view.setSelectionMode(QtWidgets.QAbstractItemView.SelectionMode.SingleSelection)

        simulation_runs_list_scrollarea = QtWidgets.QScrollArea()
        simulation_runs_list_scrollarea.setAutoFillBackground(False)
        simulation_runs_list_scrollarea.setWidget(self._simulation_runs_list_view)
        simulation_runs_list_scrollarea.setWidgetResizable(True)
        simulation_runs_list_layout.addWidget(simulation_runs_list_scrollarea)
        layout.addLayout(simulation_runs_list_layout)
        layout.addWidget(self._progress_bar)
        layout.addWidget(self._total_runtime_info_text_lbl)

        self._total_model_update_runtime_lbl = QtWidgets.QLabel()
        self._total_model_update_runtime_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self._total_model_update_runtime_lbl)

        layout.addWidget(self._dialog_button_box)
        self.setLayout(layout)

    def start_simulation(self, idx_of_sim_run_to_execute: QtCore.QModelIndex) -> None:
        self._sim_run_model_queue_batch_size = 1
        self._sim_run_result_queue_batch_size = 1

        if self._progress_bar is not None:
            self._progress_bar.setVisible(False)

        self._optional_filtered_shared_sim_run_model = SimulationRunFilterModel(self, idx_of_sim_run_to_execute)
        self._optional_filtered_shared_sim_run_model.setSourceModel(self._shared_simulation_runs_model)
        # self._simulation_runs_list_view.setModel(self._shared_simulation_runs_model)
        self._simulation_runs_list_view.setModel(self._optional_filtered_shared_sim_run_model)
        log_info_to_console(f"Starting execution of simulation run (index: {idx_of_sim_run_to_execute.row()})")
        self._title_lbl.setText(f"Executing simulation run {idx_of_sim_run_to_execute.row()}!")
        self._perform_single_sim_run_execution(idx_of_sim_run_to_execute)
        self._change_dialog_ok_button_enable_state(True)

    def start_simulations(
        self,
        stop_at_first_output_state_mismatch: bool,
        sim_run_model_queue_batch_size: int = DEFAULT_SMALL_QUEUE_SIZE,
        sim_run_result_queue_batch_size: int = DEFAULT_SMALL_QUEUE_SIZE,
    ) -> None:
        expected_input_state_size: Final[int] = self._annotatable_quantum_computation.num_data_qubits
        if sim_run_model_queue_batch_size < 1 or sim_run_result_queue_batch_size < 1 or expected_input_state_size < 1:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Invalid input parameters detected",
                message_box_content=f"Expected simulation run model queue batch size (value={sim_run_model_queue_batch_size}), simulation run result queue batch size (value={sim_run_result_queue_batch_size}) as well as the expected input state size (value={expected_input_state_size}) to be positive integers!",
                is_cancellable=False,
            )
            super().reject()
            return

        self._sim_run_model_queue_batch_size = sim_run_model_queue_batch_size
        self._sim_run_result_queue_batch_size = sim_run_result_queue_batch_size

        self._simulation_runs_list_view.setModel(self._shared_simulation_runs_model)
        self._stop_at_first_output_state_mismatch = stop_at_first_output_state_mismatch
        log_info_to_console(
            f"Starting execution of simulation runs, stopping after first output mismatch flag is set to {self._stop_at_first_output_state_mismatch}"
        )

        expected_total_num_simulation_runs: Final[int] = self._shared_simulation_runs_model.rowCount(
            QtCore.QModelIndex()
        )
        self._title_lbl.setText(
            f"Executing {expected_total_num_simulation_runs} simulation runs with batch sizes (Sim. run model queue={sim_run_model_queue_batch_size}, Sim. run result queue={sim_run_result_queue_batch_size})!"
        )
        if self._progress_bar is not None:
            if not self._can_value_can_be_used_as_progress_bar_max_value(expected_total_num_simulation_runs):
                # We do not ask for confirmation to close the dialog since we faulted before the simulation run execution started.
                super().reject()
                return

            self._progress_bar.setMinimum(0)
            self._progress_bar.setMaximum(expected_total_num_simulation_runs)
            self._progress_bar.setValue(0)
        else:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Required widget not found",
                message_box_content="Simulation run dialog was initialized without a progress bar! This should not happen.",
                is_cancellable=False,
            )

        if not self._reset_previous_simulation_runs():
            return

        # To avoid redundant comments we refer to the SimulationRunJsonImportDialog.start_import(...) function for details regarding the worker-object to perform a long running operation
        self._worker = SimulationRunWorker(
            self._annotatable_quantum_computation,
            expected_input_state_size,
            self._stop_at_first_output_state_mismatch,
            worker_recv_queue_config=QueueConfig(
                queue_instance=self._sim_run_model_queue, queue_batch_size=sim_run_model_queue_batch_size
            ),
            worker_send_queue_config=QueueConfig(
                queue_instance=self._sim_run_result_queue, queue_batch_size=sim_run_result_queue_batch_size
            ),
        )

        self._worker_thread = QtCore.QThread()
        self._worker.moveToThread(self._worker_thread)
        self._worker_thread.started.connect(self._worker.start_simulations, QtCore.Qt.ConnectionType.QueuedConnection)
        self._worker.finished.connect(
            self._handle_all_simulation_run_executions_done, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self._worker.batchCompleted.connect(
            self._handle_simulation_run_execution_batch_done, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self._worker.requestingData.connect(
            self._enqueue_next_simulation_runs, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self._worker.failed.connect(self._handle_simulation_runs_failure, QtCore.Qt.ConnectionType.QueuedConnection)

        self._worker_thread.finished.connect(self._worker_thread.deleteLater)
        self._worker_thread.finished.connect(self._reset_workers)

        self._worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancel_button_enable_state(True)
        self._enqueue_next_simulation_runs()

    # Pressing the ESC key will only close the dialog but not close it thus no closeEvent will be triggered.
    @override
    def reject(self) -> None:
        if self._handle_simulation_runs_cancel_button_click():
            super().reject()

    @override
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        # Ask for confirmation before closing
        if self._handle_simulation_runs_cancel_button_click():
            if not self._error_text_lbl.text():
                self.accept()
            else:
                # Avoid requiring duplicate confirmation of close operation by calling reject() function of super class instead of overridden reject function.
                super().reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_all_simulation_run_executions_done(self, was_cancellation_requested: bool) -> None:
        self._progress_info_text_lbl.setText("Simulation run execution finished!")
        log_info_to_console("Simulation run execution finished!")

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

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_simulation_runs_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_simulation_runs_cancel_button_click(self) -> bool:
        if self._worker is None:
            return True

        if show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.QUESTION,
            message_box_parent=self,
            message_box_title="Cancellation of simulation runs requested!",
            message_box_content="Are you sure that you want to stop the execution of the simulation runs?",
            is_cancellable=True,
            log_contents=False,
        ):
            log_info_to_console("Cancellation of simulation run execution requested!")
            self._handle_non_recoverable_error(None)
            return True
        return False

    @QtCore.pyqtSlot(float)  # type: ignore[untyped-decorator]
    def _handle_simulation_run_execution_batch_done(self, batch_generation_duration_in_seconds: float) -> None:
        if self._stop_processing_recv_batches:
            return

        n_received_sim_run_execution_results: int = 0
        to_be_updated_sim_run_number: int = -1

        batch_results_processing_start_timestamp: Final[float] = SimulationRunWorker.get_timestamp()
        try:
            for _ in range(self._sim_run_result_queue_batch_size):
                simulation_run_result: SimulationRunResult = self._sim_run_result_queue.get_nowait()
                to_be_updated_sim_run_number = simulation_run_result.simulation_run_number
                self._shared_simulation_runs_model.update_model_using_simulation_run_result(
                    self._shared_simulation_runs_model.index(to_be_updated_sim_run_number),
                    simulation_run_result.actual_output_state,
                    simulation_run_result.do_expected_and_actual_outputs_match,
                    simulation_run_result.sim_runtime_in_ms,
                )
                n_received_sim_run_execution_results += 1
        except queue.Empty:
            # The last batch generated by the worker could contain less than the expected batch size elements thus an empty queue should not be treated as an error
            pass
        except Exception as err:
            self._handle_non_recoverable_error(
                f"Error during update of shared simulation run model with data from simulation run execution result of simulation run #{to_be_updated_sim_run_number}, reason: {SimulationRunDialog._stringify_error(err)}"
            )
            return

        batch_results_processing_duration_in_seconds: Final[float] = (
            SimulationRunWorker.calc_batch_duration_and_return_end_timestamp_in_seconds(
                batch_results_processing_start_timestamp
            ).duration
        )

        self._update_progress_text_with_batch_info(
            n_received_sim_run_execution_results, batch_generation_duration_in_seconds
        )
        self._update_total_model_runtime_and_label(batch_results_processing_duration_in_seconds)
        self._accumulate_and_update_total_runtime(batch_generation_duration_in_seconds)
        self._num_executed_simulation_runs += n_received_sim_run_execution_results
        if self._progress_bar is not None:
            self._progress_bar.setValue(self._num_executed_simulation_runs)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _enqueue_next_simulation_runs(self) -> None:
        try:
            for i in range(
                self._last_fetched_simulation_run_idx,
                self._last_fetched_simulation_run_idx + self._sim_run_model_queue_batch_size,
            ):
                to_be_enqueued_sim_run_model: SimulationRunModel | None = (
                    self._shared_simulation_runs_model.get_simulation_run_model(i)
                )
                self._last_fetched_simulation_run_idx += 1
                self._sim_run_model_queue.put(to_be_enqueued_sim_run_model)
                if to_be_enqueued_sim_run_model is None:
                    break
            # After having enqueued a new batch for the worker add a small delay before allowing the worker to produce new items
            # which should improve the responsiveness of the UI due to the delay being enqueued into the UI threads event-queue thus
            # given other events (mouse-clicks, resizes, etc.) to execute before the delayed functor is called
            QtCore.QTimer.singleShot(DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, self._allow_worker_to_continue)
        except Exception as err:
            self._handle_non_recoverable_error(
                f"Error during enqueue of new simulation runs, reason: {SimulationRunDialog._stringify_error(err)}"
            )

    def _update_total_model_runtime_and_label(self, batch_model_update_runtime_in_seconds: float) -> None:
        self._total_model_update_runtime_in_seconds += batch_model_update_runtime_in_seconds
        self._total_model_update_runtime_lbl.setText(
            MODEL_UPDATE_RUNTIME_FORMAT.format(
                total_model_update_runtime_in_seconds=self._total_model_update_runtime_in_seconds
            )
        )

    def _reset_previous_simulation_runs(self) -> bool:
        progress_info_msg: Final[str] = "Resetting previous simulation run results!"
        self._progress_info_text_lbl.setText(progress_info_msg)
        log_info_to_console(progress_info_msg)

        try:
            self._shared_simulation_runs_model.reset_prev_simulation_run_execution_results()
        except Exception as err:
            self._handle_non_recoverable_error(
                f"Error during reset of previous simulation run execution results prior to new simulation, reason: {SimulationRunDialog._stringify_error(err)}"
            )
            return False
        else:
            return True

    def _handle_non_recoverable_error(self, err: Exception | str | None) -> None:
        self._progress_info_text_lbl.setText("")
        if self._progress_bar is not None:
            self._progress_bar.setVisible(False)

        if err is not None:
            self._update_displayed_error_text(err, num_additionally_skipped_stack_frames_starting_from_this_function=2)

        self._request_worker_cancellation()
        self._shutdown_worker_thread_and_await_completion()

    def _perform_single_sim_run_execution(self, idx_of_sim_run_to_execute: QtCore.QModelIndex) -> None:
        try:
            self._shared_simulation_runs_model.reset_prev_simulation_run_execution_result(idx_of_sim_run_to_execute)

            sim_run_for_idx: Final[SimulationRunModel | None] = self._shared_simulation_runs_model.data(
                idx_of_sim_run_to_execute, SIMULATION_RUN_IO_STATE_QT_ROLE
            )
            if sim_run_for_idx is None:
                err_msg = f"Failed to fetch mode for simulation run {idx_of_sim_run_to_execute.row()}"
                self._update_displayed_error_text(
                    err_msg, num_additionally_skipped_stack_frames_starting_from_this_function=1
                )
                return

            result: Final[SimulationRunResult] = SimulationRunWorker.perform_single_sim_run_execution(
                self._annotatable_quantum_computation,
                idx_of_sim_run_to_execute.row(),
                sim_run_for_idx.input_state,
                sim_run_for_idx.expected_output_state,
            )
            self._shared_simulation_runs_model.update_model_using_simulation_run_result(
                idx_of_sim_run_to_execute,
                result.actual_output_state,
                result.do_expected_and_actual_outputs_match,
                result.sim_runtime_in_ms,
            )

            self._update_total_model_runtime_and_label(result.sim_runtime_in_ms)
            self._accumulate_and_update_total_runtime(result.sim_runtime_in_ms)
        except Exception as err:
            self._handle_non_recoverable_error(
                f"Error during reset of previous simulation run execution result of simulation run {idx_of_sim_run_to_execute.row()}, reason: {SimulationRunDialog._stringify_error(err)}"
            )

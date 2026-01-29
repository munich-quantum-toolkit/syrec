# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
from typing import TYPE_CHECKING, Final, cast

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

from PyQt6 import QtCore, QtGui, QtWidgets

if TYPE_CHECKING:
    from PyQt6 import QtGui

    from mqt import syrec

    from ..simulation_run_model import QtSimulationRunModel

from ...logger_utils import log_error_to_console, log_info_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ..styled_item_delegates.simulation_run_execution_styled_item_delegate import (
    SimulationRunExecutionStyledItemDelegate,
)
from ..workers.simulation_run_worker import SimulationRunResult, SimulationRunWorker
from .base_progress_dialog import BaseProgressDialog


class SimulationRunDialog(BaseProgressDialog[SimulationRunWorker]):
    def __init__(self, parent: QtWidgets.QWidget) -> None:
        super().__init__(
            parent,
            dialog_title="Executing simulation runs...",
            optional_progress_bar_text_format="Executed simulation run %v of %m",
            create_default_layout=False,
            user_provided_dialog_size=SimulationRunDialog.get_default_big_dialog_size(),
        )
        self.annotatable_quantum_computation: syrec.annotatable_quantum_computation | None = None
        self.shared_simulation_runs_model: QtSimulationRunModel | None = None
        self.stop_at_first_output_state_mismatch: bool = False
        self.num_executed_simulation_runs: int = 0

        self.dialog_button_box.accepted.connect(self.accept)
        self.dialog_button_box.rejected.connect(self._handle_simulation_runs_cancel_button_click)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.title_lbl)
        layout.addWidget(self.progress_info_text_lbl)
        layout.addWidget(self.error_text_lbl)

        simulation_runs_list_layout = QtWidgets.QHBoxLayout()
        self.simulation_runs_list_view: QtWidgets.QListView = QtWidgets.QListView()
        self.simulation_runs_list_view.setItemDelegate(SimulationRunExecutionStyledItemDelegate())
        self.simulation_runs_list_view.setUniformItemSizes(True)
        self.simulation_runs_list_view.setResizeMode(QtWidgets.QListView.ResizeMode.Adjust)
        self.simulation_runs_list_view.setAutoFillBackground(False)
        self.simulation_runs_list_view.setSpacing(5)
        self.simulation_runs_list_view.setFlow(QtWidgets.QListView.Flow.TopToBottom)
        # By default the vertical scroll mode is set to ScrollPerItem which will prevent the user to view not displayed if the vertical viewport size is larger than the required height of the list view item.
        self.simulation_runs_list_view.setVerticalScrollMode(QtWidgets.QAbstractItemView.ScrollMode.ScrollPerPixel)
        # Select with click on item, unselect with Ctrl+Click on already selected item (see https://doc.qt.io/qt-6/qabstractitemview.html#SelectionMode-enum)
        self.simulation_runs_list_view.setSelectionMode(QtWidgets.QAbstractItemView.SelectionMode.SingleSelection)
        # self.simulation_runs_list_view.selectionModel().selectionChanged.connect(self.handle_simulation_run_selection_change)

        simulation_runs_list_scrollarea = QtWidgets.QScrollArea()
        simulation_runs_list_scrollarea.setAutoFillBackground(False)
        simulation_runs_list_scrollarea.setWidget(self.simulation_runs_list_view)
        simulation_runs_list_scrollarea.setWidgetResizable(True)
        simulation_runs_list_layout.addWidget(simulation_runs_list_scrollarea)
        layout.addLayout(simulation_runs_list_layout)
        layout.addWidget(self.progress_bar)
        layout.addWidget(self.total_runtime_info_text_lbl)
        # layout.addStretch()
        layout.addWidget(self.dialog_button_box)
        self.setLayout(layout)

    def start_simulations(
        self,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        shared_simulation_run_model: QtSimulationRunModel,
        stop_at_first_output_state_mismatch: bool,
        batch_size: int = 100,
    ) -> None:
        self.annotatable_quantum_computation = annotatable_quantum_computation
        self.shared_simulation_runs_model = shared_simulation_run_model
        self.simulation_runs_list_view.setModel(self.shared_simulation_runs_model)
        self.stop_at_first_output_state_mismatch = stop_at_first_output_state_mismatch
        log_info_to_console(
            f"Starting execution of simulation runs, stopping after first output mismatch flag is set to {self.stop_at_first_output_state_mismatch}"
        )

        expected_input_state_size: Final[int] = self.annotatable_quantum_computation.num_data_qubits
        if batch_size <= 0 or expected_input_state_size <= 0:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Invalid input parameters detected",
                message_box_content=f"Expected batch size (value={batch_size}) as well as the expected input state size (value={expected_input_state_size}) to be positive integers!",
                is_cancellable=False,
            )
            self.reject()
            return

        expected_total_num_simulation_runs: Final[int] = shared_simulation_run_model.rowCount(QtCore.QModelIndex())
        self.title_lbl.setText(
            f"Executing {expected_total_num_simulation_runs} simulation runs with batch size {batch_size}!"
        )
        if self.progress_bar is not None:
            if not self._can_value_can_be_used_as_progress_bar_max_value(expected_total_num_simulation_runs):
                # We do not ask for confirmation to close the dialog since we faulted before the simulation run execution started.
                super().reject()
                return

            self.progress_bar.setMinimum(0)
            self.progress_bar.setMaximum(expected_total_num_simulation_runs)
            self.progress_bar.setValue(0)
        else:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Required widget not found",
                message_box_content="Simulation run dialog was initialized without a progress bar! This should not happen.",
                is_cancellable=False,
            )

        # To avoid redundant comments we refer to the SimulationRunJsonImportDialog.start_import(...) function for details regarding the worker-object to perform a long running operation
        self.worker = SimulationRunWorker(
            self.annotatable_quantum_computation,
            self.shared_simulation_runs_model,
            expected_input_state_size,
            batch_size,
            self.stop_at_first_output_state_mismatch,
        )
        self.worker_thread = QtCore.QThread()
        self.worker.moveToThread(self.worker_thread)
        self.worker_thread.started.connect(self.worker.start_simulations, QtCore.Qt.ConnectionType.QueuedConnection)
        self.worker.finished.connect(
            self._handle_all_simulation_run_executions_done, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.batchCompleted.connect(
            self._handle_simulation_run_execution_batch_done, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.failed.connect(self._handle_simulation_runs_failure, QtCore.Qt.ConnectionType.QueuedConnection)

        self.worker_thread.finished.connect(self.worker_thread.deleteLater)
        self.worker_thread.finished.connect(self._reset_workers)

        self.worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancel_button_enable_state(True)

    # Pressing the ESC key will only close the dialog but not close it thus no closeEvent will be triggered.
    @override
    def reject(self) -> None:
        if self._handle_simulation_runs_cancel_button_click():
            super().reject()

    @override
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        # Ask for confirmation before closing
        if self._handle_simulation_runs_cancel_button_click():
            if not self.error_text_lbl.text():
                self.accept()
            else:
                # Avoid requiring duplicate confirmation of close operation by calling reject() function of super class instead of overridden reject function.
                super().reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_all_simulation_run_executions_done(self, was_cancellation_requested: bool) -> None:
        self.progress_info_text_lbl.setText("Simulation run execution finished!")
        log_info_to_console("Simulation run execution finished!")

        if self.progress_bar is not None:
            self.progress_bar.setVisible(False)

        # Cancelling the long running operation through a click on the cancel button of the dialog will already request a shutdown of the worker
        # and its associated thread but the same operation also needs to be execute when the worker completes successfully. However, when cancellation
        # was already requested, skip this operation.
        if not was_cancellation_requested:
            if self.worker is not None:
                self._request_worker_cancellation()
            if self.worker_thread is not None:
                self._shutdown_worker_thread_and_await_completion()

        self._change_dialog_ok_button_enable_state(should_button_be_enabled=True)
        self._change_dialog_cancel_button_enable_state(should_button_be_enabled=False)

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_simulation_runs_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_simulation_runs_cancel_button_click(self) -> bool:
        if self.worker is None:
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

    @QtCore.pyqtSlot(float, object)  # type: ignore[untyped-decorator]
    def _handle_simulation_run_execution_batch_done(
        self, simulation_run_execution_duration_in_seconds: float, batch_data: object
    ) -> None:
        if self.stop_processing_recv_batches:
            return

        if not SimulationRunWorker.is_batch_data_list_of_expected_type(
            batch_data, SimulationRunResult, parent_widget_for_error_notification=self
        ):
            if self.worker is not None:
                self.worker.ack_batch_processed()
            return

        casted_batch_data: Final[list[SimulationRunResult]] = cast("list[SimulationRunResult]", batch_data)
        generated_batch_size: Final[int] = len(casted_batch_data)
        if self.shared_simulation_runs_model is None:
            log_error_to_console("Shared simulation runs model was not initialized during handling of batch!")
            self._handle_non_recoverable_error(None)
            return

        to_be_updated_simulation_run_number: int = 0
        try:
            for i in range(generated_batch_size):
                to_be_updated_simulation_run_number = casted_batch_data[i].simulation_run_number
                self.shared_simulation_runs_model.update_model_using_simulation_run_result(
                    self.shared_simulation_runs_model.index(to_be_updated_simulation_run_number),
                    casted_batch_data[i].actual_output_state,
                    casted_batch_data[i].do_expected_and_actual_outputs_match,
                    simulation_run_execution_duration_in_seconds * 1000
                    if simulation_run_execution_duration_in_seconds > 0
                    else 0,
                )
        except Exception as err:
            self._handle_non_recoverable_error(
                f"Error during update of shared simulation run model with data from simulation run execution result of simulation run #{to_be_updated_simulation_run_number}, reason: {SimulationRunDialog._stringify_error(err)}"
            )
            return

        if self.worker is not None:
            self.worker.ack_batch_processed()

        self._update_progress_text_with_batch_info(generated_batch_size, simulation_run_execution_duration_in_seconds)
        self._accumulate_and_update_total_runtime(simulation_run_execution_duration_in_seconds)
        self.num_executed_simulation_runs += generated_batch_size
        if self.progress_bar is not None:
            self.progress_bar.setValue(self.num_executed_simulation_runs)

    def _handle_non_recoverable_error(self, err: Exception | str | None) -> None:
        self.progress_info_text_lbl.setText("")
        if self.progress_bar is not None:
            self.progress_bar.setVisible(False)

        if err is not None:
            self._update_displayed_error_text(err, num_additionally_skipped_stack_frames_starting_from_this_function=2)

        if self.worker is not None:
            self._request_worker_cancellation()
        if self.worker_thread is not None:
            self._shutdown_worker_thread_and_await_completion()

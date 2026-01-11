# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore, QtWidgets

if TYPE_CHECKING:
    from mqt import syrec

    from .qt_simulation_run_model import QtSimulationRunModel, SimulationRunModel
    from .qt_simulation_worker import SimulationRunResult

from .qt_simulation_worker import SimulationWorker, ToBeExecutedSimulationRun
from .styled_item_delegates.qt_simulation_run_execution_styled_item_delegate import (
    SimulationRunExecutionStyledItemDelegate,
)

TOTAL_RUNTIME_TIMER_TIMEOUT_IN_MS: Final[int] = 1000
TOTAL_RUNTIME_TEXT_FORMAT: Final[str] = "Total runtime [in seconds]: {total_runtime_in_seconds:f}"


class SimulationRunDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(self, shared_simulation_run_model: QtSimulationRunModel, parent: QtWidgets.QWidget):
        super().__init__(parent)

        # TODO: Member variable could also be initialized in start_simulations
        self.simulation_runs_model = shared_simulation_run_model
        self.worker_thread: QtCore.QThread | None = None
        self.worker: SimulationWorker | None = None

        self.num_completed_simulation_runs: int = 0
        self.expected_total_num_simulation_runs: int = 0
        self.did_simulation_run_fail_due_to_failure: bool = False

        self.setModal(True)
        self.setSizeGripEnabled(True)
        self.setWindowTitle("Executing simulation runs")
        left = 0
        top = 0
        width = 400
        height = 400
        self.setGeometry(left, top, width, height)

        main_layout = QtWidgets.QVBoxLayout()
        self.setLayout(main_layout)

        simulation_runs_list_layout = QtWidgets.QHBoxLayout()
        simulation_runs_list_view: QtWidgets.QListView = QtWidgets.QListView()
        simulation_runs_list_view.setModel(self.simulation_runs_model)
        simulation_runs_list_view.setItemDelegate(SimulationRunExecutionStyledItemDelegate())  # type: ignore[no-untyped-call]
        simulation_runs_list_view.setUniformItemSizes(True)
        simulation_runs_list_view.setAutoFillBackground(False)
        simulation_runs_list_view.setSpacing(5)
        simulation_runs_list_view.setFlow(QtWidgets.QListView.Flow.TopToBottom)
        # Select with click on item, unselect with Ctrl+Click on already selected item (see https://doc.qt.io/qt-6/qabstractitemview.html#SelectionMode-enum)
        simulation_runs_list_view.setSelectionMode(QtWidgets.QAbstractItemView.SelectionMode.SingleSelection)
        # simulation_runs_list_view.selectionModel().selectionChanged.connect(self.handle_simulation_run_selection_change)

        simulation_runs_list_scrollarea = QtWidgets.QScrollArea()
        simulation_runs_list_scrollarea.setAutoFillBackground(False)
        simulation_runs_list_scrollarea.setWidget(simulation_runs_list_view)
        simulation_runs_list_scrollarea.setWidgetResizable(True)

        simulation_runs_list_layout.addItem(
            QtWidgets.QSpacerItem(
                2, 2, QtWidgets.QSizePolicy.Policy.MinimumExpanding, QtWidgets.QSizePolicy.Policy.Minimum
            )
        )
        simulation_runs_list_layout.addWidget(simulation_runs_list_scrollarea)
        simulation_runs_list_layout.addItem(
            QtWidgets.QSpacerItem(
                2, 2, QtWidgets.QSizePolicy.Policy.MinimumExpanding, QtWidgets.QSizePolicy.Policy.Minimum
            )
        )
        main_layout.addLayout(simulation_runs_list_layout)

        simulation_progress_controls_layout = QtWidgets.QVBoxLayout()
        simulation_success_progress_layout = QtWidgets.QHBoxLayout()
        # self.simulation_run_total_runtime_timer = QtWidgets.QTimer(self)
        # self.simulation_run_total_runtime_timer.timeout.connect(self.)
        # self.simulation_run_total_runtime_info_label = QtWidgets.QLabel(TOTAL_RUNTIME_TEXT_FORMAT.format(total_runtime_in_seconds=0))
        self.simulation_run_progress_bar = QtWidgets.QProgressBar()
        # For placeholder values see: https://doc.qt.io/qtforpython-6/PySide6/QtWidgets/QProgressBar.html#PySide6.QtWidgets.QProgressBar.format
        self.simulation_run_progress_bar.setFormat("Executing simulation run %v of %m")
        self.simulation_run_progress_bar.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.simulation_run_progress_lbl = QtWidgets.QLabel("")
        self.simulation_run_err_lbl = QtWidgets.QLabel("")
        self.simulation_run_err_lbl.setStyleSheet("QLabel { color : red; }")

        # simulation_progress_layout.addWidget(self.simulation_run_total_runtime_info_label)
        simulation_success_progress_layout.addWidget(self.simulation_run_progress_bar)
        simulation_success_progress_layout.addWidget(self.simulation_run_progress_lbl)
        simulation_progress_controls_layout.addLayout(simulation_success_progress_layout)
        simulation_progress_controls_layout.addWidget(self.simulation_run_err_lbl)

        # simulation_progress_layout.addStretch()
        main_layout.addLayout(simulation_progress_controls_layout)

        # TODO: One could also offer a close button in the dialog (that warns the user when closing the dialog during a simulation run execution)?
        self.dialog_button_box = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.StandardButton.Cancel)
        self.dialog_button_box.setCenterButtons(True)
        self.dialog_button_box.rejected.connect(self._request_worker_cancellation)
        main_layout.addWidget(self.dialog_button_box)

    def start_simulations(
        self,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        expected_total_num_simulation_runs: int,
        stop_at_first_output_state_mismatch: bool,
    ) -> None:
        self.num_completed_simulation_runs = 0
        self.expected_total_num_simulation_runs = expected_total_num_simulation_runs
        self.simulation_run_progress_lbl.setText("")
        self.simulation_run_progress_lbl.setText("")

        self.simulation_run_progress_bar.setMinimum(0)
        self.simulation_run_progress_bar.setMaximum(expected_total_num_simulation_runs - 1)
        self.simulation_run_progress_bar.setValue(0)
        self.simulation_run_progress_bar.setVisible(True)

        # self.simulation_run_total_runtime_timer.start(TOTAL_RUNTIME_TIMER_TIMEOUT_IN_MS)

        self.worker_thread = QtCore.QThread()
        self.worker = SimulationWorker(annotatable_quantum_computation, stop_at_first_output_state_mismatch)

        # TODO: It is recommended in the official documentation to mark slots explicitly via the QtCore.pyqtSlot() decorator:
        # see https://doc.qt.io/qtforpython-6/tutorials/basictutorial/signals_and_slots.html#the-slot-class

        # Do not block the UI thread by the potentially long running operations of the worker a new thread is started (which also has its own event loop)
        # and the worker operation moved to the latter. We also do not want to block the UI thread by executing the slots of said worker in the UI thread but
        # instead want to simply send the events to the event queue of its thread thus the QueuedConnection between the signal (here the UI thread) and the receiver (worker thread)
        # needs to be defined as a QueuedConnection (QtCore.Qt.ConnectionType.QueuedConnection).
        self.worker_thread.started.connect(self.worker.start_simulations, QtCore.Qt.ConnectionType.QueuedConnection)
        self.worker.allSimulationsDone.connect(
            self._handle_all_simulation_runs_done, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.simulationRunCompleted.connect(
            self._handle_simulation_run_done, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.simulationRunMismatchBetweenOutputStates.connect(
            self._handle_simulation_runs_stopped_after_first_failure, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.errDuringSimulationRun.connect(
            self._handle_simulation_runs_stopped_due_to_err, QtCore.Qt.ConnectionType.QueuedConnection
        )

        self.worker_thread.finished.connect(self.worker_thread.deleteLater)
        self.worker_thread.finished.connect(self._reset_workers)

        self.worker.moveToThread(self.worker_thread)
        self.worker_thread.start()
        self._enqueue_next_simulation_run(0)
        self._change_dialog_cancellation_button_enable_state(True)

    # TODO: Mark remaining member functions as private via underscore prefix?
    # TODO: Not all simulation runs are executed? (2 out of 10) but no error is printed to the console or shown in the GUI.
    def _handle_all_simulation_runs_done(self) -> None:
        # self.simulation_run_total_runtime_timer.stop()

        if self.worker_thread is not None:
            self.worker_thread.quit()
            self.worker_thread.wait()
        self._change_dialog_cancellation_button_enable_state(False)
        self.simulation_run_progress_bar.setVisible(False)

        if self.num_completed_simulation_runs == self.expected_total_num_simulation_runs:
            self.simulation_run_progress_lbl.setText(
                f"Finished all {self.expected_total_num_simulation_runs} simulation runs!"
            )
        else:
            self.simulation_run_progress_lbl.setText(
                f"Finished {self.num_completed_simulation_runs} out of all {self.expected_total_num_simulation_runs} simulation runs!"
            )

    def _handle_simulation_runs_stopped_due_to_err(self, simulation_run_num_that_failed: int, err: Exception) -> None:
        self.simulation_run_err_lbl.setText(
            f"Unexpected {err=}, {type(err)=} during execution of simulation run {simulation_run_num_that_failed}"
        )
        self._request_worker_cancellation()

    def _handle_simulation_runs_stopped_after_first_failure(
        self, simulation_run_causing_err: ToBeExecutedSimulationRun
    ) -> None:
        self._update_progress_controls(simulation_run_causing_err.simulation_run_number)
        self._request_worker_cancellation()

    def _request_worker_cancellation(self) -> None:
        if self.worker is not None:
            self.worker.request_cancellation()
            self._change_dialog_cancellation_button_enable_state(False)
            self.simulation_run_progress_bar.setVisible(False)

    def _change_dialog_cancellation_button_enable_state(self, should_button_be_enabled: bool) -> None:
        dialog_cancel_button: QtWidgets.QPushButton | None = self.dialog_button_box.button(
            QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )

        if dialog_cancel_button is None:
            return

        dialog_cancel_button.setEnabled(should_button_be_enabled)

    def closeEvent(self, _):  # noqa: N802
        self._request_worker_cancellation()

        # if self.worker_thread is not None:
        #     self.worker_thread.quit()
        #     self.worker_thread.wait()

    def _handle_simulation_run_done(self, simulation_run_result: SimulationRunResult) -> None:
        self._update_progress_controls(simulation_run_result.simulation_run_number)
        try:
            self.simulation_runs_model.update_model_using_simulation_run_result(
                self.simulation_runs_model.index(simulation_run_result.simulation_run_number),
                simulation_run_result.actual_output_state,
                simulation_run_result.do_expected_and_actual_outputs_match,
                simulation_run_result.execution_runtime_in_ms,
            )
        except ValueError as err:
            self.simulation_run_err_lbl.setText(
                f"Unexpected {err=}, {type(err)=} during update of simulation run model after successful execution of simulation run {simulation_run_result.simulation_run_number}"
            )
            self._request_worker_cancellation()
        else:
            self._enqueue_next_simulation_run(simulation_run_result.simulation_run_number + 1)

    def _enqueue_next_simulation_run(self, simulation_run_number: int) -> None:
        next_simulation_run: SimulationRunModel | None = self.simulation_runs_model.get_simulation_run_model(
            simulation_run_number
        )

        if self.worker is None:
            return
        if next_simulation_run is None:
            self._request_worker_cancellation()
        else:
            self.worker.queue_new_simulation_run(
                ToBeExecutedSimulationRun(
                    simulation_run_number, next_simulation_run.input_state, next_simulation_run.expected_output_state
                )
            )

    def _reset_workers(self) -> None:
        self.worker_thread = None
        self.worker = None

    def _update_progress_controls(self, completed_simulation_run: int) -> None:
        self.simulation_run_progress_lbl.setText(f"Completed simulation run {completed_simulation_run}")
        self.simulation_run_progress_bar.setValue(self.num_completed_simulation_runs)
        self.num_completed_simulation_runs += 1

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

TOTAL_RUNTIME_TIMER_TIMEOUT_IN_MS: Final[int] = 1000
TOTAL_RUNTIME_TEXT_FORMAT: Final[str] = "Total runtime [in seconds]: {total_runtime_in_seconds:f}"


class SimulationRunDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(self, simulation_run_model: QtSimulationRunModel, parent: QtWidgets.QWidget):
        super().__init__(parent)

        # TODO: Member variable could also be initialized in start_simulations
        self.simulation_run_model = simulation_run_model
        self.worker_thread: QtCore.QThread | None = None
        self.worker: SimulationWorker | None = None

        self.num_completed_simulation_runs: int = 0
        self.expected_total_num_simulation_runs: int = 0

        self.setModal(True)
        self.setSizeGripEnabled(True)
        self.setWindowTitle("Executing simulation runs")
        left = 0
        top = 0
        width = 400
        height = 400
        self.setGeometry(left, top, width, height)

        simulation_progress_layout = QtWidgets.QHBoxLayout()
        # self.simulation_run_total_runtime_timer = QtWidgets.QTimer(self)
        # self.simulation_run_total_runtime_timer.timeout.connect(self.)
        # self.simulation_run_total_runtime_info_label = QtWidgets.QLabel(TOTAL_RUNTIME_TEXT_FORMAT.format(total_runtime_in_seconds=0))
        self.simulation_run_progress_bar = QtWidgets.QProgressBar()
        # For placeholder values see: https://doc.qt.io/qtforpython-6/PySide6/QtWidgets/QProgressBar.html#PySide6.QtWidgets.QProgressBar.format
        self.simulation_run_progress_bar.setFormat("Executing simulation run %v of %m")
        self.simulation_run_progress_bar.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.simulation_run_progress_text = QtWidgets.QLabel("")

        # simulation_progress_layout.addWidget(self.simulation_run_total_runtime_info_label)
        simulation_progress_layout.addWidget(self.simulation_run_progress_bar)
        simulation_progress_layout.addWidget(self.simulation_run_progress_text)
        simulation_progress_layout.addStretch()

        main_layout = QtWidgets.QVBoxLayout()
        main_layout.addStretch()
        main_layout.addLayout(simulation_progress_layout)

        # TODO: One could also offer a close button in the dialog (that warns the user when closing the dialog during a simulation run execution)?
        self.dialog_button_box = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.StandardButton.Cancel)
        self.dialog_button_box.setCenterButtons(True)
        self.dialog_button_box.rejected.connect(self.request_worker_cancellation)
        main_layout.addWidget(self.dialog_button_box)
        self.setLayout(main_layout)

    def start_simulations(
        self,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        expected_total_num_simulation_runs: int,
        stop_at_first_output_state_mismatch: bool,
    ) -> None:
        self.expected_total_num_simulation_runs = expected_total_num_simulation_runs
        self.num_completed_simulation_runs = 0

        self.simulation_run_progress_text.setText("")
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
            self.handle_all_simulation_runs_done, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.simulationRunCompleted.connect(
            self.handle_simulation_run_done, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.simulationRunMismatchBetweenOutputStates.connect(
            self.handle_simulation_runs_stopped_after_first_failure, QtCore.Qt.ConnectionType.QueuedConnection
        )

        self.worker_thread.finished.connect(self.worker_thread.deleteLater)
        self.worker_thread.finished.connect(self.reset_workers)

        self.worker.moveToThread(self.worker_thread)
        self.worker_thread.start()
        self.enqueue_next_simulation_run(0)
        self.change_dialog_cancellation_button_enable_state(True)

    # TODO: Mark remaining member functions as private via underscore prefix?
    def handle_all_simulation_runs_done(self) -> None:
        # self.simulation_run_total_runtime_timer.stop()

        if self.worker_thread is not None:
            self.worker_thread.quit()
            self.worker_thread.wait()
        self.change_dialog_cancellation_button_enable_state(False)
        self.simulation_run_progress_bar.setVisible(False)

        if self.num_completed_simulation_runs == self.expected_total_num_simulation_runs:
            self.simulation_run_progress_text.setText(
                f"Finished all {self.expected_total_num_simulation_runs} simulation runs!"
            )
        else:
            self.simulation_run_progress_text.setText(
                f"Finished {self.num_completed_simulation_runs} out of all {self.expected_total_num_simulation_runs} simulation runs!"
            )

    def handle_simulation_runs_stopped_after_first_failure(
        self,
        _: SimulationRunResult,
    ) -> None:
        self.request_worker_cancellation()
        # self.simulation_run_total_runtime_timer.stop()

    def request_worker_cancellation(self) -> None:
        if self.worker is not None:
            self.worker.request_cancellation()
            self.change_dialog_cancellation_button_enable_state(False)
            self.simulation_run_progress_bar.setVisible(False)

    def change_dialog_cancellation_button_enable_state(self, should_button_be_enabled: bool) -> None:
        dialog_cancel_button: QtWidgets.QPushButton | None = self.dialog_button_box.button(
            QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )

        if dialog_cancel_button is None:
            return

        dialog_cancel_button.setEnabled(should_button_be_enabled)

    def closeEvent(self, _):  # noqa: N802
        self.request_worker_cancellation()

        # if self.worker_thread is not None:
        #     self.worker_thread.quit()
        #     self.worker_thread.wait()

    def handle_simulation_run_done(self, simulation_run_result: SimulationRunResult) -> None:
        self.update_progress_controls(simulation_run_result.simulation_run_number)
        self.enqueue_next_simulation_run(simulation_run_result.simulation_run_number + 1)

    def enqueue_next_simulation_run(self, simulation_run_number: int) -> None:
        next_simulation_run: SimulationRunModel | None = self.simulation_run_model.get_simulation_run_model(
            simulation_run_number
        )

        if self.worker is None:
            return
        if next_simulation_run is None:
            self.request_worker_cancellation()
        else:
            self.worker.queue_new_simulation_run(
                ToBeExecutedSimulationRun(
                    simulation_run_number, next_simulation_run.input_state, next_simulation_run.expected_output_state
                )
            )

    def reset_workers(self) -> None:
        self.worker_thread = None
        self.worker = None

    def update_progress_controls(self, completed_simulation_run: int) -> None:
        self.simulation_run_progress_text.setText(f"Completed simulation run {completed_simulation_run}")
        self.simulation_run_progress_bar.setValue(self.num_completed_simulation_runs)
        self.num_completed_simulation_runs += 1

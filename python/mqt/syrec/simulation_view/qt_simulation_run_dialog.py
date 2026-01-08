# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING

from PyQt6 import QtCore, QtWidgets

if TYPE_CHECKING:
    from mqt import syrec

    from .qt_simulation_run_model import QtSimulationRunModel, SimulationRunModel
    from .qt_simulation_worker import SimulationRunResult

from .qt_simulation_worker import SimulationWorker, ToBeExecutedSimulationRun


class SimulationRunDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(self, simulation_run_model: QtSimulationRunModel, parent: QtWidgets.QWidget):
        super().__init__(parent)

        self.setModal(True)
        self.setSizeGripEnabled(True)
        self.setWindowTitle("Executing simulation runs")
        main_layout = QtWidgets.QVBoxLayout()
        self.setLayout(main_layout)

        # TODO: Member variable could also be initialized in start_simulations
        self.simulation_run_model = simulation_run_model
        self.worker_thread: QtCore.QThread | None = None
        self.worker: SimulationWorker | None = None

    def start_simulations(
        self,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        stop_at_first_output_state_mismatch: bool,
    ) -> None:
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

    # TODO: Mark remaining member functions as private via underscore prefix?
    def handle_all_simulation_runs_done(self) -> None:
        if self.worker_thread is not None:
            self.worker_thread.quit()
            self.worker_thread.wait()

    def handle_simulation_runs_stopped_after_first_failure(
        self,
        _: SimulationRunResult,
    ) -> None:

        if self.worker is not None:
            self.worker.request_cancellation()

    def closeEvent(self, _):  # noqa: N802
        if self.worker is not None:
            self.worker.request_cancellation()

        # if self.worker_thread is not None:
        #     self.worker_thread.quit()
        #     self.worker_thread.wait()

    def handle_simulation_run_done(self, simulation_run_result: SimulationRunResult) -> None:
        self.enqueue_next_simulation_run(simulation_run_result.simulation_run_number + 1)

    def enqueue_next_simulation_run(self, simulation_run_number: int) -> None:
        next_simulation_run: SimulationRunModel | None = self.simulation_run_model.get_simulation_run_model(
            simulation_run_number
        )

        if self.worker is None:
            return
        if next_simulation_run is None:
            self.worker.request_cancellation()
        else:
            self.worker.queue_new_simulation_run(
                ToBeExecutedSimulationRun(
                    simulation_run_number, next_simulation_run.input_state, next_simulation_run.expected_output_state
                )
            )

    def reset_workers(self) -> None:
        self.worker_thread = None
        self.worker = None

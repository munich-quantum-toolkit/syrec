# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import queue
import time
from dataclasses import dataclass

from PyQt6 import QtCore

from mqt import syrec

# One could simplify the signal and slot declarations by defining the import: from PyQt6.QtCore import pyqtSignal as Signal, pyqtSlot as Slot


@dataclass(frozen=True)
class ToBeExecutedSimulationRun:
    simulation_run_number: int
    input_state: syrec.n_bit_values_container
    expected_output_state: syrec.n_bit_values_container | None


@dataclass(frozen=True)
class SimulationRunResult:
    simulation_run_number: int
    expected_output_state: syrec.n_bit_values_container | None
    actual_output_state: syrec.n_bit_values_container
    do_expected_and_actual_outputs_match: bool | None
    execution_runtime_in_ms: float


class SimulationWorker(QtCore.QObject):  # type: ignore[misc]
    simulation_run_completed = QtCore.pyqtSignal(SimulationRunResult, name="simulationRunCompleted")
    simulation_run_mismatch_between_output_states = QtCore.pyqtSignal(
        SimulationRunResult, name="simulationRunMismatchBetweenOutputStates"
    )
    all_simulations_done = QtCore.pyqtSignal(name="allSimulationsDone")

    def __init__(
        self,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        stop_at_first_output_state_mismatch: bool,
    ):
        super().__init__()

        self.annotatable_quantum_computation = annotatable_quantum_computation
        self.cancellation_requested: bool = False
        self.simulation_run_queue: queue.SimpleQueue[ToBeExecutedSimulationRun | None] = queue.SimpleQueue()
        self.should_stop_at_first_output_state_mismatch: bool = stop_at_first_output_state_mismatch

    # TODO: Error handling
    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_simulations(self) -> None:
        while not self.cancellation_requested:
            dequeued_element: ToBeExecutedSimulationRun | None = self.simulation_run_queue.get()
            if dequeued_element is None:
                break

            actual_output_state = syrec.n_bit_values_container(dequeued_element.input_state.size())
            simulation_execution_start_time_in_seconds: float = time.time()
            # TODO: Do something
            simulation_execution_end_time_in_seconds: float = time.time()

            do_expected_and_actual_input_states_match: bool | None = False
            simulation_execution_runtime_in_ms: float = (
                simulation_execution_end_time_in_seconds - simulation_execution_start_time_in_seconds
            ) / 1000

            simulation_run_result = SimulationRunResult(
                dequeued_element.simulation_run_number,
                dequeued_element.expected_output_state,
                actual_output_state,
                do_expected_and_actual_input_states_match,
                simulation_execution_runtime_in_ms,
            )
            if self.should_stop_at_first_output_state_mismatch and not do_expected_and_actual_input_states_match:
                self.simulation_run_mismatch_between_output_states.emit(simulation_run_result)
            else:
                self.simulation_run_completed.emit(simulation_run_result)
            # time.sleep(1)

        self.all_simulations_done.emit()

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def request_cancellation(self) -> None:
        self.cancellation_requested = True
        self.simulation_run_queue.put(None)

    # TODO: Throw exceptions on validation errors?
    @QtCore.pyqtSlot(ToBeExecutedSimulationRun)  # type: ignore[untyped-decorator]
    def queue_new_simulation_run(self, to_be_executed_simulation_run: ToBeExecutedSimulationRun) -> bool:
        if self.cancellation_requested:
            return False

        if to_be_executed_simulation_run.simulation_run_number < 0:
            return False

        if (
            to_be_executed_simulation_run.expected_output_state is not None
            and to_be_executed_simulation_run.expected_output_state.size()
            != to_be_executed_simulation_run.input_state.size()
        ):
            return False

        self.simulation_run_queue.put(to_be_executed_simulation_run)
        return True

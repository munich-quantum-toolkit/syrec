# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import time
from dataclasses import dataclass
from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore

from mqt import syrec

from ..logger_utils import log_error_to_console
from .cancellable_base_worker import CancellableBaseWorker
from .qt_simulation_run_model import SimulationRunModel

if TYPE_CHECKING:
    from .cancellable_base_worker import BatchTimestamps
    from .qt_simulation_run_model import QtSimulationRunModel


@dataclass(frozen=True)
class SimulationRunResult:
    simulation_run_number: int
    actual_output_state: syrec.n_bit_values_container
    do_expected_and_actual_outputs_match: bool | None


class SimulationRunWorker(CancellableBaseWorker):
    def __init__(
        self,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        shared_simulation_runs_model: QtSimulationRunModel,
        expected_input_state_size: int,
        batch_size: int,
        stop_at_first_output_state_mismatch: bool,
    ):
        super().__init__(do_batches_require_ack=True)
        self.batch_size = batch_size
        self.expected_input_state_size = expected_input_state_size
        self.shared_simulation_runs_model = shared_simulation_runs_model
        self.annotatable_quantum_computation = annotatable_quantum_computation
        self.should_stop_at_first_output_state_mismatch: bool = stop_at_first_output_state_mismatch

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_simulations(self) -> None:
        try:
            SimulationRunWorker._validate_parameters(self.expected_input_state_size, self.batch_size)
            self_raised_error_msg: str | None = ""

            if self.wait_on_batch_processed_acknowledgement_condition is None:
                self_raised_error_msg = "Internal batch processed acknowledgement condition was not initialized"
                log_error_to_console(self_raised_error_msg)
                self.failed.emit(ValueError(self_raised_error_msg))
                return

            batch_data: list[SimulationRunResult | None] = [None for _ in range(self.batch_size)]
            batch_start_timestamp: float = 0
            batch_timestamps: BatchTimestamps | None = None
            n_sim_runs_to_execute: Final[int] = self.shared_simulation_runs_model.rowCount(QtCore.QModelIndex())

            batch_idx: int = 0
            curr_sim_run_num: int = 0
            do_output_states_match: bool | None = None
            while not self.is_cancellation_requested() and curr_sim_run_num < n_sim_runs_to_execute:
                batch_start_timestamp = SimulationRunWorker._get_timestamp()
                for _ in range(self.batch_size):
                    if self.is_cancellation_requested() or curr_sim_run_num == n_sim_runs_to_execute:
                        break

                    curr_sim_run_model: SimulationRunModel = SimulationRunWorker._fetch_sim_model_or_throw(
                        self.shared_simulation_runs_model, curr_sim_run_num
                    )
                    curr_input_state: syrec.n_bit_values_container = curr_sim_run_model.input_state
                    expected_output_state: syrec.n_bit_values_container | None = (
                        curr_sim_run_model.expected_output_state
                    )
                    actual_output_state = syrec.n_bit_values_container(self.expected_input_state_size)
                    syrec.simple_simulation(actual_output_state, self.annotatable_quantum_computation, curr_input_state)
                    do_output_states_match = SimulationRunModel.do_output_states_match(
                        expected_output_state, actual_output_state
                    )
                    batch_data[batch_idx] = SimulationRunResult(
                        curr_sim_run_num,
                        actual_output_state,
                        do_output_states_match,
                    )

                    if (
                        self.should_stop_at_first_output_state_mismatch
                        and do_output_states_match is not None
                        and not do_output_states_match
                    ):
                        self.set_cancellation_requested_flag(True)

                    curr_sim_run_num += 1
                    batch_idx += 1
                if batch_idx > 0 and batch_idx != self.batch_size:
                    del batch_data[batch_idx:]

                batch_timestamps = SimulationRunWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                    batch_start_timestamp
                )
                self.batchCompleted.emit(batch_timestamps.duration, batch_data.copy())
                with QtCore.QMutexLocker(self.batch_ack_mutex):
                    if not self.is_cancellation_requested():
                        self.wait_on_batch_processed_acknowledgement_condition.wait(self.batch_ack_mutex)
                # An artificial delay improves the responsiveness of the UI but does not seem like the best solution. However, using
                # a delayed acknowledgement in the UI thread would increase the complexity of the implementation of the UI.
                time.sleep(0.2)

                for i in range(len(batch_data)):
                    batch_data[i] = None
                batch_idx = 0
            self.finished.emit(self.cancellation_requested)
        except Exception as error:
            self_raised_error_msg = f"Error in simulation run execution worker (curr. simulation run idx: {curr_sim_run_num}), reason: {type(error)=}, {error=}"
            log_error_to_console(self_raised_error_msg)
            self.failed.emit(error)

    @staticmethod
    def _validate_parameters(expected_input_state_size: int, batch_size: int) -> None:
        if expected_input_state_size < 1:
            msg = f"Expected state size must be larger than 0 but was actually {expected_input_state_size}"
            raise ValueError(msg)

        if batch_size < 1:
            msg = f"Batch size must be larger than 0 but was actually {batch_size}"
            raise ValueError(msg)

    @staticmethod
    def _fetch_sim_model_or_throw(sim_runs_model: QtSimulationRunModel, sim_run_num: int) -> SimulationRunModel:
        sim_run_model: SimulationRunModel | None = sim_runs_model.get_simulation_run_model(sim_run_num)

        if sim_run_model is None:
            msg = f"Failed to fetch simulation run model #{sim_run_num}"
            raise ValueError(msg)
        return sim_run_model

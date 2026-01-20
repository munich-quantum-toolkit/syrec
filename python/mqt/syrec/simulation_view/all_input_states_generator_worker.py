# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import time
from typing import Final

from PyQt6 import QtCore

from mqt import syrec

from ..logger_utils import log_error_to_console
from .cancellable_base_worker import CancellableBaseWorker
from .qt_simulation_run_model import SimulationRunModel


class AllInputStatesGeneratorWorker(CancellableBaseWorker):
    def __init__(self, expected_input_state_size: int, batch_size: int):
        super().__init__(do_batches_require_ack=True)
        self.expected_input_state_size: Final[int] = expected_input_state_size
        self.batch_size: Final[int] = batch_size

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_generation(self) -> None:
        try:
            AllInputStatesGeneratorWorker._validate_parameters(self.expected_input_state_size, self.batch_size)
            n_states_to_generate: int = 2**self.expected_input_state_size
            n_batches: int = n_states_to_generate // self.batch_size

            batch_start_timestamp: float = AllInputStatesGeneratorWorker._get_timestamp()
            batch_generation_duration: float = 0
            if self.wait_on_batch_processed_acknowledgement_condition is None:
                self.failed.emit(ValueError("Internal batch processed acknowledgement condition was not initialized"))
                return

            first_integer_encoding_first_state_of_batch: int = 0
            batch_data: list[SimulationRunModel | None] = [None for _ in range(self.batch_size)]
            for _ in range(n_batches):
                if self.is_cancellation_requested():
                    break

                for i in range(self.batch_size):
                    batch_data[i] = AllInputStatesGeneratorWorker._generate_sim_run_model_for_input_state(
                        self.expected_input_state_size, first_integer_encoding_first_state_of_batch + i
                    )
                batch_generation_duration = (
                    AllInputStatesGeneratorWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                        batch_start_timestamp
                    )
                )
                self.batchCompleted.emit(batch_generation_duration, batch_data.copy())
                with QtCore.QMutexLocker(self.batch_ack_mutex):
                    if not self.is_cancellation_requested():
                        self.wait_on_batch_processed_acknowledgement_condition.wait(self.batch_ack_mutex)

                # An artificial delay improves the responsiveness of the UI but does not seem like the best solution. However, using
                # a delayed acknowledgement in the UI thread would increase the complexity of the implementation of the UI.
                time.sleep(0.1)
                first_integer_encoding_first_state_of_batch += self.batch_size
                for i in range(self.batch_size):
                    batch_data[i] = None

            n_elems_in_last_batch: int = n_states_to_generate % self.batch_size
            if n_elems_in_last_batch != 0 and not self.is_cancellation_requested():
                last_batch_data: list[SimulationRunModel | None] = [None for _ in range(n_elems_in_last_batch)]
                for i in range(n_elems_in_last_batch):
                    last_batch_data[i] = AllInputStatesGeneratorWorker._generate_sim_run_model_for_input_state(
                        self.expected_input_state_size, first_integer_encoding_first_state_of_batch + i
                    )
                batch_generation_duration = (
                    AllInputStatesGeneratorWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                        batch_start_timestamp
                    )
                )
                self.batchCompleted.emit(batch_generation_duration, last_batch_data)
            self.finished.emit(self.cancellation_requested)
        except Exception as error:
            error_msg: Final[str] = f"Error in all input states generator worker! Reason: {type(error)=}, {error=}"
            log_error_to_console(error_msg)
            self.failed.emit(error)

    @staticmethod
    def _validate_parameters(expected_input_state_size: int, batch_size: int) -> None:
        if expected_input_state_size < 0:
            msg = f"Expected input state size must be a positive integer but was actually {expected_input_state_size}!"
            raise ValueError(msg)

        if batch_size < 1:
            msg = f"Batch size must be larger than 0 but was actually {batch_size}"
            raise ValueError(msg)

    @staticmethod
    def _generate_sim_run_model_for_input_state(
        expected_input_state_size: int, integer_defining_input_state: int
    ) -> SimulationRunModel:
        input_state = syrec.n_bit_values_container(expected_input_state_size)
        for qubit in range(expected_input_state_size):
            input_state.set(qubit, bool((integer_defining_input_state >> qubit) & 1))
        return SimulationRunModel(input_state, expected_output_state=None)

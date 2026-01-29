# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import time
from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore

from mqt import syrec

from ...logger_utils import log_error_to_console
from ..simulation_run_model import SimulationRunModel
from .cancellable_base_worker import CancellableBaseWorker

if TYPE_CHECKING:
    from .cancellable_base_worker import BatchTimestamps


class AllInputStatesGeneratorWorker(CancellableBaseWorker):
    def __init__(self, expected_input_state_size: int, batch_size: int) -> None:
        super().__init__(do_batches_require_ack=True)
        self.expected_input_state_size: Final[int] = expected_input_state_size
        self.batch_size: Final[int] = batch_size

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_generation(self) -> None:
        try:
            AllInputStatesGeneratorWorker._validate_parameters(self.expected_input_state_size, self.batch_size)
            self_raised_error_msg: str | None = None
            if self.wait_on_batch_processed_acknowledgement_condition is None:
                self_raised_error_msg = "Internal batch processed acknowledgement condition was not initialized"
                log_error_to_console(self_raised_error_msg)
                self.failed.emit(ValueError(self_raised_error_msg))
                return

            integer_encoding_input_state: int = 0
            n_states_to_generate: Final[int] = 2**self.expected_input_state_size

            batch_data: list[SimulationRunModel | None] = [None for _ in range(self.batch_size)]
            batch_start_timestamp: float = 0
            batch_timestamps: BatchTimestamps | None = None

            batch_idx: int = 0
            while not self.is_cancellation_requested() and integer_encoding_input_state < n_states_to_generate:
                batch_start_timestamp = AllInputStatesGeneratorWorker._get_timestamp()
                for _ in range(self.batch_size):
                    if self.is_cancellation_requested() or integer_encoding_input_state == n_states_to_generate:
                        break

                    batch_data[batch_idx] = AllInputStatesGeneratorWorker._generate_sim_run_model_for_input_state(
                        self.expected_input_state_size, integer_encoding_input_state
                    )
                    integer_encoding_input_state += 1
                    batch_idx += 1

                if batch_idx > 0 and batch_idx != self.batch_size:
                    del batch_data[batch_idx:]

                batch_timestamps = (
                    AllInputStatesGeneratorWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                        batch_start_timestamp
                    )
                )
                self.batchCompleted.emit(batch_timestamps.duration, batch_data.copy())
                with QtCore.QMutexLocker(self.batch_ack_mutex):
                    if not self.is_cancellation_requested():
                        self.wait_on_batch_processed_acknowledgement_condition.wait(self.batch_ack_mutex)
                # An artificial delay improves the responsiveness of the UI but does not seem like the best solution. However, using
                # a delayed acknowledgement in the UI thread would increase the complexity of the implementation of the UI.
                time.sleep(0.1)

                for i in range(len(batch_data)):
                    batch_data[i] = None
                batch_idx = 0
            self.finished.emit(self.is_cancellation_requested())
        except Exception as error:
            self_raised_error_msg = f"Error in all input states generator worker! Reason: {type(error)=}, {error=}"
            log_error_to_console(self_raised_error_msg)
            self.failed.emit(error)

    @staticmethod
    def _validate_parameters(expected_input_state_size: int, batch_size: int) -> None:
        if expected_input_state_size < 1:
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

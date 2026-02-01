# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
from typing import TYPE_CHECKING, Final

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

from PyQt6 import QtCore

from mqt import syrec

from ...logger_utils import log_error_to_console
from ..simulation_run_model import SimulationRunModel
from .cancellable_worker_variants import CancellableProducerWorker

if TYPE_CHECKING:
    from .cancellable_worker_variants import BatchTimestamps, QueueConfig


class AllInputStatesGeneratorWorker(CancellableProducerWorker[SimulationRunModel]):
    def __init__(
        self, expected_input_state_size: int, worker_send_queue_config: QueueConfig[SimulationRunModel]
    ) -> None:
        super().__init__(worker_send_queue_config)
        self.expected_input_state_size: Final[int] = expected_input_state_size

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_generation(self) -> None:
        batch_start_timestamp: float = 0
        batch_timestamps: BatchTimestamps | None = None
        integer_encoding_first_input_state_of_batch: int = 0

        try:
            self._assert_valid_user_provided_parameter_values()

            # We are assuming that the caller has validated that the 2^x operation will not overflow the maximum value of a 32 bit integer.
            n_states_to_generate: Final[int] = 2**self.expected_input_state_size
            batch_start_timestamp = AllInputStatesGeneratorWorker.get_timestamp()
            while (
                not self.is_cancellation_requested()
                and integer_encoding_first_input_state_of_batch < n_states_to_generate
            ):
                self._wait_on_cancellation_or_input_data()
                for integer_encoding_input_state in range(
                    integer_encoding_first_input_state_of_batch,
                    min(integer_encoding_first_input_state_of_batch + self.send_queue_batch_size, n_states_to_generate),
                ):
                    if self.is_cancellation_requested():
                        break

                    self.send_queue.put_nowait(
                        AllInputStatesGeneratorWorker._generate_sim_run_model_for_input_state(
                            self.expected_input_state_size, integer_encoding_input_state
                        )
                    )

                if self.is_cancellation_requested():
                    break

                # The addition operation will produce the wrong integer encoding the next input state in case of an cancellation request but this is ok since
                # the cancellation also stops the generation of further input states.
                integer_encoding_first_input_state_of_batch += self.send_queue_batch_size

                batch_timestamps = (
                    AllInputStatesGeneratorWorker.calc_batch_duration_and_return_end_timestamp_in_seconds(
                        batch_start_timestamp
                    )
                )
                self.batchCompleted.emit(batch_timestamps.duration)
                batch_start_timestamp = batch_timestamps.end
            self.finished.emit(self.is_cancellation_requested())
        except Exception as error:
            self_raised_error_msg = f"Error in all input states generator worker! Reason: {type(error)=}, {error=}"
            log_error_to_console(self_raised_error_msg)
            self.failed.emit(error)

    @override
    def _assert_valid_user_provided_parameter_values(self) -> None:
        super()._assert_valid_user_provided_parameter_values()
        if self.expected_input_state_size < 1:
            msg = f"Expected input state size must be a positive integer but was actually {self.expected_input_state_size}!"
            raise ValueError(msg)

    @staticmethod
    def _generate_sim_run_model_for_input_state(
        expected_input_state_size: int, integer_defining_input_state: int
    ) -> SimulationRunModel:
        input_state = syrec.n_bit_values_container(expected_input_state_size)
        for qubit in range(expected_input_state_size):
            input_state.set(qubit, bool((integer_defining_input_state >> qubit) & 1))
        return SimulationRunModel(input_state, expected_output_state=None)

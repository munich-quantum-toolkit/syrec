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
from dataclasses import dataclass
from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore

from mqt.syrec import NBitValuesContainer, simple_simulation

from ...logger_utils import log_error_to_console
from ..simulation_run_model import SimulationRunModel
from .cancellable_worker_variants import CancellableProducerConsumerWorker

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

if TYPE_CHECKING:
    from mqt.syrec import AnnotatableQuantumComputation

    from ..simulation_run_model import DataQubitsLookup
    from .cancellable_worker_variants import BatchTimestamps, QueueConfig


@dataclass(frozen=True)
class SimulationRunResult:
    simulation_run_number: int
    actual_output_state: NBitValuesContainer
    do_expected_and_actual_outputs_match: bool | None
    sim_runtime_in_ms: float


class SimulationRunWorker(CancellableProducerConsumerWorker[SimulationRunModel, SimulationRunResult]):
    def __init__(
        self,
        annotatable_quantum_computation: AnnotatableQuantumComputation,
        expected_input_state_size: int,
        data_qubits_lookup: DataQubitsLookup,
        worker_recv_queue_config: QueueConfig[SimulationRunModel | None],
        worker_send_queue_config: QueueConfig[SimulationRunResult],
        *,
        stop_at_first_output_state_mismatch: bool,
    ) -> None:
        super().__init__(
            worker_send_queue_config=worker_send_queue_config,
            worker_recv_queue_config=worker_recv_queue_config,
        )
        self._expected_input_state_size: Final[int] = expected_input_state_size
        self._annotatable_quantum_computation: Final[AnnotatableQuantumComputation] = annotatable_quantum_computation
        self._data_qubits_lookup: Final[DataQubitsLookup] = data_qubits_lookup
        self._should_stop_at_first_output_state_mismatch: Final[bool] = stop_at_first_output_state_mismatch

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_simulations(self) -> None:
        curr_sim_run_num: int = 0
        request_more_queue_size_threshold: Final[int] = int(self._recv_queue_batch_size * 0.2)

        batch_start_timestamp: float = 0
        batch_timestamps: BatchTimestamps | None = None

        found_outputs_mismatch: bool = False
        has_reached_end_sentinel: bool = False

        try:
            # TODO: Validate data qubits lookup
            self._assert_valid_user_provided_parameter_values()
            batch_start_timestamp = SimulationRunWorker.get_timestamp()
            n_remaining_batch_elems_to_generate: int = self._send_queue_batch_size

            while self._should_continue_processing(
                output_state_mismatch_flag=found_outputs_mismatch, reached_end_sentinel_flag=has_reached_end_sentinel
            ):
                self._wait_on_cancellation_or_input_data()

                one_time_request_new_data_flag: bool = False
                for _ in range(self._send_queue_batch_size):
                    if (
                        not self._should_continue_processing(
                            output_state_mismatch_flag=found_outputs_mismatch,
                            reached_end_sentinel_flag=has_reached_end_sentinel,
                        )
                        or n_remaining_batch_elems_to_generate < 0
                    ):
                        break

                    dequeued_sim_run_model: SimulationRunModel | None = None
                    try:
                        dequeued_sim_run_model = self.recv_queue.get(block=False)
                    except queue.Empty:
                        self.requestingData.emit()
                        break

                    has_reached_end_sentinel = dequeued_sim_run_model is None
                    # We use an element that is None as the sentinel value of the receive queue (i.e. dequeueing None means that we have reached processed the last enqueued element from the sender)
                    if has_reached_end_sentinel:
                        break

                    if (
                        not one_time_request_new_data_flag
                        and self.recv_queue.qsize() < request_more_queue_size_threshold
                    ):
                        self.requestingData.emit()
                        # The sender could take some time to produce new data so we do not want to repeatedly trigger this process by emitting the associated signal
                        # but only notify the sender once in the current loop. This can still trigger multiple signal emits depending on how fast the sender enqueues elements
                        # in the receive queue but will at least limit the signal emits for the current number of remaining queue elements.
                        one_time_request_new_data_flag = True

                    # The mypy type-checker does not seem to infer that the dequeued simulation run model should be not None at this point since we already covered
                    # the None case in our check for the sentinel value
                    assert dequeued_sim_run_model is not None
                    sim_run_execution_result: SimulationRunResult = (
                        SimulationRunWorker.perform_single_sim_run_execution(
                            self._annotatable_quantum_computation,
                            curr_sim_run_num,
                            dequeued_sim_run_model.input_state,
                            dequeued_sim_run_model.expected_output_state,
                            self._data_qubits_lookup,
                        )
                    )
                    self.send_queue.put_nowait(sim_run_execution_result)
                    found_outputs_mismatch |= (
                        sim_run_execution_result.do_expected_and_actual_outputs_match is not None
                        and not sim_run_execution_result.do_expected_and_actual_outputs_match
                    )
                    n_remaining_batch_elems_to_generate -= 1
                    curr_sim_run_num += 1

                if self.is_cancellation_requested():
                    break

                if (
                    not (self._should_stop_at_first_output_state_mismatch and found_outputs_mismatch)
                    and n_remaining_batch_elems_to_generate > 0
                    and not has_reached_end_sentinel
                ):
                    # We dequeued all elements of the receive queue but have not reached the required batch size in the send queue to emit a new batch.
                    # Since we are expecting more elements from the sender due to not having reached the sentinel value of the receive queue we simply continue
                    # in the processing queue
                    continue

                n_remaining_batch_elems_to_generate = self._send_queue_batch_size
                batch_timestamps = SimulationRunWorker.calc_batch_duration_and_return_end_timestamp_in_seconds(
                    batch_start_timestamp
                )
                self.batchCompleted.emit(batch_timestamps.duration)
                batch_start_timestamp = batch_timestamps.end
            self.finished.emit(self.is_cancellation_requested())
        except Exception as error:
            error_msg = f"Error in simulation run execution worker (curr. simulation run idx: {curr_sim_run_num}), reason: {type(error)=}, {error=}"
            log_error_to_console(error_msg)
            self.failed.emit(error)

    def _should_continue_processing(self, *, output_state_mismatch_flag: bool, reached_end_sentinel_flag: bool) -> bool:
        return (
            not self.is_cancellation_requested()
            and (not self._should_stop_at_first_output_state_mismatch or not output_state_mismatch_flag)
            and not reached_end_sentinel_flag
        )

    @override
    def _assert_valid_user_provided_parameter_values(self) -> None:
        super()._assert_valid_user_provided_parameter_values()
        SimulationRunWorker._assert_valid_data_qubits_lookup_values(
            self._data_qubits_lookup, self._expected_input_state_size
        )

    @staticmethod
    def perform_single_sim_run_execution(
        annotatable_quantum_computation: AnnotatableQuantumComputation,
        sim_run_num: int,
        input_state: NBitValuesContainer,
        expected_output_state: NBitValuesContainer | None,
        data_qubits_lookup: DataQubitsLookup,
    ) -> SimulationRunResult:
        actual_output_state = NBitValuesContainer(input_state.size())

        sim_start_timestamp: Final[float] = SimulationRunWorker.get_timestamp()
        simple_simulation(actual_output_state, annotatable_quantum_computation, input_state)
        do_output_states_match: Final[bool | None] = SimulationRunModel.do_output_states_match(
            expected_output_state, actual_output_state, data_qubits_lookup
        )
        sim_duration_in_ms: Final[float] = (
            SimulationRunWorker.calc_batch_duration_and_return_end_timestamp_in_seconds(sim_start_timestamp).duration
            * 1000
        )
        return SimulationRunResult(sim_run_num, actual_output_state, do_output_states_match, sim_duration_in_ms)

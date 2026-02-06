# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import json
import queue
import re
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any, Final

from PyQt6 import QtCore

from ...logger_utils import log_error_to_console
from ..simulation_run_model import SimulationRunModel
from .cancellable_worker_variants import CancellableProducerConsumerWorker

if TYPE_CHECKING:
    from pathlib import Path

    from .cancellable_worker_variants import BatchTimestamps, QueueConfig


@dataclass(frozen=True)
class ExportedBatchData:
    exported_sim_runs: int
    skipped_sim_runs: int


class SimulationRunJsonExportWorker(CancellableProducerConsumerWorker[SimulationRunModel, ExportedBatchData]):
    def __init__(
        self,
        path_to_json_file: Path,
        associated_stringified_syrec_program: str,
        worker_recv_queue_config: QueueConfig[SimulationRunModel | None],
        worker_send_queue_config: QueueConfig[ExportedBatchData],
    ) -> None:
        super().__init__(
            worker_send_queue_config=worker_send_queue_config,
            worker_recv_queue_config=worker_recv_queue_config,
        )

        self._associated_stringified_syrec_program: Final[str] = associated_stringified_syrec_program
        self._path_to_json_file: Final[Path] = path_to_json_file

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_export(self) -> None:
        request_more_queue_size_threshold: Final[int] = int(self._recv_queue_batch_size * 0.2)

        batch_start_timestamp: float = 0
        batch_timestamps: BatchTimestamps | None = None

        n_remaining_sim_runs_in_batch_to_process: int = self._recv_queue_batch_size
        n_skipped_sim_runs_in_batch: int = 0
        n_exported_sim_runs_in_batch: int = 0
        has_reached_end_sentinel: bool = False
        has_exported_any_sim_run: bool = False
        try:
            self._assert_valid_user_provided_parameter_values()

            batch_start_timestamp = SimulationRunJsonExportWorker.get_timestamp()
            with self._path_to_json_file.open("w") as file:
                file.write(
                    f'{{"inputCircuit":"{SimulationRunJsonExportWorker.convert_to_single_line_string(self._associated_stringified_syrec_program)}", "simulationRuns":['
                )

                while (
                    not self.is_cancellation_requested()
                    and n_remaining_sim_runs_in_batch_to_process > 0
                    and not has_reached_end_sentinel
                ):
                    self._wait_on_cancellation_or_input_data()

                    one_time_request_new_data_flag: bool = False
                    for _ in range(self._recv_queue_batch_size):
                        if self.is_cancellation_requested() or n_remaining_sim_runs_in_batch_to_process < 0:
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

                        n_remaining_sim_runs_in_batch_to_process -= 1
                        # The mypy type-checker does not seem to infer that the dequeued simulation run model should be not None at this point since we already covered
                        # the None case in our check for the sentinel value
                        assert dequeued_sim_run_model is not None
                        if dequeued_sim_run_model.expected_output_state is None:
                            n_skipped_sim_runs_in_batch += 1
                            continue

                        if has_exported_any_sim_run:
                            file.write(",")

                        file.write(
                            json.dumps(dequeued_sim_run_model, default=SimulationRunJsonExportWorker.serialize_to_json)
                        )
                        has_exported_any_sim_run = True
                        n_exported_sim_runs_in_batch += 1

                    if self.is_cancellation_requested():
                        break

                    if n_remaining_sim_runs_in_batch_to_process > 0 and not has_reached_end_sentinel:
                        continue

                    batch_timestamps = (
                        SimulationRunJsonExportWorker.calc_batch_duration_and_return_end_timestamp_in_seconds(
                            batch_start_timestamp
                        )
                    )
                    batch_start_timestamp = batch_timestamps.end
                    self.send_queue.put_nowait(
                        ExportedBatchData(
                            exported_sim_runs=n_exported_sim_runs_in_batch, skipped_sim_runs=n_skipped_sim_runs_in_batch
                        )
                    )
                    self.batchCompleted.emit(batch_timestamps.duration)

                    n_skipped_sim_runs_in_batch = 0
                    n_exported_sim_runs_in_batch = 0
                    n_remaining_sim_runs_in_batch_to_process = self._recv_queue_batch_size

                # An error during during the serialization of the simulation runs to their .json representation will cause the content of the
                # exported to .json file to be invalid .json due to the simulation runs JSON array as well as the top level JSON object missing
                # their closing symbol.
                file.write("]}")
            self.finished.emit(self.is_cancellation_requested())
        except Exception as error:
            error_msg: Final[str] = (
                f"Error in simulaton run export worker (exported .json file could be incomplete)! Reason: {type(error)=}, {error=}"
            )
            log_error_to_console(error_msg)
            self.failed.emit(error)

    @staticmethod
    def _validate_parameters(batch_size: int) -> None:
        if batch_size < 1:
            msg = f"Batch size must be larger than 0 but was actually {batch_size}"
            raise ValueError(msg)

    @staticmethod
    def serialize_to_json(obj: Any) -> object:
        if not isinstance(obj, SimulationRunModel):
            msg = f"Cannot serialize object of {type(obj)}"
            raise TypeError(msg)

        if obj.expected_output_state is None:
            msg = "Cannot serialize simulation run with unknown expected output state"
            raise TypeError(msg)

        return {"in": str(obj.input_state), "out": str(obj.expected_output_state)}

    @staticmethod
    def convert_to_single_line_string(stringified_syrec_program: str) -> str:
        return re.sub(r"\s+", " ", stringified_syrec_program)

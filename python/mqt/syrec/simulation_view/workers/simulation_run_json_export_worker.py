# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any, Final

from PyQt6 import QtCore

from ...logger_utils import log_error_to_console
from ..simulation_run_model import SimulationRunModel
from .cancellable_base_worker import CancellableBaseWorker

if TYPE_CHECKING:
    from collections.abc import Iterable
    from pathlib import Path

    from .cancellable_base_worker import BatchTimestamps


@dataclass(frozen=True)
class ExportedBatchData:
    exported_sim_runs: int
    skipped_sim_runs: int


class SimulationRunJsonExportWorker(CancellableBaseWorker):
    def __init__(
        self,
        path_to_json_file: Path,
        associated_stringified_syrec_program: str,
        simulation_runs_to_export: Iterable[SimulationRunModel],
        export_batch_size: int,
    ):
        super().__init__(do_batches_require_ack=False)

        self.associated_stringified_syrec_program = associated_stringified_syrec_program
        self.path_to_json_file: Final[Path] = path_to_json_file
        self.simulation_runs_to_export: Iterable[SimulationRunModel] = simulation_runs_to_export
        self.export_batch_size: Final[int] = export_batch_size

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_export(self) -> None:
        try:
            SimulationRunJsonExportWorker._validate_parameters(self.export_batch_size)

            n_generated_batches: int = 0
            batch_idx: int = 0
            n_skipped_sim_runs_in_batch: int = 0
            n_exported_sim_runs_in_batch: int = 0
            with self.path_to_json_file.open("w") as file:
                file.write(
                    f'{{"inputCircuit":"{SimulationRunJsonExportWorker.convert_to_single_line_string(self.associated_stringified_syrec_program)}", "simulationRuns":['
                )
                batch_start_timestamp: float = SimulationRunJsonExportWorker._get_timestamp()
                batch_timestamps: BatchTimestamps | None = None
                for sim_run in self.simulation_runs_to_export:
                    if self.is_cancellation_requested():
                        break

                    if sim_run.expected_output_state is None:
                        n_skipped_sim_runs_in_batch += 1
                    else:
                        if n_exported_sim_runs_in_batch > 0 or (
                            n_exported_sim_runs_in_batch == 0 and n_generated_batches > 0
                        ):
                            file.write(",")
                        file.write(json.dumps(sim_run, default=SimulationRunJsonExportWorker.serialize_to_json))
                        n_exported_sim_runs_in_batch += 1

                    batch_idx += 1
                    if batch_idx == self.export_batch_size:
                        batch_timestamps = (
                            SimulationRunJsonExportWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                                batch_start_timestamp
                            )
                        )
                        batch_start_timestamp = batch_timestamps.end
                        self.batchCompleted.emit(
                            batch_timestamps.duration,
                            ExportedBatchData(n_exported_sim_runs_in_batch, n_skipped_sim_runs_in_batch),
                        )
                        batch_idx = 0
                        n_skipped_sim_runs_in_batch = 0
                        n_exported_sim_runs_in_batch = 0
                        n_generated_batches += 1
                # An error during during the serialization of the simulation runs to their .json representation will cause the content of the
                # exported to .json file to be invalid .json due to the simulation runs JSON array as well as the top level JSON object missing
                # their closing symbol.
                file.write("]}")

            if batch_idx > 0 and not self.is_cancellation_requested():
                batch_timestamps = (
                    SimulationRunJsonExportWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                        batch_start_timestamp
                    )
                )
                self.batchCompleted.emit(
                    batch_timestamps.duration,
                    ExportedBatchData(batch_idx - n_skipped_sim_runs_in_batch, n_skipped_sim_runs_in_batch),
                )
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
        return json.dumps(re.sub(r"\s+", " ", stringified_syrec_program))

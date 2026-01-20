# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import json
from typing import TYPE_CHECKING, Any, Final

from PyQt6 import QtCore

from .cancellable_base_worker import CancellableBaseWorker
from .qt_simulation_run_model import SimulationRunModel

if TYPE_CHECKING:
    from collections.abc import Iterable
    from pathlib import Path


class SimulationRunJsonExportWorker(CancellableBaseWorker):
    def __init__(
        self, path_to_json_file: Path, simulation_runs_to_export: Iterable[SimulationRunModel], export_batch_size: int
    ):
        super().__init__(do_batches_require_ack=False)

        self.path_to_json_file: Final[Path] = path_to_json_file
        self.simulation_runs_to_export: Iterable[SimulationRunModel] = simulation_runs_to_export
        self.export_batch_size: Final[int] = export_batch_size

    # TODO: Pretty printing
    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_export(self) -> None:
        if self.export_batch_size < 1:
            return

        n_generated_batches: int = 0
        try:
            batch_idx: int = 0
            with self.path_to_json_file.open("w", encoding="ascii") as file:
                # file.write("{\n\t\"simulationRuns\": [\n")
                file.write('{"simulationRuns":[')
                batch_start_timestamp: float = SimulationRunJsonExportWorker._get_timestamp()
                batch_generation_duration: float = 0
                for sim_run in self.simulation_runs_to_export:
                    if self.is_cancellation_requested():
                        break

                    # if batch_idx > 0:
                    #     file.write(",\n")
                    # file.write(json.dumps(sim_run, default=SimulationRunJsonExportWorker.serialize_to_json, indent=2))

                    if batch_idx > 0 or (batch_idx == 0 and n_generated_batches > 0):
                        file.write(",")
                    file.write(json.dumps(sim_run, default=SimulationRunJsonExportWorker.serialize_to_json))

                    batch_idx += 1
                    if batch_idx == self.export_batch_size:
                        batch_generation_duration = (
                            SimulationRunJsonExportWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                                batch_start_timestamp
                            )
                        )
                        self.batchCompleted.emit(batch_generation_duration, self.export_batch_size)
                        batch_idx = 0
                        n_generated_batches += 1
                # file.write("\n\t]\n}")
                file.write("]}")

            if batch_idx > 0 and not self.is_cancellation_requested():
                batch_generation_duration = (
                    SimulationRunJsonExportWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                        batch_start_timestamp
                    )
                )
                self.batchCompleted.emit(batch_generation_duration, batch_idx)
            self.finished.emit(self.cancellation_requested)
        except Exception as err:
            self.failed.emit(err)

    @staticmethod
    def serialize_to_json(obj: Any) -> object:
        if isinstance(obj, SimulationRunModel):
            if obj.expected_output_state is None:
                return {"in": str(obj.input_state)}
            return {"in": str(obj.input_state), "out": str(obj.expected_output_state)}
        msg = f"Cannot serialize object of {type(obj)}"
        raise TypeError(msg)
